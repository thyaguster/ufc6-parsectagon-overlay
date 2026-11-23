"""Regression checks for the live UFC 4 damage hook boundaries and mask."""

import ctypes
import importlib.util
import pathlib
import struct
import sys
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class LiveDamageBridgeTest(unittest.TestCase):
    def test_linker_preserves_branch_entries_and_relocates_impact_constant(self):
        with tempfile.TemporaryDirectory() as folder:
            stub = pathlib.Path(folder) / "cheats_portheon.py"
            stub.write_text(
                "def load(*args): return [], set()\n"
                "def apply(image, segments, eboot_end, packs, enabled):\n"
                "    return image, segments, {'applied': [], 'skipped': [], 'code_bytes': 0}\n",
                encoding="utf-8",
            )
            sys.path.insert(0, folder)
            try:
                spec = importlib.util.spec_from_file_location("ufc4_damage_bridge_test", ROOT / "port_bridge" / "cheats.py")
                bridge = importlib.util.module_from_spec(spec)
                spec.loader.exec_module(bridge)
                image = bytearray(0x700000)
                image[0x698113:0x698113 + len(bridge.PATTERN)] = bridge.PATTERN
                sites = (
                    (bridge.HEALTH_SITE, bridge.HEALTH_ORIGINAL),
                    (bridge.OTHER_HEALTH_SITE, bridge.OTHER_HEALTH_ORIGINAL),
                    (bridge.AUX_HEALTH_SITE, bridge.AUX_HEALTH_ORIGINAL),
                    (bridge.HUD_SITE, bridge.HUD_ORIGINAL),
                    (bridge.IMPACT_SITE, bridge.IMPACT_ORIGINAL),
                )
                for site, original in sites:
                    image[site:site + len(original)] = original
                    image[site + len(original):site + len(original) + 4] = b"NEXT"
                linked, segments, report = bridge.apply(
                    image, [(0, len(image), 5)], len(image), [], {bridge.LIVE_ID}
                )
                self.assertEqual(report["skipped"], [])
                self.assertEqual(report["applied"], ["overlay_stamina_live", "overlay_no_damage_live"])
                mask = report["overlay_health_store_mask_offset"]
                self.assertEqual(linked[mask:mask + 4], b"\0" * 4)
                self.assertEqual(linked[mask + 16:mask + 16 + len(bridge.HEALTH_MARKER)], bridge.HEALTH_MARKER)
                self.assertEqual(linked[mask + 600:mask + 608], b"\0" * 8)
                self.assertEqual(struct.unpack_from("<f", linked, mask + 608)[0], -1000000.0)
                self.assertEqual(struct.unpack_from("<I", linked, mask + 612)[0], 0xffffffff)
                for site, original in sites:
                    self.assertEqual(linked[site], 0xE9)
                    self.assertEqual(linked[site + len(original):site + len(original) + 4], b"NEXT")
                impact = bridge.IMPACT_SITE + 5 + struct.unpack_from("<i", linked, bridge.IMPACT_SITE + 1)[0]
                # The failed build used 41 8B 5B BC here: it wrote EBX and
                # corrupted the game's RBX pointer before returning.
                self.assertIn(bytes.fromhex("45 8B 5B BC"), linked[impact:impact + 120])
                self.assertNotIn(bytes.fromhex("41 8B 5B BC"), linked[impact:impact + 120])
                ko_side = linked.find(bytes.fromhex("44 3B 1D"), impact, impact + 120)
                self.assertGreaterEqual(ko_side, impact)
                self.assertEqual(ko_side + 7 + struct.unpack_from("<i", linked, ko_side + 3)[0], mask + 612)
                # A landed strike produces several callbacks.  The bridge must
                # keep the arm live through all of them; the overlay disarms it
                # only after the match finishes or the window expires.
                validate = linked.find(bytes.fromhex("C5 E8 57 D2 C5 F8 2E CA"), ko_side, impact + 220)
                self.assertGreater(validate, ko_side)
                self.assertNotIn(bytes.fromhex("45 31 FF 44 89 7D C0"), linked[validate:impact + 220])
                self.assertNotIn(bytes.fromhex("44 87 1D"), linked[validate:impact + 220])
                vandps = linked.find(bytes.fromhex("C5 E0 54 15"), impact, mask)
                self.assertGreaterEqual(vandps, impact)
                target = vandps + 8 + struct.unpack_from("<i", linked, vandps + 4)[0]
                original_target = (bridge.IMPACT_SITE + len(bridge.IMPACT_ORIGINAL) +
                                   struct.unpack_from("<i", bridge.IMPACT_ORIGINAL, 8)[0])
                self.assertEqual(target, original_target)
                self.assertEqual(segments[-2][2], 5)
                self.assertEqual(segments[-1], (mask, 4096, 6))
            finally:
                sys.path.remove(folder)
                sys.modules.pop("cheats_portheon", None)

    def test_overlay_toggles_only_the_live_damage_mask(self):
        core = ctypes.WinDLL(str(ROOT / "dist" / "ufc4_actions_core_test.dll"))
        kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel.VirtualAlloc.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_ulong, ctypes.c_ulong]
        kernel.VirtualAlloc.restype = ctypes.c_void_p
        address = kernel.VirtualAlloc(None, 4096, 0x3000, 0x04)
        self.assertTrue(address)
        marker = b"UFC4LIVE_NODAMAGE1"
        ctypes.memmove(address + 16, marker, len(marker))
        ctypes.c_float.from_address(address + 608).value = -1000000.0
        self.assertEqual(core.Ufc4TestDamagePrepare(), 1)
        toggle = core.Ufc4TestDamageToggle
        toggle.restype = ctypes.c_uint
        self.assertEqual([toggle(), toggle()], [2, 0])
        self.assertEqual(ctypes.c_uint.from_address(address).value, 0)
        self.assertEqual(ctypes.string_at(address + 16, len(marker)), marker)
        self.assertEqual(core.Ufc4TestKoPrepare(), 1)
        core.Ufc4TestKoSide(0)
        ko_toggle = core.Ufc4TestKoToggle
        ko_toggle.restype = ctypes.c_uint
        self.assertEqual(ko_toggle(), 1)
        self.assertEqual(ctypes.c_uint.from_address(address + 600).value, 1)
        self.assertEqual(ctypes.c_uint.from_address(address + 612).value, 0)
        self.assertEqual(core.Ufc4TestKoPoll(), 1)
        core.Ufc4TestKoStartWindow()
        self.assertEqual(core.Ufc4TestKoPoll(), 1)
        ctypes.c_uint.from_address(address + 604).value = 1
        core.Ufc4TestKoMatchState(3)
        self.assertEqual(core.Ufc4TestKoPoll(), 2)
        self.assertEqual(ctypes.c_uint.from_address(address + 600).value, 0)
        core.Ufc4TestKoMatchState(4)
        self.assertEqual(ko_toggle(), 1)
        self.assertEqual(ko_toggle(), 0)
        self.assertEqual(ko_toggle(), 1)
        core.Ufc4TestKoMatchState(3)
        self.assertEqual(core.Ufc4TestKoPoll(), 3)
        self.assertEqual(ctypes.c_uint.from_address(address + 600).value, 0)



if __name__ == "__main__":
    unittest.main()
