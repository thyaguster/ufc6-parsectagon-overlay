"""Prepare reversible UFC 4 button art from the user's own game archive.

The game SWF contains PlayStation and Xbox artwork. Xbox mode switches symbol
IDs; keyboard mode replaces the existing bitmaps with rendered keycaps. Both
variants preserve the archive slot size.
"""

import argparse
import hashlib
import pathlib
import sys
import zlib

ART_OFFSET = 0xF66A200
ART_SIZE = 88036
TAIL_OFFSET = 84758
TAIL_END = 88034
SYMBOL_OFFSET = 154522
SYMBOL_SIZE = 3322
ORIGINAL_SHA256 = "f3d69e4ec09c88b869686724674096a0eae95af18104c67b21c52e0767cc4de3"


def keyboard_art(original: bytes, bindings_path: pathlib.Path | None = None) -> bytes:
    """Use the overlay's existing keycap renderer inside UFC 4's Flash bitmaps."""
    sys.path.insert(0, str(pathlib.Path(__file__).with_name("ufc4_keyboard")))
    from keyboard_bindings import load_bbport
    from keyboard_flash import decode_swf, library_map, patch

    if hashlib.sha256(original).hexdigest() != ORIGINAL_SHA256:
        raise ValueError("Keyboard artwork requires the supported original UFC 4 archive")
    size = len(original)
    frame = (size & 0xffffff).to_bytes(4, "big") + bytes((0, (size >> 16) & 15)) + (size & 0xffff).to_bytes(2, "big")
    mapped = library_map(decode_swf(original))
    if len(mapped) != 93:
        raise ValueError("UFC 4 keyboard bitmap inventory changed")
    result = patch(frame + original, load_bbport(bindings_path), len(mapped))[8:]
    if len(result) != ART_SIZE or result[:8] != original[:8]:
        raise ValueError("Keyboard button-art patch failed verification")
    return result


def xbox_art(original: bytes) -> bytes:
    if len(original) != ART_SIZE or original[:3] != b"CWS":
        raise ValueError("UFC 4 button art does not match the supported version")
    data = bytearray(zlib.decompress(original[8:]))
    symbols = data[SYMBOL_OFFSET:SYMBOL_OFFSET + SYMBOL_SIZE]
    count = int.from_bytes(symbols[:2], "little")
    position = 2
    names = {}
    for _ in range(count):
        cid = int.from_bytes(symbols[position:position + 2], "little")
        cid_offset = SYMBOL_OFFSET + position
        position += 2
        end = symbols.index(0, position)
        name = symbols[position:end].decode("latin1")
        position = end + 1
        names[name] = (cid_offset, cid)
    swaps = 0
    for name, (ps_offset, ps_cid) in names.items():
        if "_ps_" not in name:
            continue
        other = names.get(name.replace("_ps_", "_xb_"))
        if other is None:
            continue
        xb_offset, xb_cid = other
        data[ps_offset:ps_offset + 2] = xb_cid.to_bytes(2, "little")
        data[xb_offset:xb_offset + 2] = ps_cid.to_bytes(2, "little")
        swaps += 1
    if swaps < 4:
        raise ValueError("Xbox artwork mappings were not found")
    tail = (pathlib.Path(__file__).with_name("ufc4_buttonart_xbox_tail.bin")).read_bytes()
    if len(tail) != TAIL_END - TAIL_OFFSET:
        raise ValueError("Xbox button-art patch is incomplete")
    result = original[:TAIL_OFFSET] + tail + original[TAIL_END:]
    if zlib.decompress(result[8:]) != data:
        raise ValueError("Xbox button-art patch failed verification")
    return result


def prepare(archive: pathlib.Path, output: pathlib.Path,
            bindings_path: pathlib.Path | None = None) -> None:
    with archive.open("rb") as stream:
        stream.seek(ART_OFFSET)
        current = stream.read(ART_SIZE)
    ps_file = output / "ufc4_buttonart_ps.bin"
    xbox_file = output / "ufc4_buttonart_xbox.bin"
    keyboard_file = output / "ufc4_buttonart_keyboard.bin"
    if ps_file.exists() and xbox_file.exists():
        original, xbox = ps_file.read_bytes(), xbox_file.read_bytes()
        if hashlib.sha256(original).hexdigest() != ORIGINAL_SHA256 or xbox_art(original) != xbox:
            raise ValueError("Prepared button artwork failed validation")
        previous_keyboard = keyboard_file.read_bytes() if keyboard_file.exists() else None
        keyboard = keyboard_art(original, bindings_path)
        if current not in (original, xbox, keyboard, previous_keyboard):
            raise ValueError("Archive button art changed; refusing to overwrite it")
        output.mkdir(parents=True, exist_ok=True)
        keyboard_file.write_bytes(keyboard)
        if previous_keyboard is not None and current == previous_keyboard and current != keyboard:
            with archive.open("r+b") as stream:
                stream.seek(ART_OFFSET)
                stream.write(keyboard)
                stream.flush()
        (output / "ufc4_buttonart_archive.txt").write_text(str(archive.resolve()), encoding="utf-8")
        return
    original = current
    if hashlib.sha256(original).hexdigest() != ORIGINAL_SHA256:
        raise ValueError("UFC 4 archive version is not supported for button icons")
    xbox = xbox_art(original)
    keyboard = keyboard_art(original, bindings_path)
    output.mkdir(parents=True, exist_ok=True)
    ps_file.write_bytes(original)
    xbox_file.write_bytes(xbox)
    keyboard_file.write_bytes(keyboard)
    (output / "ufc4_buttonart_archive.txt").write_text(str(archive.resolve()), encoding="utf-8")


def set_style(archive: pathlib.Path, output: pathlib.Path, style: str) -> None:
    original = (output / "ufc4_buttonart_ps.bin").read_bytes()
    xbox = (output / "ufc4_buttonart_xbox.bin").read_bytes()
    keyboard = (output / "ufc4_buttonart_keyboard.bin").read_bytes()
    desired = {"ps": original, "xbox": xbox, "keyboard": keyboard}[style]
    with archive.open("r+b") as stream:
        stream.seek(ART_OFFSET)
        current = stream.read(ART_SIZE)
        if current not in (original, xbox, keyboard):
            raise ValueError("Archive button art changed; refusing to overwrite it")
        if current == desired:
            return
        stream.seek(ART_OFFSET)
        stream.write(desired)
        stream.flush()
        stream.seek(ART_OFFSET)
        if stream.read(ART_SIZE) != desired:
            raise IOError("Button art verification failed")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("command", choices=("prepare", "set"))
    parser.add_argument("--archive", required=True, type=pathlib.Path)
    parser.add_argument("--output", required=True, type=pathlib.Path)
    parser.add_argument("--style", choices=("xbox", "ps", "keyboard"))
    parser.add_argument("--bindings", type=pathlib.Path,
                        help="PORTHEON/SacredFire bbport.ini with key.<action> bindings")
    args = parser.parse_args()
    if args.command == "prepare":
        prepare(args.archive, args.output, args.bindings)
    else:
        if not args.style:
            parser.error("--style is required for set")
        set_style(args.archive, args.output, args.style)


if __name__ == "__main__":
    main()
