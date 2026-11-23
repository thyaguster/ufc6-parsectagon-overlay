"""Reversible UFC 4 fight-HUD and tutorial icon textures."""
from __future__ import annotations

import csv
import hashlib
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).with_name("ufc4_keyboard")))

from ufc4_gnf_codec import dimensions, replace_image
from ufc4_keyboard.keyboard_art import descriptor, render
from ufc4_keyboard.keyboard_bindings import load_bbport
from ufc4_stock_xbox import render as render_stock_xbox

PREFIX = "ufc4_direct_icons_"


def _rows(manifest: Path) -> dict[tuple[int, str], str]:
    result = {}
    with manifest.open(encoding="utf-8-sig", newline="") as stream:
        for row in csv.DictReader(stream):
            path = row["RelPath"]
            if "/skins/buttons/ps/" not in path or row["TakeFrom"] != "TakeSourceAsIs":
                continue
            name = path.split("/skins/buttons/ps/", 1)[1]
            if descriptor(name.removesuffix(".gnf")) is None or name.startswith("controller"):
                continue
            result[(int(row["InArchiveSize"]), row["Blake2b"].lower())] = name
    return result


def prepare(archive: Path, manifest: Path, output: Path, bindings_path: Path | None = None) -> int:
    """Build a validated original and two variants from the player's archive."""
    map_file = output / f"{PREFIX}map.json"
    if map_file.exists():
        metadata = json.loads(map_file.read_text(encoding="utf-8"))
        prior = {key: (output / f"{PREFIX}{key}.bin").read_bytes() for key in ("ps", "xbox", "keyboard")}
        # Accept the earlier full-size Xbox artwork during an upgrade.
        legacy_xbox = bytearray()
        legacy_cursor = 0
        for row in metadata:
            size = row["size"]
            original = prior["ps"][legacy_cursor:legacy_cursor+size]
            width, height = dimensions(original)
            rgba = render_stock_xbox(output / "ufc4_buttonart_ps.bin", width, height,
                                     row["name"], scale_factor=1.0)
            legacy_xbox += replace_image(original, rgba) if rgba else original
            legacy_cursor += size
        cursor = 0
        with archive.open("rb") as stream:
            for row in metadata:
                size = row["size"]
                stream.seek(row["offset"])
                current = stream.read(size)
                candidates = [blob[cursor:cursor+size] for blob in prior.values()]
                candidates.append(legacy_xbox[cursor:cursor+size])
                if current not in candidates:
                    raise ValueError(f"UFC 4 icon texture changed: {row['name']}")
                cursor += size
        if cursor != len(prior["ps"]):
            raise ValueError("Prepared UFC 4 icon texture inventory is incomplete")
    else:
        targets = _rows(manifest)
        sizes = {size for size, _ in targets}
        data = archive.read_bytes()
        positions = {}
        at = 0
        while True:
            at = data.find(b"GNF ", at)
            if at < 0:
                break
            for size in sizes:
                if at+size > len(data):
                    continue
                digest = hashlib.blake2b(data[at:at+size], digest_size=32).hexdigest()
                name = targets.get((size, digest))
                if name:
                    positions[name] = (at, size)
            at += 4
        if len(positions) != len(targets) or len(targets) < 100:
            raise ValueError(f"Found {len(positions)} of {len(targets)} original UFC 4 icon textures")
        metadata = [{"name": name, "offset": offset, "size": size}
                    for name, (offset, size) in sorted(positions.items(), key=lambda item: item[1][0])]
        prior = None
    bindings = load_bbport(bindings_path)
    blobs = {"ps": bytearray(), "xbox": bytearray(), "keyboard": bytearray()}
    cursor = 0
    for row in metadata:
        name, offset, size = row["name"], row["offset"], row["size"]
        original = prior["ps"][cursor:cursor+size] if prior else data[offset:offset+size]
        width, height = dimensions(original)
        desc = descriptor(name.removesuffix(".gnf"))
        keyboard = replace_image(original, render(width, height, desc, bindings))
        # The original ground stick indicators read better than the Xbox
        # replacement in the fight HUD.
        ground_stick = name in ("text_ground_game_hud/las.gnf", "text_ground_game_hud/ras.gnf")
        xbox_rgba = None if ground_stick else render_stock_xbox(output / "ufc4_buttonart_ps.bin", width, height, name)
        clean_inline = (name.startswith("white/") and
                        name.rsplit("/", 1)[-1] in {"left_trigger.gnf", "right_trigger.gnf",
                                                       "left_bumper.gnf", "right_bumper.gnf"})
        xbox = replace_image(original, xbox_rgba, separate_alpha=clean_inline) if xbox_rgba else original
        for style, variant in (("ps", original), ("xbox", xbox), ("keyboard", keyboard)):
            if len(variant) != size:
                raise ValueError(f"Size mismatch in {name}")
            blobs[style] += variant
        cursor += size
    # Migrate whichever prepared variant is currently installed before the
    # previous asset snapshot is replaced (including older drawn Xbox art).
    if prior:
        cursor = 0
        with archive.open("r+b") as stream:
            for row in metadata:
                size = row["size"]
                stream.seek(row["offset"])
                current = stream.read(size)
                for key in blobs:
                    if current == prior[key][cursor:cursor+size] or (key == "xbox" and current == legacy_xbox[cursor:cursor+size]):
                        stream.seek(row["offset"])
                        stream.write(blobs[key][cursor:cursor+size])
                        break
                cursor += size
            stream.flush()
    output.mkdir(parents=True, exist_ok=True)
    for style, blob in blobs.items():
        (output / f"{PREFIX}{style}.bin").write_bytes(blob)
    (output / f"{PREFIX}map.json").write_text(json.dumps(metadata, indent=2), encoding="utf-8")
    return len(metadata)


def set_style(archive: Path, output: Path, style: str) -> None:
    metadata = json.loads((output / f"{PREFIX}map.json").read_text(encoding="utf-8"))
    blobs = {key: (output / f"{PREFIX}{key}.bin").read_bytes() for key in ("ps", "xbox", "keyboard")}
    total = sum(row["size"] for row in metadata)
    if any(len(blob) != total for blob in blobs.values()):
        raise ValueError("Prepared direct icon assets are incomplete")
    patches = []
    cursor = 0
    with archive.open("r+b") as stream:
        for row in metadata:
            size = row["size"]
            stream.seek(row["offset"])
            current = stream.read(size)
            if current not in tuple(blob[cursor:cursor+size] for blob in blobs.values()):
                raise ValueError(f"UFC 4 icon texture changed: {row['name']}")
            desired = blobs[style][cursor:cursor+size]
            if desired != current:
                patches.append((row["offset"], current, desired))
            cursor += size
        try:
            for offset, _, desired in patches:
                stream.seek(offset)
                stream.write(desired)
            stream.flush()
            for offset, _, desired in patches:
                stream.seek(offset)
                if stream.read(len(desired)) != desired:
                    raise IOError("Direct icon verification failed")
        except Exception:
            for offset, original, _ in patches:
                stream.seek(offset)
                stream.write(original)
            stream.flush()
            raise
