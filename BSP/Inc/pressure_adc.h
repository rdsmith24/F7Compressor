#ifndef PRESSURE_ADC_H_
#define PRESSURE_ADC_H_

#include <stdint.h>
#include <stdbool.h>

/* Sensor range: 4 mA = 0 PSI, 20 mA = 200 PSI through a 150 Ω shunt on PA3 */
#define PADC_SENSOR_RANGE_PSI   200u
/* 12-bit ADC counts corresponding to 0.6 V (4 mA) and 3.0 V (20 mA) */
#define PADC_ADC_4MA            744u
#define PADC_ADC_20MA          3724u

/* Calibrate and enable ADC1; call once at startup before PADC_Sample() */
void     PADC_Init(void);

/* Trigger one conversion and update the PSI reading.
   Returns false and sets the fault flag if the result is outside the
   4-20 mA window (±5%) or the conversion times out. Call at ~10 Hz. */
bool     PADC_Sample(void);

/* Return the most recent valid PSI reading (0 when a fault is active) */
uint16_t PADC_GetPSI(void);

/* Return true if the last sample was outside the sensor range or timed out */
bool     PADC_IsFault(void);

#endif /* PRESSURE_ADC_H_ */
