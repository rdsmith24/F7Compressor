#ifndef XPT2046_SPI_H_
#define XPT2046_SPI_H_

#include <stdint.h>
#include <stdbool.h>

/* ── Calibration constants ─────────────────────────────────────────────────
 * Adjust these after a physical touch test on first hardware bring-up.
 * X_MIN/MAX and Y_MIN/MAX are the raw 12-bit ADC values at the screen edges.
 * SWAP_XY, FLIP_X, FLIP_Y correct for panel orientation vs. display axes. */
#define XPT_CAL_X_MIN    200
#define XPT_CAL_X_MAX   3900
#define XPT_CAL_Y_MIN    200
#define XPT_CAL_Y_MAX   3900
#define XPT_SWAP_XY      1   /* 1 = raw X maps to screen Y (landscape layout) */
#define XPT_FLIP_X       0
#define XPT_FLIP_Y       1

typedef struct {
    uint16_t x;        /* screen pixel X (0–319) */
    uint16_t y;        /* screen pixel Y (0–239) */
    bool     pressed;  /* true when TP_IRQ is asserted */
} Touch_t;

/* Initialise touch driver — de-asserts CS, ready for first transaction */
void XPT2046_Init(void);

/* Sample the touch panel via SPI5 (polled). Populates *touch and returns true
   when touched; returns false immediately if TP_IRQ is not asserted.
   Call from the main loop only — not ISR-safe (blocking SPI transfer). */
bool XPT2046_Read(Touch_t *touch);

/* Called from EXTI3_IRQHandler on falling edge of TP_IRQ.
   Sets an internal flag; the actual SPI read is deferred to XPT2046_Read(). */
void XPT2046_IRQHandler(void);

#endif /* XPT2046_SPI_H_ */
