#ifndef TFT_H
#define TFT_H

#include "main.h"
#include <stdint.h>

/* =====================================================================
 *  Black Pill (STM32F411CEU6)  -  ILI9341 TFT + XPT2046 touch
 *  ---------------------------------------------------------------------
 *  One SPI bus (SPI1, alternate function 5) is shared by both chips.
 *  Each chip has its own chip-select, so only one listens at a time.
 *
 *      SCK  = PA5      MISO = PA6      MOSI = PA7
 *      TFT   : CS = PA4    DC = PB0    RESET = PB1
 *      Touch : CS = PB10   IRQ = PA3
 * ===================================================================== */
#define TFT_CS_PORT   GPIOA
#define TFT_CS_PIN    4U
#define TFT_DC_PORT   GPIOB
#define TFT_DC_PIN    0U
#define TFT_RST_PORT  GPIOB
#define TFT_RST_PIN   1U
#define TS_CS_PORT    GPIOB
#define TS_CS_PIN     10U
#define TS_IRQ_PORT   GPIOA
#define TS_IRQ_PIN    3U

/* ---- screen: landscape 320 x 240 ---------------------------------- */
#define SCR_W         320
#define SCR_H         240

/* MADCTL (command 0x36) decides rotation / mirroring / RGB order.
 * 0x28 = landscape, BGR.  If the picture is mirrored or the colours are
 * swapped, try 0x68, 0xA8 or 0xE8 (one of the four is right). */
#define TFT_MADCTL    0x28U

/* ---- SPI speed: baud = APB2 clock / 2^(BR+1) ----------------------
 * Your clock tree gives APB2 = 96 MHz.
 *   BR = 2  ->  96 / 8  = 12 MHz   (ILI9341, comfortable)
 *   BR = 5  ->  96 / 64 = 1.5 MHz  (XPT2046 allows at most 2 MHz)   */
#define SPI_BR_TFT    2U
#define SPI_BR_TOUCH  5U

/* ---- RGB565 colours ------------------------------------------------ */
#define RGB565(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | (((b) & 0xFF) >> 3)))
#define C_BLACK   0x0000U
#define C_WHITE   0xFFFFU

/* ---- tiny register-level GPIO helpers ------------------------------ */
#define PM_IN       0U      /* MODER values */
#define PM_OUT      1U
#define PM_AF       2U
#define PUD_NONE    0U      /* PUPDR values */
#define PUD_UP      1U
#define PUD_DOWN    2U

static inline void pin_mode(GPIO_TypeDef *p, uint8_t n, uint32_t m)
{
    p->MODER = (p->MODER & ~(3U << (n * 2U))) | (m << (n * 2U));
}
static inline void pin_af(GPIO_TypeDef *p, uint8_t n, uint32_t af)
{
    uint32_t sh = (uint32_t)(n & 7U) * 4U;
    p->AFR[n >> 3] = (p->AFR[n >> 3] & ~(0xFU << sh)) | (af << sh);
}
static inline void pin_speed_max(GPIO_TypeDef *p, uint8_t n)
{
    p->OSPEEDR |= (3U << (n * 2U));
}
static inline void pin_pull(GPIO_TypeDef *p, uint8_t n, uint32_t pud)
{
    p->PUPDR = (p->PUPDR & ~(3U << (n * 2U))) | (pud << (n * 2U));
}
static inline void pin_high(GPIO_TypeDef *p, uint8_t n) { p->BSRR = (1U << n); }
static inline void pin_low(GPIO_TypeDef *p, uint8_t n)  { p->BSRR = (1U << (n + 16U)); }
static inline uint8_t pin_read(GPIO_TypeDef *p, uint8_t n) { return (uint8_t)((p->IDR >> n) & 1U); }

/* ---- SPI1 bus (shared with touch.c) -------------------------------- */
void    spi1_init(void);
void    spi1_set_speed(uint8_t br);      /* waits for the bus to be idle first */
void    spi1_wait_idle(void);
uint8_t spi1_xfer(uint8_t b);            /* send one byte, return the byte clocked in */

/* ---- display ------------------------------------------------------- */
void     tft_init(void);
void     tft_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void     tft_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t t, uint16_t color);
void     tft_text(int16_t x, int16_t y, const char *s, uint16_t fg, uint16_t bg, uint8_t scale);
uint16_t tft_text_width(const char *s, uint8_t scale);

#endif /* TFT_H */
