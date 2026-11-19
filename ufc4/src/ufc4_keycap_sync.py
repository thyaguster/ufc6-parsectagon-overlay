"""Rebuild UFC 4 keyboard prompts from PORTHEON's saved SacredFire keys."""

import os
import pathlib
import sys

# Explicit imports keep these modules available in the standalone helper.
from ufc4_keyboard import keyboard_art, keyboard_bindings, keyboard_flash, patcher_core  # noqa: F401
from ufc4_icon_art import ART_OFFSET, ART_SIZE, prepare, set_style as set_flash_style
from ufc4_direct_icons import prepare as prepare_direct, set_style as set_direct_style
import ufc4_stock_xbox  # bundled stock artwork decoder for the standalone helper


def main() -> int:
    if len(sys.argv) != 3 or sys.argv[2] not in ("ps", "xbox", "keyboard"):
        return 2
    assets = pathlib.Path(sys.argv[1])
    style = sys.argv[2]
    try:
        archive = pathlib.Path((assets / "ufc4_buttonart_archive.txt").read_text(encoding="utf-8").strip())
    except OSError as error:
        print(f"UFC 4 artwork location unavailable: {error}", file=sys.stderr)
        return 1
    local = os.environ.get("LOCALAPPDATA")
    bindings = pathlib.Path(local) / "UFC4Native" / "bbport.ini" if local else None
    try:
        if style == "keyboard":
            prepare(archive, assets, bindings)
            prepare_direct(archive, archive.parent / "00index.csv", assets, bindings)
        with archive.open("rb") as stream:
            stream.seek(ART_OFFSET)
            current_flash = stream.read(ART_SIZE)
        old_style = next((name for name in ("ps", "xbox", "keyboard")
                          if current_flash == (assets / f"ufc4_buttonart_{name}.bin").read_bytes()), None)
        if old_style is None:
            raise ValueError("Current UFC 4 Flash icons are not recognized")
        set_direct_style(archive, assets, style)
        try:
            set_flash_style(archive, assets, style)
        except Exception:
            # Keep both art sets on the previous selection when the Flash
            # archive slot cannot be switched.
            set_direct_style(archive, assets, old_style)
            raise
    except (OSError, ValueError) as error:
        print(f"UFC 4 keyboard prompt sync failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
