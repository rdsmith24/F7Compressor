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
 * Oil-switch debounce state. OIL_SW is a mechanical contact: a single bounce
 * read as "not OK" while RUNNING would trip a hard FAULT. We poll the pin in
 * DigIn_Update() and only commit a change to s_oil_ok after the new level has
 * been stable for OIL_DEBOUNCE_MS.
 */
#define OIL_DEBOUNCE_MS   30u
static bool     s_oil_raw_last  = true;
static uint32_t s_oil_change_tick = 0;

/*
 * Latch initial pin states at startup so the state machine sees correct
 * values before the first edge interrupt fires.
 */
void DigIn_Init(void)
{
    s_oil_ok          = (LL_GPIO_IsInputPinSet(OIL_SW_GPIO_Port, OIL_SW_Pin) == 0);
    s_oil_raw_last    = s_oil_ok;
    s_oil_change_tick = HAL_GetTick();
    s_estop_active    = (LL_GPIO_IsInputPinSet(ESTOP_GPIO_Port, ESTOP_Pin) != 0);
    s_estop_reset     = false;
}

/*
 * Poll and debounce the oil-pressure switch. Call once per main-loop iteration.
 * This is the single writer of s_oil_ok — the EXTI handler no longer touches it,
 * so a contact bounce can't momentarily flip the flag and nuisance-trip a fault.
 */
void DigIn_Update(void)
{
    bool raw = (LL_GPIO_IsInputPinSet(OIL_SW_GPIO_Port, OIL_SW_Pin) == 0);

    if (raw != s_oil_raw_last) {
        /* Level just changed — restart the stability timer */
        s_oil_raw_last    = raw;
        s_oil_change_tick = HAL_GetTick();
    } else if (raw != s_oil_ok &&
               (HAL_GetTick() - s_oil_change_tick) >= OIL_DEBOUNCE_MS) {
        /* New level held stable long enough — commit it */
        s_oil_ok = raw;
    }
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
    /* Read-and-clear must be atomic vs. the EXTI2 setter, or a reset edge that
       lands between the test and the clear would be silently lost. */
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    bool ret = s_estop_reset;
    s_estop_reset = false;
    __set_PRIMASK(primask);
    return ret;
}

/*
 * Called from EXTI0_IRQHandler — oil pressure switch changed state.
 * Intentionally a no-op: s_oil_ok is owned by the debounced poll in
 * DigIn_Update(). The EXTI keeps firing (and the flag is cleared in the IT
 * handler) but we no longer act on the raw, un-debounced edge here.
 */
void DigIn_OilSW_IRQHandler(void)
{
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
