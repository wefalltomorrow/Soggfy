"""Static, pinned-client parent/functor evidence; never executes client code.

Requires pefile and capstone. Output contains bounded disassembly, not a dump.
This is an audit probe, not a supported playback-ID reader.
"""
import argparse
import bisect
import hashlib
import json
from pathlib import Path
import struct
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('dll', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--capstone-path', type=Path)
    args = parser.parse_args()
    if args.capstone_path:
        sys.path.insert(0, str(args.capstone_path.resolve()))
    import pefile
    from capstone import Cs, CS_ARCH_X86, CS_MODE_64

    expected = '210dcf0de1d9641e7860a183205e20bc4d55e659994a4f73f9d9412ddd0e516f'
    raw = args.dll.read_bytes()
    if hashlib.sha256(raw).hexdigest() != expected:
        raise ValueError('Unsupported DLL hash; no offset assumptions applied')
    pe = pefile.PE(data=raw)
    data = pe.get_memory_mapped_image().ljust(pe.OPTIONAL_HEADER.SizeOfImage, b'\0')
    base = pe.OPTIONAL_HEADER.ImageBase
    directory = pe.OPTIONAL_HEADER.DATA_DIRECTORY[3]
    functions = [struct.unpack_from('<III', data, at)[:2]
                 for at in range(directory.VirtualAddress,
                                 directory.VirtualAddress + directory.Size, 12)]
    starts = [begin for begin, _ in functions]
    dis = Cs(CS_ARCH_X86, CS_MODE_64)
    # Decode from unwind starts, never from an arbitrary byte in an instruction.
    cache = {}
    def instructions(address):
        index = bisect.bisect_right(starts, address) - 1
        begin, end = functions[index]
        if not begin <= address < end:
            raise ValueError(f'No unwind range at {address:x}')
        if begin not in cache:
            cache[begin] = list(dis.disasm(data[begin:end], begin))
        return cache[begin]

    checks = {
        0xe41e62: 'mov ecx, 0x998',
        0xe41e6c: 'mov r14, rax',
        0xe41f53: 'mov ecx, 0x118',
        0xe41f60: 'lea rax, [rip + 0xc10c99]',
        0xe41f67: 'mov qword ptr [r8], rax',
        0xe41fcc: 'mov qword ptr [rbp + 0x1b8], r8',
        0xe42155: 'lea rax, [rbp + 0x180]',
        0xe4215c: 'mov qword ptr [rsp + 0x30], rax',
        0xe42182: 'mov rcx, r14',
        0xe42185: 'call 0xe4c000',
        0xe42301: 'mov ecx, 0x118',
        0xe4230b: 'lea rcx, [rip + 0xc108ee]',
        0xe4231b: 'mov qword ptr [rax], rcx',
        0xe4c01a: 'lea rbp, [rax - 0xe8]',
        0xe4c041: 'mov rdi, rcx',
        0xe4c074: 'mov rbx, qword ptr [rbp + 0x120]',
        0xe4c11a: 'lea r15, [rdi + 0x50]',
        0xe4c126: 'mov rcx, qword ptr [rbx + 0x38]',
        0xe4c135: 'call qword ptr [rax]',
        0xe4c137: 'mov qword ptr [r15 + 0x38], rax',
        0xe52b07: 'mov rsi, rcx',
        0xe52b80: 'mov rcx, qword ptr [rsi + 0x88]',
        0xe52b93: 'lea r8, [rsi + 0x20]',
        0xe52b9c: 'call qword ptr [rax + 0x10]',
        0xe423e5: 'mov r13, r8',
        0xe42517: 'mov r8, r13',
        0xe42525: 'call 0xe5b088',
        0xe5b0b5: 'mov rsi, r8',
        0xe5b117: 'mov qword ptr [r15 + 0x78], rsi',
        0xe4c2ba: 'lea rcx, [rdi + 0x200]',
        0xe4c2cb: 'mov rax, qword ptr [rbp + 0x1c8]',
        0xe4c2d2: 'test rax, rax',
        0xe4c2d5: 'cmovne rcx, rax',
        0xe4c2d9: 'mov qword ptr [rdi + 0x208], rcx',
        0xe42b20: 'mov r13, rdx',
        0xe42e6c: 'lea rdx, [r13 + 0x3b8]',
        0xe42ed1: 'mov qword ptr [rsp + 0xd8], rdx',
        0xe42fc5: 'call 0xe41cd4',
        0xe41ce3: 'lea rbp, [rax - 0x2b8]',
        0xe41e3c: 'mov rax, qword ptr [rbp + 0x398]',
        0xe41e43: 'mov qword ptr [rbp - 0x68], rax',
        0xe41ea9: 'mov rcx, qword ptr [rbp - 0x68]',
        0xe41ead: 'mov rax, qword ptr [rcx]',
        0xe41eb0: 'mov qword ptr [rbp - 0x68], rax',
        0xe4207b: 'mov rax, qword ptr [rbp - 0x68]',
        0xe4207f: 'mov qword ptr [rsp + 0xd8], rax',
    }
    windows = []
    for address, expected_instruction in checks.items():
        decoded = instructions(address)
        index = next(i for i, ins in enumerate(decoded) if ins.address == address)
        ins = decoded[index]
        actual = f'{ins.mnemonic} {ins.op_str}'
        if actual != expected_instruction:
            raise ValueError(f'Instruction mismatch at {address:x}: {actual}')
        windows.append(dict(check_rva=hex(address), instructions=[
            dict(rva=hex(i.address), bytes=i.bytes.hex(),
                 instruction=f'{i.mnemonic} {i.op_str}')
            for i in decoded[max(0, index - 3):index + 4]]))
    table = [struct.unpack_from('<Q', data, 0x1a52c00 + i * 8)[0] - base
             for i in range(3)]
    if table[0] != 0xe422f8 or table[2] != 0xe423cc:
        raise ValueError('Unexpected functor table')
    report = dict(artifact_sha256=expected,
        mapped_sha256=hashlib.sha256(data).hexdigest(),
        method='Python/Capstone static disassembly; no client execution or REA provider evidence',
        verified_instruction_count=len(checks),
        functor_table_rva='0x1a52c00', table_entries=[hex(v) for v in table],
        windows=windows,
        conditional_inference=dict(source='parent + 0x20',
            state_pointer_member=hex(0x20 + 0x1e8),
            constructor_null_argument_state='parent + 0x200',
            upstream_read_start=hex(0x200 + 0x9b8),
            upstream_read_end_exclusive=hex(0x200 + 0x9b8 + 16),
            parent_allocation_size='0x998',
            condition='Null optional constructor state argument, same object path, and fallback unchanged before pump use'),
        limitations=[
            'Assertions verify selected instructions; dataflow interpretation still requires review',
            'Actual runtime argument, dynamic dispatch and later member mutation not established',
            'Non-null external state type, ID encoding, ownership and lifetime unresolved',
            'No observed out-of-bounds runtime read or universal incompatibility claim',
            'Constructor linear decoding is not complete control-flow recovery; jump at e4ca06 leaves the inspected range',
            'Readable memory alone does not establish object ownership or an instance identity'])
    args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(f'Verified {len(checks)} instruction checks; saved {args.output}')


if __name__ == '__main__':
    main()
