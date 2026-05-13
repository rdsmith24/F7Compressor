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

void DigIn_Init(void)
{
    /* Read initial pin states */
    s_oil_ok       = (LL_GPIO_IsInputPinSet(OIL_SW_GPIO_Port,  OIL_SW_Pin)  == 0);
    s_estop_active = (LL_GPIO_IsInputPinSet(ESTOP_GPIO_Port,   ESTOP_Pin)   != 0);
    s_estop_reset  = false;
}

bool DigIn_IsOilPressureOK(void)
{
    return s_oil_ok;
}

bool DigIn_IsEStopActive(void)
{
    return s_estop_active;
}

bool DigIn_ConsumeEStopReset(void)
{
    if (!s_estop_reset) return false;
    s_estop_reset = false;
    return true;
}

/* Called from EXTI0_IRQHandler — oil pressure switch changed state */
void DigIn_OilSW_IRQHandler(void)
{
    s_oil_ok = (LL_GPIO_IsInputPinSet(OIL_SW_GPIO_Port, OIL_SW_Pin) == 0);
}

/* Called from EXTI2_IRQHandler — e-stop NC contact closed (falling edge on PC2) */
void DigIn_EStop_IRQHandler(void)
{
    s_estop_active = (LL_GPIO_IsInputPinSet(ESTOP_GPIO_Port, ESTOP_Pin) != 0);
    if (!s_estop_active) {
        /* Falling edge: contact closed, e-stop physically reset */
        s_estop_reset = true;
    }
}
