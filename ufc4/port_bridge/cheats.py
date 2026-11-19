# SPDX-License-Identifier: GPL-2.0-or-later
"""PORTHEON UFC 4 cheat-linker wrapper with dormant overlay stamina support.

Install beside a byte-for-byte copy of the port's original script named
``cheats_portheon.py``. The original linker still handles every user-selected
cheat; this wrapper adds one dormant code block and a writable four-byte mask.
The overlay changes only that mask while the game runs.
"""
from pathlib import Path
import struct

import cheats_portheon as upstream

LIVE_ID = "ufc4_overlay_stamina_runtime"
MARKER = b"UFC4LIVE_STAMINA1"
PATTERN = bytes.fromhex("C5 FA 5D C1 C5 FA 11 00 49 8B 00")


def load(port_root, data_dir, title_id=None):
    packs, selected = upstream.load(port_root, data_dir, title_id)
    if title_id is None or title_id in ("CUSA14209", "CUSA14204"):
        selected.add(LIVE_ID)
    return packs, selected


def _align(value, alignment):
    return (value + alignment - 1) & -alignment


def _block(site, code_start, mask_address):
    """Emit the game's eight-byte stamina operation and a data-mask branch."""
    code = bytearray.fromhex("C5 FA 5D C1")  # vminss xmm0,xmm0,xmm1

    def emit(hex_bytes):
        at = len(code)
        code.extend(bytes.fromhex(hex_bytes))
        return at

    def data_rel32(at, after):
        struct.pack_into("<i", code, at, mask_address - (code_start + after))

    empty = emit("83 3D 00 00 00 00 00 74 00")  # cmp mask,0; je original
    data_rel32(empty + 2, empty + 7)
    side = emit("83 B8 18 01 00 00 00 75 00")  # cmp [rax+0x118],0; jne right
    left = emit("F6 05 00 00 00 00 01 EB 00")  # test mask,1; jmp decide
    data_rel32(left + 2, left + 7)
    right = len(code)
    right_test = emit("F6 05 00 00 00 00 02")  # test mask,2
    data_rel32(right_test + 2, right_test + 7)
    decide = len(code)
    choice = emit("74 00 C5 FA 11 08 EB 00")  # jz original; store xmm1; jmp back
    original = len(code)
    emit("C5 FA 11 00")  # original store xmm0
    back = len(code)
    emit("E9 00 00 00 00")

    for offset, target in ((empty + 8, original), (side + 8, right),
                           (left + 8, decide), (choice + 1, original),
                           (choice + 7, back)):
        delta = target - (offset + 1)
        if not -128 <= delta <= 127:
            raise ValueError("overlay stamina branch out of range")
        code[offset] = delta & 255
    struct.pack_into("<i", code, back + 1, site + 8 - (code_start + back + 5))
    return code


def apply(image, segments, eboot_end, packs, enabled):
    user_selected = set(enabled) - {LIVE_ID}
    result, mapped, report = upstream.apply(image, segments, eboot_end, packs, user_selected)
    if LIVE_ID not in enabled:
        return result, mapped, report
    if "ufc4_stamina_left" in user_selected or "ufc4_stamina_right" in user_selected:
        report["skipped"].append("overlay live stamina: PORTHEON startup stamina selected")
        return result, mapped, report

    pattern_site = result.find(PATTERN, 0, eboot_end)
    if pattern_site < 0 or result.find(PATTERN, pattern_site + 1, eboot_end) >= 0:
        report["skipped"].append("overlay live stamina: exact site missing or duplicated")
        return result, mapped, report
    site = pattern_site
    code_start = _align(len(result), 65536)
    data_start = _align(code_start + 4096, 4096)
    code = _block(site, code_start, data_start)
    if code_start - (site + 5) > 0x7fffffff:
        report["skipped"].append("overlay live stamina: hook out of range")
        return result, mapped, report

    result = bytearray(result)
    result.extend(bytes(data_start + 4096 - len(result)))
    result[code_start:code_start + len(code)] = code
    result[data_start + 16:data_start + 16 + len(MARKER)] = MARKER
    result[site:site + 8] = b"\xe9" + struct.pack("<i", code_start - (site + 5)) + b"\x90" * 3
    mapped = list(mapped) + [(code_start, data_start - code_start, 5), (data_start, 4096, 6)]
    report["applied"].append("overlay_stamina_live")
    report["code_bytes"] += len(code)
    report["overlay_live_mask_offset"] = data_start
    return result, mapped, report
