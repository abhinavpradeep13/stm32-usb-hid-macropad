#include "tft.h"

/* =====================================================================
 *  5x7 font, ASCII 0x20 (space) .. 0x5F ('_').  Lower-case letters are
 *  drawn as capitals.  Each glyph is 5 columns; bit 0 of a column byte
 *  is the TOP pixel row.
 * ===================================================================== */
static const uint8_t font5x7[] = {
    0x00,0x00,0x00,0x00,0x00, /* 20 ' ' */
    0x00,0x00,0x5F,0x00,0x00, /* 21 '!' */
    0x00,0x07,0x00,0x07,0x00, /* 22 '"' */
    0x14,0x7F,0x14,0x7F,0x14, /* 23 '#' */
    0x24,0x2A,0x7F,0x2A,0x12, /* 24 '$' */
    0x23,0x13,0x08,0x64,0x62, /* 25 '%' */
    0x36,0x49,0x56,0x20,0x50, /* 26 '&' */
    0x00,0x08,0x07,0x03,0x00, /* 27 ''' */
    0x00,0x1C,0x22,0x41,0x00, /* 28 '(' */
    0x00,0x41,0x22,0x1C,0x00, /* 29 ')' */
    0x2A,0x1C,0x7F,0x1C,0x2A, /* 2A '*' */
    0x08,0x08,0x3E,0x08,0x08, /* 2B '+' */
    0x00,0x80,0x70,0x30,0x00, /* 2C ',' */
    0x08,0x08,0x08,0x08,0x08, /* 2D '-' */
    0x00,0x00,0x60,0x60,0x00, /* 2E '.' */
    0x20,0x10,0x08,0x04,0x02, /* 2F '/' */
    0x3E,0x51,0x49,0x45,0x3E, /* 30 '0' */
    0x00,0x42,0x7F,0x40,0x00, /* 31 '1' */
    0x72,0x49,0x49,0x49,0x46, /* 32 '2' */
    0x21,0x41,0x49,0x4D,0x33, /* 33 '3' */
    0x18,0x14,0x12,0x7F,0x10, /* 34 '4' */
    0x27,0x45,0x45,0x45,0x39, /* 35 '5' */
    0x3C,0x4A,0x49,0x49,0x31, /* 36 '6' */
    0x41,0x21,0x11,0x09,0x07, /* 37 '7' */
    0x36,0x49,0x49,0x49,0x36, /* 38 '8' */
    0x46,0x49,0x49,0x29,0x1E, /* 39 '9' */
    0x00,0x00,0x14,0x00,0x00, /* 3A ':' */
    0x00,0x40,0x34,0x00,0x00, /* 3B ';' */
    0x00,0x08,0x14,0x22,0x41, /* 3C '<' */
    0x14,0x14,0x14,0x14,0x14, /* 3D '=' */
    0x00,0x41,0x22,0x14,0x08, /* 3E '>' */
    0x02,0x01,0x59,0x09,0x06, /* 3F '?' */
    0x3E,0x41,0x5D,0x59,0x4E, /* 40 '@' */
    0x7C,0x12,0x11,0x12,0x7C, /* 41 'A' */
    0x7F,0x49,0x49,0x49,0x36, /* 42 'B' */
    0x3E,0x41,0x41,0x41,0x22, /* 43 'C' */
    0x7F,0x41,0x41,0x41,0x3E, /* 44 'D' */
    0x7F,0x49,0x49,0x49,0x41, /* 45 'E' */
    0x7F,0x09,0x09,0x09,0x01, /* 46 'F' */
    0x3E,0x41,0x41,0x51,0x73, /* 47 'G' */
    0x7F,0x08,0x08,0x08,0x7F, /* 48 'H' */
    0x00,0x41,0x7F,0x41,0x00, /* 49 'I' */
    0x20,0x40,0x41,0x3F,0x01, /* 4A 'J' */
    0x7F,0x08,0x14,0x22,0x41, /* 4B 'K' */
    0x7F,0x40,0x40,0x40,0x40, /* 4C 'L' */
    0x7F,0x02,0x1C,0x02,0x7F, /* 4D 'M' */
    0x7F,0x04,0x08,0x10,0x7F, /* 4E 'N' */
    0x3E,0x41,0x41,0x41,0x3E, /* 4F 'O' */
    0x7F,0x09,0x09,0x09,0x06, /* 50 'P' */
    0x3E,0x41,0x51,0x21,0x5E, /* 51 'Q' */
    0x7F,0x09,0x19,0x29,0x46, /* 52 'R' */
    0x26,0x49,0x49,0x49,0x32, /* 53 'S' */
    0x03,0x01,0x7F,0x01,0x03, /* 54 'T' */
    0x3F,0x40,0x40,0x40,0x3F, /* 55 'U' */
    0x1F,0x20,0x40,0x20,0x1F, /* 56 'V' */
    0x3F,0x40,0x38,0x40,0x3F, /* 57 'W' */
    0x63,0x14,0x08,0x14,0x63, /* 58 'X' */
    0x03,0x04,0x78,0x04,0x03, /* 59 'Y' */
    0x61,0x59,0x49,0x4D,0x43, /* 5A 'Z' */
    0x00,0x7F,0x41,0x41,0x41, /* 5B '[' */
    0x02,0x04,0x08,0x10,0x20, /* 5C '\' */
    0x41,0x41,0x41,0x7F,0x00, /* 5D ']' */
    0x04,0x02,0x01,0x02,0x04, /* 5E '^' */
    0x40,0x40,0x40,0x40,0x40  /* 5F '_' */
};
_Static_assert(sizeof(font5x7) == 64U * 5U, "font table must hold exactly 64 glyphs");

/* =====================================================================
 *  SPI1 : master, mode 0 (CPOL=0, CPHA=0), 8-bit, MSB first,
 *         software chip-select (SSM/SSI), so PA4 is just a normal GPIO.
 * ===================================================================== */
void spi1_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN;
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
    (void)RCC->APB2ENR;                         /* read back: lets the clock settle */

    pin_mode(GPIOA, 5U, PM_AF);  pin_af(GPIOA, 5U, 5U);  pin_speed_max(GPIOA, 5U);   /* SCK  */
    pin_mode(GPIOA, 6U, PM_AF);  pin_af(GPIOA, 6U, 5U);  pin_pull(GPIOA, 6U, PUD_UP);/* MISO */
    pin_mode(GPIOA, 7U, PM_AF);  pin_af(GPIOA, 7U, 5U);  pin_speed_max(GPIOA, 7U);   /* MOSI */

    SPI1->CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI | (SPI_BR_TFT << SPI_CR1_BR_Pos);
    SPI1->CR1 |= SPI_CR1_SPE;
}

/* Send a byte, do not wait for it to finish (used for pixel streaming). */
static inline void spi1_write(uint8_t b)
{
    while (!(SPI1->SR & SPI_SR_TXE)) { }
    *(volatile uint8_t *)&SPI1->DR = b;         /* byte access = 8-bit frame */
}

/* Wait until everything has really left the shift register, then throw
 * away the byte(s) that came back, so the next spi1_xfer() starts clean. */
void spi1_wait_idle(void)
{
    while (!(SPI1->SR & SPI_SR_TXE)) { }
    while (SPI1->SR & SPI_SR_BSY)    { }
    (void)SPI1->DR;                             /* empties RX, clears RXNE      */
    (void)SPI1->SR;                             /* DR then SR read clears OVR   */
}

uint8_t spi1_xfer(uint8_t b)
{
    while (!(SPI1->SR & SPI_SR_TXE))  { }
    *(volatile uint8_t *)&SPI1->DR = b;
    while (!(SPI1->SR & SPI_SR_RXNE)) { }
    return *(volatile uint8_t *)&SPI1->DR;
}

void spi1_set_speed(uint8_t br)
{
    spi1_wait_idle();
    SPI1->CR1 = (SPI1->CR1 & ~SPI_CR1_BR) | ((uint32_t)br << SPI_CR1_BR_Pos);
}

/* =====================================================================
 *  ILI9341 low level
 * ===================================================================== */
#define TFT_SELECT()    pin_low (TFT_CS_PORT, TFT_CS_PIN)
#define TFT_DESELECT()  pin_high(TFT_CS_PORT, TFT_CS_PIN)
#define TFT_DC_CMD()    pin_low (TFT_DC_PORT, TFT_DC_PIN)
#define TFT_DC_DATA()   pin_high(TFT_DC_PORT, TFT_DC_PIN)

/* One command byte (DC low). */
static void tft_cmd(uint8_t c)
{
    TFT_DC_CMD();
    TFT_SELECT();
    spi1_write(c);
    spi1_wait_idle();                           /* DC must not change mid-byte */
    TFT_DESELECT();
}

/* A command followed by n data bytes (DC high). */
static void tft_cmd_args(uint8_t c, const uint8_t *a, uint8_t n)
{
    TFT_DC_CMD();
    TFT_SELECT();
    spi1_write(c);
    spi1_wait_idle();
    TFT_DC_DATA();
    for (uint8_t i = 0U; i < n; i++) { spi1_write(a[i]); }
    spi1_wait_idle();
    TFT_DESELECT();
}

/* Choose the rectangle that the next pixels will fill, then open RAM write. */
static void tft_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint8_t a[4];
    a[0] = (uint8_t)(x0 >> 8);  a[1] = (uint8_t)x0;  a[2] = (uint8_t)(x1 >> 8);  a[3] = (uint8_t)x1;
    tft_cmd_args(0x2AU, a, 4U);                 /* column address set */
    a[0] = (uint8_t)(y0 >> 8);  a[1] = (uint8_t)y0;  a[2] = (uint8_t)(y1 >> 8);  a[3] = (uint8_t)y1;
    tft_cmd_args(0x2BU, a, 4U);                 /* page (row) address set */
    tft_cmd(0x2CU);                             /* memory write */
}

void tft_init(void)
{
    spi1_init();

    /* Chip select first set HIGH, then made an output: no glitch on the line. */
    pin_high(TFT_CS_PORT, TFT_CS_PIN);   pin_mode(TFT_CS_PORT, TFT_CS_PIN, PM_OUT);
    pin_mode(TFT_DC_PORT, TFT_DC_PIN, PM_OUT);   pin_speed_max(TFT_DC_PORT, TFT_DC_PIN);
    pin_high(TFT_RST_PORT, TFT_RST_PIN); pin_mode(TFT_RST_PORT, TFT_RST_PIN, PM_OUT);

    /* hardware reset */
    pin_high(TFT_RST_PORT, TFT_RST_PIN); HAL_Delay(10);
    pin_low (TFT_RST_PORT, TFT_RST_PIN); HAL_Delay(20);
    pin_high(TFT_RST_PORT, TFT_RST_PIN); HAL_Delay(150);

    static const uint8_t i_ef[] = {0x03, 0x80, 0x02};
    static const uint8_t i_cf[] = {0x00, 0xC1, 0x30};
    static const uint8_t i_ed[] = {0x64, 0x03, 0x12, 0x81};
    static const uint8_t i_e8[] = {0x85, 0x00, 0x78};
    static const uint8_t i_cb[] = {0x39, 0x2C, 0x00, 0x34, 0x02};
    static const uint8_t i_f7[] = {0x20};
    static const uint8_t i_ea[] = {0x00, 0x00};
    static const uint8_t i_c0[] = {0x23};
    static const uint8_t i_c1[] = {0x10};
    static const uint8_t i_c5[] = {0x3E, 0x28};
    static const uint8_t i_c7[] = {0x86};
    static const uint8_t i_36[] = {TFT_MADCTL};
    static const uint8_t i_3a[] = {0x55};                 /* 16 bits per pixel */
    static const uint8_t i_b1[] = {0x00, 0x18};
    static const uint8_t i_b6[] = {0x08, 0x82, 0x27};
    static const uint8_t i_f2[] = {0x00};
    static const uint8_t i_26[] = {0x01};
    static const uint8_t i_e0[] = {0x0F,0x31,0x2B,0x0C,0x0E,0x08,0x4E,0xF1,0x37,0x07,0x10,0x03,0x0E,0x09,0x00};
    static const uint8_t i_e1[] = {0x00,0x0E,0x14,0x03,0x11,0x07,0x31,0xC1,0x48,0x08,0x0F,0x0C,0x31,0x36,0x0F};

    tft_cmd_args(0xEF, i_ef, 3);  tft_cmd_args(0xCF, i_cf, 3);  tft_cmd_args(0xED, i_ed, 4);
    tft_cmd_args(0xE8, i_e8, 3);  tft_cmd_args(0xCB, i_cb, 5);  tft_cmd_args(0xF7, i_f7, 1);
    tft_cmd_args(0xEA, i_ea, 2);  tft_cmd_args(0xC0, i_c0, 1);  tft_cmd_args(0xC1, i_c1, 1);
    tft_cmd_args(0xC5, i_c5, 2);  tft_cmd_args(0xC7, i_c7, 1);  tft_cmd_args(0x36, i_36, 1);
    tft_cmd_args(0x3A, i_3a, 1);  tft_cmd_args(0xB1, i_b1, 2);  tft_cmd_args(0xB6, i_b6, 3);
    tft_cmd_args(0xF2, i_f2, 1);  tft_cmd_args(0x26, i_26, 1);
    tft_cmd_args(0xE0, i_e0, 15); tft_cmd_args(0xE1, i_e1, 15);
    tft_cmd(0x11);  HAL_Delay(150);             /* sleep out  */
    tft_cmd(0x29);  HAL_Delay(20);              /* display on */

    tft_fill_rect(0, 0, SCR_W, SCR_H, C_BLACK);
}

/* =====================================================================
 *  Drawing
 * ===================================================================== */
void tft_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    /* clip to the screen */
    if (x < 0) { w = (int16_t)(w + x); x = 0; }
    if (y < 0) { h = (int16_t)(h + y); y = 0; }
    if (x + w > SCR_W) { w = (int16_t)(SCR_W - x); }
    if (y + h > SCR_H) { h = (int16_t)(SCR_H - y); }
    if (w <= 0 || h <= 0) { return; }

    tft_window((uint16_t)x, (uint16_t)y, (uint16_t)(x + w - 1), (uint16_t)(y + h - 1));

    uint8_t  hi = (uint8_t)(color >> 8);
    uint8_t  lo = (uint8_t)color;
    uint32_t n  = (uint32_t)w * (uint32_t)h;

    TFT_DC_DATA();
    TFT_SELECT();
    while (n--) { spi1_write(hi); spi1_write(lo); }
    spi1_wait_idle();
    TFT_DESELECT();
}

/* Outline rectangle, t pixels thick, drawn inside the given box. */
void tft_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t t, uint16_t color)
{
    tft_fill_rect(x, y, w, t, color);                         /* top    */
    tft_fill_rect(x, (int16_t)(y + h - t), w, t, color);      /* bottom */
    tft_fill_rect(x, y, t, h, color);                         /* left   */
    tft_fill_rect((int16_t)(x + w - t), y, t, h, color);      /* right  */
}

/* One character, drawn with an opaque background so no clearing is needed.
 * Cell size is (6*scale) x (8*scale): 5 glyph columns + 1 gap, 7 rows + 1 gap. */
static void tft_char(int16_t x, int16_t y, char ch, uint16_t fg, uint16_t bg, uint8_t scale)
{
    if (ch >= 'a' && ch <= 'z') { ch = (char)(ch - 32); }
    if (ch < 32 || ch > 95)     { ch = '?'; }
    const uint8_t *g = &font5x7[((uint8_t)ch - 32U) * 5U];

    int16_t w = (int16_t)(6 * scale);
    int16_t h = (int16_t)(8 * scale);
    if (x < 0 || y < 0 || x + w > SCR_W || y + h > SCR_H) { return; }

    tft_window((uint16_t)x, (uint16_t)y, (uint16_t)(x + w - 1), (uint16_t)(y + h - 1));

    TFT_DC_DATA();
    TFT_SELECT();
    for (uint8_t row = 0U; row < 8U; row++) {
        for (uint8_t sy = 0U; sy < scale; sy++) {
            for (uint8_t col = 0U; col < 6U; col++) {
                uint16_t c = (col < 5U && ((g[col] >> row) & 1U)) ? fg : bg;
                uint8_t  hi = (uint8_t)(c >> 8);
                uint8_t  lo = (uint8_t)c;
                for (uint8_t sx = 0U; sx < scale; sx++) { spi1_write(hi); spi1_write(lo); }
            }
        }
    }
    spi1_wait_idle();
    TFT_DESELECT();
}

void tft_text(int16_t x, int16_t y, const char *s, uint16_t fg, uint16_t bg, uint8_t scale)
{
    while (*s) {
        tft_char(x, y, *s++, fg, bg, scale);
        x = (int16_t)(x + 6 * scale);
    }
}

uint16_t tft_text_width(const char *s, uint8_t scale)
{
    uint16_t n = 0U;
    while (*s++) { n++; }
    return (uint16_t)(n * 6U * scale);
}
