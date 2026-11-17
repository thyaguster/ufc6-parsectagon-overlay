"""Read a bounded segment of a UFC 6 service-response blob (read-only)."""
from __future__ import annotations

import argparse
import ctypes as ct


kernel = ct.WinDLL("kernel32", use_last_error=True)
kernel.OpenProcess.argtypes = (ct.c_ulong, ct.c_int, ct.c_ulong)
kernel.OpenProcess.restype = ct.c_void_p
kernel.ReadProcessMemory.argtypes = (ct.c_void_p, ct.c_void_p, ct.c_void_p, ct.c_size_t, ct.POINTER(ct.c_size_t))
kernel.ReadProcessMemory.restype = ct.c_int
kernel.CloseHandle.argtypes = (ct.c_void_p,)


def inspect(pid: int, address: int, length: int, find: list[str]) -> None:
    if length < 1 or length > 65536:
        raise ValueError("length must be 1..65536")
    handle = kernel.OpenProcess(0x1010, 0, pid)
    if not handle:
        raise OSError(ct.get_last_error(), "OpenProcess failed")
    try:
        buffer = ct.create_string_buffer(length)
        got = ct.c_size_t()
        if not kernel.ReadProcessMemory(handle, ct.c_void_p(address), buffer, length, ct.byref(got)) or got.value != length:
            raise OSError(ct.get_last_error(), "ReadProcessMemory failed")
        data = buffer.raw
    finally:
        kernel.CloseHandle(handle)
    if find:
        for word in find:
            needle = word.encode("ascii")
            positions = []
            cursor = 0
            while (found := data.find(needle, cursor)) >= 0:
                positions.append(found)
                cursor = found + 1
            print(f"{word}: {len(positions)} occurrence(s)")
            for offset in positions[:12]:
                row = data[max(0, offset - 8):min(length, offset + len(needle) + 24)]
                print(f"  +0x{offset:x} {row.hex(' ', 1)}")
        return
    for offset in range(0, length, 16):
        row = data[offset:offset + 16]
        ascii_text = ''.join(chr(c) if 32 <= c < 127 else '.' for c in row)
        print(f"{address + offset:012x}  {row.hex(' ', 1):47s}  {ascii_text}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("pid", type=int)
    parser.add_argument("address", type=lambda x: int(x, 0))
    parser.add_argument("length", type=lambda x: int(x, 0))
    parser.add_argument("--find", action="append", default=[])
    args = parser.parse_args()
    inspect(args.pid, args.address, args.length, args.find)
