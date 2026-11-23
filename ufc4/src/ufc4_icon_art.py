"""Prepare reversible UFC 4 button art from the user's own game archive.

The game SWF contains PlayStation and Xbox artwork. Xbox mode switches symbol
IDs; keyboard mode replaces the existing bitmaps with rendered keycaps. Both
variants preserve the archive slot size.
"""

import argparse
import hashlib
import pathlib
import struct
import sys
import zlib

ART_OFFSET = 0xF66A200
ART_SIZE = 88036
TAIL_OFFSET = 84758
TAIL_END = 88034
SYMBOL_OFFSET = 154522
SYMBOL_SIZE = 3322
ORIGINAL_SHA256 = "f3d69e4ec09c88b869686724674096a0eae95af18104c67b21c52e0767cc4de3"


def _flash_helpers():
    keyboard = pathlib.Path(__file__).with_name("ufc4_keyboard")
    if str(keyboard) not in sys.path:
        sys.path.insert(0, str(keyboard))
    from keyboard_art import pillow
    from keyboard_flash import bitmap_for_shape, records
    from patcher_core import swf_symbols, swf_tags
    return pillow, bitmap_for_shape, records, swf_symbols, swf_tags


def _bitmap_for_symbol(plain: bytes, symbol_name: str) -> tuple[int, int]:
    _, bitmap_for_shape, records, swf_symbols, swf_tags = _flash_helpers()
    definitions = {struct.unpack_from("<H", plain, pos)[0]: (kind, pos, size)
                   for kind, pos, size in swf_tags(plain)
                   if kind in (2, 22, 32, 39, 20, 36)}
    symbols = {name: cid for name, cid, _ in swf_symbols(plain)}
    cid = symbols.get(symbol_name)
    if cid not in definitions:
        raise ValueError(f"UFC 4 footer symbol is missing: {symbol_name}")
    kind, pos, size = definitions[cid]
    if kind != 39:
        raise ValueError(f"Unexpected UFC 4 footer symbol: {symbol_name}")
    children = [struct.unpack_from("<H", plain, at + 3)[0]
                for tag, at, _, _ in records(plain, pos + 4, pos + size)
                if tag == 26]
    if len(children) != 1 or children[0] not in definitions:
        raise ValueError(f"Unexpected UFC 4 footer sprite: {symbol_name}")
    _, shape_pos, shape_size = definitions[children[0]]
    bitmap = bitmap_for_shape(plain, shape_pos, shape_size)
    if bitmap not in definitions or definitions[bitmap][0] != 36:
        raise ValueError(f"Unexpected UFC 4 footer bitmap: {symbol_name}")
    return definitions[bitmap][1:]


def _bitmap_rgba(plain: bytes, pos: int, size: int):
    pillow, _, _, _, _ = _flash_helpers()
    Image, _, _ = pillow()
    if plain[pos + 2] != 5:
        raise ValueError("Unexpected UFC 4 footer bitmap format")
    width, height = struct.unpack_from("<HH", plain, pos + 3)
    argb = zlib.decompress(plain[pos + 7:pos + size])
    if len(argb) != width * height * 4:
        raise ValueError("Unexpected UFC 4 footer bitmap size")
    rgba = bytearray(len(argb))
    for at in range(0, len(argb), 4):
        alpha, red, green, blue = argb[at:at + 4]
        rgba[at:at + 4] = (
            min(255, round(red * 255 / alpha)) if alpha else 0,
            min(255, round(green * 255 / alpha)) if alpha else 0,
            min(255, round(blue * 255 / alpha)) if alpha else 0,
            alpha,
        )
    return Image.frombytes("RGBA", (width, height), bytes(rgba))


def _write_bitmap(plain: bytearray, pos: int, size: int, image) -> None:
    width, height = struct.unpack_from("<HH", plain, pos + 3)
    if image.size != (width, height):
        raise ValueError("UFC 4 footer bitmap dimensions changed")
    rgba = image.tobytes()
    argb = bytearray(len(rgba))
    for at in range(0, len(rgba), 4):
        red, green, blue, alpha = rgba[at:at + 4]
        argb[at:at + 4] = (
            alpha,
            (red * alpha + 127) // 255,
            (green * alpha + 127) // 255,
            (blue * alpha + 127) // 255,
        )
    compressed = zlib.compress(argb, 9)
    if len(compressed) > size - 7:
        raise ValueError("Xbox footer Menu glyph exceeds its Flash allocation")
    plain[pos + 7:pos + size] = compressed + b"\0" * (size - 7 - len(compressed))


def _patch_footer_buttons(plain: bytearray, include_select: bool = True,
                          smooth_select: bool = True) -> None:
    pillow, _, _, _, _ = _flash_helpers()
    Image, _, _ = pillow()
    pairs = [
        ("buttons.Btn_start_xb_mc", "buttons.Btn_start_ps_mc"),
        ("buttons.Btn_start_xb_highLight_mc", "buttons.Btn_start_ps_highLight_mc"),
    ]
    if include_select:
        pairs.extend((
            ("buttons.Btn_select_xb_mc", "buttons.Btn_select_ps_mc"),
            ("buttons.Btn_select_xb_highLight_mc", "buttons.Btn_select_ps_highLight_mc"),
        ))
    for source_name, target_name in pairs:
        source_pos, source_size = _bitmap_for_symbol(plain, source_name)
        target_pos, target_size = _bitmap_for_symbol(plain, target_name)
        source = _bitmap_rgba(plain, source_pos, source_size)
        width, height = struct.unpack_from("<HH", plain, target_pos + 3)
        glyph_size = min(20, height - 2, source.width, source.height) if "Btn_select_" in target_name and smooth_select else min(height - 2, source.width, source.height)
        glyph = source.resize((glyph_size, glyph_size), Image.Resampling.LANCZOS)
        if "Btn_select_" in target_name:
            # The Select bitmap has a much smaller compressed Flash allocation.
            # Retain graded edges; the old binary mask looked visibly jagged.
            if smooth_select:
                color_levels, alpha_levels = (8, 8) if "highLight" in target_name else (16, 16)
                def levels(channel, count):
                    return channel.point(lambda value: round(value * (count - 1) / 255) * 255 // (count - 1))
                red, green, blue, alpha = glyph.split()
                glyph = Image.merge("RGBA", (levels(red, color_levels),
                                             levels(green, color_levels),
                                             levels(blue, color_levels),
                                             levels(alpha, alpha_levels)))
            else:
                alpha = glyph.getchannel("A").point(lambda value: 255 if value >= 96 else 0)
                light = glyph.getchannel("R").point(lambda value: 255 if value >= 128 else 0)
                glyph = Image.merge("RGBA", (light, light, light, alpha))
        canvas = Image.new("RGBA", (width, height))
        canvas.alpha_composite(glyph, (width - glyph_size, (height - glyph_size) // 2))
        _write_bitmap(plain, target_pos, target_size, canvas)


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


def legacy_xbox_art(original: bytes) -> bytes:
    """Return the earlier symbol-swap-only Xbox variant."""
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


def xbox_art(original: bytes, include_select: bool = True,
             smooth_select: bool = True) -> bytes:
    """Build Xbox mappings with footer glyphs that cannot cover their labels."""
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
        if "_ps_" not in name or "Btn_start_ps_" in name or (include_select and "Btn_select_ps_" in name):
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
    plain = bytearray(original[:8]) + data
    _patch_footer_buttons(plain, include_select, smooth_select)
    compressed = original[:8] + zlib.compress(plain[8:], 9)
    if len(compressed) > ART_SIZE:
        raise ValueError("Xbox button-art patch exceeds the UFC 4 allocation")
    result = compressed + b"\0" * (ART_SIZE - len(compressed))
    if zlib.decompress(result[8:]) != plain[8:]:
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
        original, previous_xbox = ps_file.read_bytes(), xbox_file.read_bytes()
        legacy_xbox = legacy_xbox_art(original)
        xbox = xbox_art(original)
        prior_xbox = xbox_art(original, include_select=False)
        prior_binary_select = xbox_art(original, smooth_select=False)
        if (hashlib.sha256(original).hexdigest() != ORIGINAL_SHA256 or
                previous_xbox not in (legacy_xbox, prior_xbox, prior_binary_select, xbox)):
            raise ValueError("Prepared button artwork failed validation")
        previous_keyboard = keyboard_file.read_bytes() if keyboard_file.exists() else None
        keyboard = keyboard_art(original, bindings_path)
        if current not in (original, legacy_xbox, prior_xbox, prior_binary_select, previous_xbox, xbox, keyboard, previous_keyboard):
            raise ValueError("Archive button art changed; refusing to overwrite it")
        output.mkdir(parents=True, exist_ok=True)
        xbox_file.write_bytes(xbox)
        keyboard_file.write_bytes(keyboard)
        desired = None
        if current == previous_xbox and current != xbox:
            desired = xbox
        elif previous_keyboard is not None and current == previous_keyboard and current != keyboard:
            desired = keyboard
        if desired is not None:
            with archive.open("r+b") as stream:
                stream.seek(ART_OFFSET)
                stream.write(desired)
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
