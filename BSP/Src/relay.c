#include "relay.h"
#include "main.h"

static RelayCmd_t s_current = RELAY_CMD_IDLE;

/* Initialise relay driver — de-energises PG4 and resets internal state */
void Relay_Init(void)
{
    LL_GPIO_ResetOutputPin(RELAY_START_GPIO_Port, RELAY_START_Pin);
    s_current = RELAY_CMD_IDLE;
}

/*
 * Request a relay state change.
 * RELAY_CMD_START energises PG4 (compressor runs).
 * RELAY_CMD_STOP or RELAY_CMD_IDLE de-energises PG4 (compressor stops).
 * Repeated calls with the same command are no-ops.
 */
void Relay_Request(RelayCmd_t cmd)
{
    if (cmd == s_current) return;
    s_current = cmd;

    if (cmd == RELAY_CMD_START)
        LL_GPIO_SetOutputPin(RELAY_START_GPIO_Port, RELAY_START_Pin);
    else
        LL_GPIO_ResetOutputPin(RELAY_START_GPIO_Port, RELAY_START_Pin);
}

/* No-op — retained so the main-loop call site compiles without change.
   Single relay needs no dead-time sequencing. */
void Relay_Update(void) {}

/* Return the last command passed to Relay_Request() */
RelayCmd_t Relay_GetState(void)
{
    return s_current;
}
