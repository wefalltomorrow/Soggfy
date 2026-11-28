"""Verify the pinned notification scheduler's static dispatch boundary.

This checks how the factory forwards a scheduler interface into a streaming
object and how notification submission and cleanup dispatch through it. It
does not identify or execute the runtime concrete scheduler implementation.
Requires pefile and Capstone.
"""
import argparse
import bisect
import hashlib
import json
from pathlib import Path
import struct
import sys


EXPECTED_SHA256 = "210dcf0de1d9641e7860a183205e20bc4d55e659994a4f73f9d9412ddd0e516f"

EXPECTED = {
    0x82D4B0: "mov r8, qword ptr [r15 + 8]",
    0x82D4B4: "mov rcx, qword ptr [r8 + 0x10]",
    0x82D4B8: "mov rax, qword ptr [rcx]",
    0x82D4BB: "mov r8d, dword ptr [r8 + 0x18]",
    0x82D4BF: "lea rdx, [rbp - 0x80]",
    0x82D4C3: "call qword ptr [rax + 8]",
    0x82D4C6: "movups xmm6, xmmword ptr [rax]",
    0x82D4C9: "movups xmm7, xmmword ptr [rax + 0x10]",
    0x82D4DC: "movaps xmmword ptr [rsp + 0x60], xmm6",
    0x82D4E1: "movaps xmmword ptr [rsp + 0x70], xmm7",
    0x82D503: "lea r9, [rsp + 0x60]",
    0x82D50B: "mov rcx, r12",
    0x82D50E: "call 0x82d7fc",
    0x82D82C: "mov qword ptr [rbp - 0x40], r9",
    0x82DD67: "mov rdx, qword ptr [rbp - 0x40]",
    0x82DD6B: "call 0x82bac0",
    0x82BAF5: "mov qword ptr [rbp - 0x30], rdx",
    0x82BE7A: "mov rax, qword ptr [rbp - 0x30]",
    0x82BE7E: "movups xmm0, xmmword ptr [rax]",
    0x82BE81: "movaps xmmword ptr [rbp + 0xa0], xmm0",
    0x82BF51: "lea rdx, [rbp + 0xa0]",
    0x82BF5C: "call 0x8310d4",
    0x831223: "mov rax, qword ptr [rdx]",
    0x831226: "mov qword ptr [r13 + 8], rax",
    0x831629: "call 0x17d24dc",
    0x17D2519: "mov rax, qword ptr [r9]",
    0x17D2524: "call qword ptr [rax + 0x60]",
    0x8398B2: "lea rcx, [rdi + 0x538]",
    0x8398CD: "call 0x17d2698",
    0x17D271D: "call qword ptr [rax + 0x68]",
    0x83415B: "call 0x17d25b8",
    0x17D25D9: "mov rbx, qword ptr [rax + 0x70]",
    0x17D25E8: "xor r8d, r8d",
    0x17D25F1: "call rbx",
    0x17D2638: "call qword ptr [rax]",
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dll", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--capstone-path", type=Path)
    args = parser.parse_args()
    if args.capstone_path:
        sys.path.insert(0, str(args.capstone_path.resolve()))

    import pefile
    from capstone import Cs, CS_ARCH_X86, CS_MODE_64

    raw = args.dll.read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    if digest != EXPECTED_SHA256:
        raise ValueError("Unsupported Spotify.dll hash; no offsets applied")

    pe = pefile.PE(data=raw)
    if pe.FILE_HEADER.Machine != 0x8664:
        raise ValueError("Expected an AMD64 PE image")
    image = pe.get_memory_mapped_image().ljust(pe.OPTIONAL_HEADER.SizeOfImage, b"\0")
    pdata = pe.OPTIONAL_HEADER.DATA_DIRECTORY[3]
    functions = sorted(
        struct.unpack_from("<II", image, offset)
        for offset in range(pdata.VirtualAddress, pdata.VirtualAddress + pdata.Size, 12)
    )
    starts = [begin for begin, _ in functions]
    disassembler = Cs(CS_ARCH_X86, CS_MODE_64)
    decoded = {}
    checks = []

    for address, expected in EXPECTED.items():
        index = bisect.bisect_right(starts, address) - 1
        if index < 0 or not functions[index][0] <= address < functions[index][1]:
            raise ValueError(f"No function range covers RVA {address:#x}")
        begin, end = functions[index]
        if begin not in decoded:
            decoded[begin] = {
                ins.address: ins
                for ins in disassembler.disasm(image[begin:end], begin)
            }
        instruction = decoded[begin].get(address)
        if instruction is None:
            raise ValueError(f"RVA {address:#x} is not an instruction boundary")
        actual = f"{instruction.mnemonic} {instruction.op_str}".strip()
        if actual != expected:
            raise ValueError(
                f"Unexpected instruction at {address:#x}: {actual!r}; expected {expected!r}"
            )
        checks.append({
            "rva": hex(address),
            "function_range": [hex(begin), hex(end)],
            "bytes": instruction.bytes.hex(),
            "instruction": actual,
        })

    text_sections = [
        section
        for section in pe.sections
        if section.Name.rstrip(b"\0") == b".text"
    ]
    if len(text_sections) != 1:
        raise ValueError("Expected one .text section")
    text_section = text_sections[0]
    text_begin = text_section.VirtualAddress
    text_end = text_begin + text_section.Misc_VirtualSize
    call_targets = {
        0x82D7FC: [0x82D50E],
        0x82BAC0: [0x82DD6B],
        0x8310D4: [0x82BF5C],
    }
    call_references = {target: [] for target in call_targets}
    scan_at = text_begin
    while True:
        call_at = image.find(b"\xe8", scan_at, text_end)
        if call_at < 0 or call_at + 5 > text_end:
            break
        scan_at = call_at + 1
        destination = call_at + 5 + struct.unpack_from("<i", image, call_at + 1)[0]
        if destination not in call_references:
            continue
        index = bisect.bisect_right(starts, call_at) - 1
        if index < 0 or not functions[index][0] <= call_at < functions[index][1]:
            raise ValueError(f"No function range covers call candidate {call_at:#x}")
        begin, end = functions[index]
        if begin not in decoded:
            decoded[begin] = {
                ins.address: ins
                for ins in disassembler.disasm(image[begin:end], begin)
            }
        instruction = decoded[begin].get(call_at)
        actual = (
            f"{instruction.mnemonic} {instruction.op_str}".strip()
            if instruction is not None
            else "not an instruction boundary"
        )
        if actual != f"call {destination:#x}":
            raise ValueError(
                f"Candidate at {call_at:#x} is not the expected direct call: {actual}"
            )
        call_references[destination].append({
            "call_rva": hex(call_at),
            "caller_function_range": [hex(begin), hex(end)],
            "bytes": instruction.bytes.hex(),
            "instruction": actual,
        })

    for target, expected_call_rvas in call_targets.items():
        observed_call_rvas = [
            int(reference["call_rva"], 16)
            for reference in call_references[target]
        ]
        if observed_call_rvas != expected_call_rvas:
            raise ValueError(
                f"Unexpected direct-call references to {target:#x}: "
                f"{call_references[target]}"
            )

    report = {
        "artifact_sha256": digest,
        "method": "Hash-pinned AMD64 PE instruction checks; no Spotify execution",
        "verified_instruction_count": len(checks),
        "verified_instructions": checks,
        "direct_call_scan": {
            "section": ".text",
            "range_rva": [hex(text_begin), hex(text_end)],
            "references": {
                hex(target): references
                for target, references in call_references.items()
            },
        },
        "scheduler_boundary": {
            "injection": (
                "The sole direct caller at 0x82d3b8 dispatches through the"
                " provider object's virtual slot +8, copies the returned"
                " 32-byte value to a local context, and passes it in r9 to"
                " factory 0x82d7fc. The factory forwards that context to"
                " 0x82bac0, which copies it before 0x8310d4 stores its first"
                " qword at streaming-object +8."
            ),
            "dispatch": (
                "Task initialization calls the injected interface's virtual"
                " slot +0x60; submission calls +0x68; cleanup obtains and calls"
                " +0x70 with r8=0, then destroys the task-side object."
            ),
            "conclusion": (
                "The pinned static path establishes provider-to-context"
                " provenance and virtual dispatch. The concrete runtime value"
                " still depends on provider selection; slot +0x70 is not shown"
                " to cancel queued work, wait for running work, or both."
            ),
            "limitation": (
                "No Spotify code was executed. Runtime selection, scheduling,"
                " callback completion, races, and exception paths remain unknown."
            ),
        },
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"Verified {len(checks)} pinned instruction locations; saved {args.output}")


if __name__ == "__main__":
    main()
