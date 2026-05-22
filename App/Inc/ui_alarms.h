#ifndef UI_ALARMS_H_
#define UI_ALARMS_H_

#include <stdint.h>
#include <stdbool.h>

/* Redraw the alarms screen. Pass full_redraw=true on entry;
   alarm rows are refreshed on every call to keep colours current. */
void UI_Alarms_Draw(bool full_redraw);

/* Process a touch event. Header tap: back to main.
   Bottom bar tap: acknowledge all active alarms. */
void UI_Alarms_Touch(uint16_t x, uint16_t y);

#endif /* UI_ALARMS_H_ */
