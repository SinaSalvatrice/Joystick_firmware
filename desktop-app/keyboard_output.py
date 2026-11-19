"""Windows keyboard shortcuts. Each action has a matching key release."""

import ctypes
import sys
from ctypes import wintypes

KEYS = {
    "Enter": 0x0D, "Escape": 0x1B, "Tab": 0x09, "Leertaste": 0x20,
    "Links": 0x25, "Oben": 0x26, "Rechts": 0x27, "Unten": 0x28,
    "Pos1": 0x24, "Ende": 0x23, "Bild auf": 0x21, "Bild ab": 0x22,
    "Strg": 0x11, "Shift": 0x10, "Alt": 0x12, "Win": 0x5B,
}
KEYS.update({letter: ord(letter) for letter in "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"})
KEYS.update({f"F{i}": 0x6F + i for i in range(1, 25)})
EXTENDED = {0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x5B}


def shortcut(text):
    text = text.strip()
    if not text or text == "Aus":
        return ()
    names = [part.strip() for part in text.split("+")]
    try:
        codes = tuple(KEYS[name] for name in names)
    except KeyError as error:
        raise ValueError(f"Unbekannte Taste: {error.args[0]}") from error
    if len(codes) > 5 or len(set(codes)) != len(codes):
        raise ValueError("Bis zu fünf verschiedene Tasten pro Tastenkürzel.")
    return codes


def send(codes):
    if not codes:
        return
    if sys.platform != "win32":
        raise OSError("Die Tastaturausgabe ist für Windows implementiert.")

    class KeyInput(ctypes.Structure):
        _fields_ = [("vk", wintypes.WORD), ("scan", wintypes.WORD),
                    ("flags", wintypes.DWORD), ("time", wintypes.DWORD),
                    ("extra", ctypes.c_size_t)]

    class MouseInput(ctypes.Structure):
        _fields_ = [("dx", wintypes.LONG), ("dy", wintypes.LONG),
                    ("mouseData", wintypes.DWORD), ("flags", wintypes.DWORD),
                    ("time", wintypes.DWORD), ("extra", ctypes.c_size_t)]

    class InputData(ctypes.Union):
        _fields_ = [("key", KeyInput), ("mouse", MouseInput)]

    class Input(ctypes.Structure):
        _fields_ = [("type", wintypes.DWORD), ("data", InputData)]

    def event(code, up):
        return Input(1, InputData(key=KeyInput(code, 0, int(code in EXTENDED) | (2 if up else 0), 0, 0)))

    events = (Input * (len(codes) * 2))(
        *(event(code, False) for code in codes), *(event(code, True) for code in reversed(codes)))
    send_input = ctypes.windll.user32.SendInput
    send_input.argtypes = (wintypes.UINT, ctypes.POINTER(Input), ctypes.c_int)
    send_input.restype = wintypes.UINT
    if send_input(len(events), events, ctypes.sizeof(Input)) != len(events):
        releases = (Input * len(codes))(*(event(code, True) for code in reversed(codes)))
        send_input(len(releases), releases, ctypes.sizeof(Input))
        raise OSError("Windows hat die Tastaturausgabe blockiert oder unterbrochen.")
