#include "touch.h"
#include "tft.h"

/* XPT2046 control bytes: start bit | channel | 12-bit | differential | power-down
 *   0xD0 = measure X position      0x90 = measure Y position            */
#define TS_CMD_X   0xD0U
#define TS_CMD_Y   0x90U

/* The three calibration points (same as the irrigation project, 320 x 240):
 *   A = top-left   B = top-right   C = bottom-right                       */
#define CAL_A_X   20
#define CAL_A_Y   20
#define CAL_B_X   ((int32_t)SCR_W - 20)
#define CAL_B_Y   20
#define CAL_C_X   ((int32_t)SCR_W - 20)
#define CAL_C_Y   ((int32_t)SCR_H - 20)

/* calibration routine settings */
#define TS_CAL_SAMPLES     8U        /* readings averaged per tap                   */
#define TS_CAL_TIMEOUT_MS  30000UL   /* give up waiting for a tap after this        */
#define TS_CAL_MIN_SPAN    1000      /* raw counts between two points, else bad taps */

/* The calibration in use.  Starts as the defaults from touch.h and is
 * replaced (until the next reset) when touch_calibrate() succeeds. */
static uint8_t cal_swap = TS_SWAP_XY;
static int32_t cal_ax_a = TS_CAL_AX_A, cal_ax_b = TS_CAL_AX_B;
static int32_t cal_ay_b = TS_CAL_AY_B, cal_ay_c = TS_CAL_AY_C;

void touch_init(void)
{
    pin_high(TS_CS_PORT, TS_CS_PIN);  pin_mode(TS_CS_PORT, TS_CS_PIN, PM_OUT);
    pin_mode(TS_IRQ_PORT, TS_IRQ_PIN, PM_IN);
    pin_pull(TS_IRQ_PORT, TS_IRQ_PIN, PUD_UP);
}

/* The chip pulls IRQ low while the panel is pressed. */
uint8_t touch_pressed(void)
{
    return (uint8_t)(pin_read(TS_IRQ_PORT, TS_IRQ_PIN) == 0U);
}

/* One conversion: send the control byte, then clock out 16 bits.
 * The 12-bit result sits in bits 14..3 of those 16 bits. */
static uint16_t ts_read12(uint8_t cmd)
{
    (void)spi1_xfer(cmd);
    uint8_t hi = spi1_xfer(0x00U);
    uint8_t lo = spi1_xfer(0x00U);
    return (uint16_t)(((((uint16_t)hi << 8) | lo) >> 3) & 0x0FFFU);
}

/* Sort 5 values, return the middle one, and report whether they agreed. */
static uint8_t filter5(uint16_t *v, uint16_t *out)
{
    for (uint8_t i = 1U; i < 5U; i++) {
        uint16_t k = v[i];
        int8_t   j = (int8_t)(i - 1U);
        while (j >= 0 && v[j] > k) { v[j + 1] = v[j]; j--; }
        v[j + 1] = k;
    }
    if ((uint16_t)(v[4] - v[0]) > TS_MAX_SPREAD) { return 0U; }
    *out = v[2];
    return 1U;
}

uint8_t touch_read_raw(uint16_t *rx, uint16_t *ry)
{
    uint16_t xs[5], ys[5];

    if (!touch_pressed()) { return 0U; }

    spi1_set_speed(SPI_BR_TOUCH);                 /* slow down for the XPT2046 */
    pin_low(TS_CS_PORT, TS_CS_PIN);

    (void)ts_read12(TS_CMD_X);                    /* throw-away: lets the ADC settle */
    for (uint8_t i = 0U; i < 5U; i++) {
        xs[i] = ts_read12(TS_CMD_X);
        ys[i] = ts_read12(TS_CMD_Y);
    }

    pin_high(TS_CS_PORT, TS_CS_PIN);
    spi1_set_speed(SPI_BR_TFT);                   /* back to full speed for the display */

    if (!touch_pressed())                  { return 0U; }   /* finger lifted meanwhile */
    if (!filter5(xs, rx) || !filter5(ys, ry)) { return 0U; }
    return 1U;
}

/* raw reading -> screen pixels: a straight line through two points per axis */
static void map_raw(uint16_t rx, uint16_t ry, int16_t *x, int16_t *y)
{
    int32_t ax = cal_swap ? ry : rx;              /* raw value that drives screen X */
    int32_t ay = cal_swap ? rx : ry;              /* raw value that drives screen Y */
    int32_t dx = cal_ax_b - cal_ax_a;
    int32_t dy = cal_ay_c - cal_ay_b;
    if (dx == 0) { dx = 1; }
    if (dy == 0) { dy = 1; }

    int32_t sx = CAL_A_X + (ax - cal_ax_a) * (CAL_B_X - CAL_A_X) / dx;
    int32_t sy = CAL_B_Y + (ay - cal_ay_b) * (CAL_C_Y - CAL_B_Y) / dy;

    if (sx < 0)                  { sx = 0; }
    if (sx > (int32_t)SCR_W - 1) { sx = (int32_t)SCR_W - 1; }
    if (sy < 0)                  { sy = 0; }
    if (sy > (int32_t)SCR_H - 1) { sy = (int32_t)SCR_H - 1; }

    *x = (int16_t)sx;
    *y = (int16_t)sy;
}

uint8_t touch_read(int16_t *x, int16_t *y)
{
    uint16_t rx, ry;
    if (!touch_read_raw(&rx, &ry)) { return 0U; }
    map_raw(rx, ry, x, y);
    return 1U;
}

/* ===================================================================== *
 *  On-screen calibration
 * ===================================================================== */

#define CAL_BG    RGB565(0, 0, 0)
#define CAL_FG    RGB565(240, 244, 250)
#define CAL_ACC   RGB565(0, 190, 255)
#define CAL_OK    RGB565(60, 220, 120)
#define CAL_BAD   RGB565(255, 80, 80)

static int32_t iabs32(int32_t v) { return (v < 0) ? -v : v; }

/* tiny text builders (no printf needed) */
static char *put_str(char *p, const char *s)
{
    while (*s != '\0') { *p++ = *s++; }
    return p;
}

static char *put_int(char *p, int32_t v, uint8_t force_sign)
{
    char    t[12];
    uint8_t n = 0U;
    if (v < 0)           { *p++ = '-'; v = -v; }
    else if (force_sign) { *p++ = '+'; }
    do { t[n++] = (char)('0' + (v % 10)); v /= 10; } while (v > 0);
    while (n > 0U) { *p++ = t[--n]; }
    return p;
}

static void screen_clear(void)
{
    tft_fill_rect(0, 0, SCR_W, SCR_H, CAL_BG);
}

static void msg(int16_t y, const char *s, uint16_t col, uint8_t scale)
{
    int16_t w = (int16_t)tft_text_width(s, scale);
    tft_text((int16_t)((SCR_W - w) / 2), y, s, col, CAL_BG, scale);
}

static void draw_target(int16_t x, int16_t y, uint16_t col)
{
    tft_fill_rect((int16_t)(x - 12), y, 25, 1, col);
    tft_fill_rect(x, (int16_t)(y - 12), 1, 25, col);
    tft_draw_rect((int16_t)(x - 4), (int16_t)(y - 4), 9, 9, 1, col);
}

/* Wait for the finger to go away for 80 ms. */
static void wait_release(void)
{
    uint32_t t = HAL_GetTick();
    while ((HAL_GetTick() - t) < 80UL) {
        if (touch_pressed()) { t = HAL_GetTick(); }
    }
}

/* Wait for one tap and return the average raw reading while it lasted.
 * Returns 0 if nobody touched the screen for TS_CAL_TIMEOUT_MS. */
static uint8_t wait_tap(uint16_t *rx, uint16_t *ry)
{
    uint32_t t0 = HAL_GetTick();

    for (;;) {
        uint16_t x, y;
        uint32_t sx, sy;
        uint8_t  n;

        while (!touch_read_raw(&x, &y)) {
            if ((HAL_GetTick() - t0) > TS_CAL_TIMEOUT_MS) { return 0U; }
        }
        sx = x; sy = y; n = 1U;
        while (n < TS_CAL_SAMPLES && touch_pressed()) {
            if (touch_read_raw(&x, &y)) { sx += x; sy += y; n++; }
        }
        wait_release();

        if (n >= 3U) {                            /* ignore very short brushes */
            *rx = (uint16_t)(sx / n);
            *ry = (uint16_t)(sy / n);
            return 1U;
        }
    }
}

/* Work out swap and the four raw values from the three taps
 * (0 = A top-left, 1 = B top-right, 2 = C bottom-right).
 * Returns 0 (and changes nothing) if the taps do not make sense. */
static uint8_t cal_compute(const uint16_t *rx, const uint16_t *ry)
{
    /* A -> B: only screen X changes.  The raw channel that moved most is X. */
    int32_t dxa = iabs32((int32_t)rx[1] - rx[0]);
    int32_t dya = iabs32((int32_t)ry[1] - ry[0]);
    uint8_t swap = (dya > dxa) ? 1U : 0U;

    /* B -> C: only screen Y changes.  The OTHER raw channel must move most,
     * otherwise the taps were not on the right crosses. */
    int32_t dxb = iabs32((int32_t)rx[2] - rx[1]);
    int32_t dyb = iabs32((int32_t)ry[2] - ry[1]);
    if (swap ? (dxb <= dyb) : (dyb <= dxb)) { return 0U; }

    int32_t ax_a = swap ? ry[0] : rx[0];
    int32_t ax_b = swap ? ry[1] : rx[1];
    int32_t ay_b = swap ? rx[1] : ry[1];
    int32_t ay_c = swap ? rx[2] : ry[2];

    if (iabs32(ax_b - ax_a) < TS_CAL_MIN_SPAN || iabs32(ay_c - ay_b) < TS_CAL_MIN_SPAN) {
        return 0U;
    }

    cal_swap = swap;
    cal_ax_a = ax_a;  cal_ax_b = ax_b;
    cal_ay_b = ay_b;  cal_ay_c = ay_c;
    return 1U;
}

uint8_t touch_calibrate_requested(void)
{
    if (!touch_pressed()) { return 0U; }
    uint32_t t = HAL_GetTick();
    while ((HAL_GetTick() - t) < 600UL) {
        if (!touch_pressed()) { return 0U; }
    }
    return 1U;
}

void touch_calibrate(void)
{
    const int16_t px[3] = { CAL_A_X, (int16_t)CAL_B_X, (int16_t)CAL_C_X };
    const int16_t py[3] = { CAL_A_Y, CAL_B_Y, (int16_t)CAL_C_Y };
    const char *cal_names[3] = { "TOP LEFT", "TOP RIGHT", "BOTTOM RIGHT" };
    uint16_t rx[3], ry[3];
    char     line[48];
    char    *p;

    /* ---- pass 1: tap the three crosses A, B, C ----------------------- */
    for (uint8_t i = 0U; i < 3U; i++) {
        screen_clear();
        msg(70, "TOUCH CALIBRATION", CAL_ACC, 2U);
        msg(100, "TOUCH THE CROSS FIRMLY", CAL_FG, 1U);
        msg(115, cal_names[i], CAL_FG, 1U);
        draw_target(px[i], py[i], CAL_FG);

        if (!wait_tap(&rx[i], &ry[i])) {
            screen_clear();
            msg(110, "TIMED OUT - OLD VALUES KEPT", CAL_BAD, 1U);
            HAL_Delay(1500U);
            return;
        }
        draw_target(px[i], py[i], CAL_OK);
        HAL_Delay(150U);
    }

    if (!cal_compute(rx, ry)) {
        screen_clear();
        msg(100, "BAD TAPS - OLD VALUES KEPT", CAL_BAD, 1U);
        msg(120, "TAP THE CROSSES AS EXACTLY AS YOU CAN", CAL_FG, 1U);
        HAL_Delay(2500U);
        return;
    }

    /* ---- pass 2: check - four corners and the center ------------------
     * Only A, B and C were used to calibrate, so a panel that is slightly
     * rotated (skewed) is exact at some points and off at others.  One bad
     * point = a sloppy tap.  Several bad points = skew (or bad calibration
     * taps).                                                              */
    const char   *chk_names[5] = { "TOP LEFT", "TOP RIGHT", "BOTTOM LEFT", "BOTTOM RIGHT", "CENTER" };
    const int16_t vx[5] = { CAL_A_X, (int16_t)CAL_B_X, CAL_A_X, (int16_t)CAL_C_X, (int16_t)(SCR_W / 2) };
    const int16_t vy[5] = { CAL_A_Y, CAL_B_Y, (int16_t)CAL_C_Y, (int16_t)CAL_C_Y, (int16_t)(SCR_H / 2) };
    int32_t maxerr = 0;
    uint8_t n_bad = 0U;

    for (uint8_t i = 0U; i < 5U; i++) {
        uint16_t a, b;
        int16_t  sx, sy;

        screen_clear();
        msg(70, "CHECK", CAL_ACC, 2U);
        msg(100, "TOUCH THE CROSS", CAL_FG, 1U);
        msg(115, chk_names[i], CAL_FG, 1U);
        draw_target(vx[i], vy[i], CAL_FG);

        if (!wait_tap(&a, &b)) { break; }
        map_raw(a, b, &sx, &sy);

        int32_t dx = (int32_t)sx - vx[i];
        int32_t dy = (int32_t)sy - vy[i];
        int32_t e  = (iabs32(dx) > iabs32(dy)) ? iabs32(dx) : iabs32(dy);
        if (e > maxerr) { maxerr = e; }
        if (e > 6)      { n_bad++; }

        draw_target(vx[i], vy[i], (e <= 6) ? CAL_OK : CAL_BAD);
        p = put_str(line, "OFF X");  p = put_int(p, dx, 1U);
        p = put_str(p, "  Y");       p = put_int(p, dy, 1U);
        p = put_str(p, " PX");       *p = '\0';
        msg(150, line, (e <= 6) ? CAL_OK : CAL_BAD, 1U);
        HAL_Delay(700U);
    }

    /* ---- results ------------------------------------------------------ */
    screen_clear();
    msg(8, "RESULT", CAL_ACC, 2U);

    p = put_str(line, "TS_SWAP_XY   ");   p = put_int(p, cal_swap, 0U);   *p = '\0';
    tft_text(16, 40, line, CAL_FG, CAL_BG, 1U);
    p = put_str(line, "TS_CAL_AX_A  ");   p = put_int(p, cal_ax_a, 0U);   *p = '\0';
    tft_text(16, 56, line, CAL_FG, CAL_BG, 1U);
    p = put_str(line, "TS_CAL_AX_B  ");   p = put_int(p, cal_ax_b, 0U);   *p = '\0';
    tft_text(16, 72, line, CAL_FG, CAL_BG, 1U);
    p = put_str(line, "TS_CAL_AY_B  ");   p = put_int(p, cal_ay_b, 0U);   *p = '\0';
    tft_text(16, 88, line, CAL_FG, CAL_BG, 1U);
    p = put_str(line, "TS_CAL_AY_C  ");   p = put_int(p, cal_ay_c, 0U);   *p = '\0';
    tft_text(16, 104, line, CAL_FG, CAL_BG, 1U);

    p = put_str(line, "MAX ERR ");   p = put_int(p, maxerr, 0U);
    p = put_str(p, " PX   POINTS OFF ");  p = put_int(p, n_bad, 0U);
    p = put_str(p, " OF 5");         *p = '\0';
    tft_text(16, 128, line, CAL_FG, CAL_BG, 1U);

    if (maxerr <= 6) {
        tft_text(16, 148, "GOOD - COPY THE 5 VALUES INTO touch.h", CAL_OK, CAL_BG, 1U);
    } else if (n_bad >= 2U) {
        tft_text(16, 148, "OFF AT SEVERAL POINTS. TRY AGAIN.", CAL_BAD, CAL_BG, 1U);
        tft_text(16, 164, "IF IT REPEATS, THE PANEL IS SKEWED", CAL_BAD, CAL_BG, 1U);
        tft_text(16, 180, "AND NEEDS AN AFFINE CALIBRATION.", CAL_BAD, CAL_BG, 1U);
    } else {
        tft_text(16, 148, "ONE TAP WAS OFF. TRY AGAIN.", CAL_BAD, CAL_BG, 1U);
    }
    msg(200, "TAP TO FINISH", CAL_ACC, 1U);

    uint16_t a, b;
    (void)wait_tap(&a, &b);
    screen_clear();
}
