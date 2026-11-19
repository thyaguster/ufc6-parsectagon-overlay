"""Exercise UFC 4's saved PORTHEON cheat choices without launching the game."""

import ctypes
import json
import pathlib
import tempfile
import unittest


class CheatConfigTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        dll = pathlib.Path(__file__).resolve().parents[1] / "dist" / "ufc4_actions_core_test.dll"
        cls.core = ctypes.WinDLL(str(dll))
        cls.core.Ufc4TestCheatPath.argtypes = [ctypes.c_wchar_p]
        cls.core.Ufc4TestCheatRead.restype = ctypes.c_int
        cls.core.Ufc4TestCheatToggle.argtypes = [ctypes.c_int]
        cls.core.Ufc4TestCheatToggle.restype = ctypes.c_int

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.path = pathlib.Path(self.temporary.name) / "cheats.json"
        self.core.Ufc4TestCheatPath(str(self.path))

    def tearDown(self):
        self.core.Ufc4TestCheatPath(None)
        self.temporary.cleanup()

    def test_toggle_preserves_other_cheats_and_fields(self):
        original = '{"enabled":["other_cheat","ufc4_stamina_right"],"extra":{"enabled":"keep"}}\n'
        self.path.write_text(original, encoding="utf-8")
        self.assertEqual(self.core.Ufc4TestCheatRead(), 2)
        self.assertEqual(self.core.Ufc4TestCheatToggle(0), 3)
        changed = json.loads(self.path.read_text(encoding="utf-8"))
        self.assertEqual(changed["enabled"], ["other_cheat", "ufc4_stamina_right", "ufc4_stamina_left"])
        self.assertEqual(changed["extra"], {"enabled": "keep"})
        self.assertEqual((self.path.with_name("cheats.json.ufc4-overlay.bak")).read_text(encoding="utf-8"), original)
        self.assertEqual(self.core.Ufc4TestCheatToggle(1), 1)
        self.assertEqual(json.loads(self.path.read_text())["enabled"], ["other_cheat", "ufc4_stamina_left"])

    def test_invalid_or_missing_config_is_left_untouched(self):
        self.assertEqual(self.core.Ufc4TestCheatRead(), -1)
        self.assertEqual(self.core.Ufc4TestCheatToggle(0), -1)
        invalid = '{"enabled":[1],"extra":true}\n'
        self.path.write_text(invalid, encoding="utf-8")
        self.assertEqual(self.core.Ufc4TestCheatToggle(0), -1)
        self.assertEqual(self.path.read_text(encoding="utf-8"), invalid)
        self.assertFalse((self.path.with_name("cheats.json.ufc4-overlay.bak")).exists())


if __name__ == "__main__":
    unittest.main()
