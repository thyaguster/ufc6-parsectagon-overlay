"""BC7 encoders for UFC 4's tiled UI button textures."""
from __future__ import annotations

import struct

WEIGHTS = (0, 4, 9, 13, 17, 21, 26, 30, 34, 38, 43, 47, 51, 55, 60, 64)
WEIGHTS_2 = (0, 21, 43, 64)


def dimensions(gnf: bytes) -> tuple[int, int]:
    if gnf[:4] != b"GNF " or len(gnf) < 256:
        raise ValueError("Invalid GNF texture")
    if (struct.unpack_from("<I", gnf, 20)[0] >> 20) & 63 != 41:
        raise ValueError("Expected BC7 GNF texture")
    word = struct.unpack_from("<I", gnf, 24)[0]
    return (word & 0x3fff) + 1, ((word >> 14) & 0x3fff) + 1


def tile_offset(x: int, y: int, pitch: int) -> int:
    """PS4 8x8 block microtile, Morton within a tile."""
    low = sum((((x >> bit) & 1) << (2 * bit)) |
              (((y >> bit) & 1) << (2 * bit + 1)) for bit in range(3))
    return (((y // 8) * (pitch // 8) + x // 8) * 64 + low) * 16


def _bits(value: int, width: int, result: list[int]) -> None:
    result.extend((value >> bit) & 1 for bit in range(width))


def bc7_mode6(pixels: list[tuple[int, int, int, int]]) -> bytes:
    """Encode one 4x4 block. Quality is sufficient for small UI glyphs."""
    if len(pixels) != 16:
        raise ValueError("BC7 block needs 16 pixels")
    # Choose endpoints with the greatest RGBA separation; this preserves the
    # transparent edges of these compact controller glyphs.
    p, q = max(((a, b) for a in pixels for b in pixels),
               key=lambda pair: sum((pair[0][c] - pair[1][c]) ** 2 for c in range(4)))
    def quantize(color: tuple[int, ...]) -> tuple[list[int], int, list[int]]:
        choices = []
        for pbit in (0, 1):
            seven = [min(127, max(0, round((c - pbit) / 2))) for c in color]
            decoded = [(n << 1) | pbit for n in seven]
            error = sum((a - b) ** 2 for a, b in zip(color, decoded))
            choices.append((error, seven, pbit, decoded))
        _, seven, pbit, decoded = min(choices)
        return seven, pbit, decoded
    ep0, pbit0, a = quantize(p)
    ep1, pbit1, b = quantize(q)
    def indices(first: list[int], second: list[int]) -> list[int]:
        return [min(range(16), key=lambda i: sum(
            (pix[c] - ((64-WEIGHTS[i])*first[c]+WEIGHTS[i]*second[c]+32)//64) ** 2
            for c in range(4))) for pix in pixels]
    idx = indices(a, b)
    if idx[0] >= 8:
        ep0, ep1 = ep1, ep0
        pbit0, pbit1 = pbit1, pbit0
        idx = indices(b, a)
    bits: list[int] = []
    _bits(1 << 6, 7, bits)  # BC7 mode 6 prefix, LSB first.
    for channel in range(4):
        _bits(ep0[channel], 7, bits)
        _bits(ep1[channel], 7, bits)
    _bits(pbit0, 1, bits)
    _bits(pbit1, 1, bits)
    _bits(idx[0], 3, bits)  # Anchor texel has one fewer index bit.
    for index in idx[1:]:
        _bits(index, 4, bits)
    if len(bits) != 128:
        raise AssertionError("BC7 mode 6 block length changed")
    return sum(bit << i for i, bit in enumerate(bits)).to_bytes(16, "little")


def bc7_mode5(pixels: list[tuple[int, int, int, int]]) -> bytes:
    """Encode one block with independent RGB and alpha indices.

    The separate alpha indices keep small opaque letters and antialiased
    outlines from corrupting one another in UFC 4's inline prompts.
    """
    if len(pixels) != 16:
        raise ValueError("BC7 block needs 16 pixels")

    # Ignore invisible RGB when choosing color endpoints. It has no visual
    # meaning, but would otherwise pull the endpoints toward transparent black.
    visible = [p for p in pixels if p[3] >= 16] or [(255, 255, 255, 0)]
    dark = min(visible, key=lambda p: sum(p[:3]))
    light = max(visible, key=lambda p: sum(p[:3]))
    rgb0 = [min(127, max(0, round(c * 127 / 255))) for c in dark[:3]]
    rgb1 = [min(127, max(0, round(c * 127 / 255))) for c in light[:3]]
    decoded0 = [(c << 1) | (c >> 6) for c in rgb0]
    decoded1 = [(c << 1) | (c >> 6) for c in rgb1]
    alpha0, alpha1 = min(p[3] for p in pixels), max(p[3] for p in pixels)

    def color_indices(first: list[int], second: list[int]) -> list[int]:
        return [min(range(4), key=lambda i: sum(
            (p[c] - ((64-WEIGHTS_2[i])*first[c] + WEIGHTS_2[i]*second[c] + 32)//64) ** 2
            for c in range(3))) if p[3] >= 16 else 0 for p in pixels]

    def alpha_indices(first: int, second: int) -> list[int]:
        return [min(range(4), key=lambda i: abs(
            p[3] - ((64-WEIGHTS_2[i])*first + WEIGHTS_2[i]*second + 32)//64))
            for p in pixels]

    color_idx = color_indices(decoded0, decoded1)
    if color_idx[0] >= 2:
        rgb0, rgb1 = rgb1, rgb0
        color_idx = color_indices(decoded1, decoded0)
    alpha_idx = alpha_indices(alpha0, alpha1)
    if alpha_idx[0] >= 2:
        alpha0, alpha1 = alpha1, alpha0
        alpha_idx = alpha_indices(alpha0, alpha1)

    bits: list[int] = []
    _bits(1 << 5, 6, bits)  # BC7 mode 5, rotation 0 (RGB and alpha unchanged).
    _bits(0, 2, bits)
    for channel in range(3):
        _bits(rgb0[channel], 7, bits)
        _bits(rgb1[channel], 7, bits)
    _bits(alpha0, 8, bits)
    _bits(alpha1, 8, bits)
    _bits(color_idx[0], 1, bits)
    for index in color_idx[1:]:
        _bits(index, 2, bits)
    _bits(alpha_idx[0], 1, bits)
    for index in alpha_idx[1:]:
        _bits(index, 2, bits)
    if len(bits) != 128:
        raise AssertionError("BC7 mode 5 block length changed")
    return sum(bit << i for i, bit in enumerate(bits)).to_bytes(16, "little")


def replace_image(original: bytes, rgba: bytes, *, separate_alpha: bool = False) -> bytes:
    width, height = dimensions(original)
    if len(rgba) != width * height * 4:
        raise ValueError("Replacement artwork dimensions differ")
    blocks_w, blocks_h = (width+3)//4, (height+3)//4
    pitch = 8 if len(original) - 256 <= 2048 else 16
    body = bytearray(original[256:])
    transparent = bc7_mode6([(0, 0, 0, 0)] * 16)
    body[:] = transparent * (len(body) // 16)
    for y in range(blocks_h):
        for x in range(blocks_w):
            block = []
            for dy in range(4):
                for dx in range(4):
                    px, py = x*4+dx, y*4+dy
                    if px >= width or py >= height:
                        block.append((0, 0, 0, 0))
                    else:
                        at = (py*width+px)*4
                        block.append(tuple(rgba[at:at+4]))
            offset = tile_offset(x, y, pitch)
            if offset + 16 > len(body):
                raise ValueError("GNF texture allocation is too small")
            body[offset:offset+16] = bc7_mode5(block) if separate_alpha else bc7_mode6(block)
    return original[:256] + bytes(body)
