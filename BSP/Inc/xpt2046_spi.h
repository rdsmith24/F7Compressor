#ifndef XPT2046_SPI_H_
#define XPT2046_SPI_H_

#include <stdint.h>
#include <stdbool.h>

/* Calibration constants — adjust after physical touch test */
#define XPT_CAL_X_MIN    200
#define XPT_CAL_X_MAX   3900
#define XPT_CAL_Y_MIN    200
#define XPT_CAL_Y_MAX   3900
/* Set to 1 if raw X maps to screen Y and vice versa (landscape flip) */
#define XPT_SWAP_XY      1
#define XPT_FLIP_X       0
#define XPT_FLIP_Y       1

typedef struct {
    uint16_t x;
    uint16_t y;
    bool     pressed;
} Touch_t;

void XPT2046_Init(void);
bool XPT2046_Read(Touch_t *touch);
void XPT2046_IRQHandler(void);

#endif /* XPT2046_SPI_H_ */
