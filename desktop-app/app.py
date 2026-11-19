"""Joystick editor for the ESP32-C3 variant over USB serial."""

import copy
import json
import os
from pathlib import Path
import re
import sys
import time
import tkinter as tk
from tkinter import colorchooser, filedialog, messagebox, ttk

import serial
from serial.tools import list_ports

from keyboard_output import KEYS, send, shortcut


BG = "#17191e"
CARD = "#252a33"
INK = "#f6eadf"
MUTED = "#aab1bd"
ACCENT = "#f3ad83"
SETTINGS = Path(os.getenv("LOCALAPPDATA", Path.home())) / "JoystickEditor" / "profile.json"
DIRECTIONS = ("Links", "Rechts", "Oben", "Unten")
BUTTONS = ("Taste 1", "Taste 2", "Stick-Klick")
SIGNALS = DIRECTIONS + BUTTONS
MODES = ("Aus", "Festfarbe", "X-Position", "Tastenfeedback")
PRESETS = (
    ("Navigation", ("Links", "Rechts", "Oben", "Unten", "Tab", "Escape", "Enter")),
    ("WASD", ("A", "D", "W", "S", "Shift", "Escape", "Leertaste")),
)
DEFAULT = {
    "version": 1,
    "actions": dict(zip(SIGNALS, PRESETS[0][1])),
    "center": [2048, 2048], "deadzone": 550, "reverse": [False, True],
    "repeat_ms": 170, "led_mode": 0, "color": "#d2875c", "brightness": 24,
}
PACKET = re.compile(rb"JOY1,(\d{1,4}),(\d{1,4}),([0-7]),([0-7])")
LEGACY_PACKET = re.compile(
    rb"X:(\d{1,4}) Y:(\d{1,4}) LED:\d+ DIR:[A-Z]+ Joy:([01]) B1:([01]) B2:([01])")


def parse_line(line):
    match = PACKET.fullmatch(line)
    if match:
        x, y, held, edges = map(int, match.groups())
    elif (match := LEGACY_PACKET.fullmatch(line)):
        x, y, click, first, second = map(int, match.groups())
        held, edges = first | (second << 1) | (click << 2), 0
    else:
        return None
    if x > 4095 or y > 4095:
        return None
    return x, y, held, edges, line.startswith(b"JOY1,")


def profile_checked(raw):
    if not isinstance(raw, dict) or raw.get("version") != 1:
        raise ValueError("Unbekanntes Joystick-Profil.")
    result = copy.deepcopy(DEFAULT)
    for key in DEFAULT:
        result[key] = raw[key]
    if not isinstance(result["actions"], dict) or set(result["actions"]) != set(SIGNALS):
        raise ValueError("Das Profil benötigt Belegungen für alle sieben Eingaben.")
    for value in result["actions"].values():
        if not isinstance(value, str):
            raise ValueError("Die Belegung muss ein Tastenkürzel sein.")
        shortcut(value)
    for key, low, high in (("deadzone", 40, 2000), ("repeat_ms", 70, 2000),
                           ("led_mode", 0, 3), ("brightness", 0, 64)):
        if type(result[key]) is not int or not low <= result[key] <= high:
            raise ValueError(f"Ungültiger Wert für {key}.")
    if not isinstance(result["center"], list) or len(result["center"]) != 2 or any(
            type(value) is not int or not 0 <= value <= 4095 for value in result["center"]):
        raise ValueError("Ungültige Achsenmitte.")
    if not isinstance(result["reverse"], list) or len(result["reverse"]) != 2 or any(
            type(value) is not bool for value in result["reverse"]):
        raise ValueError("Ungültige Achsenrichtung.")
    if not isinstance(result["color"], str) or not re.fullmatch(r"#[0-9a-fA-F]{6}", result["color"]):
        raise ValueError("Ungültige LED-Farbe.")
    return result


def write_profile(path, profile):
    path.parent.mkdir(parents=True, exist_ok=True)
    temp = path.with_suffix(".tmp")
    temp.write_text(json.dumps(profile, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    temp.replace(path)


def axis_directions(axes, profile):
    x, y = (raw - center for raw, center in zip(axes, profile["center"]))
    reverse_x, reverse_y = profile["reverse"]
    x = -x if reverse_x else x
    y = -y if reverse_y else y
    zone = profile["deadzone"]
    return x, y, (x < -zone, x > zone, y < -zone, y > zone)


class Editor:
    def __init__(self, root):
        self.root = root
        root.title("Joystick Editor")
        root.geometry("890x640")
        root.minsize(760, 560)
        root.configure(bg=BG)
        style = ttk.Style(root)
        style.theme_use("clam")
        style.configure(".", background=CARD, foreground=INK, fieldbackground=BG)
        style.configure("TNotebook", background=BG)
        style.configure("TNotebook.Tab", padding=(15, 8))
        style.map("TNotebook.Tab", foreground=[("selected", ACCENT)])
        style.configure("TButton", padding=(9, 5))
        style.map("TCombobox", selectbackground=[("readonly", CARD)],
                  fieldbackground=[("readonly", CARD)])
        style.configure("Treeview", background=CARD, fieldbackground=CARD, foreground=INK, rowheight=25)

        self.profile = copy.deepcopy(DEFAULT)
        try:
            self.profile = profile_checked(json.loads(SETTINGS.read_text(encoding="utf-8")))
        except (OSError, KeyError, ValueError, TypeError):
            pass
        self.port = None
        self.firmware = None
        self.buffer = bytearray()
        self.next_poll = self.last_seen = 0.0
        self.axes = None
        self.held = 0
        self.last_directions = (False,) * 4
        self.next_actions = [0.0] * 4
        self.armed = False
        self.color = self.profile["color"]
        self.port_name = tk.StringVar()
        self.status = tk.StringVar(value="COM-Port des Joysticks wählen")
        self.enabled = tk.BooleanVar(value=False)

        bar = ttk.Frame(root, padding=12)
        bar.pack(fill="x", padx=14, pady=(14, 0))
        ttk.Label(bar, text="Joystick", font=("Segoe UI", 20, "bold")).pack(side="left", padx=(0, 18))
        self.port_picker = ttk.Combobox(bar, textvariable=self.port_name, state="readonly", width=12)
        self.port_picker.pack(side="left")
        ttk.Button(bar, text="Ports suchen", command=self.scan).pack(side="left", padx=6)
        self.connection = ttk.Button(bar, text="Verbinden", command=self.connect)
        self.connection.pack(side="left")
        self.output = ttk.Checkbutton(bar, text="Tastaturausgabe", variable=self.enabled, command=self.toggle_output)
        self.output.pack(side="right")
        self.output.state(["disabled"])
        ttk.Label(root, textvariable=self.status, background=BG, foreground=MUTED).pack(anchor="w", padx=18, pady=7)

        self.tabs = ttk.Notebook(root)
        self.tabs.pack(fill="both", expand=True, padx=14, pady=(0, 14))
        self.bindings = ttk.Frame(self.tabs, padding=14)
        self.leds = ttk.Frame(self.tabs, padding=14)
        self.tester = ttk.Frame(self.tabs, padding=14)
        self.tabs.add(self.bindings, text="Belegung")
        self.tabs.add(self.leds, text="NeoPixels · GPIO5")
        self.tabs.add(self.tester, text="Tester")
        self.tabs.bind("<<NotebookTabChanged>>", self.tab_changed)
        self.make_bindings()
        self.make_leds()
        self.make_tester()
        self.load_form()
        self.scan()
        root.protocol("WM_DELETE_WINDOW", self.close)
        root.after(33, self.tick)

    def make_bindings(self):
        form = self.bindings
        form.columnconfigure(1, weight=1)
        form.columnconfigure(3, weight=1)
        self.actions = {}
        for index, name in enumerate(SIGNALS):
            value = tk.StringVar()
            self.actions[name] = value
            ttk.Label(form, text=name).grid(row=index, column=0, sticky="w", padx=(0, 8), pady=4)
            picker = ttk.Combobox(form, textvariable=value, width=15, values=("Aus", *KEYS))
            picker.grid(row=index, column=1, sticky="ew")
        self.deadzone = tk.StringVar()
        self.repeat = tk.StringVar()
        for row, (caption, variable) in enumerate((("Totzone (ADC)", self.deadzone),
                                                      ("Wiederholung (ms)", self.repeat))):
            ttk.Label(form, text=caption).grid(row=row, column=2, sticky="w", padx=(20, 8))
            ttk.Entry(form, textvariable=variable, width=10).grid(row=row, column=3, sticky="ew")
        self.reverse = [tk.BooleanVar(), tk.BooleanVar()]
        for row, axis in enumerate("XY", start=2):
            ttk.Checkbutton(form, text=f"{axis}-Achse umkehren", variable=self.reverse[row - 2]).grid(
                row=row, column=2, columnspan=2, sticky="w", padx=20)
        self.center = ttk.Label(form)
        self.center.grid(row=4, column=2, columnspan=2, sticky="w", padx=20)
        ttk.Button(form, text="Aktuelle Position als Mitte", command=self.calibrate).grid(
            row=5, column=2, columnspan=2, sticky="w", padx=20)
        ttk.Label(form, text="Tastenkürzel kannst du direkt eingeben, etwa Strg+Z.", foreground=MUTED).grid(
            row=7, column=0, columnspan=4, sticky="w", pady=14)
        tools = ttk.Frame(form)
        tools.grid(row=8, column=0, columnspan=4, sticky="ew")
        ttk.Button(tools, text="Speichern", command=self.save).pack(side="left")
        for name, values in PRESETS:
            ttk.Button(tools, text=name, command=lambda codes=values: self.preset(codes)).pack(side="left", padx=5)
        ttk.Button(tools, text="Import", command=self.import_profile).pack(side="right")
        ttk.Button(tools, text="Export", command=self.export_profile).pack(side="right", padx=5)

    def make_leds(self):
        self.mode = tk.StringVar()
        self.brightness = tk.StringVar()
        ttk.Label(self.leds, text="Effekt").grid(row=0, column=0, pady=5, sticky="w")
        ttk.Combobox(self.leds, textvariable=self.mode, values=MODES, state="readonly").grid(
            row=0, column=1, padx=12, sticky="w")
        ttk.Label(self.leds, text="Helligkeit (0–64)").grid(row=1, column=0, sticky="w")
        ttk.Entry(self.leds, textvariable=self.brightness, width=8).grid(row=1, column=1, padx=12, sticky="w")
        self.swatch = tk.Button(self.leds, text="Farbe wählen", relief="flat", command=self.pick_color)
        self.swatch.grid(row=2, column=1, padx=12, pady=10, sticky="w")
        ttk.Button(self.leds, text="Speichern und anwenden", command=self.save).grid(
            row=3, column=0, columnspan=2, sticky="w", pady=(0, 10))
        tests = ttk.Frame(self.leds)
        tests.grid(row=4, column=0, columnspan=2, sticky="w")
        self.pixel_buttons = []
        for index in range(3):
            control = ttk.Button(tests, text=f"LED {index + 1} testen", command=lambda i=index: self.test_pixel(i))
            control.pack(side="left", padx=(0, 5))
            control.state(["disabled"])
            self.pixel_buttons.append(control)

    def make_tester(self):
        preview = ttk.Frame(self.tester)
        preview.pack(side="left", fill="y", padx=(0, 12))
        self.canvas = tk.Canvas(preview, bg=BG, width=228, height=228, highlightthickness=0)
        self.canvas.pack()
        self.raw = ttk.Label(preview, text="X · GPIO0     Y · GPIO1")
        self.raw.pack(anchor="w", pady=(8, 0))
        self.preview()
        events = ttk.Frame(self.tester)
        events.pack(side="left", fill="both", expand=True)
        self.log = ttk.Treeview(events, columns=("signal", "pin", "value"), show="headings", height=12)
        for key, caption, width in (("signal", "Signal", 100), ("pin", "Pin", 66), ("value", "Funktion / Wert", 170)):
            self.log.heading(key, text=caption)
            self.log.column(key, width=width, minwidth=55)
        scroll = ttk.Scrollbar(events, orient="vertical", command=self.log.yview)
        self.log.configure(yscrollcommand=scroll.set)
        scroll.pack(side="right", fill="y")
        self.log.pack(fill="both", expand=True)
        ttk.Button(preview, text="Liste leeren", command=self.clear_log).pack(anchor="w", pady=10)

    def load_form(self):
        profile = self.profile
        for name, value in self.actions.items():
            value.set(profile["actions"][name])
        self.deadzone.set(profile["deadzone"])
        self.repeat.set(profile["repeat_ms"])
        for var, reverse in zip(self.reverse, profile["reverse"]):
            var.set(reverse)
        self.center.configure(text="Mitte · " + " / ".join(map(str, profile["center"])))
        self.mode.set(MODES[profile["led_mode"]])
        self.brightness.set(profile["brightness"])
        self.color = profile["color"]
        self.swatch.configure(bg=self.color)
        self.preview()

    def capture(self):
        return profile_checked({
            "version": 1, "actions": {name: field.get().strip() for name, field in self.actions.items()},
            "center": self.profile["center"], "deadzone": int(self.deadzone.get()),
            "reverse": [var.get() for var in self.reverse], "repeat_ms": int(self.repeat.get()),
            "led_mode": MODES.index(self.mode.get()), "color": self.color, "brightness": int(self.brightness.get()),
        })

    def save(self):
        try:
            updated = self.capture()
            write_profile(SETTINGS, updated)
            self.profile = updated
            self.toggle_output()
            self.apply_leds()
            self.status.set("Profil gespeichert.")
        except (OSError, ValueError) as error:
            messagebox.showerror("Profil", str(error), parent=self.root)

    def preset(self, codes):
        for name, value in zip(SIGNALS, codes):
            self.actions[name].set(value)

    def calibrate(self):
        if self.axes is None:
            self.status.set("Für die Kalibrierung den Joystick verbinden.")
            return
        previous = self.profile["center"]
        self.profile["center"] = list(self.axes)
        try:
            updated = self.capture()
            write_profile(SETTINGS, updated)
            self.profile = updated
        except (OSError, ValueError) as error:
            self.profile["center"] = previous
            self.status.set(str(error))
            return
        self.center.configure(text="Mitte · " + " / ".join(map(str, self.axes)))
        self.toggle_output()
        self.preview()

    def pick_color(self):
        _, color = colorchooser.askcolor(self.color, parent=self.root)
        if color:
            self.color = color
            self.swatch.configure(bg=color)

    def import_profile(self):
        path = filedialog.askopenfilename(parent=self.root, filetypes=(("Joystick-Profil", "*.json"),))
        if path:
            try:
                loaded = profile_checked(json.loads(Path(path).read_text(encoding="utf-8-sig")))
                write_profile(SETTINGS, loaded)
                self.profile = loaded
                self.load_form()
                self.toggle_output()
                self.apply_leds()
            except (OSError, KeyError, TypeError, ValueError) as error:
                messagebox.showerror("Import", str(error), parent=self.root)

    def export_profile(self):
        path = filedialog.asksaveasfilename(parent=self.root, defaultextension=".json", initialfile="joystick.json",
                                            filetypes=(("Joystick-Profil", "*.json"),))
        if path:
            try:
                write_profile(Path(path), self.capture())
            except (OSError, ValueError) as error:
                messagebox.showerror("Export", str(error), parent=self.root)

    def scan(self):
        ports = sorted(port.device for port in list_ports.comports() if port.vid is not None)
        self.port_picker.configure(values=ports)
        if self.port_name.get() not in ports and not self.port:
            self.port_name.set(ports[0] if len(ports) == 1 else "")

    def connect(self):
        if self.port:
            self.disconnect("Verbindung getrennt")
            return
        if not self.port_name.get():
            self.status.set("Einen COM-Port auswählen.")
            return
        try:
            self.port = serial.Serial(self.port_name.get(), 115200, timeout=0, write_timeout=0.05)
        except (OSError, serial.SerialException) as error:
            self.status.set(str(error))
            return
        self.last_seen = time.monotonic()
        self.next_poll = self.last_seen + 0.2
        self.connection.configure(text="Trennen")
        self.port_picker.state(["disabled"])
        self.status.set("Warte auf Joystick-Signal …")

    def disconnect(self, note):
        if self.port:
            try:
                self.port.close()
            except serial.SerialException:
                pass
        self.port = self.firmware = self.axes = None
        self.buffer.clear()
        self.held = 0
        self.enabled.set(False)
        self.toggle_output()
        self.connection.configure(text="Verbinden")
        self.port_picker.state(["!disabled"])
        self.output.state(["disabled"])
        for control in self.pixel_buttons:
            control.state(["disabled"])
        self.status.set(note)
        self.raw.configure(text="X · GPIO0     Y · GPIO1")
        self.preview()

    def send_command(self, command):
        if self.port:
            self.port.write(command.encode("ascii") + b"\n")

    def apply_leds(self):
        if self.firmware == "JOY1":
            p = self.profile
            rgb = tuple(int(p["color"][start:start + 2], 16) for start in (1, 3, 5))
            self.send_command("L %d %d %d %d %d" % (p["led_mode"], *rgb, p["brightness"]))

    def test_pixel(self, index):
        try:
            self.send_command(f"T {index}")
        except serial.SerialException as error:
            self.disconnect(str(error))

    def tab_changed(self, _):
        if self.tabs.select() == str(self.tester):
            self.enabled.set(False)
            self.toggle_output()
            self.output.state(["disabled"])
        elif self.firmware == "JOY1" and sys.platform == "win32":
            self.output.state(["!disabled"])

    def toggle_output(self):
        self.next_actions = [0.0] * 4
        # Require a return to neutral after enabling output or changing the profile.
        self.armed = False

    def handle_line(self, line, now):
        packet = parse_line(line)
        if not packet:
            if self.firmware == "JOY1" and line in (b"OK T 0", b"OK T 1", b"OK T 2"):
                self.add_log("NeoPixel", "GPIO5", f"LED {int(line[-1:]) + 1} · Sichttest gesendet")
            elif line.startswith(b"JOY1,"):
                self.add_log("Nicht zugeordnet", "Unbekannt", "Ungültiger Eingabebericht")
            return
        x, y, held, edges, modern = packet
        if self.firmware is not None and modern != (self.firmware == "JOY1"):
            return
        fresh = self.axes is None
        self.last_seen = now
        if self.firmware is None:
            self.firmware = "JOY1" if modern else "alt"
            self.status.set(f"{self.port_name.get()} verbunden" if modern else "Älterer Sketch · Eingabetest verfügbar")
            if modern:
                self.apply_leds()
                self.tab_changed(None)
                for control in self.pixel_buttons:
                    control.state(["!disabled"])
        self.axes, rising, self.held = (x, y), (held & ~self.held) | edges, held
        _, _, directions = axis_directions(self.axes, self.profile)
        if not fresh:
            for i, active in enumerate(directions):
                if active != self.last_directions[i]:
                    self.add_log(DIRECTIONS[i], "GPIO0" if i < 2 else "GPIO1", "Ausgelenkt" if active else "Zurück")
            for i, name in enumerate(BUTTONS):
                if rising & (1 << i):
                    pin = f"GPIO{i + 2}" if modern or i == 2 else "Unbekannt"
                    self.add_log(name, pin, "Gedrückt")
        self.last_directions = directions
        if self.enabled.get() and modern:
            if not self.armed:
                self.armed = not any(directions) and not held and not rising
            else:
                self.emit(now, directions, rising)
        self.preview()

    def emit(self, now, directions, rising):
        try:
            for i, active in enumerate(directions):
                if not active:
                    self.next_actions[i] = 0
                elif now >= self.next_actions[i]:
                    send(shortcut(self.profile["actions"][DIRECTIONS[i]]))
                    self.next_actions[i] = now + self.profile["repeat_ms"] / 1000
            for i, name in enumerate(BUTTONS):
                if rising & (1 << i):
                    send(shortcut(self.profile["actions"][name]))
        except OSError as error:
            self.enabled.set(False)
            self.status.set(str(error))

    def preview(self):
        canvas = self.canvas
        canvas.delete("all")
        canvas.create_oval(21, 21, 207, 207, outline=CARD, width=2)
        canvas.create_line(114, 21, 114, 207, fill=CARD)
        canvas.create_line(21, 114, 207, 114, fill=CARD)
        radius = self.profile["deadzone"] * 85 / 2048
        canvas.create_oval(114-radius, 114-radius, 114+radius, 114+radius, outline=MUTED, dash=(2, 4))
        if self.axes is not None:
            x, y, _ = axis_directions(self.axes, self.profile)
            px = 114 + max(-85, min(85, x * 85 / 2048))
            py = 114 + max(-85, min(85, y * 85 / 2048))
            canvas.create_oval(px-6, py-6, px+6, py+6, fill=ACCENT, outline="")
            self.raw.configure(text=f"X {self.axes[0]} · GPIO0    Y {self.axes[1]} · GPIO1")

    def add_log(self, name, pin, detail):
        self.log.insert("", 0, values=(name, pin, detail))
        old = self.log.get_children()[100:]
        if old:
            self.log.delete(*old)

    def clear_log(self):
        items = self.log.get_children()
        if items:
            self.log.delete(*items)

    def tick(self):
        if self.port:
            now = time.monotonic()
            try:
                if now >= self.next_poll and self.firmware != "alt":
                    self.send_command("?")
                    self.next_poll = now + (0.033 if self.firmware else 0.25)
                chunk = self.port.read(min(self.port.in_waiting, 2048))
                if chunk:
                    self.buffer.extend(chunk)
                    *lines, tail = self.buffer.split(b"\n")
                    self.buffer = bytearray(tail if len(tail) < 128 else b"")
                    for line in lines:
                        self.handle_line(line.strip(), now)
                if now - self.last_seen > (2.0 if self.firmware else 7.0):
                    self.disconnect("Kein Joystick-Signal am gewählten COM-Port.")
            except (serial.SerialException, OSError) as error:
                self.disconnect(str(error))
        self.root.after(33, self.tick)

    def close(self):
        self.disconnect("")
        self.root.destroy()


if __name__ == "__main__":
    root = tk.Tk()
    Editor(root)
    root.mainloop()
