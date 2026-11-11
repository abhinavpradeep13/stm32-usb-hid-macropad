#ifndef UI_H
#define UI_H

#include <stdint.h>

/* Called when a macro button is tapped: modifier byte + HID keycode. */
typedef void (*ui_send_fn)(uint8_t mod, uint8_t key);

/* Initialises the display and touch, draws the whole screen. */
void ui_init(ui_send_fn send);

/* Pass in the latest values from the PC.  0xFF means "not available" and is
 * drawn as "--".  link = 1 while packets are arriving.  Only what changed is
 * redrawn, so calling this every loop pass is cheap. */
void ui_update_stats(uint8_t cpu, uint8_t ram, uint8_t bat, uint8_t temp,
                     uint8_t seq, uint8_t link);

/* Reads the touch panel; on a new tap of a button it calls the send function.
 * Call it every pass of the main loop. */
void ui_poll(void);

#endif /* UI_H */
