# Touch calibration

The XPT2046 returns raw 12-bit readings (0 to 4095) that depend on the individual panel. The firmware turns them into screen pixels using a **3-point calibration**.

## How it works

You tap three crosses:

| Point | Screen position (320 x 240) |
|---|---|
| A | top-left (20, 20) |
| B | top-right (300, 20) |
| C | bottom-right (300, 220) |

- Between A and B only the screen X changes, so those two readings fix the scale and offset of X. The raw channel that moved the most is the one that drives X, which also detects a swapped X/Y panel.
- Between B and C only the screen Y changes, which fixes Y.
- Each axis is a straight line through two points, so an inverted axis is handled by the sign of the slope with no extra flag.

Each tap is the average of several readings, and each reading is itself the median of five. A tap on the wrong cross (for example, the readings do not change on the expected axis) is rejected and the old values are kept.

## Running it

- Hold a finger on the screen while the board powers up, for about a second, **or**
- set `UI_FORCE_CAL` to 1 in `ui.c` to calibrate at every boot.

1. Tap the three crosses (A, B, C) firmly.
2. **Check pass**: tap five more crosses (four corners and the center). Each turns green if the tap lands within 6 px and shows the offset, for example `OFF X+1 Y-2 PX`.
3. The **result screen** shows five values and the worst error.


## Making it permanent

The calibration lives in RAM and is lost at reset. Copy the five values from the result screen into `firmware/Core/Inc/touch.h`:

```c
#define TS_SWAP_XY    1
#define TS_CAL_AX_A   3668
#define TS_CAL_AX_B   420
#define TS_CAL_AY_B   3590
#define TS_CAL_AY_C   490
```

(The numbers above are placeholders. Use your own.) Then set `UI_FORCE_CAL` back to 0.

## Reading the check pass

| Result screen | Meaning |
|---|---|
| `GOOD` | All five points within 6 px |
| `ONE TAP WAS OFF` | One point is off, probably a sloppy tap. Run it again. |
| `OFF AT SEVERAL POINTS` | Several points are off. Try once more with careful taps. If it repeats, the panel is rotated (skewed) slightly, which a 3-point calibration cannot correct. A full affine calibration would. |

