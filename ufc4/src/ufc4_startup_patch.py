"""Guarded, reversible local UFC 4 startup-flow prototype.

This script contains patch recipes only. Original game bytes stay in a private backup.
"""
from __future__ import annotations

import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import struct
import zipfile


RECORDS = {
    "logo": (1777552, 2916, "c0e34c3c61b20eafc8a4d44a163441ea2e065740a0bc6b048d17772f584ba7a8"),
    "title": (1850720, 2862, "824f533983645bdf86f9e8fee545db520a4b487e8f51771c1599c9aca9a381dc"),
    "login": (4374240, 6427, "4fb3365068e3f42519fb4cae484f8fa7a4bfc2b3a9e24c44076d6478bfd99abe"),
}
BACKUP_NAME = "ufc4-startup-originals.zip"
BUNDLE_MAGIC = b"UFC4ST01"


def check(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def zstd_library():
    candidates = (
        r"C:\Program Files\Git\mingw64\bin\libzstd.dll",
        r"C:\Program Files (x86)\EaseUS\Todo Backup\bin\x64\zstd.dll",
    )
    for path in candidates:
        try:
            lib = ctypes.CDLL(path)
            break
        except OSError:
            continue
    else:
        raise RuntimeError("64-bit Zstandard library not found")
    lib.ZSTD_compressBound.argtypes = [ctypes.c_size_t]
    lib.ZSTD_compressBound.restype = ctypes.c_size_t
    lib.ZSTD_compress.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p, ctypes.c_size_t, ctypes.c_int]
    lib.ZSTD_compress.restype = ctypes.c_size_t
    lib.ZSTD_decompress.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p, ctypes.c_size_t]
    lib.ZSTD_decompress.restype = ctypes.c_size_t
    return lib


def make_logo(old: bytes) -> bytes:
    check(old.count(b'"duration": 2500') == 1 and old.count(b'"duration": 250,') == 2,
          "Unknown legal/logo layout")
    new = old.replace(b'"duration": 2500', b'"duration": 1   ')
    new = new.replace(b'"duration": 250,', b'"duration": 1  ,')
    check(len(new) == len(old), "Logo layout changed length")
    json.loads(new)
    return new


def make_title(old: bytes) -> bytes:
    check(old[:8] == b"chunzstd" and old[48:52] == b"\x28\xb5\x2f\xfd",
          "Unknown title compression")
    check(int.from_bytes(old[12:16], "big") == 14303 and int.from_bytes(old[40:44], "big") == len(old)-48,
          "Unknown title frame sizes")
    lib = zstd_library()
    decoded = ctypes.create_string_buffer(14303)
    n = lib.ZSTD_decompress(decoded, 14303, old[48:], len(old)-48)
    check(n == 14303, "Could not decompress title script")
    source = decoded.raw
    first, second = b'o.mControllerId = -1', b'o.mAutoLogin = false'
    check(source.count(first) == source.count(second) == 1, "Unknown title auto-login path")
    desired = source.replace(first, b'o.mControllerId = 0 ').replace(second, b'o.mAutoLogin = true ')
    # Keep the same source and frame lengths; only an existing comment loses text.
    comment = b'-- Invalidate all save/load data since the primary user may change after this point'
    check(desired.count(comment) == 1, "Unknown title comment layout")
    desired = desired.replace(comment, comment[:59] + b' ' * (len(comment)-59))
    check(len(desired) == 14303, "Title script changed length")
    encoded = ctypes.create_string_buffer(lib.ZSTD_compressBound(len(desired)))
    n = lib.ZSTD_compress(encoded, len(encoded), desired, len(desired), 18)
    check(n == len(old)-48, f"Title frame length changed: {n}")
    check(lib.ZSTD_decompress(decoded, 14303, encoded, n) == 14303 and decoded.raw == desired,
          "Title recompression failed verification")
    return old[:48] + encoded.raw[:n]


def find_state(node: dict, name: str) -> dict | None:
    if node.get("name") == name:
        return node
    for child in node.get("states", []):
        found = find_state(child, name)
        if found is not None:
            return found
    return None


def make_login(old: bytes) -> bytes:
    nav = json.loads(old)
    state = find_state(nav, "login_failure")
    check(state is not None, "Login failure state missing")
    original_enter = [["set", ["popup", {}]], ["openPopup", [".event.data", ".popup"]]]
    original_exit = [["closePopup", [".popup"]], ["invoke", ["LOGIN_TRANSITION_FAILURE"]]]
    check(state["onEnter"] == original_enter and state["onExit"] == original_exit,
          "Unknown login failure actions")
    known = ["TXT_UFC_BLAZE_CONNECTION_FAILURE", "TXT_UNABLE_TO_LOGIN", "TXT_UNABLE_TO_LOGIN_EA"]
    predicate = ["or", ["==", ".event.data.body", known[0]],
                 ["or", ["==", ".event.data.body", known[1]], ["==", ".event.data.body", known[2]]]]
    state["onEnter"] = [["set", ["overlayKnownOffline", predicate]],
                        ["if", [".overlayKnownOffline", [["invoke", ["force_accept_login_fail_popup"]]], original_enter]]]
    state["onExit"] = [["if", [".overlayKnownOffline", [], [["closePopup", [".popup"]]]]],
                       ["invoke", ["LOGIN_TRANSITION_FAILURE"]]]
    encoded = json.dumps(nav, separators=(",", ":"), ensure_ascii=True).encode("ascii")
    check(len(encoded) <= len(old), "Login flow patch does not fit")
    new = encoded + b" " * (len(old)-len(encoded))
    check(json.loads(new)["name"] == "loginflow", "Login flow JSON verification failed")
    return new


RECIPES = {"logo": make_logo, "title": make_title, "login": make_login}


def backup_read(path: Path) -> dict[str, bytes]:
    with zipfile.ZipFile(path) as z:
        check(set(z.namelist()) == {f"{k}.bin" for k in RECORDS}, "Unexpected startup backup contents")
        result = {k: z.read(f"{k}.bin") for k in RECORDS}
    for key, blob in result.items():
        check(len(blob) == RECORDS[key][1] and sha(blob) == RECORDS[key][2],
              f"Invalid {key} startup backup")
    return result


def backup_create(path: Path, original: dict[str, bytes]) -> None:
    check(not path.exists(), "Backup already exists")
    path.parent.mkdir(parents=True, exist_ok=True)
    temp = path.with_suffix(".zip.new")
    check(not temp.exists(), "Stale backup temporary file")
    with temp.open("xb") as stream:
        with zipfile.ZipFile(stream, "w", compression=zipfile.ZIP_STORED) as z:
            for key, blob in original.items():
                z.writestr(f"{key}.bin", blob)
        stream.flush()
        os.fsync(stream.fileno())
    backup_read(temp)
    os.replace(temp, path)


def write_bundle(backup: Path, output: Path) -> None:
    original = backup_read(backup)
    patched = {key: RECIPES[key](original[key]) for key in RECORDS}
    data = bytearray(BUNDLE_MAGIC + struct.pack("<I", len(RECORDS)))
    for key, (offset, size, _) in RECORDS.items():
        data += struct.pack("<QI", offset, size)
        data += original[key] + patched[key]
    check(len(data) < 32768, "Startup bundle too large")
    output.parent.mkdir(parents=True, exist_ok=True)
    temp = output.with_suffix(output.suffix + ".new")
    temp.write_bytes(data)
    os.replace(temp, output)


def run(action: str, archive: Path, backup: Path) -> str:
    check(action in ("status", "prepare", "install", "restore"), "Unknown action")
    check(archive.is_file() and archive.stat().st_size > 10_000_000, "UFC 4 UI archive missing")
    with archive.open("rb" if action == "status" else "r+b") as stream:
        current = {}
        for key, (offset, size, _) in RECORDS.items():
            stream.seek(offset)
            current[key] = stream.read(size)
            check(len(current[key]) == size, f"Truncated {key} startup record")
        original = backup_read(backup) if backup.exists() else current
        for key, blob in original.items():
            check(sha(blob) == RECORDS[key][2], f"Unknown {key} original version")
        patched = {key: RECIPES[key](original[key]) for key in RECORDS}
        for key in RECORDS:
            check(current[key] in (original[key], patched[key]), f"Unknown installed {key} startup record")
        state = "installed" if current == patched else "original" if current == original else "mixed"
        if action == "status":
            return state
        check(not any(p.name.lower() == "sacredfire-game.exe" for p in _game_processes()),
              "Close UFC 4 before changing startup assets")
        if action in ("prepare", "install") and not backup.exists():
            backup_create(backup, original)
        if action == "prepare":
            return state
        target = patched if action == "install" else original
        changes = [(key, current[key], target[key]) for key in RECORDS if current[key] != target[key]]
        written = []
        try:
            for key, before, after in changes:
                stream.seek(RECORDS[key][0])
                stream.write(after)
                written.append((key, before))
            stream.flush()
            os.fsync(stream.fileno())
            for key in target:
                stream.seek(RECORDS[key][0])
                check(stream.read(RECORDS[key][1]) == target[key], f"{key} write verification failed")
        except BaseException:
            for key, before in written:
                stream.seek(RECORDS[key][0])
                stream.write(before)
            stream.flush()
            os.fsync(stream.fileno())
            raise
        return "installed" if action == "install" else "original"


def _game_processes():
    # Windows process enumeration without third-party dependencies.
    import subprocess
    output = subprocess.check_output(["tasklist", "/FO", "CSV", "/NH"], text=True, errors="replace")
    import csv
    return [Path(row[0]) for row in csv.reader(output.splitlines()) if row]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("action", choices=("status", "prepare", "install", "restore", "bundle"))
    parser.add_argument("--archive", type=Path)
    parser.add_argument("--backup", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if args.action == "bundle":
        check(args.output is not None, "Bundle output missing")
        write_bundle(args.backup, args.output)
        print("bundle-ready")
    else:
        check(args.archive is not None, "Archive path missing")
        print(run(args.action, args.archive, args.backup))


if __name__ == "__main__":
    main()
