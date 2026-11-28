"""Hash-pinned static check of Spotify callback object dispatch and data flow.

Requires pefile and Capstone. It disassembles the pinned Spotify.dll only; it
does not execute client code, install hooks, or define a production identity
reader.
"""
import argparse
import bisect
import hashlib
import json
from pathlib import Path
import struct
import sys


EXPECTED_SHA256 = "210dcf0de1d9641e7860a183205e20bc4d55e659994a4f73f9d9412ddd0e516f"
OBJECT_VTABLE_RVA = 0x19eede0
PROTO_DESCRIPTOR_RVA = 0x1a1ede0
VTABLE_DESTRUCTOR_RVA = 0x82b3fc
VTABLE_METHOD_RVA = 0x852c34
DIRECT_CALL_TARGET = 0x8527c0
ONCE_INITIALIZER_RVA = 0x852064
LAZY_WRITER_RVA = 0x852d40
INIT_ONCE_BEGIN_IAT_RVA = 0x18f80a0
INIT_ONCE_COMPLETE_IAT_RVA = 0x18f80a8


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
    from capstone.x86_const import X86_REG_RIP

    raw = args.dll.read_bytes()
    artifact_sha256 = hashlib.sha256(raw).hexdigest()
    if artifact_sha256 != EXPECTED_SHA256:
        raise ValueError("Unsupported Spotify.dll hash; no offset assumptions applied")

    pe = pefile.PE(data=raw)
    if pe.FILE_HEADER.Machine != 0x8664:
        raise ValueError("Expected an AMD64 PE image")
    image = pe.get_memory_mapped_image().ljust(pe.OPTIONAL_HEADER.SizeOfImage, b"\0")
    image_base = pe.OPTIONAL_HEADER.ImageBase
    pdata = pe.OPTIONAL_HEADER.DATA_DIRECTORY[3]
    functions = sorted(
        struct.unpack_from("<II", image, offset)
        for offset in range(pdata.VirtualAddress, pdata.VirtualAddress + pdata.Size, 12)
    )
    starts = [begin for begin, _ in functions]
    disassembler = Cs(CS_ARCH_X86, CS_MODE_64)
    disassembler.detail = True
    decoded_functions = {}

    def function_instructions(address):
        index = bisect.bisect_right(starts, address) - 1
        if index < 0:
            raise ValueError(f"No unwind range covers RVA {address:#x}")
        begin, end = functions[index]
        if not begin <= address < end:
            raise ValueError(f"No unwind range covers RVA {address:#x}")
        if begin not in decoded_functions:
            decoded_functions[begin] = list(disassembler.disasm(image[begin:end], begin))
        return begin, end, decoded_functions[begin]

    def instruction_at(address):
        begin, end, decoded = function_instructions(address)
        for instruction in decoded:
            if instruction.address == address:
                return instruction, begin, end
        raise ValueError(f"RVA {address:#x} is not an instruction boundary in {begin:#x}-{end:#x}")

    expected_instructions = {
        0x82b7d5: "call 0x8527c0",
        0x82b7da: "mov rsi, rax",
        0x82b8f4: "mov qword ptr [rsp + 0x68], rsi",
        0x82b93b: "lea r8, [rsp + 0x68]",
        0x82b946: "call 0x82c788",
        0x82b94b: "xor esi, esi",
        0x82b95f: "mov rcx, rsi",
        0x82b962: "call 0x852a50",
        0x82b967: "mov edx, 0x10",
        0x82b96f: "call 0x17f0ca8",
        0x82c7c0: "mov r14, rcx",
        0x82c7c8: "mov qword ptr [rsp + 0x68], r8",
        0x82c80e: "mov rax, qword ptr [r8]",
        0x82c811: "mov qword ptr [r8], rbx",
        0x82c814: "mov qword ptr [rcx + 0x10], rax",
        0x82c896: "mov qword ptr [rbp - 0x30], r14",
        0x82c951: "lea rcx, [rbp - 0x30]",
        0x82c955: "call 0x82c1b4",
        0x82c1c9: "mov rsi, rcx",
        0x82c1e7: "mov rdx, qword ptr [rsi]",
        0x82c1ea: "mov qword ptr [r14 + 8], rdx",
        0x82c804: "lea rax, [rip + 0x11c0c0d]",
        0x82c80b: "mov qword ptr [rcx], rax",
        0x8527ed: "lea rcx, [rip + 0x119c5ec]",
        0x8527f4: "mov qword ptr [rdi], rcx",
        0x8527fb: "lea rcx, [rdi + 8]",
        0x852811: "call 0x8520d0",
        0x852191: "mov qword ptr [rdi], rbx",
        0x852194: "mov rax, rdi",
        0x852832: "mov rax, rdi",
        0x852c34: "push rbx",
        0x852c3a: "mov rcx, qword ptr [rcx + 8]",
        0x852c3e: "mov rbx, rdx",
        0x852c41: "call 0x852b48",
        0x852b66: "mov rbx, rdx",
        0x852b69: "mov rdi, rcx",
        0x852b87: "je 0x852c00",
        0x852b8c: "jne 0x852c2c",
        0x852bf7: "mov rcx, qword ptr [rdi + 0x80]",
        0x852c00: "mov rcx, qword ptr [rdi + 0x78]",
        0x852c04: "mov rdx, rbx",
        0x852c07: "call 0x852c50",
        0x852c5f: "mov rdi, rdx",
        0x852c62: "mov rbx, rcx",
        0x852c9d: "mov qword ptr [rdi], rax",
        0x852ccb: "mov qword ptr [rdi + 8], rax",
        0x852cfa: "mov qword ptr [rdi + 0x10], rax",
        0x852d29: "mov qword ptr [rdi + 0x18], rax",
        0x85207e: "call qword ptr [rip + 0x10a601c]",
        0x852086: "je 0x8520c8",
        0x852088: "cmp dword ptr [rsp + 0x40], 0",
        0x85208d: "je 0x8520b7",
        0x85209c: "mov rcx, qword ptr [rdi]",
        0x85209f: "call 0x852d40",
        0x8520a5: "xor r8d, r8d",
        0x8520a8: "xor edx, edx",
        0x8520aa: "mov rcx, rbx",
        0x8520ad: "call qword ptr [rip + 0x10a5ff5]",
        0x8520b3: "test eax, eax",
        0x852368: "mov qword ptr [rbx + 0x70], r12",
        0x85236c: "mov qword ptr [rbx + 0x78], r12",
        0x852370: "mov qword ptr [rbx + 0x80], r12",
        0x852b72: "mov qword ptr [rsp + 0x20], rcx",
        0x852b77: "add rcx, 0x70",
        0x852b7b: "lea rdx, [rsp + 0x20]",
        0x852b80: "call 0x852064",
        0x852e27: "mov rbx, qword ptr [rdi + 0x78]",
        0x852e2b: "mov qword ptr [rdi + 0x78], r14",
        0x852e47: "cmp byte ptr [rdi + 0x40], 0",
        0x852e4b: "je 0x852eb1",
        0x852e6f: "mov rbx, qword ptr [rdi + 0x80]",
        0x852e76: "mov qword ptr [rdi + 0x80], rdx",
        0x82d3e4: "mov r15, rcx",
        0x82d4b0: "mov r8, qword ptr [r15 + 8]",
        0x82d4b4: "mov rcx, qword ptr [r8 + 0x10]",
        0x82d4b8: "mov rax, qword ptr [rcx]",
        0x82d4bb: "mov r8d, dword ptr [r8 + 0x18]",
        0x82d4bf: "lea rdx, [rbp - 0x80]",
        0x82d4c3: "call qword ptr [rax + 8]",
        0x82d4c6: "movups xmm6, xmmword ptr [rax]",
        0x82d4c9: "movups xmm7, xmmword ptr [rax + 0x10]",
    }

    checked_instructions = []
    instruction_objects = {}
    for address, expected in expected_instructions.items():
        instruction, begin, end = instruction_at(address)
        actual = f"{instruction.mnemonic} {instruction.op_str}".strip()
        if actual != expected:
            raise ValueError(
                f"Unexpected instruction at {address:#x}: {actual!r}; expected {expected!r}"
            )
        instruction_objects[address] = instruction
        checked_instructions.append(
            {
                "rva": hex(address),
                "function_range": [hex(begin), hex(end)],
                "bytes": instruction.bytes.hex(),
                "instruction": actual,
            }
        )

    lea = instruction_objects[0x8527ed]
    vtable_target = lea.address + lea.size + lea.operands[1].mem.disp
    if vtable_target != OBJECT_VTABLE_RVA:
        raise ValueError(f"Unexpected object-vtable target {vtable_target:#x}")

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
    call_candidates_by_target = {
        DIRECT_CALL_TARGET: [],
        ONCE_INITIALIZER_RVA: [],
        LAZY_WRITER_RVA: [],
    }
    scan_at = text_begin
    while True:
        call_at = image.find(b"\xe8", scan_at, text_end)
        if call_at < 0 or call_at + 5 > text_end:
            break
        scan_at = call_at + 1
        destination = call_at + 5 + struct.unpack_from("<i", image, call_at + 1)[0]
        if destination in call_candidates_by_target:
            instruction, begin, end = instruction_at(call_at)
            actual = f"{instruction.mnemonic} {instruction.op_str}".strip()
            if actual != f"call {destination:#x}":
                raise ValueError(
                    f"Candidate at {call_at:#x} is not a direct-call instruction: {actual}"
                )
            call_candidates_by_target[destination].append(
                {
                    "call_rva": hex(call_at),
                    "caller_function_range": [hex(begin), hex(end)],
                    "bytes": instruction.bytes.hex(),
                    "instruction": actual,
                }
            )
    expected_callers = {
        DIRECT_CALL_TARGET: [0x82b7d5],
        ONCE_INITIALIZER_RVA: [0x852b80],
        LAZY_WRITER_RVA: [0x85209f],
    }
    for target, expected_call_rvas in expected_callers.items():
        observed_call_rvas = [
            int(item["call_rva"], 16)
            for item in call_candidates_by_target[target]
        ]
        if observed_call_rvas != expected_call_rvas:
            raise ValueError(
                f"Unexpected direct-call references to {target:#x}: "
                f"{call_candidates_by_target[target]}"
            )

    imports_by_iat_rva = {}
    for entry in getattr(pe, "DIRECTORY_ENTRY_IMPORT", []):
        library = entry.dll.decode("ascii", errors="replace")
        for imported in entry.imports:
            if imported.name is not None:
                imports_by_iat_rva[imported.address - image_base] = (
                    library,
                    imported.name.decode("ascii", errors="replace"),
                )
    expected_once_imports = {
        INIT_ONCE_BEGIN_IAT_RVA: "InitOnceBeginInitialize",
        INIT_ONCE_COMPLETE_IAT_RVA: "InitOnceComplete",
    }
    once_imports = []
    for call_rva, iat_rva in (
        (0x85207e, INIT_ONCE_BEGIN_IAT_RVA),
        (0x8520ad, INIT_ONCE_COMPLETE_IAT_RVA),
    ):
        instruction = instruction_objects[call_rva]
        operand = instruction.operands[0]
        if operand.mem.base != X86_REG_RIP:
            raise ValueError(f"Expected RIP-relative IAT call at {call_rva:#x}")
        resolved_iat_rva = instruction.address + instruction.size + operand.mem.disp
        if resolved_iat_rva != iat_rva:
            raise ValueError(
                f"Unexpected IAT slot at {call_rva:#x}: {resolved_iat_rva:#x}"
            )
        imported = imports_by_iat_rva.get(resolved_iat_rva)
        expected_name = expected_once_imports[iat_rva]
        if (
            imported is None
            or imported[0].casefold() != "kernel32.dll"
            or imported[1] != expected_name
        ):
            raise ValueError(
                f"Unexpected import at IAT RVA {resolved_iat_rva:#x}: {imported}"
            )
        once_imports.append(
            {
                "call_rva": hex(call_rva),
                "iat_rva": hex(resolved_iat_rva),
                "dll": imported[0],
                "name": imported[1],
            }
        )

    vtable_sections = []
    for section in pe.sections:
        begin = section.VirtualAddress
        end = begin + max(section.Misc_VirtualSize, section.SizeOfRawData)
        if begin <= OBJECT_VTABLE_RVA < end:
            vtable_sections.append(section)
    if len(vtable_sections) != 1:
        raise ValueError("Object-vtable RVA does not map to exactly one PE section")
    vtable_section = vtable_sections[0]
    vtable_section_name = vtable_section.Name.rstrip(b"\0").decode("ascii")
    vtable_writable = bool(vtable_section.Characteristics & 0x80000000)
    if vtable_section_name != ".rdata" or vtable_writable:
        raise ValueError("Expected the object vtable in non-writable .rdata")

    vtable_entries = struct.unpack_from("<QQ", image, OBJECT_VTABLE_RVA)
    vtable_targets = tuple(entry - image_base for entry in vtable_entries)
    if vtable_targets != (VTABLE_DESTRUCTOR_RVA, VTABLE_METHOD_RVA):
        raise ValueError(f"Unexpected object-vtable entries: {vtable_targets}")

    descriptor_context = image[0x1a1edc0 : 0x1a1ee20]
    if b"_player.proto.PlayedItems" not in descriptor_context:
        raise ValueError("Expected protobuf descriptor text at the separate descriptor RVA")

    callable_table = 0x19ed4c0
    callable_entry = struct.unpack_from("<Q", image, callable_table + 0x10)[0]
    if callable_entry - image_base != 0x82d3b8:
        raise ValueError("Callable table slot +0x10 no longer targets 0x82d3b8")

    report = {
        "artifact_sha256": artifact_sha256,
        "method": "Hash-pinned PE mapping and Capstone static disassembly; no Spotify execution or hooks",
        "verified_instruction_count": len(checked_instructions),
        "instructions": checked_instructions,
        "direct_call_scan": {
            "section": ".text",
            "range_rva": [hex(text_begin), hex(text_end)],
            "target_rva": hex(DIRECT_CALL_TARGET),
            "verified_references": call_candidates_by_target[DIRECT_CALL_TARGET],
        },
        "windows_init_once_imports": once_imports,
        "lazy_initialization": {
            "initializer_rva": hex(ONCE_INITIALIZER_RVA),
            "initializer_call_references": call_candidates_by_target[ONCE_INITIALIZER_RVA],
            "writer_rva": hex(LAZY_WRITER_RVA),
            "writer_call_references": call_candidates_by_target[LAZY_WRITER_RVA],
            "state_member_offset": "0x70",
            "constructor_zeroed_member_offsets": ["0x70", "0x78", "0x80"],
            "writer_outputs": {
                "unconditional_member_offset": "0x78",
                "conditional_member_offset": "0x80",
                "condition": "inner-object byte +0x40 is nonzero",
            },
            "conditional_inference": "The callback helper passes the inner object's +0x70 member to 0x852064. That function calls the imported InitOnceBeginInitialize and, only when it reports initialization pending, invokes 0x852d40 with the inner object before calling InitOnceComplete. The writer stores its first generated pointer at inner +0x78 and creates/stores the second at +0x80 only when byte +0x40 is nonzero. Constructor 0x8522c8 zeros +0x70, +0x78, and +0x80. These instructions establish a statically serialized lazy-initialization path before selected-field use; they do not establish runtime factory selection, the semantic identity of either value, identity correctness, complete lifetime, or synchronization of later reads/replacement.",
        },
        "object_vtable": {
            "rva": hex(OBJECT_VTABLE_RVA),
            "section": vtable_section_name,
            "section_characteristics": hex(vtable_section.Characteristics),
            "writable": vtable_writable,
            "bytes": image[OBJECT_VTABLE_RVA : OBJECT_VTABLE_RVA + 16].hex(),
            "slot_0_target_rva": hex(vtable_targets[0]),
            "slot_plus_8_target_rva": hex(vtable_targets[1]),
        },
        "separate_protobuf_descriptor": {
            "rva": hex(PROTO_DESCRIPTOR_RVA),
            "ascii_context_rva": [hex(0x1a1edc0), hex(0x1a1ee20)],
            "ascii_context": "".join(chr(value) if 32 <= value < 127 else "." for value in descriptor_context),
            "is_object_vtable_target": False,
        },
        "callable_table": {
            "rva": hex(callable_table),
            "slot_plus_0x10_target_rva": hex(callable_entry - image_base),
        },
        "conditional_inference": {
            "construction": "0x8527c0 places a pointer to the verified two-entry vtable at object +0 and stores an inner-object pointer at +8. Its sole verified direct caller passes the returned object pointer by address into 0x82c788; the constructor moves it into its own +0x10 member and clears the source slot.",
            "callback": "The retained callable captures the provider pointer. The callback reads the object at provider +0x10, obtains its vtable from object +0, reads a mode from provider +0x18, and dispatches through vtable slot +8 to 0x852c34. That method loads the inner-object pointer from object +8 and calls 0x852b48; mode 0 selects inner object +0x78 and mode 1 selects inner object +0x80. The helper fills four qwords in the 32-byte output buffer, which the callback then reads.",
            "correction": "The computed vtable target is 0x19eede0. The protobuf text _player.proto.PlayedItems is present at 0x1a1ede0, a separate RVA; the callback does not dispatch through that descriptor data.",
            "limit": "This establishes a static construction and method-call path, not the semantic type of the four-qword data, runtime selection, identity correctness, object replacement, complete lifetime, or synchronization of later reads/replacement. The separate lazy-initialization evidence verifies the one-time initialization path only. No Spotify code was executed and no runtime hooks were installed.",
        },
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(
        f"Verified {len(checked_instructions)} instruction locations, import names, "
        f"and direct-call references; saved {args.output}"
    )


if __name__ == "__main__":
    main()
