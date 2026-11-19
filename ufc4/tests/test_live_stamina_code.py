"""Check the generated live detour's branch destinations before in-game use."""

import ctypes
import pathlib
import unittest


class LiveStaminaCodeTest(unittest.TestCase):
    def test_left_right_and_original_store_paths(self):
        dll = pathlib.Path(__file__).resolve().parents[1] / "dist" / "ufc4_actions_core_test.dll"
        core = ctypes.WinDLL(str(dll))
        code = (ctypes.c_ubyte * 96)()
        build = core.Ufc4TestStaminaDetour
        build.argtypes = [ctypes.POINTER(ctypes.c_ubyte), ctypes.c_size_t]
        self.assertEqual(build(code, len(code)), 1)
        data = bytes(code)
        self.assertTrue(data.startswith(bytes.fromhex("c5 fa 5d c1 41 52 49 ba")))

        right = data.index(bytes.fromhex("41 f6 c2 02"))
        choose = data.index(bytes.fromhex("41 5a 74"))
        keep = data.index(bytes.fromhex("c5 fa 11 08"))
        original = data.index(bytes.fromhex("c5 fa 11 00"))
        tail = data.index(bytes.fromhex("49 8b 00 48 3b 45 d0"))
        branch_right = data.index(bytes.fromhex("75"), 24)
        branch_choose = data.index(bytes.fromhex("eb"), branch_right)
        branch_original = data.index(bytes.fromhex("74"), choose)
        branch_tail = data.index(bytes.fromhex("eb"), keep)

        for branch, target in (
            (branch_right, right),
            (branch_choose, choose),
            (branch_original, original),
            (branch_tail, tail),
        ):
            self.assertEqual(branch + 2 + ctypes.c_int8(data[branch + 1]).value, target)
        self.assertEqual(int.from_bytes(data[tail + 13 : tail + 21], "little"), 0x10000F)

    def test_exact_site_is_hooked_and_toggle_is_session_only(self):
        dll = pathlib.Path(__file__).resolve().parents[1] / "dist" / "ufc4_actions_core_test.dll"
        core = ctypes.WinDLL(str(dll))
        kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel.VirtualAlloc.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_ulong, ctypes.c_ulong]
        kernel.VirtualAlloc.restype = ctypes.c_void_p
        kernel.VirtualProtect.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_ulong, ctypes.POINTER(ctypes.c_ulong)]
        address = kernel.VirtualAlloc(None, 4096, 0x3000, 0x04)
        self.assertTrue(address)
        signature = bytes.fromhex("c5 fa 5d c1 c5 fa 11 00 49 8b 00 48 3b 45 d0")
        ctypes.memmove(address, signature + b"\xc3", 16)
        old = ctypes.c_ulong()
        self.assertTrue(kernel.VirtualProtect(address, 4096, 0x20, ctypes.byref(old)))
        self.assertEqual(core.Ufc4TestStaminaPrepare(), 1)
        self.assertEqual(ctypes.string_at(address, 2), bytes.fromhex("ff 25"))
        toggle = core.Ufc4TestStaminaToggle
        toggle.argtypes = [ctypes.c_int]
        toggle.restype = ctypes.c_uint
        self.assertEqual([toggle(i) for i in (0, 1, 0, 1)], [1, 3, 2, 0])


if __name__ == "__main__":
    unittest.main()
