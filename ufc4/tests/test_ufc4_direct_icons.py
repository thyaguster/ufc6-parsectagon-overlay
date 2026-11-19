import json
import pathlib
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "src"))
from ufc4_direct_icons import set_style
from ufc4_gnf_codec import dimensions, replace_image
from ufc4_keyboard.keyboard_art import descriptor, render
from ufc4_keyboard.keyboard_bindings import load_bbport


class DirectIconsTest(unittest.TestCase):
    def test_prepared_assets_switch_and_restore(self):
        dist = pathlib.Path(__file__).resolve().parents[1] / "dist"
        map_file = dist / "ufc4_direct_icons_map.json"
        if not map_file.exists():
            self.skipTest("UFC 4 artwork not prepared")
        rows = json.loads(map_file.read_text())
        self.assertGreaterEqual(len(rows), 100)
        blobs = {key: (dist / f"ufc4_direct_icons_{key}.bin").read_bytes()
                 for key in ("ps", "xbox", "keyboard")}
        self.assertTrue(all(len(blob) == sum(row["size"] for row in rows) for blob in blobs.values()))
        wanted = ("white/digital_one.gnf", "digital_one.gnf",
                  "left_right_bumper.gnf", "left_right_trigger.gnf",
                  "text_ground_game_hud/las.gnf")
        picked = []
        cursor = 0
        for row in rows:
            if row["name"] in wanted:
                picked.append((row, cursor))
            cursor += row["size"]
        self.assertEqual(len(picked), len(wanted))
        self.assertTrue(any(blobs["xbox"][at:at+row["size"]] != blobs["ps"][at:at+row["size"]]
                            for row, at in picked))
        for row, at in picked:
            if "bumper" in row["name"] or "trigger" in row["name"]:
                self.assertNotEqual(blobs["xbox"][at:at+row["size"]],
                                    blobs["ps"][at:at+row["size"]])
            if row["name"] == "text_ground_game_hud/las.gnf":
                self.assertEqual(blobs["xbox"][at:at+row["size"]],
                                 blobs["ps"][at:at+row["size"]])
        with tempfile.TemporaryDirectory() as folder:
            root = pathlib.Path(folder)
            small_rows = []
            small_blobs = {key: bytearray() for key in blobs}
            for row, at in picked:
                small_rows.append({**row, "offset": len(small_blobs["ps"])})
                for key in blobs:
                    small_blobs[key] += blobs[key][at:at+row["size"]]
            (root / "ufc4_direct_icons_map.json").write_text(json.dumps(small_rows))
            for key, blob in small_blobs.items():
                (root / f"ufc4_direct_icons_{key}.bin").write_bytes(blob)
            game = root / "archive.big"
            game.write_bytes(small_blobs["ps"])
            for key in ("xbox", "keyboard", "ps"):
                set_style(game, root, key)
                self.assertEqual(game.read_bytes(), small_blobs[key])
            game.write_bytes(b"?" + game.read_bytes()[1:])
            with self.assertRaises(ValueError):
                set_style(game, root, "xbox")

    def test_keyboard_ground_indicator_has_four_keys(self):
        self.assertEqual(descriptor("text_ground_game_hud/las")["layout"], "cross")
        dist = pathlib.Path(__file__).resolve().parents[1] / "dist"
        map_file = dist / "ufc4_direct_icons_map.json"
        if not map_file.exists():
            self.skipTest("UFC 4 artwork not prepared")
        rows = json.loads(map_file.read_text())
        row = next(r for r in rows if r["name"] == "text_ground_game_hud/las.gnf")
        cursor = sum(r["size"] for r in rows[:rows.index(row)])
        original = (dist / "ufc4_direct_icons_ps.bin").read_bytes()[cursor:cursor+row["size"]]
        width, height = dimensions(original)
        replacement = replace_image(original, render(width, height, descriptor("las"), load_bbport(None)))
        self.assertEqual(len(replacement), len(original))
        self.assertNotEqual(replacement, original)


if __name__ == "__main__":
    unittest.main()
