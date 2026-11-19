"""Flash parsing used by the UFC 4 keycap patch (adapted from the UFC 6 patcher)."""
import struct
import zlib


class PatchError(Exception):
    pass


def check(condition, message):
    if not condition:
        raise PatchError(message)


def unframe(data):
    plain = bytearray()
    frames = []
    pos = 0
    while pos < len(data):
        check(pos + 8 <= len(data), 'Invalid CAS framing.')
        size = ((data[pos + 5] & 15) << 16) | int.from_bytes(data[pos + 6:pos + 8], 'big')
        expected = int.from_bytes(data[pos:pos + 4], 'big') & 0xffffff
        check(data[pos + 4] == 0 and size == expected and pos + 8 + size <= len(data),
              'Unsupported CAS compression in UI library.')
        frames.append((pos + 8, len(plain), size))
        plain.extend(data[pos + 8:pos + 8 + size])
        pos += size + 8
    return bytes(plain), frames


def decode_swf(data):
    check(len(data) >= 12 and data[:3] in (b'CWS', b'FWS'), 'Unsupported UI library format.')
    length = struct.unpack_from('<I', data, 4)[0]
    check(12 <= length <= 8 * 1024 * 1024, 'Invalid UI library size.')
    if data[:3] == b'CWS':
        decoder = zlib.decompressobj()
        body = decoder.decompress(data[8:], length - 7)
        check(decoder.eof and len(body) == length - 8, 'Invalid compressed UI library.')
        plain = data[:8] + body
    else:
        plain = data
    check(len(plain) == length, 'UI library length mismatch.')
    return plain


def swf_tags(data):
    pos = 8 + (5 + 4 * (data[8] >> 3) + 7) // 8 + 4
    while pos + 2 <= len(data):
        word = struct.unpack_from('<H', data, pos)[0]
        pos += 2
        kind, size = word >> 6, word & 63
        if size == 63:
            check(pos + 4 <= len(data), 'Truncated Flash tag.')
            size = struct.unpack_from('<I', data, pos)[0]
            pos += 4
        check(pos + size <= len(data), 'Invalid Flash tag size.')
        yield kind, pos, size
        pos += size
        if kind == 0:
            break


def swf_symbols(data):
    symbols = []
    for kind, pos, size in swf_tags(data):
        if kind != 76:
            continue
        end = pos + size
        check(pos + 2 <= end, 'Invalid symbol table.')
        count = struct.unpack_from('<H', data, pos)[0]
        pos += 2
        for _ in range(count):
            check(pos + 2 <= end, 'Truncated symbol table.')
            character = struct.unpack_from('<H', data, pos)[0]
            offset = pos
            pos += 2
            stop = data.find(b'\0', pos, end)
            check(stop >= pos, 'Invalid symbol name.')
            symbols.append((data[pos:stop].decode('utf-8'), character, offset))
            pos = stop + 1
        check(pos == end, 'Unexpected symbol table bytes.')
    return symbols
