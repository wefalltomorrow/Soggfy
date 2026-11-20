#!/usr/bin/env python3
"""Validate a Soggfy stream dump and reconstruct accepted raw input streams."""

from __future__ import annotations

import argparse
import json
import re
import sys
import zlib
from dataclasses import dataclass, field
from pathlib import Path
from typing import BinaryIO


FORMAT = "soggfy-stream-dump"
VERSION = 1
MAX_PAYLOAD = 65307
MAX_PAYLOAD_FILE = 48 * 1024 * 1024
MAX_INDEX_FILE = 16 * 1024 * 1024
MAX_INDEX_LINE = 1024
HEADER_KEYS = {"format", "version", "data"}
RECORD_KEYS = {
    "seq", "time_ms", "stream_id", "epoch", "codec", "event", "state",
    "status", "offset", "length", "input_length", "dropped_before", "crc32",
}
TOKEN = re.compile(r"^[a-z0-9_]+$")
CRC = re.compile(r"^[0-9a-f]{8}$")
EVENTS = {
    "ogg_page", "ogg_restarted", "ogg_capture_rejected", "flac_init", "flac_read",
    "flac_frame", "flac_complete", "flac_eof", "flac_error", "flac_rejected",
    "flac_restarted", "stream_aborted", "capture_queue_loss", "queue_loss", "session_limit",
}


class DumpError(ValueError):
    """The dump is malformed or incomplete and cannot be replayed safely."""


@dataclass
class Stream:
    codec: str
    pieces: list[tuple[int, int]] = field(default_factory=list)
    complete: bool = False
    problems: set[str] = field(default_factory=set)


@dataclass
class Dump:
    folder: Path
    payload_path: Path
    streams: dict[int, Stream]
    global_problems: set[str]

    @property
    def incomplete(self) -> bool:
        return bool(self.global_problems or any(s.problems or not s.complete for s in self.streams.values()))

    def incomplete_reasons(self) -> list[str]:
        reasons = sorted(self.global_problems)
        for stream_id, stream in sorted(self.streams.items()):
            problems = set(stream.problems)
            if not stream.complete:
                problems.add("no_complete_terminal_event")
            reasons.extend(f"stream {stream_id}: {reason}" for reason in sorted(problems))
        return reasons


def _object_without_duplicates(pairs: list[tuple[str, object]]) -> dict[str, object]:
    result: dict[str, object] = {}
    for key, value in pairs:
        if key in result:
            raise DumpError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def _json_object(raw: bytes, label: str) -> dict[str, object]:
    try:
        value = json.loads(raw.decode("utf-8"), object_pairs_hook=_object_without_duplicates)
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise DumpError(f"invalid UTF-8 JSON in {label}: {exc}") from exc
    if not isinstance(value, dict):
        raise DumpError(f"{label} must be a JSON object")
    return value


def _integer(value: object, field_name: str, maximum: int = (1 << 64) - 1) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value < 0 or value > maximum:
        raise DumpError(f"{field_name} must be an unsigned integer")
    return value


def inspect_dump(folder: Path) -> Dump:
    folder = folder.resolve()
    index_path = folder / "records.jsonl"
    try:
        index = index_path.open("rb")
    except OSError as exc:
        raise DumpError(f"cannot open {index_path}: {exc}") from exc

    with index:
        first = index.readline(MAX_INDEX_LINE + 1)
        if not first.endswith(b"\n") or len(first) > MAX_INDEX_LINE:
            raise DumpError("missing, truncated, or oversized JSONL header")
        header = _json_object(first[:-1], "header")
        version = header.get("version")
        if (set(header) != HEADER_KEYS or header.get("format") != FORMAT or
                isinstance(version, bool) or version != VERSION):
            raise DumpError("unsupported dump header or version")
        if header.get("data") != "payload.bin":
            raise DumpError("header must name the local payload.bin file")
        payload_path = folder / "payload.bin"
        try:
            payload = payload_path.open("rb")
            payload_size = payload_path.stat().st_size
        except OSError as exc:
            raise DumpError(f"cannot open payload.bin: {exc}") from exc
        if payload_size > MAX_PAYLOAD_FILE or index_path.stat().st_size > MAX_INDEX_FILE:
            payload.close()
            raise DumpError("dump exceeds the documented per-session file limits")

        streams: dict[int, Stream] = {}
        global_problems: set[str] = set()
        cursor = 0
        expected_seq = 1
        with payload:
            while True:
                line = index.readline(MAX_INDEX_LINE + 1)
                if not line:
                    break
                if len(line) > MAX_INDEX_LINE or not line.endswith(b"\n"):
                    raise DumpError(f"truncated or oversized record line at sequence {expected_seq}")
                row = _json_object(line[:-1], f"record {expected_seq}")
                if set(row) != RECORD_KEYS:
                    raise DumpError(f"record {expected_seq} has an unexpected field set")

                seq = _integer(row["seq"], "seq")
                if seq != expected_seq:
                    raise DumpError(f"record sequence mismatch: expected {expected_seq}, found {seq}")
                expected_seq += 1
                _integer(row["time_ms"], "time_ms")
                stream_id = _integer(row["stream_id"], "stream_id")
                epoch = _integer(row["epoch"], "epoch", (1 << 32) - 1)
                status = _integer(row["status"], "status", (1 << 32) - 1)
                offset = _integer(row["offset"], "offset")
                length = _integer(row["length"], "length", MAX_PAYLOAD)
                _integer(row["input_length"], "input_length")
                dropped_before = _integer(row["dropped_before"], "dropped_before")
                codec, event, state, crc = (row[name] for name in ("codec", "event", "state", "crc32"))
                if not all(isinstance(value, str) and TOKEN.fullmatch(value) for value in (codec, event, state)):
                    raise DumpError(f"record {seq} has an invalid codec, event, or state token")
                if codec not in {"none", "ogg", "flac"}:
                    raise DumpError(f"record {seq} has an unknown codec")
                if event not in EVENTS:
                    raise DumpError(f"record {seq} has an unknown event")
                if codec == "ogg" and event not in {
                        "ogg_page", "ogg_restarted", "ogg_capture_rejected", "stream_aborted"}:
                    raise DumpError(f"record {seq} has an event incompatible with Ogg")
                if codec == "flac" and event not in {
                        "flac_init", "flac_read", "flac_frame", "flac_complete", "flac_eof",
                        "flac_error", "flac_rejected", "flac_restarted", "stream_aborted"}:
                    raise DumpError(f"record {seq} has an event incompatible with FLAC")
                if codec == "none" and event not in {"capture_queue_loss", "queue_loss", "session_limit"}:
                    raise DumpError(f"record {seq} has an event incompatible with codec none")
                if state not in {"accepted", "replayed", "rejected", "observed", "read", "complete",
                                 "truncated", "error", "incomplete"}:
                    raise DumpError(f"record {seq} has an unknown event state")
                if (stream_id == 0) != (codec == "none"):
                    raise DumpError(f"record {seq} has inconsistent stream_id and codec")
                if not isinstance(crc, str) or not CRC.fullmatch(crc):
                    raise DumpError(f"record {seq} has an invalid CRC field")
                if offset != cursor:
                    raise DumpError(f"payload offset mismatch at record {seq}: expected {cursor}, found {offset}")
                if cursor + length > payload_size:
                    raise DumpError(f"payload is truncated at record {seq}")
                data = payload.read(length)
                if len(data) != length or f"{zlib.crc32(data) & 0xffffffff:08x}" != crc:
                    raise DumpError(f"payload CRC mismatch at record {seq}")
                cursor += length

                if stream_id:
                    stream = streams.get(stream_id)
                    if stream is None:
                        stream = streams[stream_id] = Stream(codec)
                    elif stream.codec != codec:
                        raise DumpError(f"stream {stream_id} changes codec")
                    if codec == "ogg" and event == "ogg_page" and state == "accepted":
                        if status not in {2, 3, 4}:
                            raise DumpError(f"record {seq} has an invalid accepted Ogg page status")
                        if _integer(row["input_length"], "input_length") != length:
                            raise DumpError(f"record {seq} has inconsistent Ogg page length")
                        stream.pieces.append((offset, length))
                        if status == 4:  # history::Result::Complete
                            stream.complete = True
                    elif codec == "flac" and event == "flac_read" and state == "read":
                        if _integer(row["input_length"], "input_length") != length:
                            raise DumpError(f"record {seq} has inconsistent FLAC read length")
                        stream.pieces.append((offset, length))
                    if (event in {"stream_aborted", "ogg_restarted", "ogg_capture_rejected",
                                  "flac_restarted", "flac_rejected", "flac_error"}
                            or state in {"incomplete", "rejected", "error", "truncated"}):
                        stream.problems.add(f"{event}:{state}")
                    if codec == "flac" and event in {"flac_complete", "flac_eof"} and state == "complete":
                        stream.complete = True
                if dropped_before:
                    global_problems.add(f"diagnostic_queue_dropped_records:{dropped_before}")
                if event in {"queue_loss", "capture_queue_loss", "session_limit"}:
                    global_problems.add(f"{event}:{status}")

        if cursor != payload_size:
            raise DumpError(f"payload has {payload_size - cursor} unindexed trailing bytes")
    return Dump(folder, payload_path, streams, global_problems)


def replay_dump(dump: Dump, output: Path, allow_incomplete: bool = False) -> list[Path]:
    reasons = dump.incomplete_reasons()
    if reasons and not allow_incomplete:
        raise DumpError("dump is incomplete; use --allow-incomplete to inspect reconstructed prefixes: "
                        + "; ".join(reasons))
    output = output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    created: list[Path] = []
    try:
        with dump.payload_path.open("rb") as payload:
            for stream_id, stream in sorted(dump.streams.items()):
                if not stream.pieces:
                    continue
                destination = output / f"stream-{stream_id}.{stream.codec}"
                with destination.open("xb") as target:
                    for offset, length in stream.pieces:
                        payload.seek(offset)
                        remaining = length
                        while remaining:
                            chunk = payload.read(min(remaining, MAX_PAYLOAD))
                            if not chunk:
                                raise DumpError(f"payload became truncated while replaying stream {stream_id}")
                            target.write(chunk)
                            remaining -= len(chunk)
                created.append(destination)
    except OSError as exc:
        raise DumpError(f"cannot write replay output: {exc}") from exc
    return created


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dump", type=Path, help="session directory containing records.jsonl and payload.bin")
    parser.add_argument("--output", type=Path, help="output directory (default: <dump>/replayed)")
    parser.add_argument("--allow-incomplete", action="store_true",
                        help="write available stream prefixes despite explicit loss or missing completion")
    args = parser.parse_args(argv)
    try:
        dump = inspect_dump(args.dump)
        output = args.output if args.output is not None else dump.folder / "replayed"
        files = replay_dump(dump, output, args.allow_incomplete)
    except (DumpError, OSError) as exc:
        print(f"stream dump rejected: {exc}", file=sys.stderr)
        return 2
    print(f"validated {len(dump.streams)} streams; reconstructed {len(files)} raw stream(s) in {output}")
    if dump.incomplete:
        print("incomplete evidence: " + "; ".join(dump.incomplete_reasons()), file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
