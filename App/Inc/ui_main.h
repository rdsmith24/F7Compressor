#ifndef UI_MAIN_H_
#define UI_MAIN_H_

#include <stdint.h>
#include <stdbool.h>

/* Redraw the main screen. Pass full_redraw=true on first entry or when
   returning from another screen; false for ~20 Hz incremental refresh. */
void UI_Main_Draw(bool full_redraw);

/* Process a touch event. Routes button-bar taps to START/STOP, RESET,
   ALARMS, or SETTINGS; ignores touches outside the button bar. */
void UI_Main_Touch(uint16_t x, uint16_t y);

#endif /* UI_MAIN_H_ */
