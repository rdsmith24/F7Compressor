#include "xpt2046_spi.h"
#include "main.h"
#include <string.h>

/* Commands for 12-bit differential mode, power-down between conversions */
#define CMD_READ_X   0xD0u
#define CMD_READ_Y   0x90u
#define SAMPLES      8u

static volatile bool s_irq_flag = false;

/* ── SPI helpers ──────────────────────────────────────────────────────────── */

/* Assert TP_CS (active LOW) to begin a transaction */
static void cs_low(void)
{
    LL_GPIO_ResetOutputPin(TP_CS_GPIO_Port, TP_CS_Pin);
}

/* De-assert TP_CS to end a transaction */
static void cs_high(void)
{
    LL_GPIO_SetOutputPin(TP_CS_GPIO_Port, TP_CS_Pin);
}

/*
 * Exchange len bytes over SPI5 (polling, LL driver).
 * tx may be NULL to send dummy bytes; rx may be NULL to discard received bytes.
 * Follows the STM32H7 LL pattern: SetTransferSize → Enable → StartMasterTransfer
 * → byte loop → wait EOT → clear flags → Disable.
 */
static void spi_xfer(const uint8_t *tx, uint8_t *rx, uint16_t len)
{
    LL_SPI_SetTransferSize(SPI5, len);
    LL_SPI_Enable(SPI5);
    LL_SPI_StartMasterTransfer(SPI5);

    for (uint16_t i = 0; i < len; i++) {
        while (!LL_SPI_IsActiveFlag_TXP(SPI5));
        LL_SPI_TransmitData8(SPI5, tx ? tx[i] : 0x00u);
        while (!LL_SPI_IsActiveFlag_RXP(SPI5));
        uint8_t d = LL_SPI_ReceiveData8(SPI5);
        if (rx) rx[i] = d;
    }

    while (!LL_SPI_IsActiveFlag_EOT(SPI5));
    LL_SPI_ClearFlag_EOT(SPI5);
    LL_SPI_ClearFlag_TXTF(SPI5);
    LL_SPI_Disable(SPI5);
}

/*
 * Send a 3-byte read command to the XPT2046 and return the 12-bit result.
 * CS must already be asserted by the caller.
 * Bit layout: rx[1][6:0] = bits 11:5, rx[2][7:3] = bits 4:0.
 */
static uint16_t read_channel(uint8_t cmd)
{
    uint8_t tx[3] = { cmd, 0x00u, 0x00u };
    uint8_t rx[3];
    spi_xfer(tx, rx, 3);
    return (uint16_t)(((rx[1] & 0x7Fu) << 5) | (rx[2] >> 3));
}

/* ── Public API ───────────────────────────────────────────────────────────── */

/* Initialise touch driver — de-asserts CS ready for the first transaction */
void XPT2046_Init(void)
{
    cs_high();
}

/*
 * Sample the touch panel and populate touch->x, touch->y, touch->pressed.
 *
 * Returns false (touch->pressed = false) when TP_IRQ is HIGH (no touch).
 * When touched: takes SAMPLES readings per axis, averages them, clamps to the
 * calibrated range, scales to screen pixels, then applies XPT_SWAP_XY /
 * XPT_FLIP_X / XPT_FLIP_Y orientation corrections.
 *
 * Call from the main loop; do not call from an ISR (SPI transfer is polled).
 */
bool XPT2046_Read(Touch_t *touch)
{
    /* TP_IRQ is active-low: high = not touched */
    if (LL_GPIO_IsInputPinSet(TP_IRQ_GPIO_Port, TP_IRQ_Pin) != 0) {
        touch->pressed = false;
        s_irq_flag = false;
        return false;
    }

    /* Average SAMPLES readings per axis to reduce noise */
    uint32_t sum_x = 0, sum_y = 0;
    cs_low();
    for (uint8_t i = 0; i < SAMPLES; i++) {
        sum_x += read_channel(CMD_READ_X);
        sum_y += read_channel(CMD_READ_Y);
    }
    cs_high();

    uint16_t raw_x = (uint16_t)(sum_x / SAMPLES);
    uint16_t raw_y = (uint16_t)(sum_y / SAMPLES);

    /* Clamp to calibrated range before scaling */
    if (raw_x < XPT_CAL_X_MIN) raw_x = XPT_CAL_X_MIN;
    if (raw_x > XPT_CAL_X_MAX) raw_x = XPT_CAL_X_MAX;
    if (raw_y < XPT_CAL_Y_MIN) raw_y = XPT_CAL_Y_MIN;
    if (raw_y > XPT_CAL_Y_MAX) raw_y = XPT_CAL_Y_MAX;

    uint16_t px = (uint16_t)((raw_x - XPT_CAL_X_MIN) * 320u / (XPT_CAL_X_MAX - XPT_CAL_X_MIN));
    uint16_t py = (uint16_t)((raw_y - XPT_CAL_Y_MIN) * 240u / (XPT_CAL_Y_MAX - XPT_CAL_Y_MIN));

#if XPT_FLIP_X
    px = 319u - px;
#endif
#if XPT_FLIP_Y
    py = 239u - py;
#endif

#if XPT_SWAP_XY
    touch->x = py;
    touch->y = px;
#else
    touch->x = px;
    touch->y = py;
#endif

    touch->pressed = true;
    s_irq_flag = false;
    return true;
}

/*
 * Called from EXTI3_IRQHandler — falling edge on TP_IRQ signals a touch.
 * Sets a flag only; the actual SPI read happens in the main loop via
 * XPT2046_Read() to avoid blocking inside an ISR.
 */
void XPT2046_IRQHandler(void)
{
    s_irq_flag = true;
}
