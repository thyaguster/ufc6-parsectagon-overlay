"""Switch the temporary FightStatsHelper diagnostic to a local file output."""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
import fight_stats_probe as probe

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "vendor" / "xbox-icons"))
import patcher_core as core

OUTPUT = "C:/Scripts/kyty-ufc6-overlay-mod/.build/fight-stats-guest.txt"


def replacement(original: bytes) -> bytes:
    probe.replacement(original)  # Check the exact original record first.
    script = (
        "if self.mIsPostgame and io and io.open then "
        f"local f=io.open('{OUTPUT}','w');if f then "
        "for _,c in ipairs({{'red',statsObj.RED_CORNER_FIGHTER},{'blue',statsObj.BLUE_CORNER_FIGHTER}}) do "
        "local s=c[2];if s and s.TOTAL and s.ROUNDS then "
        "f:write(c[1],'\\t',#s.ROUNDS,'\\n');local t={{'TOTAL',s.TOTAL}};"
        "for i,r in ipairs(s.ROUNDS) do t[#t+1]={'R'..i,r} end;"
        "for _,a in ipairs(t) do for k,v in pairs(a[2]) do "
        "if type(v)=='number' or type(v)=='string' then "
        "f:write(c[1],'\\t',a[1],'\\t',k,'\\t',v,'\\n') "
        "elseif k=='JUDGES_SCORES' and type(v)=='table' then "
        "for i,n in ipairs(v) do f:write(c[1],'\\t',a[1],'\\tJUDGE',i,'\\t',n,'\\n') end "
        "end end end end end end;f:close() end end\r\n"
    ).encode("ascii")
    if len(script) > probe.LENGTH:
        raise ValueError("File probe exceeds the original comment")
    return script + b" " * (probe.LENGTH - len(script))


def run(action: str, archive: Path, backup: Path) -> str:
    if action != "status" and core.game_running():
        raise RuntimeError("Close UFC 6 before changing the fight stats probe")
    original = backup.read_bytes()
    file_patch = replacement(original)
    print_patch = probe.replacement(original)
    with archive.open("rb") as stream:
        stream.seek(probe.OFFSET)
        current = stream.read(probe.LENGTH)
    if current not in (original, print_patch, file_patch):
        raise ValueError("Archive contains unknown bytes")
    if action == "status":
        return {original: "original", print_patch: "print probe", file_patch: "file probe"}[current]
    desired = file_patch if action == "install" else original
    if current != desired:
        with archive.open("r+b") as stream:
            stream.seek(probe.OFFSET)
            stream.write(desired)
            stream.flush()
            os.fsync(stream.fileno())
        with archive.open("rb") as stream:
            stream.seek(probe.OFFSET)
            if stream.read(probe.LENGTH) != desired:
                raise OSError("File probe verification failed")
    return "file probe" if action == "install" else "original"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("action", choices=("status", "install", "restore"))
    parser.add_argument("archive", type=Path)
    parser.add_argument("backup", type=Path)
    args = parser.parse_args()
    print(run(args.action, args.archive, args.backup))


if __name__ == "__main__":
    main()
