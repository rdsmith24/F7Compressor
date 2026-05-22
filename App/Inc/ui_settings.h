#ifndef UI_SETTINGS_H_
#define UI_SETTINGS_H_

#include <stdint.h>
#include <stdbool.h>

/* Copy live settings into the local edit buffer.
   Must be called each time the screen is entered, before UI_Settings_Draw(). */
void UI_Settings_Enter(void);

/* Redraw the settings screen. Pass full_redraw=true on entry;
   incremental calls are no-ops (rows redraw individually on each tap). */
void UI_Settings_Draw(bool full_redraw);

/* Process a touch event. Bottom bar: CANCEL discards / SAVE commits to flash.
   Header tap: back to main. Row area: [-]/[+] adjust and immediately redraw. */
void UI_Settings_Touch(uint16_t x, uint16_t y);

#endif /* UI_SETTINGS_H_ */
