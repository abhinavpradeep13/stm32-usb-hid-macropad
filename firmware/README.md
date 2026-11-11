# Firmware (STM32CubeIDE project)

Target: **STM32F411CEU6 "Black Pill"**, programmed over SWD with an ST-Link.

## CubeMX / project settings

| Setting | Value |
|---|---|
| MCU | STM32F411CEU6 |
| HSE | 25 MHz crystal |
| PLL | M = 25, N = 192, P = 2, Q = 4 → **96 MHz** system clock, **48 MHz** USB clock |
| USB | `USB_OTG_FS` in device mode, USB_DEVICE middleware with the HID class |
| Debug | Serial Wire |

The USB identifiers are set in `USB_DEVICE/App/usbd_desc.c`: VID 1155 (`0x0483`), PID 22315 (`0x572B`), product string `MacroPad`.

## Source map

| File | What it does |
|---|---|
| `Core/Src/main.c` | Clock setup, peripheral init, main loop |
| `Core/Src/ui.c`, `Core/Inc/ui.h` | Screen layout, the six macro buttons, drawing the stats, touch handling |
| `Core/Src/tft.c`, `Core/Inc/tft.h` | ILI9341 driver and register-level SPI1 driver, pin definitions |
| `Core/Src/touch.c`, `Core/Inc/touch.h` | XPT2046 driver, filtering, 3-point calibration |
| `USB_DEVICE/App/usbd_desc.c` | USB device descriptor (VID / PID / strings) |
| `Middlewares/.../Class/HID/` | HID class with the custom report descriptor ([docs/protocol.md](../docs/protocol.md)) |

Modules you write yourself live in the `Core` folder; everything CubeMX generates around them follows the usual layout.

## Button table (`ui.c`)

Each of the six buttons is either a keyboard shortcut or a launcher:

```c
typedef struct {
    int16_t     x, y, w, h;
    const char *label;
    uint8_t     mod, key;
    int8_t      app;        
} Macro_t;
```

An `app` of 0, 1, 2 ... selects the matching key in `host/actions.json`. HID modifiers: `0x01` Left Ctrl, `0x04` Left Alt. Key codes: `C` = `0x06`, `V` = `0x19`, `Tab` = `0x2B`.

## Options in `ui.c`

| Define | Meaning |
|---|---|
| `UI_TOUCH_DEBUG` | 1 = show raw and mapped touch values on screen |
| `UI_FORCE_CAL` | 1 = run the touch calibration at every power-up |

## Building and flashing

1. In STM32CubeIDE: *File → Import → Existing Projects into Workspace*, and select this `firmware` folder.
2. Build the project (hammer icon).
3. Connect the ST-Link and flash (*Run* or *Debug*). To test USB, **let the program run freely**: a CPU halted at a breakpoint cannot answer USB requests.
4. Follow [docs/bring-up.md](../docs/bring-up.md).

> Note: regenerating code from the `.ioc` file can overwrite changes you made inside the middleware folders. Keep your edits to the HID class in version control (this repository does that) and re-check them after any regeneration.
