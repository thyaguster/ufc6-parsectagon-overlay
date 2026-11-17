"""Read-only, low-rate trace of cumulative UFC 6 post-fight counters."""
from __future__ import annotations

import argparse
import ctypes as ct
import csv
from pathlib import Path
import struct
import time


SLOTS = {
    "state": 0x90B444C10,
    "round": 0x90B444F08,
    "round_elapsed": 0x90B445750,
    "result": 0x90B46A820,
    "knockdowns": 0x90B46A988,
    "sig_attempted": 0x90B46A960,
    "sig_landed": 0x90B46AC30,
    "strikes_attempted": 0x90B46A9D8,
    "strikes_landed": 0x90B46A9B0,
    "td_attempted": 0x90B46AA78,
    "td_landed": 0x90B46AAA0,
    "stuns": 0x90B46AC80,
    "control": 0x90B46ACA8,
    "judge1_total": 0x90B451210,
    "judge2_total": 0x90B451260,
    "judge3_total": 0x90B4512B0,
}

kernel = ct.WinDLL("kernel32", use_last_error=True)
kernel.OpenProcess.argtypes = [ct.c_ulong, ct.c_int, ct.c_ulong]
kernel.OpenProcess.restype = ct.c_void_p
kernel.ReadProcessMemory.argtypes = [ct.c_void_p, ct.c_void_p, ct.c_void_p, ct.c_size_t, ct.POINTER(ct.c_size_t)]
kernel.ReadProcessMemory.restype = ct.c_int
kernel.CloseHandle.argtypes = [ct.c_void_p]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("pid", type=int)
    parser.add_argument("output", type=Path)
    parser.add_argument("--seconds", type=int, default=900)
    args = parser.parse_args()
    handle = kernel.OpenProcess(0x1010, 0, args.pid)
    if not handle:
        raise OSError(ct.get_last_error(), "OpenProcess failed")

    def read(address: int, size: int) -> bytes | None:
        data = ct.create_string_buffer(size)
        read_count = ct.c_size_t()
        ok = kernel.ReadProcessMemory(handle, ct.c_void_p(address), data, size, ct.byref(read_count))
        return data.raw if ok and read_count.value == size else None

    def value(slot: int) -> tuple[int, int] | None:
        pointer = read(slot, 8)
        if not pointer:
            return None
        header = read(struct.unpack("<Q", pointer)[0], 0x68)
        if not header or struct.unpack_from("<Q", header)[0] != 0x909C227F0:
            return None
        data = read(struct.unpack_from("<Q", header, 0x60)[0], 40)
        if not data or struct.unpack_from("<H", data, 0x1C)[0] != 32:
            return None
        return struct.unpack_from("<II", data, 32)

    try:
        with args.output.open("w", newline="", encoding="utf-8") as stream:
            writer = csv.writer(stream)
            writer.writerow(["elapsed"] + [f"{name}_{corner}" for name in SLOTS for corner in ("blue", "red")])
            start = time.monotonic()
            previous = None
            while time.monotonic() - start < args.seconds:
                current = tuple(value(slot) for slot in SLOTS.values())
                if current != previous:
                    writer.writerow([round(time.monotonic() - start, 2)] + [n for pair in current for n in (pair or ("", ""))])
                    stream.flush()
                    previous = current
                time.sleep(0.5)
    finally:
        kernel.CloseHandle(handle)


if __name__ == "__main__":
    main()
