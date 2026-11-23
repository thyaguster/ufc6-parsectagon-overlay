"""Read-only UFC 4 fight-result watcher. Saves locally, then uploads when linked."""

import argparse
import configparser
import ctypes
import json
import os
import sys
import time
import uuid
from pathlib import Path

import psutil

import export_fight_report as fight

# Reuse the UFC 6 overlay's uploader and linked-account token.  The extra path
# is needed only when this file is run from the source tree; PyInstaller bundles
# the same module into the standalone watcher executable.
SCRIPTS = Path(__file__).resolve().parents[2] / "scripts"
if SCRIPTS.is_dir():
    sys.path.insert(0, str(SCRIPTS))
from upload_fight_report import run as upload_report


DEFAULT_LOBBY_URL = "https://parsectagon.cc"


def install_folder():
    return Path(sys.executable).resolve().parent if getattr(sys, "frozen", False) \
        else Path(__file__).resolve().parents[1] / "dist"


def configured_lobby_url(explicit=None):
    """Use the same server setting convention as the UFC 6 overlay."""
    if explicit:
        return explicit
    environment = os.environ.get("PARSECTAGON_LOBBY_URL", "").strip()
    if environment:
        return environment
    settings = install_folder() / "ufc4_actions.ini"
    parser = configparser.ConfigParser()
    try:
        parser.read(settings, encoding="utf-8-sig")
        configured = parser.get("Menu", "LobbyServerURL", fallback="").strip()
        if configured:
            return configured
    except (OSError, configparser.Error):
        pass
    return DEFAULT_LOBBY_URL


def state(handle):
    """Return the sole live fight context and state, or None during transitions."""
    if fight.read(handle, 0x800000000 + 0x1F209EF, len(b"Global_MatchState")) != b"Global_MatchState":
        raise RuntimeError("This is not the verified UFC 4 v28 image.")
    needle = {"Global_MatchState": fight.struct.pack("<I", fight.fnv_key("Global_MatchState"))}
    candidates = []
    for _, address in fight.scan(handle, needle):
        header = fight.read(handle, address - 8, 0x70)
        if not header:
            continue
        vtable = fight.struct.unpack_from("<Q", header, 0x18)[0]
        world = fight.struct.unpack_from("<Q", header, 0x28)[0]
        count = fight.struct.unpack_from("<I", header, 0x50)[0]
        values_ptr = fight.struct.unpack_from("<Q", header, 0x58)[0]
        if not (0x7000000000 <= vtable < 0x7100000000
                and 0x7000000000 <= world < 0x7100000000 and count == 4):
            continue
        values = fight.read(handle, values_ptr, 0x28)
        if values:
            value = fight.struct.unpack_from("<I", values, 0x20)[0]
            if value in (1, 3):
                candidates.append((world, value))
    return candidates[0] if len(candidates) == 1 else None


class CaptureState:
    def __init__(self):
        self.fight_id = None
        self.captured = False

    def observe(self, current):
        if current is None:
            return None
        _, phase = current
        if phase == 1:
            if self.fight_id is None or self.captured:
                self.fight_id = str(uuid.uuid4())
                self.captured = False
            return None
        if phase == 3 and not self.captured:
            if self.fight_id is None:
                self.fight_id = str(uuid.uuid4())
            return self.fight_id
        return None


def capture(handle, report_id, output_dir):
    before = fight.active_fields(handle)
    time.sleep(0.15)
    after = fight.active_fields(handle)
    if before != after:
        raise RuntimeError("Fight data changed during capture.")
    report = fight.report_from_fields(after)
    report["id"] = report_id
    output_dir.mkdir(parents=True, exist_ok=True)
    target = output_dir / f"ufc4-{report_id}.json"
    target.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return target


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pid", type=int)
    parser.add_argument("--output-dir", type=Path, default=fight.fight_reports_folder())
    parser.add_argument("--poll", type=float, default=1.0)
    parser.add_argument("--once", action="store_true")
    parser.add_argument("--lobby-url")
    parser.add_argument("--no-upload", action="store_true",
                        help="save locally without submitting to Parsectagon")
    args = parser.parse_args()
    if not 0.25 <= args.poll <= 30:
        parser.error("--poll must be between 0.25 and 30 seconds")
    pids = [p.pid for p in psutil.process_iter(["name"])
            if p.info["name"] == "sacredfire-game.exe"]
    if args.pid:
        if args.pid not in pids:
            raise RuntimeError("The selected SacredFire game process is not running.")
        pid = args.pid
    elif len(pids) == 1:
        pid = pids[0]
    else:
        raise RuntimeError("Start exactly one SacredFire UFC 4 process.")
    fight.kernel.CreateMutexW.argtypes = [ctypes.c_void_p, ctypes.c_bool, ctypes.c_wchar_p]
    fight.kernel.CreateMutexW.restype = ctypes.c_void_p
    mutex = fight.kernel.CreateMutexW(None, False, "Local\\UFC4ParsectagonReportWatcher")
    if not mutex:
        raise OSError(ctypes.get_last_error(), "Cannot start report watcher.")
    if ctypes.get_last_error() == 183:
        fight.kernel.CloseHandle(mutex)
        return
    handle = fight.kernel.OpenProcess(0x410, False, pid)
    if not handle:
        fight.kernel.CloseHandle(mutex)
        raise OSError(fight.ctypes.get_last_error(), "Cannot read SacredFire.")
    tracker = CaptureState()
    try:
        while psutil.pid_exists(pid):
            current = state(handle)
            report_id = tracker.observe(current)
            if report_id is not None:
                try:
                    target = capture(handle, report_id, args.output_dir)
                except RuntimeError:
                    if args.once:
                        raise
                else:
                    tracker.captured = True
                    print(target, flush=True)
                    if not args.no_upload:
                        # Upload markers and errors are written beside the JSON,
                        # exactly like UFC 6. A missing account token returns 2
                        # and leaves the local report untouched.
                        upload_report(target, configured_lobby_url(args.lobby_url))
                    if args.once:
                        return
            time.sleep(args.poll)
    finally:
        fight.kernel.CloseHandle(handle)
        fight.kernel.CloseHandle(mutex)


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError) as exc:
        raise SystemExit(f"UFC 4 watcher stopped: {exc}")
