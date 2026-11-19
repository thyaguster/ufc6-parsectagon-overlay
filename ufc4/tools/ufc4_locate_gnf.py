"""Locate uncompressed UFC 4 button textures by their archive manifest hashes."""
import csv
import hashlib
import pathlib
import sys

manifest = pathlib.Path(sys.argv[1])
archive = pathlib.Path(sys.argv[2])
rows = [r for r in csv.DictReader(manifest.open(encoding="utf-8-sig", errors="replace"))
        if "/skins/buttons/ps/" in r["RelPath"] and r["TakeFrom"] == "TakeSourceAsIs"]
targets = {(int(r["InArchiveSize"]), r["Blake2b"].lower()): r["RelPath"] for r in rows}
sizes = {key[0] for key in targets}
data = archive.read_bytes()
found = {}
pos = 0
while True:
    pos = data.find(b"GNF ", pos)
    if pos < 0:
        break
    for size in sizes:
        if pos + size > len(data):
            continue
        key = (size, hashlib.blake2b(data[pos:pos + size], digest_size=32).hexdigest())
        if key in targets:
            found[targets[key]] = (pos, size)
    pos += 4
for path, (offset, size) in sorted(found.items()):
    print(f"{offset:#x},{size},{path}")
print(f"FOUND {len(found)}/{len(rows)}", file=sys.stderr)
