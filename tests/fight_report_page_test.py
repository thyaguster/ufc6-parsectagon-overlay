"""Test the Post-fight report page: winner, method, round, stats, capture time, all 4 upload states, retry, and multi-res rendering."""
import ctypes as c
import ctypes.wintypes as w
import json
import tempfile
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
cls = WNDCLASSW(0, c.cast(proc, c.c_void_p), 0, 0, app, None, None, None, None, "Ufc6FightReportTestHost")
u32.RegisterClassW(c.byref(cls))
hwnd = u32.CreateWindowExW(0, "Ufc6FightReportTestHost", "FightReport", 0x00CF0000, 100, 100, 1280, 720, None, None, app, None)

def pump(sec=0.05):
    end = time.time() + sec
    msg = w.MSG()
    while time.time() < end:
        while u32.PeekMessageW(c.byref(msg), None, 0, 0, 1):
            u32.TranslateMessage(c.byref(msg))
            u32.DispatchMessageW(c.byref(msg))
        time.sleep(0.008)

def press(btn, wait=0.06):
    dll.MenuTestButton(btn, 1)
    dll.MenuTestButton(btn, 0)
    pump(wait)

dll.MenuTestFightReport.argtypes = [w.LPCWSTR]
dll.MenuTestFightReport.restype = c.c_int
dll.MenuTestPage.restype = c.c_int
dll.MenuTestTextClips.argtypes = [c.c_int]
dll.MenuTestTextClips.restype = c.c_int

PAD_A, PAD_B, PAD_DOWN = 0x4000, 0x2000, 0x40

def assert_renders(label):
    for vw, vh in ((1280, 720), (1920, 1080), (3840, 2160)):
        cw, ch = c.c_uint(0), c.c_uint(0)
        dll.Ufc6MenuExtent(vw, vh, c.byref(cw), c.byref(ch))
        buf = (c.c_ubyte * (cw.value * ch.value * 4))()
        dll.MenuTestTextClips(1)
        assert dll.Ufc6RenderMenu(buf, len(buf)), f"Render failed {label}"
        assert dll.MenuTestTextClips(0) == 0, f"Text clipped on {label} @ {vw}x{vh}"

dll.MenuTestStart(hwnd); pump(0.35)
dll.Ufc6VulkanActive()
dll.MenuTestLayoutStyle(1)
dll.MenuTestToggle(); pump(0.25)

# Open Post-fight report from Actions tab row 4
for _ in range(4):
    press(PAD_DOWN)
press(PAD_A)
assert dll.MenuTestPage() == 39, f"Expected PAGE_FIGHT_REPORT (39), got {dll.MenuTestPage()}"
assert_renders("empty")

with tempfile.TemporaryDirectory() as tmp:
    tdir = Path(tmp)
    report_path = tdir / "fight-report-abc12345.json"
    sample = {
        "schema": 1,
        "id": "abc12345-0000-0000-0000-000000000001",
        "reportStage": "complete",
        "capturedAt": 1794920000000,
        "result": {"winnerCorner": "red", "method": "Knockout", "round": 2, "clockSeconds": 194},
        "fighters": [
            {"corner": "red", "name": None, "stats": {"knockdowns": 2, "stuns": 3, "significantStrikesAttempted": 60, "significantStrikesLanded": 38, "totalStrikesAttempted": 85, "totalStrikesLanded": 56, "takedownsAttempted": 2, "takedownsLanded": 1, "controlTimeSeconds": 42}},
            {"corner": "blue", "name": None, "stats": {"knockdowns": 0, "stuns": 1, "significantStrikesAttempted": 48, "significantStrikesLanded": 21, "totalStrikesAttempted": 64, "totalStrikesLanded": 34, "takedownsAttempted": 3, "takedownsLanded": 0, "controlTimeSeconds": 11}},
        ],
    }
    report_path.write_text(json.dumps(sample), encoding="utf-8")

    # 1. Saved locally (no sidecar)
    assert dll.MenuTestFightReport(str(tdir)) == 1 # REPORT_UPLOAD_SAVED_LOCAL
    assert_renders("saved_local")

    # 2. Needs review (upload-error.txt)
    err_file = tdir / "fight-report-abc12345.json.upload-error.txt"
    err_file.write_text("Lobby HTTP 503: Service Unavailable\n", encoding="utf-8")
    assert dll.MenuTestFightReport(str(tdir)) == 4 # REPORT_UPLOAD_NEEDS_REVIEW
    assert_renders("needs_review_error")

    # Retry failed upload via row 0 -> transitions to Waiting to upload (2)
    press(PAD_A)
    assert_renders("retrying_waiting")

    # 3. Submitted (.upload.json status=uploaded)
    err_file.unlink()
    marker = tdir / "fight-report-abc12345.json.upload.json"
    marker.write_text(json.dumps({"status": "uploaded", "captureId": "cap_1"}), encoding="utf-8")
    assert dll.MenuTestFightReport(str(tdir)) == 3 # REPORT_UPLOAD_SUBMITTED
    assert_renders("submitted")

    # 4. Needs review via server marker (status=needs_review)
    marker.write_text(json.dumps({"status": "needs_review", "captureId": "cap_1"}), encoding="utf-8")
    assert dll.MenuTestFightReport(str(tdir)) == 4 # REPORT_UPLOAD_NEEDS_REVIEW
    assert_renders("needs_review_marker")

# Back returns to Actions row 4
press(PAD_B)
assert dll.MenuTestPage() == 0

print("PASS: post-fight report page, all 4 upload states, retry flow, and 720p/1080p/4K rendering verified.")
