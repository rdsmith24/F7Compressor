#include "digital_in.h"
#include "main.h"

/*
 * PC0  OIL_SW  — NC contact to GND, pull-up.  LOW = pressure OK, HIGH = fault.
 * PC2  ESTOP   — NC contact to GND, pull-up.  LOW = normal,      HIGH = tripped.
 *
 * EXTI0 fires on both edges (oil switch state change).
 * EXTI2 fires on falling edge (e-stop contact re-closes = system reset allowed).
 */

static volatile bool s_oil_ok       = true;
static volatile bool s_estop_active = false;
static volatile bool s_estop_reset  = false;

/*
 * Latch initial pin states at startup so the state machine sees correct
 * values before the first edge interrupt fires.
 */
void DigIn_Init(void)
{
    s_oil_ok       = (LL_GPIO_IsInputPinSet(OIL_SW_GPIO_Port,  OIL_SW_Pin)  == 0);
    s_estop_active = (LL_GPIO_IsInputPinSet(ESTOP_GPIO_Port,   ESTOP_Pin)   != 0);
    s_estop_reset  = false;
}

/* Return true if the oil pressure switch NC contact is closed (oil pressure OK).
   Updated in real-time by DigIn_OilSW_IRQHandler(). */
bool DigIn_IsOilPressureOK(void)
{
    return s_oil_ok;
}

/* Return true if the e-stop NC contact is open (e-stop tripped).
   Updated in real-time by DigIn_EStop_IRQHandler(). */
bool DigIn_IsEStopActive(void)
{
    return s_estop_active;
}

/*
 * Returns true exactly once after the e-stop NC contact re-closes.
 * The flag is consumed on read; subsequent calls return false until
 * the next falling edge on PC2.
 */
bool DigIn_ConsumeEStopReset(void)
{
    if (!s_estop_reset) return false;
    s_estop_reset = false;
    return true;
}

/*
 * Called from EXTI0_IRQHandler — oil pressure switch changed state.
 * Samples the pin directly so the reading is correct regardless of
 * which edge triggered the interrupt.
 */
void DigIn_OilSW_IRQHandler(void)
{
    s_oil_ok = (LL_GPIO_IsInputPinSet(OIL_SW_GPIO_Port, OIL_SW_Pin) == 0);
}

/*
 * Called from EXTI2_IRQHandler — falling edge on PC2 (e-stop contact closes).
 * Samples pin to capture current state; sets the reset flag when the
 * contact re-closes so the SM can permit a controlled restart.
 */
void DigIn_EStop_IRQHandler(void)
{
    s_estop_active = (LL_GPIO_IsInputPinSet(ESTOP_GPIO_Port, ESTOP_Pin) != 0);
    if (!s_estop_active) {
        s_estop_reset = true;
    }
}
