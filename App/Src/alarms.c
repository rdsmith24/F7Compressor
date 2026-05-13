#include "alarms.h"

static uint8_t s_active = ALARM_NONE;
static uint8_t s_acked  = ALARM_NONE;

void Alarms_Init(void)
{
    s_active = ALARM_NONE;
    s_acked  = ALARM_NONE;
}

void Alarms_Set(uint8_t bits)
{
    s_active |= bits;
    /* Newly set alarms are automatically un-acknowledged */
    s_acked &= ~bits;
}

void Alarms_Clear(uint8_t bits)
{
    s_active &= ~bits;
    s_acked  &= ~bits;
}

uint8_t Alarms_Get(void)
{
    return s_active;
}

void Alarms_Acknowledge(uint8_t bits)
{
    s_acked |= (bits & s_active);
}

uint8_t Alarms_GetUnacknowledged(void)
{
    return s_active & ~s_acked;
}

bool Alarms_AnyActive(void)
{
    return s_active != ALARM_NONE;
}
