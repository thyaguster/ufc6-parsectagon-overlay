"""Reuse UFC 4's authored Xbox Flash bitmaps in its separate GNF prompts."""
from __future__ import annotations

from functools import lru_cache
from pathlib import Path
import struct
import sys
import zlib

sys.path.insert(0, str(Path(__file__).with_name('ufc4_keyboard')))
from ufc4_keyboard.patcher_core import decode_swf, swf_symbols, swf_tags
from ufc4_keyboard.keyboard_flash import bitmap_for_shape, records
from ufc4_keyboard.keyboard_art import pillow


@lru_cache(maxsize=4)
def stock_images(source: Path) -> dict[str, object]:
    Image, _, _ = pillow()
    data = decode_swf(source.read_bytes())
    definitions = {struct.unpack_from('<H', data, pos)[0]: (pos, size)
                   for kind, pos, size in swf_tags(data)
                   if kind in (2, 22, 32, 39, 20, 36)}
    result = {}
    for name, cid, _ in swf_symbols(data):
        if not name.startswith('buttons.Btn_') or '_xb_' not in name:
            continue
        stem = name.removeprefix('buttons.Btn_').split('_xb_', 1)[0]
        if stem.startswith('ue_'):
            stem = stem[3:]
        if stem not in {'digital_one', 'digital_two', 'digital_three', 'digital_four',
                        'left_bumper', 'right_bumper', 'left_trigger', 'right_trigger',
                        'left_right_bumper', 'left_right_trigger', 'start', 'select', 'guide',
                        'las_c', 'ras_c'}:
            continue
        state = 'hold' if 'highLight' in name else 'normal'
        pos, size = definitions[cid]
        children = [struct.unpack_from('<H', data, at+3)[0]
                    for kind, at, _, _ in records(data, pos+4, pos+size)
                    if kind == 26]
        if len(children) != 1:
            raise ValueError(f'Unexpected UFC 4 Xbox sprite: {name}')
        shape_pos, shape_size = definitions[children[0]]
        bitmap = bitmap_for_shape(data, shape_pos, shape_size)
        at, length = definitions[bitmap]
        if data[at+2] != 5:
            raise ValueError(f'Unexpected UFC 4 Xbox bitmap format: {name}')
        width, height = struct.unpack_from('<HH', data, at+3)
        argb = zlib.decompress(data[at+7:at+length])
        if len(argb) != width*height*4:
            raise ValueError(f'Unexpected UFC 4 Xbox bitmap size: {name}')
        rgba = bytearray(len(argb))
        for i in range(0, len(argb), 4):
            alpha, red, green, blue = argb[i:i+4]
            rgba[i:i+4] = (min(255, round(red*255/alpha)) if alpha else 0,
                          min(255, round(green*255/alpha)) if alpha else 0,
                          min(255, round(blue*255/alpha)) if alpha else 0, alpha)
        result[f'{stem}:{state}'] = Image.frombytes('RGBA', (width, height), bytes(rgba))
    return result


def render(source: Path, width: int, height: int, name: str, scale_factor: float = 0.72) -> bytes | None:
    stem = name.rsplit('/', 1)[-1].removesuffix('.gnf').removeprefix('ue_')
    state = 'hold' if stem.endswith('_hold') else 'normal'
    stem = stem.removesuffix('_hold')
    if stem in ('las', 'ras'):
        stem += '_c'
    images = stock_images(source)
    stock = images.get(f'{stem}:{state}') or images.get(f'{stem}:normal')
    if stock is None:
        return None
    Image, ImageDraw, _ = pillow()
    # UFC 4 draws these textures at their full slot size in tutorial text.
    # Leave breathing room around the authored Xbox glyphs.
    scale = scale_factor * min(width/stock.width, height/stock.height)
    size = (max(1, round(stock.width*scale)), max(1, round(stock.height*scale)))
    fitted = stock.resize(size, Image.Resampling.LANCZOS)
    canvas = Image.new('RGBA', (width, height))
    canvas.alpha_composite(fitted, ((width-size[0])//2, (height-size[1])//2))
    result = bytearray(canvas.tobytes())
    if name.startswith('white/') and stem in {'left_trigger', 'right_trigger',
                                                'left_bumper', 'right_bumper'}:
        # These four stock bitmaps have torn, irregular outer edges that become
        # conspicuous after downscaling into the inline tutorial slots. Keep
        # their lettering, but give the white button body a clean contour.
        mask = Image.new('L', (width*4, height*4))
        draw = ImageDraw.Draw(mask)
        if stem.endswith('trigger'):
            points = [(5, 5), (29, 5), (32, 8), (32, 24), (27, 30), (5, 30)]
            base_width, base_height = 36, 36
        else:
            points = [(5, 13), (10, 6), (31, 6), (34, 10), (34, 26), (5, 26)]
            base_width, base_height = 40, 32
        if stem.startswith('right_'):
            points = [(base_width-x, y) for x, y in points]
        draw.polygon([(round(x*4*width/base_width), round(y*4*height/base_height))
                      for x, y in points], fill=255)
        alpha = mask.resize((width, height), Image.Resampling.LANCZOS).tobytes()
        for at in range(0, len(result), 4):
            opacity = alpha[at//4]
            opacity = 0 if opacity < 80 else opacity
            if opacity and (result[at+3] < 200 or result[at:at+3] == b'\0\0\0'):
                result[at:at+3] = b'\xff\xff\xff'
            result[at+3] = opacity
    if stem in {'digital_one', 'digital_two', 'digital_three', 'digital_four'}:
        # BC7 mode 6 shares one interpolation index across RGB and alpha.
        # Transparent black next to the white face-button edge produces bright
        # stray pixels outside the circle. Bleed white into fully transparent
        # texels; their alpha remains zero, so only compression quality changes.
        for at in range(0, len(result), 4):
            if result[at+3] == 0:
                result[at:at+3] = b'\xff\xff\xff'
    return bytes(result)
