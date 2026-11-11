"""
Macropad host script (PC side).

Does two things over the vendor HID collection:
  1. Sends CPU / RAM / battery / temp stats to the board once a second
     (Output report ID 3, 64 bytes).
  2. Listens for "launch button pressed" reports from the board
     (Input report ID 2, 64 bytes: [0x02, app_id, event, 0...]) and runs the
     action mapped to that app_id in actions.json.

Install:  pip install hidapi psutil
Run:      python macropad_host.py
"""
import json
import os
import subprocess
import sys
import time
import webbrowser
from pathlib import Path

import hid
import psutil

# --- from usbd_desc.c: USBD_VID 1155 (0x0483), USBD_PID_FS 22315 (0x572B) ---
VID = 1155
PID = 22315
VENDOR_USAGE_PAGE = 0xFF00

REPORT_ID_BUTTON = 0x02     # board -> PC (vendor Input report)
REPORT_ID_STATS = 0x03      # PC -> board (vendor Output report)
CMD_STATS = 0x01
PACKET_SIZE = 64
NA = 0xFF                   # "not available"
EVENT_PRESS = 1

STATS_PERIOD = 1.0          # seconds
ACTIONS_FILE = Path(__file__).with_name("actions.json")


# ----------------------------------------------------------------- stats ---
def clamp(value, lo, hi):
    return max(lo, min(hi, int(value)))


def read_temperature():
    # psutil only provides temperatures on Linux/FreeBSD.
    try:
        temps = psutil.sensors_temperatures()
    except AttributeError:
        return NA
    for entries in temps.values():
        if entries:
            return clamp(entries[0].current, 0, 120)
    return NA


def read_battery():
    b = psutil.sensors_battery()
    return NA if b is None else clamp(b.percent, 0, 100)


def build_packet(seq):
    packet = bytearray(PACKET_SIZE)
    packet[0] = REPORT_ID_STATS
    packet[1] = CMD_STATS
    packet[2] = clamp(psutil.cpu_percent(interval=None), 0, 100)
    packet[3] = clamp(psutil.virtual_memory().percent, 0, 100)
    packet[4] = read_battery()
    packet[5] = read_temperature()
    packet[6] = seq & 0xFF
    return bytes(packet)


# --------------------------------------------------------------- actions ---
def load_actions():
    with open(ACTIONS_FILE, "r", encoding="utf-8") as f:
        return json.load(f)


def run_action(action):
    kind = action.get("type")
    target = action.get("target", "")
    args = action.get("args", [])

    if kind == "app":                 # run a program
        subprocess.Popen([target, *args])
    elif kind == "file":              # open a file/folder with its default program
        if sys.platform.startswith("win"):
            os.startfile(target)
        elif sys.platform == "darwin":
            subprocess.Popen(["open", target])
        else:
            subprocess.Popen(["xdg-open", target])
    elif kind == "url":
        webbrowser.open(target)
    elif kind == "command":           # any shell command
        subprocess.Popen(target, shell=True)
    else:
        print(f"  unknown action type: {kind!r}")


def handle_report(data, actions):
    # hidapi returns the report ID as byte 0
    if len(data) < 3 or data[0] != REPORT_ID_BUTTON:
        return
    app_id, event = data[1], data[2]
    if event != EVENT_PRESS:
        return
    action = actions.get(str(app_id))
    if action is None:
        print(f"Button {app_id}: nothing mapped in actions.json")
        return
    print(f"Button {app_id}: {action.get('name', action.get('type'))}")
    try:
        run_action(action)
    except Exception as exc:          # a bad path must not kill the listener
        print(f"  failed: {exc}")


# ---------------------------------------------------------------- device ---
def find_device_path():
    found = hid.enumerate(VID, PID)
    for info in found:                # Windows: vendor collection is its own device
        if info.get("usage_page") == VENDOR_USAGE_PAGE:
            return info["path"]
    return found[0]["path"] if found else None   # Linux hidraw: one node


def run(dev, actions):
    last_stats = 0.0
    last_mtime = ACTIONS_FILE.stat().st_mtime
    seq = 0
    while True:
        data = dev.read(PACKET_SIZE, timeout_ms=50)
        if data:
            handle_report(data, actions)

        now = time.monotonic()
        if now - last_stats >= STATS_PERIOD:
            if dev.write(build_packet(seq)) < 0:
                raise OSError("write failed")
            seq += 1
            last_stats = now

        mtime = ACTIONS_FILE.stat().st_mtime      # hot-reload the mapping
        if mtime != last_mtime:
            last_mtime = mtime
            try:
                actions = load_actions()
                print("actions.json reloaded")
            except (OSError, ValueError) as exc:
                print(f"actions.json not reloaded: {exc}")


def main():
    psutil.cpu_percent(interval=None)   # first call always returns 0.0, so prime it
    actions = load_actions()
    while True:
        path = find_device_path()
        if path is None:
            print("Waiting for macropad...")
            time.sleep(2)
            continue
        dev = hid.device()
        try:
            dev.open_path(path)
            print("Connected.")
            run(dev, actions)
        except (OSError, ValueError) as exc:
            print(f"Device lost ({exc}), reconnecting...")
        finally:
            dev.close()
            time.sleep(1)


if __name__ == "__main__":
    main()
