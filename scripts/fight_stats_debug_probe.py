"""Reversible FightStatsHelper probe using the game's DebugPrint API."""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
import fight_stats_probe as probe
import fight_stats_file_probe as file_probe

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "vendor" / "xbox-icons"))
import patcher_core as core


def replacement(original: bytes) -> bytes:
    probe.replacement(original)
    script = (
        "if self.mIsPostgame then "
        "local ok=pcall(function() "
        "local d=self.api('DebugPrint');"
        "if d and d.PrintTable then d.PrintTable({'UFC6_FULL_STATS',"
        "statsObj.RED_CORNER_FIGHTER,statsObj.BLUE_CORNER_FIGHTER}) end "
        "end);"
        "filterList[1].LABEL='REPORT PROBE '..tostring(io~=nil)..' '..tostring(ok) "
        "end\r\n"
    ).encode("ascii")
    if len(script) > probe.LENGTH:
        raise ValueError("Debug probe exceeds the original comment")
    return script + b" " * (probe.LENGTH - len(script))


def run(action: str, archive: Path, backup: Path) -> str:
    if action != "status" and core.game_running():
        raise RuntimeError("Close UFC 6 before changing the fight stats probe")
    original = backup.read_bytes()
    choices = {
        original: "original",
        probe.replacement(original): "print probe",
        file_probe.replacement(original): "file probe",
        replacement(original): "debug probe",
    }
    with archive.open("rb") as stream:
        stream.seek(probe.OFFSET)
        current = stream.read(probe.LENGTH)
    if current not in choices:
        raise ValueError("Archive contains unknown bytes")
    if action == "status":
        return choices[current]
    desired = replacement(original) if action == "install" else original
    if current != desired:
        with archive.open("r+b") as stream:
            stream.seek(probe.OFFSET)
            stream.write(desired)
            stream.flush()
            os.fsync(stream.fileno())
        with archive.open("rb") as stream:
            stream.seek(probe.OFFSET)
            if stream.read(probe.LENGTH) != desired:
                raise OSError("Debug probe verification failed")
    return "debug probe" if action == "install" else "original"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("action", choices=("status", "install", "restore"))
    parser.add_argument("archive", type=Path)
    parser.add_argument("backup", type=Path)
    args = parser.parse_args()
    print(run(args.action, args.archive, args.backup))


if __name__ == "__main__":
    main()
