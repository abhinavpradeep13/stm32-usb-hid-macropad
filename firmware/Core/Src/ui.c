#include "ui.h"
#include "tft.h"
#include "touch.h"
#include "usb_device.h"
#include "usbd_hid.h"
extern USBD_HandleTypeDef hUsbDeviceFS;

/* 1 = show raw and mapped touch values on screen, for calibration. */
#define UI_TOUCH_DEBUG   0
#define UI_FORCE_CAL   0     /* 1 = run the calibration at every power-up */

/* ---- colours ------------------------------------------------------- */
#define COL_BG        RGB565(10, 14, 22)
#define COL_PANEL     RGB565(26, 34, 52)
#define COL_TEXT      RGB565(240, 244, 250)
#define COL_DIM       RGB565(130, 142, 165)
#define COL_ACCENT    RGB565(0, 190, 255)
#define COL_GOOD      RGB565(60, 220, 120)
#define COL_WARN      RGB565(255, 190, 40)
#define COL_BAD       RGB565(255, 80, 80)
#define COL_BTN       RGB565(32, 62, 112)
#define COL_BTN_DOWN  RGB565(0, 170, 230)

/* ---- layout (320 x 240) ------------------------------------------- *
 *  y   0..23   title bar + link status
 *  y  34..127  four stat rows: CPU, RAM, BAT, TEMP + SEQ
 *  y 140..233  2 x 3 macro buttons                                      */
#define TITLE_H    24
#define ROW_Y0     34
#define ROW_DY     26
#define ROW_Y(n)   ((int16_t)(ROW_Y0 + (n) * ROW_DY))
#define LABEL_X    8
#define BAR_X      56
#define BAR_W      196
#define BAR_H      16
#define VAL_X      260
#define SEQ_X      226

#define NA         0xFFU       /* "not available" in the PC packet */
#define NEVER      0xFEU       /* cache marker: field never drawn  */

/* ---- macro buttons -------------------------------------------------- */
typedef struct {
    int16_t     x, y, w, h;
    const char *label;
    uint8_t     mod, key;
    int8_t      app;        /* >= 0: launch app #app on the PC, -1: keyboard shortcut */
} Macro_t;

#define N_MACROS 6U
static const Macro_t macros[N_MACROS] = {
    {   8, 140, 96, 44, "COPY",    0x01U, 0x06U, -1 },
    { 112, 140, 96, 44, "PASTE",   0x01U, 0x19U, -1 },
    { 216, 140, 96, 44, "ALT TAB", 0x04U, 0x2BU, -1 },
    {   8, 190, 96, 44, "ChatGPT", 0U,    0U,     0 },
    { 112, 190, 96, 44, "NOTEPAD", 0U,    0U,     1 },
    { 216, 190, 96, 44, "CLAUDE", 0U,    0U,     2 },
};

/* Ask the PC to launch app #app_id (vendor Input report ID 2, 64 bytes) */
static void send_launch(uint8_t app_id)
{
    static uint8_t report[64];     /* static: must stay valid until the USB transfer finishes */
    report[0] = 0x02U;             /* report ID */
    report[1] = app_id;
    report[2] = 1U;                /* 1 = press */
    for (uint8_t t = 0U; t < 10U; t++) {
        if (USBD_HID_SendReport(&hUsbDeviceFS, report, sizeof(report)) == (uint8_t)USBD_OK) { break; }
        HAL_Delay(1U);             /* endpoint busy, retry */
    }
}

static ui_send_fn send_fn;

/* what is currently on screen, so only changes are redrawn */
static uint8_t  s_cpu = NEVER, s_ram = NEVER, s_bat = NEVER, s_temp = NEVER;
static uint8_t  s_link = 0xFFU;
static uint16_t s_seq  = 0xFFFFU;

/* ---- small helpers -------------------------------------------------- */

/* Right-aligned decimal in w characters, padded with spaces. */
static void fmt_num(char *b, uint16_t v, uint8_t w)
{
    for (int8_t i = (int8_t)(w - 1U); i >= 0; i--) {
        if (v > 0U || i == (int8_t)(w - 1U)) { b[i] = (char)('0' + (v % 10U)); v = (uint16_t)(v / 10U); }
        else                                  { b[i] = ' '; }
    }
    b[w] = '\0';
}

static void fmt_pct(char *b, uint8_t v)               /* "100%", " 45%", "  --" */
{
    if (v == NA) { b[0] = ' '; b[1] = ' '; b[2] = '-'; b[3] = '-'; b[4] = '\0'; return; }
    if (v > 100U) { v = 100U; }
    fmt_num(b, v, 3U);
    b[3] = '%';
    b[4] = '\0';
}

/* Colour of a bar: for CPU/RAM high is bad, for the battery low is bad. */
static uint16_t level_color(uint8_t v, uint8_t low_is_bad)
{
    if (v == NA) { return COL_DIM; }
    if (low_is_bad) { return (v > 40U) ? COL_GOOD : (v > 20U) ? COL_WARN : COL_BAD; }
    return (v < 60U) ? COL_GOOD : (v < 85U) ? COL_WARN : COL_BAD;
}

/* ---- drawing -------------------------------------------------------- */
static void draw_bar(uint8_t row, uint8_t v, uint16_t col)
{
    uint8_t  pct = (v == NA) ? 0U : (v > 100U ? 100U : v);
    int16_t  fw  = (int16_t)(((uint32_t)BAR_W * pct) / 100U);
    if (fw > 0) { tft_fill_rect(BAR_X, ROW_Y(row), fw, BAR_H, col); }
    tft_fill_rect((int16_t)(BAR_X + fw), ROW_Y(row), (int16_t)(BAR_W - fw), BAR_H, COL_PANEL);
}

static void draw_percent_row(uint8_t row, uint8_t v, uint8_t low_is_bad)
{
    char b[8];
    draw_bar(row, v, level_color(v, low_is_bad));
    fmt_pct(b, v);
    tft_text(VAL_X, ROW_Y(row), b, (v == NA) ? COL_DIM : COL_TEXT, COL_BG, 2U);
}

static void draw_status(uint8_t link)
{
    const char *s = link ? "PC LINKED" : "NO DATA";
    uint16_t    tw = tft_text_width(s, 2U);
    tft_fill_rect(176, 0, (int16_t)(SCR_W - 176), TITLE_H, COL_PANEL);
    tft_text((int16_t)(SCR_W - 8 - (int16_t)tw), 4, s, link ? COL_GOOD : COL_BAD, COL_PANEL, 2U);
}

static void draw_button(uint8_t i, uint8_t pressed)
{
    const Macro_t *m = &macros[i];
    uint16_t fill = pressed ? COL_BTN_DOWN : COL_BTN;
    uint16_t ink  = pressed ? COL_BG : COL_TEXT;
    int16_t  tw   = (int16_t)tft_text_width(m->label, 2U);

    tft_fill_rect(m->x, m->y, m->w, m->h, fill);
    tft_draw_rect(m->x, m->y, m->w, m->h, 2, COL_ACCENT);
    tft_text((int16_t)(m->x + (m->w - tw) / 2), (int16_t)(m->y + (m->h - 16) / 2),
             m->label, ink, fill, 2U);
}

static void draw_frame(void)
{
    tft_fill_rect(0, 0, SCR_W, SCR_H, COL_BG);
    tft_fill_rect(0, 0, SCR_W, TITLE_H, COL_PANEL);
    tft_fill_rect(0, TITLE_H, SCR_W, 2, COL_ACCENT);
    tft_text(8, 4, "MACROPAD", COL_ACCENT, COL_PANEL, 2U);

    tft_text(LABEL_X, ROW_Y(0), "CPU",  COL_DIM, COL_BG, 2U);
    tft_text(LABEL_X, ROW_Y(1), "RAM",  COL_DIM, COL_BG, 2U);
    tft_text(LABEL_X, ROW_Y(2), "BAT",  COL_DIM, COL_BG, 2U);
    tft_text(LABEL_X, ROW_Y(3), "TEMP", COL_DIM, COL_BG, 2U);

    for (uint8_t i = 0U; i < N_MACROS; i++) { draw_button(i, 0U); }
}

/* ---- public --------------------------------------------------------- */
void ui_init(ui_send_fn send)
{
    send_fn = send;
    tft_init();
    touch_init();
    if (UI_FORCE_CAL || touch_calibrate_requested()) { touch_calibrate(); }
    draw_frame();

    s_cpu = s_ram = s_bat = s_temp = NEVER;     /* force the first update to draw */
    s_link = 0xFFU;
    s_seq  = 0xFFFFU;
}

void ui_update_stats(uint8_t cpu, uint8_t ram, uint8_t bat, uint8_t temp,
                     uint8_t seq, uint8_t link)
{
    char b[12];
    link = link ? 1U : 0U;

    if (link != s_link) { s_link = link; draw_status(link); }

    if (cpu != s_cpu) { s_cpu = cpu; draw_percent_row(0U, cpu, 0U); }
    if (ram != s_ram) { s_ram = ram; draw_percent_row(1U, ram, 0U); }
    if (bat != s_bat) { s_bat = bat; draw_percent_row(2U, bat, 1U); }

    if (temp != s_temp) {                       /* text only: "--" or " 45C" */
        s_temp = temp;
        if (temp == NA) { b[0] = '-'; b[1] = '-'; b[2] = ' '; b[3] = ' '; b[4] = '\0'; }
        else            { fmt_num(b, temp, 3U); b[3] = 'C'; b[4] = '\0'; }
        tft_text((int16_t)(BAR_X + 8), ROW_Y(3), b, (temp == NA) ? COL_DIM : COL_TEXT, COL_BG, 2U);
    }

    uint16_t seq_key = link ? seq : 0x0100U;    /* 0x100 = "no link, show ---" */
    if (seq_key != s_seq) {
        s_seq = seq_key;
        b[0] = 'S'; b[1] = 'E'; b[2] = 'Q'; b[3] = ' ';
        if (link) { fmt_num(&b[4], seq, 3U); }
        else      { b[4] = '-'; b[5] = '-'; b[6] = '-'; b[7] = '\0'; }
        tft_text(SEQ_X, ROW_Y(3), b, COL_DIM, COL_BG, 2U);
    }
}

void ui_poll(void)
{
    static uint8_t  down = 0U;
    static int8_t   active = -1;
    static uint32_t last_touch = 0U;

    int16_t  x, y;
    uint32_t now = HAL_GetTick();

    if (touch_read(&x, &y)) {
        last_touch = now;

#if UI_TOUCH_DEBUG
        {
            uint16_t rx, ry;
            char t[40];
            if (touch_read_raw(&rx, &ry)) {
                t[0] = 'R'; t[1] = 'A'; t[2] = 'W'; t[3] = ' ';
                fmt_num(&t[4], rx, 4U);  t[8] = ' ';
                fmt_num(&t[9], ry, 4U);  t[13] = ' '; t[14] = ' '; t[15] = 'X'; t[16] = 'Y'; t[17] = ' ';
                fmt_num(&t[18], (uint16_t)x, 3U);  t[21] = ' ';
                fmt_num(&t[22], (uint16_t)y, 3U);
                tft_text(8, 130, t, COL_WARN, COL_BG, 1U);
            }
        }
#endif

        if (!down) {                            /* a new press, not a hold */
            down = 1U;
            for (uint8_t i = 0U; i < N_MACROS; i++) {
                const Macro_t *m = &macros[i];
                if (x >= m->x && x < m->x + m->w && y >= m->y && y < m->y + m->h) {
                    active = (int8_t)i;
                    draw_button(i, 1U);
                    if (m->app >= 0)  { send_launch((uint8_t)m->app); }
                    else if (send_fn) { send_fn(m->mod, m->key); }
                    break;
                }
            }
        }
    } else if (down && (now - last_touch) > 40U) {   /* released for 40 ms */
        down = 0U;
        if (active >= 0) { draw_button((uint8_t)active, 0U); active = -1; }
    }
}
