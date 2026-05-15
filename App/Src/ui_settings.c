/*
 * Settings UI screen — 320×240 landscape
 *
 * Shows five adjustable parameters with [-] [+] buttons.
 * Changes are staged locally; SAVE commits to flash; CANCEL discards.
 */
#include "ui_settings.h"
#include "ili9341.h"
#include "settings.h"

extern void UI_ShowMain(void);

/* Staged copy edited in this screen */
static Settings_t s_edit;

/* --- Layout ---------------------------------------------------------------- */
#define HDR_H    32u
#define ROW_H    24u
#define ROW_Y(n) (HDR_H + (n) * ROW_H)
#define VAL_X   180u
#define BTN_W    36u
#define MINUS_X (VAL_X + 40u)
#define PLUS_X  (VAL_X + 80u)
#define BTN_Y_BOTTOM  200u   /* HDR_H + NUM_ROWS * ROW_H = 32 + 7*24 = 200 */

/* Row indices */
#define ROW_OIL_DELAY  0u
#define ROW_MIN_PSI    1u
#define ROW_MAX_PSI    2u
#define ROW_SPAN_PSI   3u
#define ROW_AUTO       4u
#define ROW_HI_ALARM   5u
#define ROW_HI_HI      6u
#define NUM_ROWS       7u

static const char *row_labels[NUM_ROWS] = {
    "Oil delay (s)",
    "Min pressure (PSI)",
    "Max pressure (PSI)",
    "Hysteresis (PSI)",
    "Auto-restart",
    "High alarm (PSI)",
    "HiHi stop (PSI)",
};

static uint16_t *row_field(uint8_t row)
{
    switch (row) {
    case ROW_OIL_DELAY: return &s_edit.oil_pressure_delay_s;
    case ROW_MIN_PSI:   return &s_edit.pressure_min_psi;
    case ROW_MAX_PSI:   return &s_edit.pressure_max_psi;
    case ROW_SPAN_PSI:  return &s_edit.pressure_span_psi;
    case ROW_AUTO:      return &s_edit.auto_restart_en;
    case ROW_HI_ALARM:  return &s_edit.pressure_high_alarm_psi;
    case ROW_HI_HI:     return &s_edit.pressure_high_high_psi;
    default:            return NULL;
    }
}

static uint16_t row_min(uint8_t row)
{
    switch (row) {
    case ROW_OIL_DELAY: return 1u;
    case ROW_MIN_PSI:   return 0u;
    case ROW_MAX_PSI:   return 10u;
    case ROW_SPAN_PSI:  return 1u;
    case ROW_AUTO:      return 0u;
    case ROW_HI_ALARM:  return 10u;
    case ROW_HI_HI:     return 10u;
    default:            return 0u;
    }
}

static uint16_t row_max(uint8_t row)
{
    switch (row) {
    case ROW_OIL_DELAY: return 60u;
    case ROW_MIN_PSI:   return 150u;
    case ROW_MAX_PSI:   return 200u;
    case ROW_SPAN_PSI:  return 50u;
    case ROW_AUTO:      return 1u;
    case ROW_HI_ALARM:  return 220u;
    case ROW_HI_HI:     return 230u;
    default:            return 0xFFFFu;
    }
}

/* --- Drawing helpers ------------------------------------------------------- */

static void draw_row(uint8_t row)
{
    uint16_t y   = ROW_Y(row);
    uint16_t val = *row_field(row);

    lcdFillRect(0, (int16_t)y, 320, ROW_H - 1, COLOR_BLACK);
    lcdSetTextFont(&Font12);
    lcdSetTextColor(COLOR_WHITE, COLOR_BLACK);
    lcdSetCursor(4, y + 8);
    lcdPrintf("%s", row_labels[row]);

    /* Value */
    lcdSetCursor(VAL_X, y + 8);
    if (row == ROW_AUTO) {
        lcdSetTextColor(val ? COLOR_GREEN : COLOR_RED, COLOR_BLACK);
        lcdPrintf(val ? " ON " : "OFF ");
    } else {
        lcdSetTextColor(COLOR_YELLOW, COLOR_BLACK);
        lcdPrintf("%3u ", (unsigned)val);
    }

    /* [-] button */
    lcdFillRect((int16_t)MINUS_X, (int16_t)(y + 3), BTN_W, ROW_H - 7, COLOR_GRAY_50);
    lcdSetTextColor(COLOR_WHITE, COLOR_GRAY_50);
    lcdSetCursor(MINUS_X + 11, y + 9);
    lcdPrintf("-");

    /* [+] button */
    lcdFillRect((int16_t)PLUS_X, (int16_t)(y + 3), BTN_W, ROW_H - 7, COLOR_GRAY_50);
    lcdSetTextColor(COLOR_WHITE, COLOR_GRAY_50);
    lcdSetCursor(PLUS_X + 11, y + 9);
    lcdPrintf("+");
}

void UI_Settings_Enter(void)
{
    s_edit = *Settings_Get();
}

void UI_Settings_Draw(bool full_redraw)
{
    if (full_redraw) {
        lcdFillRGB(COLOR_BLACK);

        /* Header */
        lcdFillRect(0, 0, 320, HDR_H, COLOR_NAVY);
        lcdSetTextFont(&Font16);
        lcdSetTextColor(COLOR_WHITE, COLOR_NAVY);
        lcdSetCursor(4, 8);
        lcdPrintf("< SETTINGS");

        /* Bottom buttons */
        lcdFillRect(0,   BTN_Y_BOTTOM, 150, 32, COLOR_MAROON);
        lcdFillRect(170, BTN_Y_BOTTOM, 150, 32, COLOR_DARKGREEN);
        lcdSetTextFont(&Font16);
        lcdSetTextColor(COLOR_WHITE, COLOR_MAROON);
        lcdSetCursor(50, BTN_Y_BOTTOM + 8);
        lcdPrintf("CANCEL");
        lcdSetTextColor(COLOR_WHITE, COLOR_DARKGREEN);
        lcdSetCursor(220, BTN_Y_BOTTOM + 8);
        lcdPrintf("SAVE");

        for (uint8_t i = 0; i < NUM_ROWS; i++) {
            draw_row(i);
        }
    }
}

void UI_Settings_Touch(uint16_t x, uint16_t y)
{
    /* Bottom buttons */
    if (y >= BTN_Y_BOTTOM) {
        if (x < 160u) {
            /* CANCEL */
            UI_ShowMain();
        } else {
            /* SAVE */
            Settings_Save(&s_edit);
            UI_ShowMain();
        }
        return;
    }

    /* Back tap on header */
    if (y < HDR_H) {
        UI_ShowMain();
        return;
    }

    /* Row buttons */
    uint8_t row = (uint8_t)((y - HDR_H) / ROW_H);
    if (row >= NUM_ROWS) return;

    uint16_t *field = row_field(row);
    if (!field) return;

    if (x >= PLUS_X && x < PLUS_X + BTN_W) {
        if (*field < row_max(row)) (*field)++;
    } else if (x >= MINUS_X && x < MINUS_X + BTN_W) {
        if (*field > row_min(row)) (*field)--;
    }

    draw_row(row);
}
