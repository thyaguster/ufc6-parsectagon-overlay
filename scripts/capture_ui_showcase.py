"""Capture clean PNG screenshots of all overlay pages, UI types, and color themes."""
import ctypes as c
import ctypes.wintypes as w
import sys
import time
from pathlib import Path
from PIL import Image

root = Path(__file__).resolve().parent.parent
ci = root / ".build" / "controller-ci"
out = root / "docs" / "screenshots" / "showcase"
out.mkdir(parents=True, exist_ok=True)

dll = c.CDLL(str(ci / "quit_menu_test.dll"))
u32 = c.windll.user32

@c.WINFUNCTYPE(w.LPARAM, w.HWND, w.UINT, w.WPARAM, w.LPARAM)
def proc(hwnd, msg, wp, lp):
    return u32.DefWindowProcW(hwnd, msg, wp, lp)

wc = (c.c_uint64 * 10)()
app = c.windll.kernel32.GetModuleHandleW(None)
class WNDCLASSW(c.Structure):
    _fields_ = [("style", c.c_uint), ("lpfnWndProc", c.c_void_p), ("cbClsExtra", c.c_int), ("cbWndExtra", c.c_int),
                ("hInstance", w.HINSTANCE), ("hIcon", w.HICON), ("hCursor", w.HICON), ("hbrBackground", w.HBRUSH),
                ("lpszMenuName", w.LPCWSTR), ("lpszClassName", w.LPCWSTR)]

cls = WNDCLASSW(0, c.cast(proc, c.c_void_p), 0, 0, app, None, None, None, None, "Ufc6ShowcaseHost")
u32.RegisterClassW(c.byref(cls))
hwnd = u32.CreateWindowExW(0, "Ufc6ShowcaseHost", "Showcase", 0x00CF0000, 100, 100, 1280, 720, None, None, app, None)

def pump(sec=0.08):
    end = time.time() + sec
    msg = w.MSG()
    while time.time() < end:
        while u32.PeekMessageW(c.byref(msg), None, 0, 0, 1):
            u32.TranslateMessage(c.byref(msg))
            u32.DispatchMessageW(c.byref(msg))
        time.sleep(0.01)

def press(btn, wait=0.06):
    dll.MenuTestButton(btn, 1)
    dll.MenuTestButton(btn, 0)
    pump(wait)

def render(name):
    cw, ch = c.c_uint(0), c.c_uint(0)
    dll.Ufc6MenuExtent(1920, 1080, c.byref(cw), c.byref(ch))
    buf = (c.c_ubyte * (cw.value * ch.value * 4))()
    assert dll.Ufc6RenderMenu(buf, len(buf)), f"Render failed for {name}"
    img = Image.frombytes("RGBA", (cw.value, ch.value), bytes(buf), "raw", "BGRA")
    img.save(out / name)
    print(f"Saved {name} ({cw.value}x{ch.value})")

PAD_A, PAD_B, PAD_LB, PAD_RB = 0x4000, 0x2000, 0x400, 0x800
PAD_UP, PAD_RIGHT, PAD_DOWN, PAD_LEFT = 0x10, 0x20, 0x40, 0x80

dll.MenuTestStart(hwnd); pump(0.35)
dll.Ufc6VulkanActive()
dll.MenuTestLayoutStyle(1)
dll.MenuTestToggle(); pump(0.25)

# 1. Capture Topbar main tabs (Actions, Cheats, Photo, Settings)
render("tab-actions.png")
press(PAD_RB); render("tab-cheats.png")
press(PAD_RB); render("tab-photo.png")
for _ in range(4): press(PAD_DOWN)
press(PAD_RIGHT); render("tab-photo-fpv.png"); press(PAD_LEFT)
for _ in range(4): press(PAD_UP)
press(PAD_RB); render("tab-settings.png")

# 2. Cycle all Master Presets (now on Settings tab, row 0 = Master preset)
preset_shots = [
    (1, "ui-topbar-apex.png"),
    (2, "ui-island-pill.png"),
    (3, "ui-topbar-solid.png"),
    (4, "ui-island-glass.png"),
    (6, "ui-sideblade.png"),
    (7, "ui-bottomdock.png"),
    (8, "ui-bento.png"),
    (9, "ui-broadcast.png"),
    (10, "ui-octagon.png"),
]
# First press Right cycles from current (Apex=1) through all 9
for idx in range(9):
    if idx == 0:
        render("ui-topbar-apex.png")
    else:
        press(PAD_RIGHT)
        names = [
            "ui-topbar-apex.png",
            "ui-island-pill.png",
            "ui-topbar-solid.png",
            "ui-island-glass.png",
            "ui-sideblade.png",
            "ui-bottomdock.png",
            "ui-bento.png",
            "ui-broadcast.png",
            "ui-octagon.png",
        ]
        render(names[idx])

# Return to Parsectagon Apex (one more Right)
press(PAD_RIGHT)

# 3. Capture subpages (Appearance, Controls, Game/Files, Performance)
press(PAD_DOWN); press(PAD_A); render("page-appearance.png")
# Open Theme page from Appearance (row 3 = Theme)
for _ in range(3): press(PAD_DOWN)
press(PAD_A); render("page-theme-parsectagon.png")
# Switch to Blue (row 2) and Green (row 1) to preview themes
press(PAD_UP); press(PAD_UP); press(PAD_UP); press(PAD_UP) # row 2 = Blue
while (dll.MenuTestSnapshot() & 15) != 2: press(PAD_DOWN)
press(PAD_A); render("page-theme-blue.png")
press(PAD_UP); press(PAD_A); render("page-theme-green.png")
# Restore Parsectagon (row 6)
while (dll.MenuTestSnapshot() & 15) != 6: press(PAD_DOWN)
press(PAD_A)
press(PAD_B) # back to Appearance
press(PAD_B) # back to Settings Home
render("page-settings-home.png")

for row, fname in ((1, "page-controls.png"), (2, "page-game-launcher.png"), (3, "page-files-support.png"), (4, "page-performance.png")):
    while (dll.MenuTestSnapshot() & 15) != row: press(PAD_DOWN)
    press(PAD_A); render(fname); press(PAD_B)

# Capture Photo Effects & Watermark pages via Photo tab
press(PAD_B) # back to Topbar Settings
press(PAD_LB) # Photo tab
press(PAD_DOWN); press(PAD_DOWN); press(PAD_A); render("page-photo-effects.png"); press(PAD_B)
press(PAD_DOWN); press(PAD_DOWN); press(PAD_DOWN); press(PAD_A); render("page-photo-watermark.png"); press(PAD_B)

print("ALL SHOWCASE SCREENSHOTS CAPTURED!")
