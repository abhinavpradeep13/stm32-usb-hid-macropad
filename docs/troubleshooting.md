# Troubleshooting

## `hid.enumerate(1155, 22315)` returns an empty list

- Is the **board's own USB-C port** plugged into the PC? The ST-Link only powers and programs the board.
- Is the cable a data cable (not charge-only)?
- Is the firmware running? Stop the debug session or press Resume, then reset the board.
- Is it the MacroPad firmware, and not another project (for example a bootloader) that you flashed last?

## "Unknown USB Device (Device Descriptor Request Failed)"

USB is trying but failing. In order of likelihood:

1. The USB clock is not exactly **48 MHz**. With the 25 MHz crystal the PLL settings are M = 25, N = 192, P = 2, Q = 4 (96 MHz system clock, 48 MHz USB).
2. `HID_MOUSE_REPORT_DESC_SIZE` does not match the real length of the report descriptor.
3. A bad connection on PA11 / PA12.

## USB works, then fails while debugging

A CPU halted at a breakpoint does not answer the host's USB requests, so Windows reports a descriptor failure even though the code is correct. Resume the program, or run it without the debugger.

## Only the keyboard interface shows up (no vendor interface)

Check that the descriptor-size constant matches the descriptor, and that the HID IN endpoint's maximum packet size is **64** (the vendor reports are 64 bytes including the report ID).

## The screen draws, but stays on `NO DATA`

The host script is not delivering stats, or the firmware is not handling Output report ID 3. Check that `macropad_host.py` printed `Connected.` and did not print `Device lost`.

## Buttons print nothing in the host terminal

The board is not sending the button report, or the report is not 64 bytes. Make sure the IN endpoint is 64 bytes and that the firmware sends `[0x02, app_id, 1, 0, ...]`.

## `Button N: nothing mapped in actions.json`

The button's `app` number has no key in `host/actions.json`.

## `failed: [WinError 2] The system cannot find the file specified`

The `target` path in `actions.json` is wrong. Copy the real path from the application's shortcut (right-click, *Properties*, *Target*) and double every backslash in JSON (`\` becomes `\\`).

## An app opens in the wrong place, or `claude.exe` opened a terminal

`app` runs a program by name. A command-line tool with the same name as a desktop app will open in the terminal. For apps installed as packaged apps, use the AppID method in the [README](../README.md#3-choose-what-each-button-does).

## The screen is white or black

- White with backlight on: the display is powered but not initialized. Check CS, DC, RST and the SPI wires.
- Black: check the backlight pin and 3V3. On many modules a solder jumper (often J1) must be bridged when powering from 3.3 V.

## Taps land in the wrong place

Run the calibration ([calibration.md](calibration.md)). If it keeps reporting several points off, the panel is slightly rotated and needs a full affine calibration.

