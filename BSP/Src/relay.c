#include "relay.h"
#include "main.h"

static RelayCmd_t s_current = RELAY_CMD_IDLE;

void Relay_Init(void)
{
    LL_GPIO_ResetOutputPin(RELAY_START_GPIO_Port, RELAY_START_Pin);
    s_current = RELAY_CMD_IDLE;
}

void Relay_Request(RelayCmd_t cmd)
{
    if (cmd == s_current) return;
    s_current = cmd;

    if (cmd == RELAY_CMD_START)
        LL_GPIO_SetOutputPin(RELAY_START_GPIO_Port, RELAY_START_Pin);
    else
        LL_GPIO_ResetOutputPin(RELAY_START_GPIO_Port, RELAY_START_Pin);
}

/* No-op retained for API compatibility — single relay needs no dead-time */
void Relay_Update(void) {}

RelayCmd_t Relay_GetState(void)
{
    return s_current;
}
