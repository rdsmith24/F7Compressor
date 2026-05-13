#ifndef UI_MAIN_H_
#define UI_MAIN_H_

#include <stdint.h>
#include <stdbool.h>

void UI_Main_Draw(bool full_redraw);
void UI_Main_Touch(uint16_t x, uint16_t y);

#endif /* UI_MAIN_H_ */
