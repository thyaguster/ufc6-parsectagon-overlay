"""Decode a UFC 4 BC3 icon allocation for visual inspection."""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "src" / "ufc4_keyboard"))
from keyboard_art import _swizzle
from PIL import Image


def block(raw):
    a0, a1 = raw[:2]
    a = [a0, a1]
    if a0 > a1:
        a.extend(((7 - i) * a0 + i * a1) // 7 for i in range(1, 7))
    else:
        a.extend(((5 - i) * a0 + i * a1) // 5 for i in range(1, 5))
        a.extend([0, 255])
    ai = int.from_bytes(raw[2:8], "little")
    c0, c1 = (int.from_bytes(raw[n:n + 2], "little") for n in (8, 10))
    def rgb(v):
        return ((v >> 11) * 255 // 31, ((v >> 5) & 63) * 255 // 63, (v & 31) * 255 // 31)
    p, q = rgb(c0), rgb(c1)
    if c0 > c1:
        cs = [p, q, tuple((2 * x + y) // 3 for x, y in zip(p, q)),
              tuple((x + 2 * y) // 3 for x, y in zip(p, q))]
    else:
        cs = [p, q, tuple((x + y) // 2 for x, y in zip(p, q)), (0, 0, 0)]
    ci = int.from_bytes(raw[12:16], "little")
    return [(*cs[(ci >> (2 * i)) & 3], a[(ai >> (3 * i)) & 7]) for i in range(16)]


data = pathlib.Path(sys.argv[1]).read_bytes()
width, height = int(sys.argv[2]), int(sys.argv[3])
body = data[256:]
out = Image.new("RGBA", (width, height))
mode = sys.argv[5] if len(sys.argv) > 5 else "ps5"
if mode == "linear":
    locations = [(x, y, (y * (width // 4) + x) * 16)
                 for y in range(height // 4) for x in range(width // 4)]
elif mode == "morton":
    locations = [(x, y, sum(((x >> i) & 1) << (2*i) | ((y >> i) & 1) << (2*i+1)
                            for i in range(4)) * 16)
                 for y in range(height // 4) for x in range(width // 4)]
elif mode in ("tile8", "tile8b", "tile8c"):
    bw = 8 if len(body) <= 2048 else 16
    locations = []
    for y in range(height // 4):
        for x in range(width // 4):
            tx, ty = x // 8, y // 8
            xx, yy = x % 8, y % 8
            if mode == "tile8":
                intra = sum(((xx >> i) & 1) << (2*i) | ((yy >> i) & 1) << (2*i+1) for i in range(3))
            elif mode == "tile8b":
                intra = sum(((yy >> i) & 1) << (2*i) | ((xx >> i) & 1) << (2*i+1) for i in range(3))
            else:
                intra = yy * 8 + xx
            locations.append((x, y, ((ty * (bw // 8) + tx) * 64 + intra) * 16))
else:
    locations = _swizzle(width, height, len(body))
for x, y, offset in locations:
    pixels = block(body[offset:offset + 16])
    for dy in range(4):
        for dx in range(4):
            out.putpixel((x * 4 + dx, y * 4 + dy), pixels[dy * 4 + dx])
out.resize((width * 6, height * 6)).save(sys.argv[4])
