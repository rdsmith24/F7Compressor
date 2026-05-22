/*
 * Main UI screen — 320×240 landscape
 *
 * Layout:
 *   Y   0- 31  Header bar: state label + PSI value
 *   Y  32-199  Centre area: big PSI number + alarm indicator
 *   Y 200-239  Button bar: START/STOP | RESET | ALARMS | SETTINGS
 */
#include "ui_main.h"
#include "ili9341.h"
#include "compressor_sm.h"
#include "pressure_adc.h"
#include "alarms.h"
#include "settings.h"

/* Screen routing — defined in main.c, declared extern in each ui_*.c */
extern void UI_ShowSettings(void);
extern void UI_ShowAlarms(void);

/* Button regions (x, y, w, h) */
#define BTN_Y       200u
#define BTN_H        40u
#define BTN_START_X   0u
#define BTN_START_W  80u
#define BTN_RESET_X  80u
#define BTN_RESET_W  80u
#define BTN_ALARMS_X 160u
#define BTN_ALARMS_W 80u
#define BTN_SETTINGS_X 240u
#define BTN_SETTINGS_W 80u

/* Map a compressor state to its header bar background colour */
static uint16_t state_color(SmState_t st)
{
    switch (st) {
    case SM_RUNNING:  return COLOR_GREEN;
    case SM_STARTING: return COLOR_YELLOW;
    case SM_STOPPING: return COLOR_CYAN;
    case SM_FAULT:    return COLOR_RED;
    case SM_ESTOP:    return COLOR_MAGENTA;
    default:          return COLOR_DARKGREY;
    }
}

/* Draw a single button: filled rectangle with a white outline and centred label */
static void draw_button(uint16_t x, uint16_t w, const char *label, uint16_t bg, uint16_t fg)
{
    lcdFillRect((int16_t)x + 2, BTN_Y + 2, (int16_t)w - 4, BTN_H - 4, bg);
    lcdDrawRect((int16_t)x,     BTN_Y,     (int16_t)w,     BTN_H,     COLOR_WHITE);

    /* Centre text in button */
    uint8_t char_w = lcdGetTextFont()->Width;
    uint16_t text_w = 0;
    for (const char *p = label; *p; p++) text_w += char_w;
    uint16_t tx = x + (w - text_w) / 2u;
    uint16_t ty = BTN_Y + (BTN_H - lcdGetTextFont()->Height) / 2u;
    lcdSetCursor(tx, ty);
    lcdSetTextColor(fg, bg);
    lcdPrintf("%s", label);
}

/*
 * Redraw the main screen.
 * Pass full_redraw=true on first entry or when returning from another screen;
 * pass false for the ~20 Hz incremental refresh (redraws only dynamic areas).
 *
 * Dynamic areas refreshed every call regardless of full_redraw:
 *   - Header bar colour + state name + PSI value
 *   - Centre area: large PSI number or SENSOR FAULT, cut-in/cut-out setpoints
 *   - Alarm banner (left 240px of status strip)
 *   - Oil pressure indicator (right 80px of status strip)
 *   - START/STOP button label and colour
 */
void UI_Main_Draw(bool full_redraw)
{
    SmState_t  st  = SM_GetState();
    uint16_t   psi = PADC_GetPSI();    /* declared in pressure_adc.h, included via sm */
    uint8_t    alm = Alarms_GetUnacknowledged();
    uint16_t   hdr_color = state_color(st);

    if (full_redraw) {
        lcdFillRGB(COLOR_BLACK);

        /* Static button outlines */
        draw_button(BTN_START_X,    BTN_START_W,    "START",    COLOR_DARKGREEN, COLOR_WHITE);
        draw_button(BTN_RESET_X,    BTN_RESET_W,    "RESET",    COLOR_DARKGREY,  COLOR_WHITE);
        draw_button(BTN_ALARMS_X,   BTN_ALARMS_W,   "ALARMS",   COLOR_DARKGREY,  COLOR_WHITE);
        draw_button(BTN_SETTINGS_X, BTN_SETTINGS_W, "SETTINGS", COLOR_NAVY,      COLOR_WHITE);
    }

    /* Header bar */
    lcdFillRect(0, 0, 320, 32, hdr_color);
    lcdSetTextFont(&Font16);
    lcdSetCursor(4, 8);
    lcdSetTextColor(COLOR_BLACK, hdr_color);
    lcdPrintf("%-10s", SM_GetStateName());
    lcdSetCursor(230, 8);
    lcdPrintf("%3u PSI", (unsigned)psi);

    /* Big PSI number in centre */
    lcdFillRect(0, 32, 320, 168, COLOR_BLACK);
    lcdSetTextFont(&Font24);
    lcdSetCursor(80, 90);
    if (PADC_IsFault()) {
        lcdSetTextColor(COLOR_RED, COLOR_BLACK);
        lcdPrintf("SENSOR FAULT");
    } else {
        lcdSetTextColor(COLOR_WHITE, COLOR_BLACK);
        lcdPrintf("%3u", (unsigned)psi);
        lcdSetTextFont(&Font16);
        lcdSetCursor(180, 98);
        lcdSetTextColor(COLOR_LIGHTGREY, COLOR_BLACK);
        lcdPrintf("PSI");

        /* Cut-in / cut-out setpoints */
        const Settings_t *cfg = Settings_Get();
        uint16_t cut_in  = cfg->pressure_max_psi - cfg->pressure_span_psi;
        uint16_t cut_out = cfg->pressure_max_psi;
        lcdSetTextFont(&Font12);
        lcdSetCursor(50, 130);
        lcdSetTextColor(COLOR_CYAN, COLOR_BLACK);
        lcdPrintf("CUT-IN :%3u PSI", (unsigned)cut_in);
        lcdSetCursor(50, 148);
        lcdPrintf("CUT-OUT:%3u PSI", (unsigned)cut_out);
    }

    /* Alarm indicator — left 240px of the strip */
    if (alm) {
        lcdFillRect(0, 172, 240, 26, COLOR_RED);
        lcdSetTextFont(&Font16);
        lcdSetCursor(4, 179);
        lcdSetTextColor(COLOR_WHITE, COLOR_RED);
        lcdPrintf("! ALARM - tap ALARMS");
    } else {
        lcdFillRect(0, 172, 240, 26, COLOR_BLACK);
    }

    /* Oil pressure indicator — right 80px of the strip, above SETTINGS button */
    {
        bool oil_ok = SM_GetOilOk();
        const char *oil_lbl;
        uint16_t    oil_bg;
        uint16_t    oil_fg;

        if (oil_ok) {
            oil_lbl = "OIL OK";
            oil_bg  = COLOR_DARKGREEN;
            oil_fg  = COLOR_WHITE;
        } else if (st == SM_STARTING) {
            oil_lbl = "OIL WAIT";
            oil_bg  = COLOR_YELLOW;
            oil_fg  = COLOR_BLACK;
        } else {
            oil_lbl = "OIL LOW";
            oil_bg  = COLOR_RED;
            oil_fg  = COLOR_WHITE;
        }

        lcdFillRect(240, 172, 80, 26, oil_bg);
        lcdSetTextFont(&Font12);
        uint16_t cw = lcdGetTextFont()->Width;
        uint16_t ch = lcdGetTextFont()->Height;
        uint16_t tw = 0;
        for (const char *p = oil_lbl; *p; p++) tw += cw;
        uint16_t tx = 240u + (80u - tw) / 2u;
        uint16_t ty = 172u + (26u - ch)  / 2u;
        lcdSetCursor(tx, ty);
        lcdSetTextColor(oil_fg, oil_bg);
        lcdPrintf("%s", oil_lbl);
    }

    /* Refresh START button label based on state */
    const char *btn_label;
    uint16_t    btn_bg;
    if (st == SM_RUNNING || st == SM_STARTING) {
        btn_label = "STOP ";
        btn_bg    = COLOR_MAROON;
    } else {
        btn_label = "START";
        btn_bg    = COLOR_DARKGREEN;
    }
    draw_button(BTN_START_X, BTN_START_W, btn_label, btn_bg, COLOR_WHITE);
}

/*
 * Process a touch event on the main screen.
 * Ignores touches outside the button bar (Y 200–239).
 * Button regions (each 80px wide):
 *   X   0- 79  START (in IDLE/STOPPING) or STOP (in RUNNING/STARTING)
 *   X  80-159  RESET — sends reset request to the state machine
 *   X 160-239  ALARMS — navigate to alarms screen
 *   X 240-319  SETTINGS — navigate to settings screen
 */
void UI_Main_Touch(uint16_t x, uint16_t y)
{
    if (y < BTN_Y || y >= BTN_Y + BTN_H) return;

    SmState_t st = SM_GetState();

    if (x < BTN_RESET_X) {
        /* START / STOP */
        if (st == SM_IDLE || st == SM_STOPPING) {
            SM_RequestStart();
        } else if (st == SM_RUNNING || st == SM_STARTING) {
            SM_RequestReset();   /* acts as a stop → IDLE path in SM */
        }
    } else if (x < BTN_ALARMS_X) {
        SM_RequestReset();
    } else if (x < BTN_SETTINGS_X) {
        UI_ShowAlarms();
    } else {
        UI_ShowSettings();
    }
}
