# Bring-up checklist

Do the steps in order. Each one proves one thing, so a failure points to one cause.

## 1. Board alone

1. Connect the ST-Link (SWDIO, SWCLK, GND, 3V3). Leave the display unplugged.
2. Flash a plain blink on **PC13** (the on-board LED, active low).
3. If it blinks, programming works and the clock setup is sane.

## 2. USB enumeration (display still unplugged)

1. Flash the macropad firmware and let it **run freely** (stop the debug session or press Resume, then reset). A CPU halted at a breakpoint cannot answer USB.
2. Plug the board's **USB-C** port into the PC.
3. Check from Python:

   ```
   python -c "import hid; print(hid.enumerate(1155, 22315))"
   ```

   Expected: two entries, one with `usage_page` 1 (usage 6, the keyboard) and one with `usage_page` 65280 (the vendor interface).

   | You see | Likely cause |
   |---|---|
   | Empty list | USB-C cable not plugged in or charge-only, firmware not running, or wrong firmware flashed |
   | "Unknown USB Device (Device Descriptor Request Failed)" | USB clock is not exactly 48 MHz, or a descriptor size mismatch |
   | Only the keyboard entry | `HID_MOUSE_REPORT_DESC_SIZE` does not match the real descriptor length, or the IN endpoint size is not 64 |

## 3. Host link

```
cd host
python macropad_host.py
```

Expected: `Connected.` and no `Device lost` messages. This proves the 64-byte vendor reports go through.

## 4. Display and touch

Wire the display (see [wiring.md](wiring.md)), with USB unplugged.

- The title bar and six buttons should draw. White screen with backlight: check CS / DC / RST. Black screen: check backlight and 3V3.
- Within about a second of the host script connecting, the corner shows **PC LINKED** and the CPU and RAM bars move.
- Calibrate the touch panel ([calibration.md](calibration.md)).

## 5. Buttons

- **Shortcut buttons**: open a text editor and tap them. Each tap should produce exactly one shortcut.
- **Launch buttons**: with the host script running, each tap should print `Button N: ...` and open the app.

## 6. Reconnect

Unplug and replug the USB cable. The script should print `Device lost`, then `Connected.` on its own, and the screen should return to `PC LINKED`.
