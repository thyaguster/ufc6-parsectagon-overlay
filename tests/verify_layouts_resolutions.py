"""Verify modern settings, confirmation dialogs, UI layouts, controller focus, and toast notifications across 720p, 1080p, and 1440p."""
import ctypes as c
import ctypes.wintypes as w
import time
from pathlib import Path
from PIL import Image

root = Path(__file__).resolve().parent.parent
ci = root / ".build" / "controller-ci"
dll = c.CDLL(str(ci / "quit_menu_test.dll"))
u32 = c.windll.user32

@c.WINFUNCTYPE(w.LPARAM, w.HWND, w.UINT, w.WPARAM, w.LPARAM)
def proc(hwnd, msg, wp, lp):
    return u32.DefWindowProcW(hwnd, msg, wp, lp)

class WNDCLASSW(c.Structure):
    _fields_ = [("style", c.c_uint), ("lpfnWndProc", c.c_void_p), ("cbClsExtra", c.c_int), ("cbWndExtra", c.c_int),
                ("hInstance", w.HINSTANCE), ("hIcon", w.HICON), ("hCursor", w.HICON), ("hbrBackground", w.HBRUSH),
                ("lpszMenuName", w.LPCWSTR), ("lpszClassName", w.LPCWSTR)]

app = c.windll.kernel32.GetModuleHandleW(None)
cls = WNDCLASSW(0, c.cast(proc, c.c_void_p), 0, 0, app, None, None, None, None, "Ufc6ResVerifyHost")
u32.RegisterClassW(c.byref(cls))
hwnd = u32.CreateWindowExW(0, "Ufc6ResVerifyHost", "Verify", 0x00CF0000, 100, 100, 1280, 720, None, None, app, None)

def pump(sec=0.05):
    end = time.time() + sec
    msg = w.MSG()
    while time.time() < end:
        while u32.PeekMessageW(c.byref(msg), None, 0, 0, 1):
            u32.TranslateMessage(c.byref(msg))
            u32.DispatchMessageW(c.byref(msg))
        time.sleep(0.008)

def press(btn, wait=0.05):
    dll.MenuTestButton(btn, 1)
    dll.MenuTestButton(btn, 0)
    pump(wait)

dll.MenuTestStatus.argtypes = [w.LPCWSTR]
dll.MenuTestPage.restype = c.c_int
dll.MenuTestSnapshot.restype = c.c_int
dll.MenuTestTextClips.argtypes = [c.c_int]
dll.MenuTestTextClips.restype = c.c_int

RESOLUTIONS = [(1280, 720), (1920, 1080), (2560, 1440)]

def check_render(label, check_pos=None):
    for vw, vh in RESOLUTIONS:
        cw, ch, ox, oy = c.c_uint(0), c.c_uint(0), c.c_uint(0), c.c_uint(0)
        dll.Ufc6MenuExtent(vw, vh, c.byref(cw), c.byref(ch))
        dll.Ufc6OverlayPosition(vw, vh, c.byref(ox), c.byref(oy))
        assert 0 < cw.value <= vw, f"{label} width {cw.value} out of {vw}"
        assert 0 < ch.value <= vh, f"{label} height {ch.value} out of {vh}"
        assert ox.value + cw.value <= vw, f"{label} x+w overflows {vw}: {ox.value}+{cw.value}"
        assert oy.value + ch.value <= vh, f"{label} y+h overflows {vh}: {oy.value}+{ch.value}"
        if check_pos == "center":
            assert abs((ox.value + cw.value / 2) - vw / 2) <= 2, f"{label} not centered X at {vw}x{vh}"
            assert abs((oy.value + ch.value / 2) - vh / 2) <= 2, f"{label} not centered Y at {vw}x{vh}"
        elif check_pos == "top":
            assert oy.value == 16 and abs((ox.value + cw.value / 2) - vw / 2) <= 2, f"{label} not top-centered at {vw}x{vh}"
        elif check_pos == "left":
            assert ox.value == 24, f"{label} not left-docked at {vw}x{vh}"
        elif check_pos == "bottom":
            assert oy.value + ch.value <= vh - 20 and abs((ox.value + cw.value / 2) - vw / 2) <= 2, f"{label} not bottom-centered at {vw}x{vh}"
        buf = (c.c_ubyte * (cw.value * ch.value * 4))()
        dll.MenuTestTextClips(1)
        assert dll.Ufc6RenderMenu(buf, len(buf)), f"Render failed: {label} @ {vw}x{vh}"
        clips = dll.MenuTestTextClips(0)
        assert clips == 0, f"Text clipped ({clips}) on {label} @ {vw}x{vh}"

PAD_A, PAD_B, PAD_LB, PAD_RB = 0x4000, 0x2000, 0x400, 0x800
PAD_UP, PAD_RIGHT, PAD_DOWN, PAD_LEFT = 0x10, 0x20, 0x40, 0x80

dll.MenuTestStart(hwnd); pump(0.35)
dll.Ufc6VulkanActive()
dll.MenuTestLayoutStyle(1)
dll.MenuTestToggle(); pump(0.25)

# 1. All 7 modern layouts + positioning
layout_checks = [
    (1, "topbar", "top"),
    (2, "island", "top"),
    (3, "sideblade", "left"),
    (4, "bottomdock", "bottom"),
    (5, "bento", "center"),
    (6, "broadcast", "left"),
    (7, "octagon", "center"),
]
for style, name, pos in layout_checks:
    dll.MenuTestLayoutStyle(style)
    check_render(f"layout:{name}", pos)

# 2. Modern settings pages & focus scrolling on 11-12 row pages
dll.MenuTestLayoutStyle(1)
for _ in range(3): # Actions -> Cheats -> Photo -> Settings (-NoLobby)
    press(PAD_RB)
press(PAD_DOWN) # Appearance & theme...
press(PAD_A)
assert dll.MenuTestPage() == 17, dll.MenuTestPage()
check_render("page:appearance", "center")

# Scroll down through Appearance (10 rows > 9 visible) and verify focus updates cleanly
for step in range(9):
    press(PAD_DOWN)
    assert (dll.MenuTestSnapshot() & 15) == step + 1
check_render("page:appearance_scrolled_bottom", "center")
press(PAD_B) # Settings Home
assert dll.MenuTestPage() == 16
check_render("page:settings_home", "center")

for row, pname, pnum in ((1, "controls", 18), (2, "game_settings", 19), (3, "files_support", 20), (4, "performance", 25)):
    while (dll.MenuTestSnapshot() & 15) != row:
        press(PAD_DOWN)
    press(PAD_A)
    assert dll.MenuTestPage() == pnum, (pname, dll.MenuTestPage())
    check_render(f"page:{pname}", "center")
    press(PAD_B)

# 3. Confirmation dialog pages (Quit / Restart) with status message
press(PAD_B) # back to Main Settings tab
for _ in range(3):
    press(PAD_LB)
while (dll.MenuTestSnapshot() & 15) != 2: # Restart game...
    press(PAD_DOWN)
press(PAD_A)
dll.MenuTestStatus("In-fight cheats require an active fight in the Octagon.")
check_render("dialog:restart", "center")
press(PAD_B)
press(PAD_DOWN) # Quit game...
press(PAD_A)
check_render("dialog:quit", "center")
press(PAD_B)

# 4. Toast notification closed menu across 720p / 1080p / 1440p
dll.MenuTestToggle(); pump(0.2)
dll.MenuTestStatus("In-fight cheats require an active fight in the Octagon.")
for vw, vh in RESOLUTIONS:
    cw, ch, ox, oy = c.c_uint(0), c.c_uint(0), c.c_uint(0), c.c_uint(0)
    dll.Ufc6MenuExtent(vw, vh, c.byref(cw), c.byref(ch))
    dll.Ufc6OverlayPosition(vw, vh, c.byref(ox), c.byref(oy))
    assert ox.value + cw.value <= vw and oy.value + ch.value <= vh
    buf = (c.c_ubyte * (cw.value * ch.value * 4))()
    dll.MenuTestTextClips(1)
    assert dll.Ufc6RenderMenu(buf, len(buf))
    assert dll.MenuTestTextClips(0) == 0, f"Toast clipped @ {vw}x{vh}"

print("PASS: all modern settings, confirmation dialogs, 7 layouts, focus scrolling, centering, and toast notifications verified at 720p, 1080p, and 1440p with zero clipping.")
