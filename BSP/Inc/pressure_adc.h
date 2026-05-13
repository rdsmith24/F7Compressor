#ifndef PRESSURE_ADC_H_
#define PRESSURE_ADC_H_

#include <stdint.h>
#include <stdbool.h>

/* Sensor range: 4mA=0 PSI, 20mA=200 PSI through 150Ω shunt */
#define PADC_SENSOR_RANGE_PSI   200
/* ADC counts for 0.6V (4mA) and 3.0V (20mA) on 12-bit 3.3V reference */
#define PADC_ADC_4MA            744u
#define PADC_ADC_20MA          3724u

void     PADC_Init(void);
bool     PADC_Sample(void);
uint16_t PADC_GetPSI(void);
bool     PADC_IsFault(void);

#endif /* PRESSURE_ADC_H_ */
