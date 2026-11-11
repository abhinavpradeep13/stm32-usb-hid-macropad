#ifndef TOUCH_H
#define TOUCH_H

#include "main.h"
#include <stdint.h>

/* =====================================================================
 *  XPT2046 touch controller, on the same SPI1 bus as the display.
 *  Pins (see tft.h):  CS = PB10   IRQ = PA3  (IRQ is LOW while touched)
 *
 *  Raw readings are 12-bit (0..4095).  Calibration is the same 3-point
 *  scheme as the irrigation project: tap A (top-left), B (top-right) and
 *  C (bottom-right).  Between A and B only screen X changes, between B and
 *  C only screen Y changes, so each axis is a straight line through two
 *  points.  The slope sign takes care of inverted axes automatically.
 *
 *  The values below are only the DEFAULTS used at power-up (a rough guess).
 *  Run the on-screen calibration (hold a finger on the screen while the
 *  board powers up, or set UI_FORCE_CAL in ui.c), then copy the numbers
 *  it shows at the end into these five lines.
 * ===================================================================== */

/* 1 = the raw Y channel drives screen X (normal for a landscape screen). */
#define TS_SWAP_XY    1
/* Raw reading (of the channel that drives that screen axis) at each point:
 *   AX_A: at A, AX_B: at B  -> screen X axis
 *   AY_B: at B, AY_C: at C  -> screen Y axis                              */
#define TS_CAL_AX_A   3650
#define TS_CAL_AX_B   490
#define TS_CAL_AY_B   3500
#define TS_CAL_AY_C   490

/* The five samples must agree within this many counts, else the reading
 * is thrown away (the finger was moving or only just landing). */
#define TS_MAX_SPREAD 300

void    touch_init(void);
uint8_t touch_pressed(void);                          /* 1 while a finger is down */
uint8_t touch_read_raw(uint16_t *rx, uint16_t *ry);   /* 1 = valid raw reading    */
uint8_t touch_read(int16_t *x, int16_t *y);           /* 1 = valid, in screen px  */

/* 1 if a finger has been held on the screen for ~0.6 s (call at power-up). */
uint8_t touch_calibrate_requested(void);
/* Interactive: 3 taps to calibrate, then 5 taps to check the result.
 * The new calibration stays active until reset; the numbers to copy into
 * this file are shown on the screen at the end. */
void    touch_calibrate(void);

#endif /* TOUCH_H */
