# USB protocol

The device is a **composite HID device** with one interface and two top-level collections:

| Collection | Usage page / usage | Report IDs | Direction |
|---|---|---|---|
| Keyboard | `0x01` / `0x06` | 1 | board → PC |
| Vendor-defined | `0xFF00` / `0x01` | 2 (Input), 3 (Output) | both |

USB identifiers: **VID `0x0483` (1155)**, **PID `0x572B` (22315)**, product string `MacroPad`.

On Windows the two collections appear as separate HID devices (`...&Col01` keyboard, `...&Col02` vendor), so the host script finds the vendor one by its usage page.

## Report descriptor

```c
__ALIGN_BEGIN static uint8_t HID_MOUSE_ReportDesc[HID_MOUSE_REPORT_DESC_SIZE] __ALIGN_END =
{
    0x05, 0x01,  // Usage Page: Generic Desktop
    0x09, 0x06,  // Usage: Keyboard
    0xA1, 0x01,  // Collection: Application
    0x85, 0x01,  // Report ID: 1

    0x05, 0x07,  // Usage Page: Keyboard/Keypad
    0x19, 0xE0,  // Usage Min: Left Control
    0x29, 0xE7,  // Usage Max: Right GUI
    0x15, 0x00,  // Logical Min: 0
    0x25, 0x01,  // Logical Max: 1
    0x75, 0x01,  // Report Size: 1 bit
    0x95, 0x08,  // Report Count: 8
    0x81, 0x02,  // Input (Data, Variable, Absolute): modifier byte

    0x95, 0x01,  // Report Count: 1
    0x75, 0x08,  // Report Size: 8 bits
    0x81, 0x01,  // Input (Constant): reserved byte

    0x95, 0x06,  // Report Count: 6
    0x75, 0x08,  // Report Size: 8 bits
    0x15, 0x00,  // Logical Min: 0
    0x25, 0x65,  // Logical Max: 101
    0x05, 0x07,  // Usage Page: Keyboard/Keypad
    0x19, 0x00,  // Usage Min: 0
    0x29, 0x65,  // Usage Max: 101
    0x81, 0x00,  // Input (Data, Array): 6 keycodes

    0xC0,        // End Collection (keyboard)

    // ---- Vendor-defined data interface ----
    0x06, 0x00, 0xFF,  // Usage Page: Vendor Defined (0xFF00)
    0x09, 0x01,        // Usage: 1
    0xA1, 0x01,        // Collection: Application

    0x15, 0x00,        //   Logical Min: 0
    0x26, 0xFF, 0x00,  //   Logical Max: 255
    0x75, 0x08,        //   Report Size: 8 bits
    0x95, 0x3F,        //   Report Count: 63 bytes

    0x85, 0x02,        //   Report ID: 2
    0x09, 0x02,        //   Usage: 2
    0x81, 0x02,        //   Input  (board -> PC), 63 bytes

    0x85, 0x03,        //   Report ID: 3
    0x09, 0x03,        //   Usage: 3
    0x91, 0x02,        //   Output (PC -> board), 63 bytes

    0xC0               // End Collection (vendor)
};
```

Each vendor report has a 63-byte payload; with the leading report ID byte that is **64 bytes**, so the HID IN endpoint's maximum packet size must be 64.

## Keyboard report (Report ID 1, board → PC)

| Byte | Meaning |
|---|---|
| 0 | Report ID = `0x01` |
| 1 | Modifier bits (`0x01` Left Ctrl, `0x02` Left Shift, `0x04` Left Alt, `0x08` Left GUI, ...) |
| 2 | Reserved (0) |
| 3 to 8 | Up to six key codes (e.g. `C` = `0x06`, `V` = `0x19`, `Tab` = `0x2B`) |

A shortcut tap sends the report with the modifier and key. A report with all zeros must follow it to release the keys, otherwise the key stays pressed.

## Button report (Report ID 2, board → PC)

Sent when a *launch* button is touched. 64 bytes, padded with zeros.

| Byte | Meaning |
|---|---|
| 0 | Report ID = `0x02` |
| 1 | Button's `app` number (the key in `actions.json`) |
| 2 | Event: `1` = press (the host ignores other values) |
| 3 to 63 | 0 |

## Stats report (Report ID 3, PC → board)

Sent by the host once per second. 64 bytes, padded with zeros.

| Byte | Meaning |
|---|---|
| 0 | Report ID = `0x03` |
| 1 | Command: `0x01` = stats |
| 2 | CPU load, percent (0 to 100) |
| 3 | RAM use, percent (0 to 100) |
| 4 | Battery, percent (0 to 100), or `0xFF` if there is no battery |
| 5 | Temperature in degrees C (0 to 120), or `0xFF` if unavailable |
| 6 | Sequence number (wraps 255 → 0) |
| 7 to 63 | 0 |

`0xFF` means "not available" and the screen draws `--` instead of a value. The sequence number lets the screen show that data is still arriving. Temperature is only available on Linux and FreeBSD through `psutil`, so it shows `--` on Windows.

## Host-side selection

`macropad_host.py` opens the HID interface whose `usage_page` is `0xFF00`. If none is found (some Linux setups expose a single node), it falls back to the first device with the right VID and PID.
