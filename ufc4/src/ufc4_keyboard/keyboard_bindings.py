"""SacredFire UFC 4 keyboard bindings, including PORTHEON's bbport.ini keys."""
from dataclasses import dataclass
from pathlib import Path

# PORTHEON src/ui_keys.c KC4, verified against SacredFire's action table.
ACTION_CONTROLS = {
    "cross": "Cross", "circle": "Circle", "square": "Square",
    "triangle": "Triangle", "l1": "L1", "r1": "R1", "l2": "L2",
    "r2": "R2", "l3": "L3", "r3": "R3", "options": "Options",
    "touchpad": "TouchPad", "touchpad_right": "TouchPadRight",
    "up": "Up", "down": "Down", "left": "Left", "right": "Right",
    "move_up": "LeftStickUp", "move_down": "LeftStickDown",
    "move_left": "LeftStickLeft", "move_right": "LeftStickRight",
    "look_up": "RightStickUp", "look_down": "RightStickDown",
    "look_left": "RightStickLeft", "look_right": "RightStickRight",
}

DEFAULTS = dict(zip(ACTION_CONTROLS.values(), (
    "Space", "Left Shift", "E", "Q", "1", "3", "R", "F", "Z", "C",
    "Return", "Tab", "Backspace", "I", "K", "J", "L", "W", "S",
    "A", "D", "Up", "Down", "Left", "Right",
)))


@dataclass
class Bindings:
    controls: dict[str, str]


def load_bbport(path: Path | None) -> Bindings:
    """Read the first key for each action, as PORTHEON's keyboard UI does."""
    controls = DEFAULTS.copy()
    if path is None or not path.is_file():
        return Bindings(controls)
    for raw in path.read_text(encoding="utf-8-sig").splitlines():
        if not raw.startswith("key.") or "=" not in raw:
            continue
        action, value = raw.split("=", 1)
        control = ACTION_CONTROLS.get(action[4:].lower())
        first_key = value.split(",", 1)[0].strip()
        if control and first_key:
            controls[control] = first_key
    return Bindings(controls)
