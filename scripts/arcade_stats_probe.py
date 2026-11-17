"""Exact-byte diagnostic for Fight Now's post-fight stats callback.

This probe logs the original get-fight-stats table at ArcadeHelper's callback.
It changes no game state. Only install or restore it while UFC 6 is closed.
"""
from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path


START = 0x2A7687D8
END = 0x2A768C0C
ORIGINAL_SHA256 = "a4fdf75771c087836e9029719c50998d6c1bc8fc86d6f220f5fec78a77515836"
ANCHOR = b"function ArcadeHelper:_handleFightData(success, fightResult, fightersOutput, statsOutput, rivalryOutput)"
INJECTION = (
    b'if success then print("UFC6_ARCADE_STATS_BEGIN") '
    b'self.api("DebugPrint").PrintTable(statsOutput) '
    b'print("UFC6_ARCADE_STATS_END")'
)


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def replacement(original: bytes) -> bytes:
    if len(original) != END - START or digest(original) != ORIGINAL_SHA256:
        raise ValueError("ArcadeHelper archive bytes do not match the verified build")
    if not original.startswith(ANCHOR):
        raise ValueError("ArcadeHelper callback anchor is missing")
    old = b"if success then"
    if original.count(old) != 1:
        raise ValueError("Expected exactly one callback success branch")
    patched = original.replace(old, INJECTION, 1)
    # Both comments are inside this callback. Their removal leaves Lua logic
    # untouched and makes room for the diagnostic in the same archive span.
    comments = (
        b" -- We need to add win-streak information in Fight Now.",
        b" -- PPV Fight week case - No need to add win-streak information.",
    )
    for comment in comments:
        if patched.count(comment) != 1:
            raise ValueError("Callback comment changed")
        patched = patched.replace(comment, b"", 1)
    if len(patched) > len(original):
        raise ValueError("Diagnostic exceeds the verified comment space")
    patched += b" " * (len(original) - len(patched))
    if len(patched) != len(original):
        raise AssertionError("Patch length changed")
    return patched


def run(action: str, archive: Path, backup: Path) -> str:
    import sys
    sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "vendor" / "xbox-icons"))
    import patcher_core as core

    if action != "status" and core.game_running():
        raise RuntimeError("Close UFC 6 before changing the diagnostic")
    with archive.open("rb") as stream:
        stream.seek(START)
        current = stream.read(END - START)
    original = backup.read_bytes() if backup.exists() else current
    patched = replacement(original)
    if current not in (original, patched):
        raise ValueError("Archive has unknown bytes; refusing to write")
    if action == "status":
        return "installed" if current == patched else "original"
    if action == "install" and not backup.exists():
        with backup.open("xb") as stream:
            stream.write(original)
            stream.flush()
            os.fsync(stream.fileno())
    desired = patched if action == "install" else original
    if current != desired:
        with archive.open("r+b") as stream:
            stream.seek(START)
            stream.write(desired)
            stream.flush()
            os.fsync(stream.fileno())
        with archive.open("rb") as stream:
            stream.seek(START)
            if stream.read(len(desired)) != desired:
                raise OSError("Archive verification failed")
    return "installed" if action == "install" else "original"


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("status", "install", "restore"))
    parser.add_argument("archive", type=Path)
    parser.add_argument("backup", type=Path)
    args = parser.parse_args()
    print(run(args.action, args.archive, args.backup))
