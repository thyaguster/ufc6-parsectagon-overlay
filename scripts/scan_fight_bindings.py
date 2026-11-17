"""Read-only inventory of UFC 6's named fight-stat bindings."""
from __future__ import annotations

import argparse
import ctypes as ct
import struct


kernel = ct.WinDLL("kernel32", use_last_error=True)
kernel.OpenProcess.argtypes = [ct.c_ulong, ct.c_int, ct.c_ulong]
kernel.OpenProcess.restype = ct.c_void_p
kernel.ReadProcessMemory.argtypes = [ct.c_void_p, ct.c_void_p, ct.c_void_p, ct.c_size_t, ct.POINTER(ct.c_size_t)]
kernel.ReadProcessMemory.restype = ct.c_int
kernel.CloseHandle.argtypes = [ct.c_void_p]

BASE = 0x90B440000
END = 0x90B480000
OBJECT_VTABLE = 0x909C227F0


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("pid", type=int)
    parser.add_argument("--value-name", action="append", default=[])
    parser.add_argument("--prefix")
    args = parser.parse_args()
    handle = kernel.OpenProcess(0x1010, 0, args.pid)
    if not handle:
        raise OSError(ct.get_last_error(), "OpenProcess failed")

    def read(address: int, length: int) -> bytes | None:
        buffer = ct.create_string_buffer(length)
        count = ct.c_size_t()
        ok = kernel.ReadProcessMemory(handle, ct.c_void_p(address), buffer, length, ct.byref(count))
        return buffer.raw if ok and count.value == length else None

    try:
        seen: set[int] = set()
        for page in range(BASE, END, 4096):
            block = read(page, 4096)
            if block is None:
                continue
            for offset in range(0, len(block), 8):
                object_address = struct.unpack_from("<Q", block, offset)[0]
                if object_address < 0x900000000 or object_address > 0x20000000000 or object_address in seen:
                    continue
                seen.add(object_address)
                header = read(object_address, 0x68)
                if header is None or struct.unpack_from("<Q", header)[0] != OBJECT_VTABLE:
                    continue
                name_address = struct.unpack_from("<Q", header, 8)[0]
                name_bytes = read(name_address, 96)
                if not name_bytes:
                    continue
                name = name_bytes.split(b"\0", 1)[0].decode("ascii", "replace")
                if name in args.value_name:
                    data_address = struct.unpack_from("<Q", header, 0x60)[0]
                    data = read(data_address, 40)
                    values = struct.unpack_from("<II", data, 32) if data else None
                    print(f"0x{page + offset:x} {name} {values}")
                elif not args.value_name and ((args.prefix and name.startswith(args.prefix)) or
                      (not args.prefix and any(word in name.lower() for word in ("stat", "round", "judge", "fight", "strike", "control")))):
                    print(f"0x{page + offset:x} 0x{object_address:x} {name}")
    finally:
        kernel.CloseHandle(handle)


if __name__ == "__main__":
    main()
