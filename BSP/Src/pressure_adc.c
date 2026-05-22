#include "pressure_adc.h"
#include "main.h"

static uint16_t s_psi   = 0;
static bool     s_fault = false;

/*
 * Calibrate and enable ADC1. Must be called once at startup, before the
 * first PADC_Sample() call. Calibration must run before the ADC is enabled —
 * the LL driver requires this order.
 */
void PADC_Init(void)
{
    LL_ADC_StartCalibration(ADC1, LL_ADC_CALIB_OFFSET, LL_ADC_SINGLE_ENDED);
    while (LL_ADC_IsCalibrationOnGoing(ADC1));

    HAL_Delay(1);   /* wait for internal regulator after calibration */

    LL_ADC_Enable(ADC1);
    while (!LL_ADC_IsActiveFlag_ADRDY(ADC1));
}

/*
 * Trigger one software-started conversion and update the PSI reading.
 *
 * Returns true on success. Returns false and sets the fault flag if:
 *   - the conversion does not complete within 10 ms, or
 *   - the raw ADC count falls outside the 4-20 mA window (±5% margin).
 *
 * The fault flag clears automatically on the next successful conversion.
 * Call at ~10 Hz from the main loop.
 */
bool PADC_Sample(void)
{
    LL_ADC_REG_StartConversion(ADC1);

    uint32_t timeout = HAL_GetTick() + 10u;
    while (!LL_ADC_IsActiveFlag_EOC(ADC1)) {
        if (HAL_GetTick() > timeout) {
            s_fault = true;
            return false;
        }
    }

    uint16_t raw = LL_ADC_REG_ReadConversionData12(ADC1);
    LL_ADC_ClearFlag_EOC(ADC1);

    /* Reject readings outside the 4-20 mA window with a 5% margin */
    uint16_t low  = (uint16_t)(PADC_ADC_4MA  - (PADC_ADC_4MA  / 20u));
    uint16_t high = (uint16_t)(PADC_ADC_20MA + (PADC_ADC_20MA / 20u));

    if (raw < low || raw > high) {
        s_fault = true;
        return false;
    }

    s_fault = false;

    /* Clamp to calibrated range before scaling to PSI */
    if (raw < PADC_ADC_4MA)  raw = PADC_ADC_4MA;
    if (raw > PADC_ADC_20MA) raw = PADC_ADC_20MA;

    s_psi = (uint16_t)((uint32_t)(raw - PADC_ADC_4MA) * PADC_SENSOR_RANGE_PSI
                        / (PADC_ADC_20MA - PADC_ADC_4MA));
    return true;
}

/* Return the most recent valid PSI reading (0 if a fault is active) */
uint16_t PADC_GetPSI(void)
{
    return s_psi;
}

/*
 * Return true if the last conversion was outside the 4-20 mA window or
 * timed out. Cleared automatically by the next successful conversion.
 */
bool PADC_IsFault(void)
{
    return s_fault;
}
