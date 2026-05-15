/*
 * Alarms UI screen — 320×240 landscape
 */
#include "ui_alarms.h"
#include "ili9341.h"
#include "alarms.h"

extern void UI_ShowMain(void);

#define HDR_H  32u
#define ROW_H  26u    /* 6 rows × 26 = 156; HDR_H + 156 = 188 = BTN_Y */
#define BTN_Y  188u

static void draw_alarm_row(uint8_t bit, uint16_t y, const char *label)
{
    bool active = (Alarms_Get() & bit) != 0;
    bool acked  = (Alarms_GetUnacknowledged() & bit) == 0;

    uint16_t bg = active ? (acked ? COLOR_DARKGREY : COLOR_MAROON) : COLOR_BLACK;
    lcdFillRect(0, (int16_t)y, 320, ROW_H - 2, bg);
    lcdSetTextFont(&Font16);
    lcdSetCursor(8, y + 5);
    if (active) {
        lcdSetTextColor(COLOR_WHITE, bg);
        lcdPrintf("%s %s", acked ? "  " : "! ", label);
    } else {
        lcdSetTextColor(COLOR_GRAY_50, COLOR_BLACK);
        lcdPrintf("   %s", label);
    }
}

void UI_Alarms_Draw(bool full_redraw)
{
    if (full_redraw) {
        lcdFillRGB(COLOR_BLACK);

        /* Header */
        lcdFillRect(0, 0, 320, HDR_H, COLOR_MAROON);
        lcdSetTextFont(&Font16);
        lcdSetTextColor(COLOR_WHITE, COLOR_MAROON);
        lcdSetCursor(4, 8);
        lcdPrintf("< ALARMS");

        /* Bottom: ACK ALL button */
        lcdFillRect(0, BTN_Y, 320, 40, COLOR_NAVY);
        lcdSetTextColor(COLOR_WHITE, COLOR_NAVY);
        lcdSetCursor(110, BTN_Y + 12);
        lcdPrintf("ACKNOWLEDGE ALL");
    }

    /* Alarm rows (refresh every draw) */
    draw_alarm_row(ALARM_OIL_TIMEOUT,    HDR_H + ROW_H * 0, "Oil pressure timeout");
    draw_alarm_row(ALARM_LOW_PRESSURE,   HDR_H + ROW_H * 1, "Low discharge pressure");
    draw_alarm_row(ALARM_PRESSURE_FAULT, HDR_H + ROW_H * 2, "Pressure sensor fault");
    draw_alarm_row(ALARM_HIGH_PRESSURE,  HDR_H + ROW_H * 3, "High pressure alarm");
    draw_alarm_row(ALARM_HIGH_HIGH_PRES, HDR_H + ROW_H * 4, "High-high shutdown");
    draw_alarm_row(ALARM_ESTOP,          HDR_H + ROW_H * 5, "Emergency stop");
}

void UI_Alarms_Touch(uint16_t x, uint16_t y)
{
    (void)x;
    if (y < HDR_H) {
        UI_ShowMain();
    } else if (y >= BTN_Y) {
        Alarms_Acknowledge(ALARM_OIL_TIMEOUT | ALARM_LOW_PRESSURE   |
                           ALARM_PRESSURE_FAULT | ALARM_HIGH_PRESSURE |
                           ALARM_HIGH_HIGH_PRES | ALARM_ESTOP);
    }
}
