#ifndef UI_SETTINGS_H_
#define UI_SETTINGS_H_

#include <stdint.h>
#include <stdbool.h>

void UI_Settings_Draw(bool full_redraw);
void UI_Settings_Touch(uint16_t x, uint16_t y);
void UI_Settings_Enter(void);   /* call before showing the screen */

#endif /* UI_SETTINGS_H_ */
