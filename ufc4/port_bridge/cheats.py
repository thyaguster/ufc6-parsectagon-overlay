# SPDX-License-Identifier: GPL-2.0-or-later
"""PORTHEON UFC 4 linker bridge for live stamina and player-1 protection.

Install beside a byte-for-byte copy of the port's original script named
``cheats_portheon.py``. The original linker still handles every user-selected
cheat. This wrapper links dormant stamina and damage hooks with separate
writable masks; the overlay changes those masks while UFC 4 runs.
"""
from pathlib import Path
import struct

import cheats_portheon as upstream

LIVE_ID = "ufc4_overlay_stamina_runtime"
MARKER = b"UFC4LIVE_STAMINA1"
PATTERN = bytes.fromhex("C5 FA 5D C1 C5 FA 11 00 49 8B 00")
HEALTH_SITE = 0x698EA5
HEALTH_ORIGINAL = bytes.fromhex("C5 FA 5D C5 C5 FA 11 00")
OTHER_HEALTH_SITE = 0x698E34
OTHER_HEALTH_ORIGINAL = bytes.fromhex("C5 F2 5D C0 C5 FA 11 01")
AUX_HEALTH_SITE = 0x699044
AUX_HEALTH_ORIGINAL = bytes.fromhex("C5 F2 5D C0 C5 FA 11 00")
IMPACT_SITE = 0x698D4E
IMPACT_ORIGINAL = bytes.fromhex("C5 F2 59 D8 C5 E0 54 15 B6 27 86 01")
HUD_SITE = 0x698F87
HUD_ORIGINAL = bytes.fromhex("C5 FA 5F E9 74 04")
HEALTH_MARKER = b"UFC4LIVE_NODAMAGE1"


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


def _health_block(site, code_start, data_start, variant=0):
    """Count decreases and preserve a selected fighter's previous health."""
    code = bytearray()

    def emit(raw):
        at = len(code)
        code.extend(bytes.fromhex(raw))
        return at

    def rip32(at, offset, size, target):
        struct.pack_into("<i", code, at + offset,
                         target - (code_start + at + size))

    base = 384 if variant == 2 else variant * 128
    original = (HEALTH_ORIGINAL, OTHER_HEALTH_ORIGINAL, AUX_HEALTH_ORIGINAL)[variant]
    emit("C5 FA 5D C5" if variant == 0 else "C5 F2 5D C0")
    emit("9C 41 53")  # preserve flags and r11 for original continuation
    total = emit("FF 05 00 00 00 00")
    rip32(total, 2, 6, data_start + base + 4)
    emit("0F 2E 01" if variant == 1 else "0F 2E 00")
    unordered = emit("0F 8A 00 00 00 00")
    nonnegative = emit("0F 83 00 00 00 00")
    decreases = emit("FF 05 00 00 00 00")
    rip32(decreases, 2, 6, data_start + base + 8)
    destination = emit("48 89 0D 00 00 00 00" if variant == 1 else
                       "48 89 05 00 00 00 00")
    rip32(destination, 3, 7, data_start + base + 48)
    owner = emit("48 89 1D 00 00 00 00")
    rip32(owner, 3, 7, data_start + base + 56)
    proposed = emit("C5 FA 11 05 00 00 00 00")
    rip32(proposed, 4, 8, data_start + base + 64)
    enabled = emit("83 3D 00 00 00 00 00 74 00")
    rip32(enabled, 2, 7, data_start)
    emit("4C 8B 9B A0 04 00 00")  # r11 = [rbx+0x4a0], head-health node
    null_node = emit("4D 85 DB 74 00")
    emit("4D 8B 5B 48")  # r11 = head-health pointer = actor+0x158
    null_side = emit("4D 85 DB 74 00")
    emit("41 83 7B BC 01")  # cmp dword [r11-0x44] = actor+0x114,1
    invalid_side = emit("77 00")  # unknown side: leave the health write alone
    emit("41 83 7B BC 00")  # cmp dword [r11-0x44],0
    left = emit("75 00 F6 05 00 00 00 00 01 EB 00")
    rip32(left + 2, 2, 7, data_start)
    right = len(code)
    right_test = emit("F6 05 00 00 00 00 02")
    rip32(right_test, 2, 7, data_start)
    decision = len(code)
    not_selected = emit("74 00")
    blocked = emit("FF 05 00 00 00 00")
    rip32(blocked, 2, 6, data_start + base + 12)
    emit("C5 FA 10 01" if variant == 1 else "C5 FA 10 00")
    store = len(code)
    emit("41 5B 9D")
    emit("C5 FA 11 01" if variant == 1 else "C5 FA 11 00")
    back = emit("E9 00 00 00 00")
    for branch in (enabled + 8, null_node + 4, null_side + 4,
                   invalid_side + 1, left + 1, left + 10,
                   not_selected + 1):
        if branch == left + 1:
            target = right
        elif branch == left + 10:
            target = decision
        else:
            target = store
        displacement = target - (branch + 1)
        if not -128 <= displacement <= 127:
            raise ValueError("health branch out of range")
        code[branch] = displacement & 255
    struct.pack_into("<i", code, unordered + 2, store - (unordered + 6))
    struct.pack_into("<i", code, nonnegative + 2, store - (nonnegative + 6))
    struct.pack_into("<i", code, back + 1,
                     site + len(original) - (code_start + back + 5))
    return code


def _hud_block(code_start, data_start):
    """Clamp the secondary head/body/leg value for the selected actor."""
    code = bytearray()

    def emit(raw):
        at = len(code)
        code.extend(bytes.fromhex(raw))
        return at

    def rip32(at, offset, size, target):
        struct.pack_into("<i", code, at + offset,
                         target - (code_start + at + size))

    emit("C5 FA 5F E9")  # original vmaxss xmm5,xmm0,xmm1
    null_destination = emit("74 00")  # retain original test rax,rax branch
    emit("9C 41 53")  # preserve flags and r11
    enabled = emit("83 3D 00 00 00 00 00 74 00")
    rip32(enabled, 2, 7, data_start)
    emit("4C 8B 9B A0 04 00 00")  # owner head-health node
    null_node = emit("4D 85 DB 74 00")
    emit("4D 8B 5B 48")  # actor+0x158
    null_actor = emit("4D 85 DB 74 00")
    emit("41 83 7B BC 01")  # actor+0x114 <= 1
    invalid_side = emit("77 00")
    emit("41 83 7B BC 00")
    left = emit("75 00 F6 05 00 00 00 00 01 EB 00")
    rip32(left + 2, 2, 7, data_start)
    right = len(code)
    right_test = emit("F6 05 00 00 00 00 02")
    rip32(right_test, 2, 7, data_start)
    decision = len(code)
    not_selected = emit("74 00")
    blocked = emit("FF 05 00 00 00 00")
    rip32(blocked, 2, 6, data_start + 256)
    emit("C5 D2 5F 28")  # vmaxss xmm5,xmm5,dword ptr [rax]
    finish = len(code)
    emit("41 5B 9D")
    resume_store = emit("E9 00 00 00 00")
    skip_store = len(code)
    skip_jump = emit("E9 00 00 00 00")

    for branch, target in ((null_destination + 1, skip_store),
                           (enabled + 8, finish), (null_node + 4, finish),
                           (null_actor + 4, finish), (invalid_side + 1, finish),
                           (left + 1, right), (left + 10, decision),
                           (not_selected + 1, finish)):
        displacement = target - (branch + 1)
        if not -128 <= displacement <= 127:
            raise ValueError("HUD branch out of range")
        code[branch] = displacement & 255
    struct.pack_into("<i", code, resume_store + 1,
                     HUD_SITE + len(HUD_ORIGINAL) - (code_start + resume_store + 5))
    struct.pack_into("<i", code, skip_jump + 1,
                     0x698F91 - (code_start + skip_jump + 5))
    return code


def _impact_block(code_start, data_start):
    """Guard player impact and turn the next valid opponent strike into a KO.

    One landed strike produces multiple category callbacks.  Keep the arm live
    for the complete strike so its real head/body/leg damage callback is also
    amplified.  The overlay disarms when the match enters its finished state
    (or the 25-second window expires), rather than consuming a preliminary
    callback here.
    """
    code = bytearray()

    def emit(raw):
        at = len(code)
        code.extend(bytes.fromhex(raw))
        return at

    def rip32(at, offset, size, target):
        struct.pack_into("<i", code, at + offset,
                         target - (code_start + at + size))

    emit("9C 41 53")  # preserve flags and r11
    ko_enabled = emit("83 3D 00 00 00 00 00 0F 84 00 00 00 00")
    rip32(ko_enabled, 2, 7, data_start + 600)
    emit("C5 E8 57 D2 C5 F8 2E C2")  # compare impact xmm0 with zero
    ko_unordered = emit("0F 8A 00 00 00 00")
    ko_nonnegative = emit("0F 83 00 00 00 00")
    emit("4C 8B 9B A0 04 00 00")  # head-health owner
    ko_null_node = emit("4D 85 DB 0F 84 00 00 00 00")
    emit("4D 8B 5B 48")  # actor
    ko_null_actor = emit("4D 85 DB 0F 84 00 00 00 00")
    emit("45 8B 5B BC")  # actor side in r11d, preserving rbx
    ko_side = emit("44 3B 1D 00 00 00 00")  # compare selected opponent side
    rip32(ko_side, 3, 7, data_start + 612)
    ko_wrong_side = emit("0F 85 00 00 00 00")
    emit("C5 E8 57 D2 C5 F8 2E CA")  # selected-category multiplier > 0
    ko_empty_category = emit("0F 86 00 00 00 00")
    emit("48 8B 83 A0 04 00 00")  # canonical head-health node
    ko_null_head_node = emit("48 85 C0 0F 84 00 00 00 00")
    emit("48 8B 40 58")  # head impact multiplier
    ko_null_head_value = emit("48 85 C0 0F 84 00 00 00 00")
    emit("C5 FA 10 08 C5 E8 57 D2 C5 F8 2E CA")
    ko_empty_head = emit("0F 86 00 00 00 00")
    ko_category = emit("44 89 3D 00 00 00 00")
    rip32(ko_category, 3, 7, data_start + 616)
    emit("4C 8B 5D 08")  # guest return address identifies the impact caller
    ko_caller = emit("4C 89 1D 00 00 00 00")
    rip32(ko_caller, 3, 7, data_start + 624)
    ko_constant = emit("C5 FA 10 05 00 00 00 00")  # xmm0 = -1000000.0
    rip32(ko_constant, 4, 8, data_start + 608)
    ko_count = emit("FF 05 00 00 00 00")
    rip32(ko_count, 2, 6, data_start + 604)
    ko_done = emit("E9 00 00 00 00")
    ordinary = len(code)
    for branch in (ko_enabled + 9, ko_unordered + 2, ko_nonnegative + 2,
                   ko_null_node + 5, ko_null_actor + 5, ko_wrong_side + 2,
                   ko_empty_category + 2, ko_null_head_node + 5,
                   ko_null_head_value + 5, ko_empty_head + 2):
        struct.pack_into("<i", code, branch, ordinary - (branch + 4))
    enabled = emit("83 3D 00 00 00 00 00 74 00")
    rip32(enabled, 2, 7, data_start)
    emit("C5 E8 57 D2 C5 F8 2E C2")  # compare input xmm0 with zero
    unordered = emit("7A 00")
    nonnegative = emit("73 00")
    emit("4C 8B 9B A0 04 00 00")  # owner head-health node
    null_node = emit("4D 85 DB 74 00")
    emit("4D 8B 5B 48")  # actor+0x158
    null_actor = emit("4D 85 DB 74 00")
    emit("41 83 7B BC 01")  # actor+0x114 <= 1
    invalid_side = emit("77 00")
    emit("41 83 7B BC 00")
    left = emit("75 00 F6 05 00 00 00 00 01 EB 00")
    rip32(left + 2, 2, 7, data_start)
    right = len(code)
    right_test = emit("F6 05 00 00 00 00 02")
    rip32(right_test, 2, 7, data_start)
    decision = len(code)
    not_selected = emit("74 00")
    blocked = emit("FF 05 00 00 00 00")
    rip32(blocked, 2, 6, data_start + 512)
    emit("C5 F8 57 C0")  # vxorps xmm0,xmm0,xmm0
    finish = len(code)
    struct.pack_into("<i", code, ko_done + 1, finish - (ko_done + 5))
    emit("41 5B 9D")
    emit("C5 F2 59 D8")  # original vmulss xmm3,xmm1,xmm0
    vandps = emit("C5 E0 54 15 00 00 00 00")  # original RIP-relative constant
    original_constant = (IMPACT_SITE + len(IMPACT_ORIGINAL) +
                         struct.unpack_from("<i", IMPACT_ORIGINAL, 8)[0])
    rip32(vandps, 4, 8, original_constant)
    back = emit("E9 00 00 00 00")
    for branch, target in ((enabled + 8, finish),
                           (unordered + 1, finish), (nonnegative + 1, finish),
                           (null_node + 4, finish), (null_actor + 4, finish),
                           (invalid_side + 1, finish), (left + 1, right),
                           (left + 10, decision), (not_selected + 1, finish)):
        displacement = target - (branch + 1)
        if not -128 <= displacement <= 127:
            raise ValueError("impact branch out of range")
        code[branch] = displacement & 255
    struct.pack_into("<i", code, back + 1,
                     IMPACT_SITE + len(IMPACT_ORIGINAL) - (code_start + back + 5))
    return code


def _ko_event_block(code_start, data_start):
    """Select UFC 4's head/KO health event before a verified opponent hit.

    The later impact hook can safely identify and consume the opponent-only
    one-shot, but UFC 4 has already selected body/leg event logic by then.
    This site has the same fighter key and manager used by the health routine,
    so resolve that fighter, verify its actor side, and change only the armed
    opponent's category to the game's head category (0).
    """
    code = bytearray()

    def emit(raw):
        at = len(code)
        code.extend(bytes.fromhex(raw))
        return at

    def rip32(at, offset, size, target):
        struct.pack_into("<i", code, at + offset,
                         target - (code_start + at + size))

    def branch32(opcode):
        return emit(opcode + " 00 00 00 00")

    emit("9C 50 51 52 41 52 41 53")  # flags, rax, rcx, rdx, r10, r11
    enabled = emit("83 3D 00 00 00 00 00")
    rip32(enabled, 2, 7, data_start + 600)
    exits = [branch32("0F 84")]
    emit("4C 8B 57 20 4D 85 D2")  # bucket count
    exits.append(branch32("0F 84"))
    emit("48 8B 4F 18 48 85 C9")  # bucket table
    exits.append(branch32("0F 84"))
    emit("48 89 F0 31 D2 49 F7 F2 4C 8B 1C D1")
    emit("4D 85 DB")
    empty_bucket = branch32("0F 84")
    chain = len(code)
    emit("49 39 33")  # record key == fighter key
    found = branch32("0F 84")
    emit("4D 8B 9B F0 04 00 00 4D 85 DB")
    loop = branch32("0F 85")
    fallback = len(code)
    emit("4E 8B 1C D1")  # table[count] fallback record
    found_at = len(code)
    emit("4D 85 DB")
    exits.append(branch32("0F 84"))
    emit("4D 8B 9B A0 04 00 00 4D 85 DB")
    exits.append(branch32("0F 84"))
    emit("4D 8B 5B 48 4D 85 DB")
    exits.append(branch32("0F 84"))
    emit("41 8B 83 BC 00 00 00")
    side = emit("3B 05 00 00 00 00")
    rip32(side, 2, 6, data_start + 612)
    exits.append(branch32("0F 85"))
    emit("45 31 F6 44 89 75 AC")  # r14d/category = head before event selection
    ordinary = len(code)
    emit("41 5B 41 5A 5A 59 58 9D")
    emit("45 89 F0 C5 F8 29 85 70 FF FF FF")
    call = emit("E8 00 00 00 00")
    original_call_target = (KO_EVENT_SITE + len(KO_EVENT_ORIGINAL) +
                            struct.unpack_from("<i", KO_EVENT_ORIGINAL, 12)[0])
    struct.pack_into("<i", code, call + 1,
                     original_call_target - (code_start + call + 5))
    back = emit("E9 00 00 00 00")
    struct.pack_into("<i", code, back + 1,
                     KO_EVENT_SITE + len(KO_EVENT_ORIGINAL) - (code_start + back + 5))
    for branch in exits:
        struct.pack_into("<i", code, branch + 2, ordinary - (branch + 6))
    struct.pack_into("<i", code, empty_bucket + 2, fallback - (empty_bucket + 6))
    struct.pack_into("<i", code, found + 2, found_at - (found + 6))
    struct.pack_into("<i", code, loop + 2, chain - (loop + 6))
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
    sites = ((HEALTH_SITE, HEALTH_ORIGINAL),
             (OTHER_HEALTH_SITE, OTHER_HEALTH_ORIGINAL),
             (AUX_HEALTH_SITE, AUX_HEALTH_ORIGINAL))
    if all(address + len(original) <= eboot_end and
           result[address:address + len(original)] == original
           for address, original in sites) and HUD_SITE + len(HUD_ORIGINAL) <= eboot_end and \
           result[HUD_SITE:HUD_SITE + len(HUD_ORIGINAL)] == HUD_ORIGINAL and \
           IMPACT_SITE + len(IMPACT_ORIGINAL) <= eboot_end and \
           result[IMPACT_SITE:IMPACT_SITE + len(IMPACT_ORIGINAL)] == IMPACT_ORIGINAL:
        health_code_start = _align(len(result), 65536)
        health_data_start = health_code_start + 4096
        health_code = bytearray()
        offsets = []
        for ordinal, (address, _) in enumerate(sites):
            while len(health_code) & 15:
                health_code.append(0x90)
            offsets.append(len(health_code))
            health_code.extend(_health_block(address, health_code_start + len(health_code),
                                             health_data_start, ordinal))
        while len(health_code) & 15:
            health_code.append(0x90)
        hud_offset = len(health_code)
        health_code.extend(_hud_block(health_code_start + hud_offset, health_data_start))
        while len(health_code) & 15:
            health_code.append(0x90)
        impact_offset = len(health_code)
        health_code.extend(_impact_block(health_code_start + impact_offset, health_data_start))
        if len(health_code) <= 4096 and all(
                health_code_start + offset - (address + 5) <= 0x7fffffff
                for (address, _), offset in zip(sites, offsets)) and \
                health_code_start + hud_offset - (HUD_SITE + 5) <= 0x7fffffff and \
                health_code_start + impact_offset - (IMPACT_SITE + 5) <= 0x7fffffff:
            result.extend(bytes(health_data_start + 4096 - len(result)))
            result[health_code_start:health_code_start + len(health_code)] = health_code
            result[health_data_start + 16:health_data_start + 16 + len(HEALTH_MARKER)] = HEALTH_MARKER
            struct.pack_into("<f", result, health_data_start + 608, -1000000.0)
            struct.pack_into("<I", result, health_data_start + 612, 0xffffffff)
            for (address, original), offset in zip(sites, offsets):
                result[address:address + len(original)] = (
                    b"\xe9" + struct.pack("<i", health_code_start + offset - (address + 5)) +
                    b"\x90" * (len(original) - 5))
            result[HUD_SITE:HUD_SITE + len(HUD_ORIGINAL)] = (
                b"\xe9" + struct.pack("<i", health_code_start + hud_offset - (HUD_SITE + 5)) +
                b"\x90" * (len(HUD_ORIGINAL) - 5))
            result[IMPACT_SITE:IMPACT_SITE + len(IMPACT_ORIGINAL)] = (
                b"\xe9" + struct.pack("<i", health_code_start + impact_offset - (IMPACT_SITE + 5)) +
                b"\x90" * (len(IMPACT_ORIGINAL) - 5))
            mapped.extend(((health_code_start, 4096, 5), (health_data_start, 4096, 6)))
            report["applied"].append("overlay_no_damage_live")
            report["code_bytes"] += len(health_code)
            report["overlay_health_store_mask_offset"] = health_data_start
        else:
            report["skipped"].append("overlay health store: hook out of range")
    else:
        report["skipped"].append("overlay health store: exact site mismatch")
    return result, mapped, report
