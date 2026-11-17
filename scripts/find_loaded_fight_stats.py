"""Locate loaded Fight Statistics Lua text in a running UFC 6 process.

Read-only: enumerates readable committed memory and searches for distinctive
FightStatsHelper strings. This does not patch game files or process memory.
"""
from __future__ import annotations

import argparse
import ctypes as ct
from ctypes import wintypes as wt
import time


class MemoryBasicInformation(ct.Structure):
    _fields_ = [
        ("BaseAddress", ct.c_void_p),
        ("AllocationBase", ct.c_void_p),
        ("AllocationProtect", wt.DWORD),
        ("PartitionId", wt.WORD),
        ("RegionSize", ct.c_size_t),
        ("State", wt.DWORD),
        ("Protect", wt.DWORD),
        ("Type", wt.DWORD),
    ]


kernel = ct.WinDLL("kernel32", use_last_error=True)
kernel.OpenProcess.argtypes = (wt.DWORD, wt.BOOL, wt.DWORD)
kernel.OpenProcess.restype = wt.HANDLE
kernel.VirtualQueryEx.argtypes = (wt.HANDLE, ct.c_void_p, ct.POINTER(MemoryBasicInformation), ct.c_size_t)
kernel.VirtualQueryEx.restype = ct.c_size_t
kernel.ReadProcessMemory.argtypes = (wt.HANDLE, ct.c_void_p, ct.c_void_p, ct.c_size_t, ct.POINTER(ct.c_size_t))
kernel.ReadProcessMemory.restype = wt.BOOL
kernel.CloseHandle.argtypes = (wt.HANDLE,)

NEEDLES = (
    b"function FightStatsHelper:_processStats",
    b"local FightStatsHelper =",
    b"ufc:01.00.00:noa:gamemodes-plugin:get-fight-stats",
)
CHUNK = 1024 * 1024
OVERLAP = max(map(len, NEEDLES)) - 1
READABLE = {0x02, 0x04, 0x08, 0x20, 0x40, 0x80}


def read(handle: int, address: int, size: int) -> bytes | None:
    buffer = ct.create_string_buffer(size)
    count = ct.c_size_t()
    if kernel.ReadProcessMemory(handle, ct.c_void_p(address), buffer, size, ct.byref(count)) and count.value == size:
        return buffer.raw
    return None


def scan(pid: int, max_gib: float, max_seconds: float, needles: tuple[bytes, ...]) -> None:
    handle = kernel.OpenProcess(0x1010, False, pid)
    if not handle:
        raise OSError(ct.get_last_error(), "OpenProcess failed")
    limit = int(max_gib * (1 << 30))
    deadline = time.monotonic() + max_seconds
    scanned = 0
    matches = 0
    address = 0x10000
    info = MemoryBasicInformation()
    seen: set[tuple[int, bytes]] = set()
    try:
        while address < 0x7FFF00000000 and scanned < limit and time.monotonic() < deadline:
            if not kernel.VirtualQueryEx(handle, ct.c_void_p(address), ct.byref(info), ct.sizeof(info)):
                break
            base = int(info.BaseAddress or 0)
            size = int(info.RegionSize)
            address = base + max(size, 0x1000)
            if info.State != 0x1000 or info.Protect & 0xFF not in READABLE or info.Protect & 0x100:
                continue
            for offset in range(0, size, CHUNK):
                if scanned >= limit or time.monotonic() >= deadline:
                    break
                length = min(CHUNK, size - offset, limit - scanned)
                if length <= 0:
                    break
                start = base + offset
                data = read(handle, start, length)
                scanned += length
                if data is None:
                    continue
                for needle in needles:
                    cursor = 0
                    while (found := data.find(needle, cursor)) >= 0:
                        item = (start + found, needle)
                        if item not in seen:
                            seen.add(item)
                            matches += 1
                            excerpt = data[max(found - 40, 0):min(found + len(needle) + 80, len(data))]
                            print(f"0x{start + found:x} {needle.decode()} {excerpt!r}", flush=True)
                        cursor = found + 1
                if length == CHUNK and size - offset > CHUNK:
                    # The next chunk starts before this one's end so a match
                    # spanning the boundary is also visible to the search.
                    tail_start = start + CHUNK - OVERLAP
                    tail = read(handle, tail_start, OVERLAP * 2)
                    if tail:
                        for needle in needles:
                            found = tail.find(needle)
                            if found >= 0:
                                item = (tail_start + found, needle)
                                if item not in seen:
                                    seen.add(item)
                                    matches += 1
                                    print(f"0x{tail_start + found:x} {needle.decode()} [boundary]", flush=True)
    finally:
        kernel.CloseHandle(handle)
    print(f"Read {scanned / (1 << 30):.2f} GiB; {matches} matches", flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("pid", type=int)
    parser.add_argument("--max-gib", type=float, default=6.0)
    parser.add_argument("--max-seconds", type=float, default=45.0)
    parser.add_argument("--needle", action="append", default=[])
    args = parser.parse_args()
    needles = tuple(s.encode("ascii") for s in args.needle) if args.needle else NEEDLES
    scan(args.pid, args.max_gib, args.max_seconds, needles)
