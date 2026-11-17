"""Reversible, version-checked capture of the post-fight stats UI input.

The probe retains the already-built stats table as a delimited string on the
PostFightStatsHelper module. It does not change the table or UI output. This is
a diagnostic; it is not installed by the overlay installer.
"""
from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path

START = 0x15F711B5
LENGTH = 12689
SHA256 = "3f59f81cdc0228e5a76641f8c25b742be1a6d11af9b7edab4106d7264d83945b"
ANCHOR = b"function PostFightStatsHelper:getStatsList(stats,redFighterName,blueFighterName,resultFightType, isOpEx)"
MARKER = b"UF6R1\n"

INJECT = b'''\n    local captureParts={"UF6R1\\n"}
    local function captureValue(value,path,depth)
        if depth>8 then return end
        local kind=type(value)
        if kind=="table" then
            if next(value)==nil then
                captureParts[#captureParts+1]=path.."\\t@empty\\n"
            end
            for key,item in pairs(value) do
                captureValue(item,path.."."..tostring(key),depth+1)
            end
        elseif kind=="number" then
            captureParts[#captureParts+1]=path.."\\t"..string.format("%.17g",value).."\\n"
        elseif kind=="string" or kind=="boolean" then
            captureParts[#captureParts+1]=path.."\\t"..tostring(value).."\\n"
        end
    end
    captureValue(stats,"stats",0)
    captureValue(redFighterName,"stats.RED_CORNER_FIGHTER.NAME",0)
    captureValue(blueFighterName,"stats.BLUE_CORNER_FIGHTER.NAME",0)
    captureParts[#captureParts+1]="UF6R1_END\\n"
    PostFightStatsHelper.__ufc6_report=table.concat(captureParts)
'''


def patch(original: bytes) -> bytes:
    if len(original) != LENGTH or hashlib.sha256(original).hexdigest() != SHA256:
        raise ValueError("Unexpected PostFightStatsHelper asset bytes")
    if original.count(ANCHOR) != 1:
        raise ValueError("PostFightStatsHelper entry not unique")
    lines = original.splitlines(keepends=True)
    compact = b"".join(
        b"" if line.lstrip().startswith(b"--") else line.lstrip(b" \t")
        for line in lines
    )
    if compact.count(ANCHOR) != 1:
        raise ValueError("Compaction changed the entry point")
    entry = compact.index(ANCHOR) + len(ANCHOR)
    patched = compact[:entry] + INJECT + compact[entry:]
    if len(patched) > LENGTH:
        raise ValueError("Capture exceeds original asset span")
    return patched + b" " * (LENGTH - len(patched))


def run(action: str, archive: Path, backup: Path) -> str:
    if action != "status":
        import sys
        sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "vendor" / "xbox-icons"))
        import patcher_core
        if patcher_core.game_running():
            raise RuntimeError("Close UFC 6 before altering the diagnostic")
    if not backup.is_file():
        if action != "install":
            raise FileNotFoundError(backup)
        with archive.open("rb") as stream:
            stream.seek(START)
            original = stream.read(LENGTH)
        if hashlib.sha256(original).hexdigest() != SHA256:
            raise ValueError("Archive does not match the verified original")
        backup.write_bytes(original)
    original = backup.read_bytes()
    patched = patch(original)
    with archive.open("rb") as stream:
        stream.seek(START)
        current = stream.read(LENGTH)
    if current not in (original, patched):
        raise ValueError("Archive has unknown bytes; refusing to overwrite")
    if action == "status":
        return "original" if current == original else "capture probe"
    desired = patched if action == "install" else original
    if current != desired:
        with archive.open("r+b") as stream:
            stream.seek(START)
            stream.write(desired)
            stream.flush()
            os.fsync(stream.fileno())
        with archive.open("rb") as stream:
            stream.seek(START)
            if stream.read(LENGTH) != desired:
                raise OSError("Archive verification failed")
    return "capture probe" if action == "install" else "original"


if __name__ == "__main__":
    cli = argparse.ArgumentParser()
    cli.add_argument("action", choices=("status", "install", "restore"))
    cli.add_argument("archive", type=Path)
    cli.add_argument("backup", type=Path)
    args = cli.parse_args()
    print(run(args.action, args.archive, args.backup))
