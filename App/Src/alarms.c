#include "alarms.h"

static uint8_t s_active = ALARM_NONE;   /* bitmask of currently active alarms */
static uint8_t s_acked  = ALARM_NONE;   /* subset of s_active that the operator has acknowledged */

/* Clear all active and acknowledged alarm bits at startup */
void Alarms_Init(void)
{
    s_active = ALARM_NONE;
    s_acked  = ALARM_NONE;
}

/*
 * Assert one or more alarm bits (OR the bits into s_active).
 * Newly set alarms are automatically unacknowledged so the UI
 * indicator activates immediately.
 */
void Alarms_Set(uint8_t bits)
{
    s_active |= bits;
    s_acked  &= ~bits;   /* strip ack for any newly raised alarm */
}

/* Clear one or more alarm bits from both the active and acknowledged masks */
void Alarms_Clear(uint8_t bits)
{
    s_active &= ~bits;
    s_acked  &= ~bits;
}

/* Return the bitmask of all currently active alarms */
uint8_t Alarms_Get(void)
{
    return s_active;
}

/*
 * Mark one or more active alarms as acknowledged (operator has seen them).
 * Acknowledged alarms still show on the alarms screen but no longer
 * trigger the main-screen banner. Has no effect on bits not currently active.
 */
void Alarms_Acknowledge(uint8_t bits)
{
    s_acked |= (bits & s_active);
}

/*
 * Return the bitmask of alarms that are active but not yet acknowledged.
 * Used by the UI to decide whether to show the alarm banner on the main screen.
 */
uint8_t Alarms_GetUnacknowledged(void)
{
    return s_active & ~s_acked;
}

/* Return true if any alarm bit is currently active */
bool Alarms_AnyActive(void)
{
    return s_active != ALARM_NONE;
}
