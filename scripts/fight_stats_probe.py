"""Reversible, exact-byte probe for the post-fight stats service response.

This is a diagnostic. It does not export or alter a fight result.
"""
from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "vendor" / "xbox-icons"))
import patcher_core as core

OFFSET = 31_473_746
LENGTH = 729
ORIGINAL_SHA256 = "9da1a01743d59d428c73542e901208c6c17464a7568b57df273948a8a3bfc209"


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def replacement(original: bytes) -> bytes:
    if len(original) != LENGTH or digest(original) != ORIGINAL_SHA256:
        raise ValueError("Unknown FightStatsHelper bytes; refusing to patch")
    if not original.startswith(b"--[[ @salonso: deprecated") or not original.endswith(b"    ]]"):
        raise ValueError("FightStatsHelper comment changed")
    script = (
        "if self.mIsPostgame then "
        "for _,c in ipairs({{'red',statsObj.RED_CORNER_FIGHTER},{'blue',statsObj.BLUE_CORNER_FIGHTER}}) do "
        "local s=c[2];if s and s.TOTAL and s.ROUNDS then "
        "print('UFC6_REPORT_BEGIN',c[1],#s.ROUNDS);local t={{'TOTAL',s.TOTAL}};"
        "for i,r in ipairs(s.ROUNDS) do t[#t+1]={'R'..i,r} end;"
        "for _,a in ipairs(t) do for k,v in pairs(a[2]) do "
        "if type(v)=='number' or type(v)=='string' then print('UFC6_REPORT_FIELD',c[1],a[1],k,v) end;"
        "if k=='JUDGES_SCORES' and type(v)=='table' then for i,n in ipairs(v) do "
        "print('UFC6_REPORT_JUDGE',c[1],a[1],i,n) end end "
        "end end end end end\r\n"
    ).encode("ascii")
    if len(script) > LENGTH:
        raise ValueError("Probe exceeds original comment")
    return script + b" " * (LENGTH - len(script))


def run(action: str, archive: Path, backup: Path) -> str:
    if action != "status" and core.game_running():
        raise RuntimeError("Close UFC 6 before changing the fight stats probe")
    with archive.open("rb") as stream:
        stream.seek(OFFSET)
        current = stream.read(LENGTH)
    if backup.exists():
        original = backup.read_bytes()
        if len(original) != LENGTH or digest(original) != ORIGINAL_SHA256:
            raise ValueError("Probe backup invalid")
    else:
        if digest(current) != ORIGINAL_SHA256:
            raise ValueError("Archive changed and no matching backup exists")
        original = current
    patched = replacement(original)
    if current not in (original, patched):
        raise ValueError("Archive contains unknown bytes")
    if action == "status":
        return "probe installed" if current == patched else "original"
    if action == "install" and not backup.exists():
        with backup.open("xb") as stream:
            stream.write(original)
            stream.flush()
            os.fsync(stream.fileno())
    desired = patched if action == "install" else original
    if current != desired:
        with archive.open("r+b") as stream:
            stream.seek(OFFSET)
            stream.write(desired)
            stream.flush()
            os.fsync(stream.fileno())
        with archive.open("rb") as stream:
            stream.seek(OFFSET)
            if stream.read(LENGTH) != desired:
                raise OSError("Probe verification failed")
    return "probe installed" if action == "install" else "original"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("action", choices=("status", "install", "restore"))
    parser.add_argument("archive", type=Path)
    parser.add_argument("backup", type=Path)
    args = parser.parse_args()
    print(run(args.action, args.archive, args.backup))


if __name__ == "__main__":
    main()
