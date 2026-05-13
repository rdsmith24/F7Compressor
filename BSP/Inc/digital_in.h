#ifndef DIGITAL_IN_H_
#define DIGITAL_IN_H_

#include <stdbool.h>

void DigIn_Init(void);
bool DigIn_IsOilPressureOK(void);  /* true = NC contact closed, pressure adequate */
bool DigIn_IsEStopActive(void);    /* true = NC contact open, e-stop triggered    */
bool DigIn_ConsumeEStopReset(void);/* returns true once when e-stop resets         */

void DigIn_OilSW_IRQHandler(void);
void DigIn_EStop_IRQHandler(void);

#endif /* DIGITAL_IN_H_ */
