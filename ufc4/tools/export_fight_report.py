"""Read-only UFC 4 v28 post-fight report prototype. Never uploads a report."""

import argparse
import ctypes
import json
import math
import struct
import time
import uuid
from ctypes import wintypes
from pathlib import Path

import psutil


class MemoryInfo(ctypes.Structure):
    _fields_ = [
        ("BaseAddress", ctypes.c_void_p), ("AllocationBase", ctypes.c_void_p),
        ("AllocationProtect", wintypes.DWORD), ("PartitionId", wintypes.WORD),
        ("RegionSize", ctypes.c_size_t), ("State", wintypes.DWORD),
        ("Protect", wintypes.DWORD), ("Type", wintypes.DWORD),
    ]


kernel = ctypes.WinDLL("kernel32", use_last_error=True)
kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
kernel.OpenProcess.restype = wintypes.HANDLE
kernel.ReadProcessMemory.argtypes = [wintypes.HANDLE, ctypes.c_void_p, ctypes.c_void_p,
                                     ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
kernel.ReadProcessMemory.restype = wintypes.BOOL
kernel.VirtualQueryEx.argtypes = [wintypes.HANDLE, ctypes.c_void_p,
                                  ctypes.POINTER(MemoryInfo), ctypes.c_size_t]
kernel.VirtualQueryEx.restype = ctypes.c_size_t
kernel.CloseHandle.argtypes = [wintypes.HANDLE]


def documents_folder():
    """Use the configured Windows Documents location, shared with UFC 6."""
    import os
    import winreg
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER,
                            r"Software\Microsoft\Windows\CurrentVersion\Explorer\User Shell Folders") as key:
            configured, _ = winreg.QueryValueEx(key, "Personal")
        configured = Path(os.path.expandvars(configured))
        if configured.is_dir():
            return configured
    except OSError:
        pass
    shell = ctypes.WinDLL("shell32", use_last_error=True)
    shell.SHGetFolderPathW.argtypes = [wintypes.HWND, ctypes.c_int, wintypes.HANDLE,
                                      wintypes.DWORD, wintypes.LPWSTR]
    shell.SHGetFolderPathW.restype = ctypes.c_long
    buffer = ctypes.create_unicode_buffer(260)
    if shell.SHGetFolderPathW(None, 5, None, 0, buffer) != 0 or not buffer.value:
        raise OSError("Cannot locate the Windows Documents folder.")
    return Path(buffer.value)


def fight_reports_folder():
    """Return UFC 4's report folder and migrate the former shared layout."""
    documents = documents_folder()
    target = documents / "UFC Parsectagon Overlay" / "UFC4" / "Fight Reports"
    target.mkdir(parents=True, exist_ok=True)
    old = documents / "UFC 6 Parsectagon Overlay" / "Fight Reports"
    if old.is_dir():
        for source in old.glob("ufc4-*"):
            destination = target / source.name
            if source.is_file() and not destination.exists():
                try:
                    source.replace(destination)
                except OSError:
                    pass
    return target

FIELDS = (
    "Global_MatchState", "Global_MatchResultType", "Global_RoundNum", "Global_RoundLength",
    "Fighter_MatchResult", "Online_MatchResultTypeString", "Online_MatchTimeSecs",
    "Online_KnockdownsLanded", "Online_AdvanceStrikesLanded",
    "Online_SignificantStrikesThrown", "Online_StrikesLanded", "Online_StrikesThrown",
    "Online_TakedownsAttempted", "Online_TakedownsLanded",
    "Online_StunsLanded", "Online_Control_Duration",
)


def fnv_key(name):
    value = 0x811C9DC5
    for byte in name.encode("ascii"):
        value = ((value * 0x1000193) ^ byte) & 0xFFFFFFFF
    return value


def read(handle, address, length):
    if not address or not 0 < length <= 1048576:
        return None
    buffer = ctypes.create_string_buffer(length)
    count = ctypes.c_size_t()
    if kernel.ReadProcessMemory(handle, address, buffer, length, ctypes.byref(count)) \
            and count.value == length:
        return buffer.raw
    return None


def scan(handle, needles):
    address, limit, scanned = 0x7007000000, 0x7008000000, 0
    while address < limit and scanned < 256 * 1048576:
        info = MemoryInfo()
        if not kernel.VirtualQueryEx(handle, address, ctypes.byref(info), ctypes.sizeof(info)):
            break
        end = (info.BaseAddress or address) + info.RegionSize
        if end <= address:
            break
        if info.State == 0x1000 and (info.Protect & 0xFF) in (2, 4, 8, 0x20, 0x40, 0x80):
            cursor = address
            while cursor < min(end, limit) and scanned < 256 * 1048576:
                size = min(1048576, end - cursor, limit - cursor, 256 * 1048576 - scanned)
                data = read(handle, cursor, size)
                scanned += size
                if data:
                    for name, needle in needles.items():
                        offset = data.find(needle)
                        while offset >= 0:
                            yield name, cursor + offset
                            offset = data.find(needle, offset + 1)
                cursor += size
        address = end


def active_fields(handle):
    # Verify a known v28 game-image string before interpreting guest objects.
    if read(handle, 0x800000000 + 0x1F209EF, len(b"Global_MatchState")) != b"Global_MatchState":
        raise RuntimeError("This is not the verified UFC 4 v28 image.")
    needles = {name: struct.pack("<I", fnv_key(name)) for name in FIELDS}
    found = {}
    for name, address in scan(handle, needles):
        header = read(handle, address - 8, 0x70)
        if not header:
            continue
        vtable = struct.unpack_from("<Q", header, 0x18)[0]
        world = struct.unpack_from("<Q", header, 0x28)[0]
        count = struct.unpack_from("<I", header, 0x50)[0]
        values_ptr = struct.unpack_from("<Q", header, 0x58)[0]
        if not (0x7000000000 <= vtable < 0x7100000000
                and 0x7000000000 <= world < 0x7100000000 and count == 4):
            continue
        values = read(handle, values_ptr, 0x60)
        if not values:
            continue
        pair = struct.unpack_from("<II", values, 0x20)
        extra = struct.unpack_from("<II", values, 0x28)
        found.setdefault(name, []).append((world, vtable, pair, extra))
    worlds = {entry[0] for entry in found.get("Global_MatchState", []) if entry[2][0] == 3}
    if len(worlds) != 1:
        raise RuntimeError("A unique finished fight was not found. Open Fight Statistics.")
    world = worlds.pop()
    result = {}
    for name in FIELDS:
        matches = [entry for entry in found.get(name, []) if entry[0] == world]
        if len(matches) != 1:
            raise RuntimeError(f"{name}: expected one field in the finished fight.")
        result[name] = matches[0]
    if len({result[name][1] for name in FIELDS if name.startswith("Online_")}) != 1:
        raise RuntimeError("The online stat objects have inconsistent layouts.")
    return result


def count(fields, name, corner):
    value = fields[name][2][0 if corner == "blue" else 1]
    if value > 9999:
        raise RuntimeError(f"{name}: invalid count.")
    return value


def float_stat(fields, name, corner):
    bits = fields[name][2][0 if corner == "blue" else 1]
    value = struct.unpack("<f", struct.pack("<I", bits))[0]
    if not math.isfinite(value) or value < 0.0 or value > 9999.0:
        raise RuntimeError(f"{name}: invalid duration.")
    return int(value + 0.5)


def stats(fields, corner):
    result = {
        "knockdowns": count(fields, "Online_KnockdownsLanded", corner),
        "stuns": count(fields, "Online_StunsLanded", corner),
        "significantStrikesLanded": count(fields, "Online_AdvanceStrikesLanded", corner),
        "significantStrikesAttempted": count(fields, "Online_SignificantStrikesThrown", corner),
        "totalStrikesLanded": count(fields, "Online_StrikesLanded", corner),
        "totalStrikesAttempted": count(fields, "Online_StrikesThrown", corner),
        "takedownsAttempted": count(fields, "Online_TakedownsAttempted", corner),
        "takedownsLanded": count(fields, "Online_TakedownsLanded", corner),
        "controlTimeSeconds": float_stat(fields, "Online_Control_Duration", corner),
    }
    for prefix in ("significantStrikes", "totalStrikes", "takedowns"):
        if result[prefix + "Landed"] > result[prefix + "Attempted"]:
            # The first observed blue-win screen actually showed 4/3. A result
            # capture remains useful; omit inconsistent stats rather than guess.
            return None
    return result


def report_from_fields(fields, red_name=None, blue_name=None):
    pair = fields["Fighter_MatchResult"][2]
    other = fields["Fighter_MatchResult"][3]
    red_code, blue_code = pair[1], other[0]
    if (red_code, blue_code) == (2, 1):
        winner = "red"
    elif (red_code, blue_code) == (1, 2):
        winner = "blue"
    else:
        raise RuntimeError("The fighter result codes do not describe a win and loss.")
    method_key = fields["Online_MatchResultTypeString"][2][0]
    methods = {
        fnv_key("SUBMISSION"): "Submission",
        fnv_key("KO"): "Knockout",
        fnv_key("TKO"): "TKO",
        fnv_key("DECISION"): "Decision",
        fnv_key("UNANIMOUS_DECISION"): "Decision",
        fnv_key("SPLIT_DECISION"): "Decision",
        fnv_key("MAJORITY_DECISION"): "Decision",
        fnv_key("DOCTOR_STOPPAGE"): "Doctor Stoppage",
        fnv_key("DQ"): "DQ",
        fnv_key("DISQUALIFICATION"): "DQ",
        fnv_key("DRAW"): "Draw",
        fnv_key("MAJORITY_DRAW"): "Draw",
        fnv_key("SPLIT_DRAW"): "Draw",
    }
    method = methods.get(method_key)
    if method is None:
        raise RuntimeError("The finish method has not been verified for UFC 4 yet.")
    red_stats, blue_stats = stats(fields, "red"), stats(fields, "blue")
    round_num, clock_secs, method_code = None, None, None
    if "Global_MatchResultType" in fields:
        code = fields["Global_MatchResultType"][2][0]
        if code <= 10:
            method_code = code
    if "Global_RoundNum" in fields and "Global_RoundLength" in fields and "Online_MatchTimeSecs" in fields:
        round_idx = fields["Global_RoundNum"][2][0]
        length_bits = fields["Global_RoundLength"][2][0]
        round_len = struct.unpack("<f", struct.pack("<I", length_bits))[0]
        match_secs = fields["Online_MatchTimeSecs"][2][0]
        if round_idx < 12 and math.isfinite(round_len) and 30.0 <= round_len <= 1800.0:
            round_len_int = int(round_len + 0.5)
            elapsed = match_secs - round_idx * round_len_int if match_secs >= round_idx * round_len_int else match_secs
            if 0 <= elapsed <= round_len_int:
                round_num = round_idx + 1
                clock_secs = elapsed
    result_obj = {"winnerCorner": winner, "method": method, "round": round_num, "clockSeconds": clock_secs}
    if method_code is not None:
        result_obj["gameMethodCode"] = method_code
    report = {
        "schema": 1, "id": str(uuid.uuid4()), "game": "ufc4",
        "gameVersion": "28.00", "engine": "sacredfire",
        "capturedAt": int(time.time() * 1000), "reportStage": "result",
        "fightStatisticsComplete": False,
        "result": result_obj,
        "fighters": [
            {"corner": "red", "name": red_name, "stats": red_stats},
            {"corner": "blue", "name": blue_name, "stats": blue_stats},
        ],
    }
    if red_stats is not None and blue_stats is not None:
        report["reportStage"] = "complete"
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pid", type=int)
    parser.add_argument("--red-name")
    parser.add_argument("--blue-name")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    pids = [p.pid for p in psutil.process_iter(["name"]) if p.info["name"] == "sacredfire-game.exe"]
    if args.pid:
        if args.pid not in pids:
            raise RuntimeError("The selected SacredFire game process is not running.")
        pid = args.pid
    elif len(pids) == 1:
        pid = pids[0]
    else:
        raise RuntimeError("Start exactly one SacredFire UFC 4 process.")
    handle = kernel.OpenProcess(0x410, False, pid)
    if not handle:
        raise OSError(ctypes.get_last_error(), "Cannot read SacredFire.")
    try:
        before = active_fields(handle)
        time.sleep(0.15)
        after = active_fields(handle)
    finally:
        kernel.CloseHandle(handle)
    if before != after:
        raise RuntimeError("Fight data changed during capture. Try again.")
    report = report_from_fields(after, args.red_name, args.blue_name)
    destination = args.output or (fight_reports_folder() / f"ufc4-{report['id']}.json")
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(destination)


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError) as exc:
        raise SystemExit(f"UFC 4 report not exported: {exc}")
