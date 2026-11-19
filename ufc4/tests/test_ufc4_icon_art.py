import hashlib
import pathlib
import sys
import tempfile
import unittest
import zipfile
import zlib

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "src"))
import ufc4_icon_art as art
from ufc4_keyboard.keyboard_bindings import DEFAULTS, load_bbport


class ButtonArtTest(unittest.TestCase):
    def test_portheon_keys_override_sacredfire_defaults(self):
        self.assertEqual(DEFAULTS["Cross"], "Space")
        self.assertEqual(DEFAULTS["Up"], "I")
        with tempfile.TemporaryDirectory() as folder:
            path = pathlib.Path(folder) / "bbport.ini"
            path.write_text("volume=100\nkey.cross=G,Space\nkey.look_up=Keypad 8\nkey.unknown=X\n", encoding="utf-8")
            keys = load_bbport(path).controls
            self.assertEqual(keys["Cross"], "G")
            self.assertEqual(keys["RightStickUp"], "Keypad 8")
            self.assertEqual(keys["Circle"], "Left Shift")

    def test_switch_and_restore_are_exact(self):
        root = pathlib.Path(__file__).resolve().parents[1]
        reference = root / "ufc4_native_plus_release.zip"
        if reference.exists():
            with zipfile.ZipFile(reference) as archive:
                original = archive.read(next(n for n in archive.namelist() if n.endswith("/buttonart_orig.bin")))
                expected = archive.read(next(n for n in archive.namelist() if n.endswith("/buttonart_xbox.bin")))
        else:
            prepared = root / "dist" / "ufc4_buttonart_ps.bin"
            if not prepared.exists():
                self.skipTest("UFC 4 v28 artwork unavailable")
            original = prepared.read_bytes()
            expected = None
        self.assertEqual(hashlib.sha256(original).hexdigest(), art.ORIGINAL_SHA256)
        xbox = art.xbox_art(original)
        keyboard = art.keyboard_art(original)
        self.assertEqual(len(keyboard), len(original))
        self.assertEqual(len(zlib.decompress(keyboard[8:])), len(zlib.decompress(original[8:])))
        self.assertNotEqual(keyboard, original)
        if expected is not None:
            self.assertEqual(xbox, expected)
        with tempfile.TemporaryDirectory() as folder:
            base = pathlib.Path(folder)
            game = base / "inxp_ui_40.big"
            game.write_bytes(original)
            old_offset = art.ART_OFFSET
            art.ART_OFFSET = 0
            try:
                art.prepare(game, base / "prepared")
                art.set_style(game, base / "prepared", "xbox")
                self.assertEqual(game.read_bytes(), xbox)
                art.set_style(game, base / "prepared", "keyboard")
                self.assertEqual(game.read_bytes(), keyboard)
                art.set_style(game, base / "prepared", "xbox")
                self.assertEqual(game.read_bytes(), xbox)
                art.set_style(game, base / "prepared", "ps")
                self.assertEqual(game.read_bytes(), original)
                game.write_bytes(b"?" + original[1:])
                with self.assertRaises(ValueError):
                    art.set_style(game, base / "prepared", "xbox")
            finally:
                art.ART_OFFSET = old_offset


if __name__ == "__main__":
    unittest.main()
