"""Capture Windows OutputDebugString messages without attaching a debugger."""
from __future__ import annotations

import argparse
import ctypes as ct
from pathlib import Path
import struct
import time


kernel = ct.WinDLL("kernel32", use_last_error=True)
kernel.CreateEventW.argtypes = [ct.c_void_p, ct.c_int, ct.c_int, ct.c_wchar_p]
kernel.CreateEventW.restype = ct.c_void_p
kernel.CreateFileMappingW.argtypes = [ct.c_void_p, ct.c_void_p, ct.c_ulong, ct.c_ulong, ct.c_ulong, ct.c_wchar_p]
kernel.CreateFileMappingW.restype = ct.c_void_p
kernel.MapViewOfFile.argtypes = [ct.c_void_p, ct.c_ulong, ct.c_ulong, ct.c_ulong, ct.c_size_t]
kernel.MapViewOfFile.restype = ct.c_void_p
kernel.SetEvent.argtypes = [ct.c_void_p]
kernel.WaitForSingleObject.argtypes = [ct.c_void_p, ct.c_ulong]
kernel.UnmapViewOfFile.argtypes = [ct.c_void_p]
kernel.CloseHandle.argtypes = [ct.c_void_p]

PAGE_READWRITE = 4
FILE_MAP_READ = 4
WAIT_OBJECT_0 = 0


def capture(output: Path, seconds: int) -> None:
    ready = kernel.CreateEventW(None, 0, 0, "DBWIN_BUFFER_READY")
    data = kernel.CreateEventW(None, 0, 0, "DBWIN_DATA_READY")
    mapping = kernel.CreateFileMappingW(ct.c_void_p(-1), None, PAGE_READWRITE, 0, 4096, "DBWIN_BUFFER")
    if not (ready and data and mapping):
        raise OSError(ct.get_last_error(), "Could not create debug output listener")
    view = kernel.MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 4096)
    if not view:
        raise OSError(ct.get_last_error(), "Could not map debug output buffer")
    try:
        deadline = time.monotonic() + seconds
        with output.open("w", encoding="utf-8") as stream:
            while time.monotonic() < deadline:
                kernel.SetEvent(ready)
                if kernel.WaitForSingleObject(data, 1000) != WAIT_OBJECT_0:
                    continue
                raw = ct.string_at(view, 4096)
                pid = struct.unpack_from("<I", raw)[0]
                message = raw[4:].split(b"\0", 1)[0].decode("utf-8", "replace")
                if not message:
                    continue
                stream.write(f"{time.time():.3f}\t{pid}\t{message}\n")
                stream.flush()
    finally:
        kernel.UnmapViewOfFile(view)
        for handle in (mapping, data, ready):
            kernel.CloseHandle(handle)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    parser.add_argument("--seconds", type=int, default=900)
    args = parser.parse_args()
    capture(args.output, args.seconds)
