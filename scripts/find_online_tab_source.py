"""Find plain-text Home Page script records in the local game archives, read-only."""
from pathlib import Path
import mmap

root = Path(r"E:\Jogos\UFC 6 v01.006\PPSA23566-app0-files\Data\Ps5\superbundlelayout\ufc_installpackage_00")
needles = (b"act_mega_tab_right", b"evt_goto_competition_subscreen", b"TXT_ONLINE", b"function HomePage", b"function UFCHomePage")
for name in ("cas_03.cas", "cas_04.cas", "cas_05.cas"):
    with (root / name).open("rb") as file, mmap.mmap(file.fileno(), 0, access=mmap.ACCESS_READ) as data:
        for needle in needles:
            cursor = 0
            hits = []
            while (at := data.find(needle, cursor)) >= 0:
                cursor = at + len(needle)
                left = data.rfind(b"\0", max(0, at - 300000), at) + 1
                right = data.find(b"\0", at, min(len(data), at + 300000))
                if right < 0 or right - left > 300000:
                    continue
                block = data[left:right]
                if block.count(b"\n") < 3 or sum(c in (9, 10, 13) or 32 <= c < 127 for c in block) / len(block) < .95:
                    continue
                hits.append((at, left, right, block))
            print(name, needle.decode(), len(hits))
            for at, left, right, block in hits[:12]:
                text = block.decode("utf-8", "replace")
                print(" ", hex(at), "record", hex(left), "bytes", len(block), "head", ascii(text[:110]))
