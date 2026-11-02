"""Static compatibility audit. Reads only the executable, never account data."""
import hashlib
from pathlib import Path
import struct
import sys

EXPECTED = 'd8fc8b43006118b9e51572581dad00db498acef100b45a75d01d0e3791eb63e7'


def audit(path):
    data = Path(path).read_bytes()
    assert hashlib.sha256(data).hexdigest() == EXPECTED, 'Unsupported executable hash'
    nt = struct.unpack_from('<I', data, 0x3C)[0]
    assert data[nt:nt + 4] == b'PE\0\0'
    machine, count, timestamp = struct.unpack_from('<HHI', data, nt + 4)
    assert machine == 0x14C and timestamp == 0x4510B6DB
    optional = nt + 24
    assert struct.unpack_from('<I', data, optional + 28)[0] == 0x400000
    assert struct.unpack_from('<I', data, optional + 56)[0] == 0x92B000
    table = optional + struct.unpack_from('<H', data, nt + 20)[0]
    sections = [struct.unpack_from('<IIII', data, table + i * 40 + 8) for i in range(count)]

    def offset(address):
        rva = address - 0x400000
        for virtual_size, start, raw_size, raw in sections:
            if start <= rva < start + max(virtual_size, raw_size):
                return raw + rva - start
        raise AssertionError(f'Unmapped address {address:x}')

    def signature(address, hex_bytes):
        expected = bytes.fromhex(hex_bytes)
        start = offset(address)
        assert data[start:start + len(expected)] == expected, f'Changed instruction at {address:x}'

    def call(address, destination):
        start = offset(address)
        assert data[start] == 0xE8
        assert address + 5 + struct.unpack_from('<i', data, start + 1)[0] == destination

    signature(0x5379A0, '55 8b ec 56 8b f1 83 7e 70 06 57 75 78')
    signature(0x537AA0, '55 8b ec 8b 15 6c d4 c0 00 53 8b 5d 0c 56 57 8d')
    signature(0x704120, '56 57 8b f9 e8 a7 ff ff ff 8b f0 6a 00 8b ce')
    signature(0x4E7686, '8b 80 68 0e 00 00 8a 98 2a 10 00 00')
    # The stock setter composes four low bits then sends opcode 0x2BF.
    signature(0x4E771D, '68 bf 02 00 00')
    # ReloadUI's client loop runs native teardown and complete initialization.
    call(0x495664, 0x490BD0)
    call(0x495669, 0x48FBF0)
    # Teardown saves macros, destroys macro objects, and destroys bindings.
    call(0x490C5A, 0x4F0C60)
    call(0x490C5F, 0x4EFFB0)
    call(0x490CB5, 0x4B6100)
    # Account/character binding and macro loaders use the disk/cache reader.
    call(0x4B62EF, 0x5AFE50)
    call(0x4B6300, 0x5AFE50)
    call(0x4F0A7C, 0x5AFE50)
    call(0x4F0A8D, 0x5AFE50)
    signature(0x5AFE87, '57 6a 00')  # reader opens the first file for reading
    print('PASS: pinned client hash, PE metadata, native hook ABI signatures, multibar field, and ReloadUI macro/binding call sites.')


if __name__ == '__main__':
    if len(sys.argv) != 2:
        raise SystemExit('Usage: python tests/audit_client.py PATH/TO/WoW.exe')
    audit(sys.argv[1])
