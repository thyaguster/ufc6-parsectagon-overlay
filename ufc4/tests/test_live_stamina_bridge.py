"""Check the launch-time hook and its live writable mask without running UFC 4."""

import ctypes
import importlib.util
import pathlib
import struct
import sys
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class LiveStaminaBridgeTest(unittest.TestCase):
    def test_linker_keeps_instruction_after_eight_byte_hook(self):
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
                spec = importlib.util.spec_from_file_location("ufc4_link_bridge_test", ROOT / "port_bridge" / "cheats.py")
                bridge = importlib.util.module_from_spec(spec)
                spec.loader.exec_module(bridge)
                image = bytearray(0x20000)
                site = 0x1000
                image[site : site + len(bridge.PATTERN)] = bridge.PATTERN
                linked, segments, report = bridge.apply(image, [(0, len(image), 5)], len(image), [], {bridge.LIVE_ID})
                mask = report["overlay_live_mask_offset"]
                self.assertEqual(report["applied"], ["overlay_stamina_live"])
                self.assertEqual(linked[site], 0xE9)
                self.assertEqual(linked[site + 8 : site + 11], bridge.PATTERN[8:11])
                self.assertEqual(linked[mask : mask + 4], b"\0" * 4)
                self.assertEqual(linked[mask + 16 : mask + 16 + len(bridge.MARKER)], bridge.MARKER)
                self.assertEqual(segments[-2][2], 5)
                self.assertEqual(segments[-1], (mask, 4096, 6))
                jump = struct.unpack_from("<i", linked, site + 1)[0]
                code = site + 5 + jump
                self.assertEqual(code, segments[-2][0])
                self.assertEqual(linked[code : code + 4], bytes.fromhex("c5 fa 5d c1"))
                for field, after in ((6, 11), (24, 29), (33, 38)):
                    displacement = struct.unpack_from("<i", linked, code + field)[0]
                    self.assertEqual(code + after + displacement, mask)
                back = code + report["code_bytes"] - 5
                self.assertEqual(back + 5 + struct.unpack_from("<i", linked, back + 1)[0], site + 8)
            finally:
                sys.path.remove(folder)
                sys.modules.pop("cheats_portheon", None)

    def test_overlay_changes_only_writable_mask(self):
        dll = ROOT / "dist" / "ufc4_actions_core_test.dll"
        core = ctypes.WinDLL(str(dll))
        kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel.VirtualAlloc.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_ulong, ctypes.c_ulong]
        kernel.VirtualAlloc.restype = ctypes.c_void_p
        address = kernel.VirtualAlloc(None, 4096, 0x3000, 0x04)
        self.assertTrue(address)
        marker = b"UFC4LIVE_STAMINA1"
        ctypes.memmove(address + 16, marker, len(marker))
        self.assertEqual(core.Ufc4TestStaminaPrepare(), 1)
        toggle = core.Ufc4TestStaminaToggle
        toggle.argtypes = [ctypes.c_int]
        toggle.restype = ctypes.c_uint
        self.assertEqual([toggle(i) for i in (0, 1, 0, 1)], [1, 3, 2, 0])
        self.assertEqual(ctypes.c_uint.from_address(address).value, 0)
        self.assertEqual(ctypes.string_at(address + 16, len(marker)), marker)


if __name__ == "__main__":
    unittest.main()
