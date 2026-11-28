import json
import sys
import tempfile
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import replay_stream_dump as replay


def write_dump(folder, events, trailing=b""):
    payload = bytearray()
    rows = []
    for seq, (stream_id, codec, event, state, status, data, input_length, dropped) in enumerate(events, 1):
        offset = len(payload)
        payload.extend(data)
        rows.append({
            "seq": seq,
            "time_ms": seq * 10,
            "stream_id": stream_id,
            "epoch": 7,
            "codec": codec,
            "event": event,
            "state": state,
            "status": status,
            "offset": offset,
            "length": len(data),
            "input_length": input_length,
            "dropped_before": dropped,
            "crc32": f"{zlib.crc32(data) & 0xffffffff:08x}",
        })
    (folder / "records.jsonl").write_text(
        json.dumps({"format": replay.FORMAT, "version": replay.VERSION, "data": "payload.bin"}) + "\n" +
        "".join(json.dumps(row, separators=(",", ":")) + "\n" for row in rows), encoding="utf-8")
    (folder / "payload.bin").write_bytes(bytes(payload) + trailing)


def run():
    with tempfile.TemporaryDirectory(prefix="soggfy-stream-replay-") as temp:
        root = Path(temp)
        good = root / "good"
        good.mkdir()
        ogg_a, ogg_b, flac_a = b"OggS-ident", b"OggS-end", b"fLaC-bytes"
        write_dump(good, [
            (11, "ogg", "ogg_page", "accepted", 2, ogg_a, len(ogg_a), 0),
            (11, "ogg", "ogg_page", "replayed", 1, b"replay", 6, 0),
            (22, "flac", "flac_init", "observed", 0, b"", 0, 0),
            (22, "flac", "flac_read", "read", 0, flac_a, len(flac_a), 0),
            (11, "ogg", "ogg_page", "accepted", 4, ogg_b, len(ogg_b), 0),
            (22, "flac", "flac_complete", "complete", 1, b"", len(flac_a), 0),
        ])
        inspected = replay.inspect_dump(good)
        assert not inspected.incomplete
        outputs = replay.replay_dump(inspected, root / "good-output")
        assert len(outputs) == 2
        assert (root / "good-output" / "stream-11.ogg").read_bytes() == ogg_a + ogg_b
        assert (root / "good-output" / "stream-22.flac").read_bytes() == flac_a

        partial = root / "partial"
        partial.mkdir()
        raw = b"OggS-prefix"
        write_dump(partial, [
            (31, "ogg", "ogg_page", "accepted", 2, raw, len(raw), 0),
            (0, "none", "capture_queue_loss", "incomplete", 1, b"", 2, 0),
        ])
        inspected = replay.inspect_dump(partial)
        assert inspected.incomplete
        try:
            replay.replay_dump(inspected, root / "must-not-exist")
        except replay.DumpError as exc:
            assert "--allow-incomplete" in str(exc)
        else:
            raise AssertionError("incomplete dump unexpectedly replayed without override")
        outputs = replay.replay_dump(inspected, root / "partial-output", allow_incomplete=True)
        assert len(outputs) == 1 and outputs[0].read_bytes() == raw

        for name, mutate in (
            ("bad-crc", lambda path: (path / "payload.bin").write_bytes(b"changed")),
            ("trailing", lambda path: (path / "payload.bin").write_bytes(raw + b"orphan")),
        ):
            case = root / name
            case.mkdir()
            write_dump(case, [(41, "ogg", "ogg_page", "accepted", 4, raw, len(raw), 0)])
            mutate(case)
            try:
                replay.inspect_dump(case)
            except replay.DumpError:
                pass
            else:
                raise AssertionError(f"{name} dump unexpectedly validated")

        gap = root / "sequence-gap"
        gap.mkdir()
        write_dump(gap, [(51, "ogg", "ogg_page", "accepted", 4, raw, len(raw), 0)])
        index = (gap / "records.jsonl").read_text(encoding="utf-8").replace('"seq":1', '"seq":2')
        (gap / "records.jsonl").write_text(index, encoding="utf-8")
        try:
            replay.inspect_dump(gap)
        except replay.DumpError:
            pass
        else:
            raise AssertionError("sequence gap unexpectedly validated")

    print("PASS: stream dump schema, CRC, offsets, loss gating and Ogg/FLAC reconstruction")


if __name__ == "__main__":
    run()
