"""Read-only UFC 4 UI archive inspection for prompt resources."""
from pathlib import Path
import sys
import hashlib

archive = Path(sys.argv[1]).read_bytes()
terms = [
    b'"name": "GroundGameContainer"',
    b'"name": "GroundGameRadialMeter"',
    b'"name": "GroundGameHUD"',
    b'mSkipButtonAssetPath',
    b'"name": "SkipCalloutOverlay"',
]
for term in terms:
    pos = archive.find(term, 0x100000)
    print(term.decode(), hex(pos))
    if pos >= 0:
        passage = archive[max(0, pos - 150):pos + 1700].decode('utf-8', 'backslashreplace')
        print(passage.encode('ascii', 'backslashreplace').decode('ascii'))

if '--find-skip-texture' in sys.argv:
    target = bytes.fromhex('88789143E9CB555882F53BE12216019E844415F1495B6903112FA698E8CDBEC3')
    pos = 0
    found = []
    while True:
        pos = archive.find(b'GNF', pos + 1)
        if pos < 0:
            break
        for start in range(max(0, pos - 16), pos + 1):
            if hashlib.blake2b(archive[start:start + 1280], digest_size=32).digest() == target:
                found.append(start)
    print('skip texture offsets:', [hex(x) for x in found])
