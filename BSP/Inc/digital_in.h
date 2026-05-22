#ifndef DIGITAL_IN_H_
#define DIGITAL_IN_H_

#include <stdbool.h>

/* Latch initial pin states; call once at startup before the main loop */
void DigIn_Init(void);

/* Return true if the oil pressure switch NC contact is closed (oil pressure OK) */
bool DigIn_IsOilPressureOK(void);

/* Return true if the e-stop NC contact is open (e-stop tripped) */
bool DigIn_IsEStopActive(void);

/* Return true exactly once after the e-stop NC contact re-closes (reset path).
   Consuming the flag clears it; returns false on all subsequent calls until the
   next falling edge on PC2. */
bool DigIn_ConsumeEStopReset(void);

/* Called from EXTI0_IRQHandler — oil switch state changed on PC0 */
void DigIn_OilSW_IRQHandler(void);

/* Called from EXTI2_IRQHandler — e-stop contact closed on PC2 (falling edge) */
void DigIn_EStop_IRQHandler(void);

#endif /* DIGITAL_IN_H_ */
