#include "pressure_adc.h"
#include "main.h"

static uint16_t s_psi   = 0;
static bool     s_fault = false;

void PADC_Init(void)
{
    /* Calibrate (must be done before enabling the ADC) */
    LL_ADC_StartCalibration(ADC1, LL_ADC_CALIB_OFFSET, LL_ADC_SINGLE_ENDED);
    while (LL_ADC_IsCalibrationOnGoing(ADC1));

    HAL_Delay(1);

    LL_ADC_Enable(ADC1);
    while (!LL_ADC_IsActiveFlag_ADRDY(ADC1));
}

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

    /* Sensor fault: reading outside 4-20mA window with 5% margin */
    uint16_t low  = (uint16_t)(PADC_ADC_4MA  - (PADC_ADC_4MA  / 20u));
    uint16_t high = (uint16_t)(PADC_ADC_20MA + (PADC_ADC_20MA / 20u));

    if (raw < low || raw > high) {
        s_fault = true;
        return false;
    }

    s_fault = false;

    if (raw < PADC_ADC_4MA)  raw = PADC_ADC_4MA;
    if (raw > PADC_ADC_20MA) raw = PADC_ADC_20MA;

    s_psi = (uint16_t)((uint32_t)(raw - PADC_ADC_4MA) * PADC_SENSOR_RANGE_PSI
                        / (PADC_ADC_20MA - PADC_ADC_4MA));
    return true;
}

uint16_t PADC_GetPSI(void)
{
    return s_psi;
}

bool PADC_IsFault(void)
{
    return s_fault;
}
