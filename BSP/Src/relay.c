#include "relay.h"
#include "main.h"
#include <stdbool.h>

#define DEAD_TIME_MS  50u

typedef enum {
    RSTATE_IDLE,
    RSTATE_START,
    RSTATE_STOP,
    RSTATE_DEAD_TO_START,
    RSTATE_DEAD_TO_STOP,
} RState_t;

static RState_t   s_state   = RSTATE_IDLE;
static RelayCmd_t s_current = RELAY_CMD_IDLE;
static uint32_t   s_dead_tick = 0;

static void set_outputs(bool start, bool stop)
{
    if (start)
        LL_GPIO_SetOutputPin(RELAY_START_GPIO_Port, RELAY_START_Pin);
    else
        LL_GPIO_ResetOutputPin(RELAY_START_GPIO_Port, RELAY_START_Pin);

    if (stop)
        LL_GPIO_SetOutputPin(RELAY_STOP_GPIO_Port, RELAY_STOP_Pin);
    else
        LL_GPIO_ResetOutputPin(RELAY_STOP_GPIO_Port, RELAY_STOP_Pin);
}

void Relay_Init(void)
{
    set_outputs(false, false);
    s_state   = RSTATE_IDLE;
    s_current = RELAY_CMD_IDLE;
}

void Relay_Request(RelayCmd_t cmd)
{
    if (cmd == s_current) return;

    if (cmd == RELAY_CMD_IDLE) {
        /* Always safe to go idle immediately */
        set_outputs(false, false);
        s_state   = RSTATE_IDLE;
        s_current = RELAY_CMD_IDLE;
        return;
    }

    /* De-assert the currently active relay, start dead-time */
    set_outputs(false, false);
    s_dead_tick = HAL_GetTick();
    s_state   = (cmd == RELAY_CMD_START) ? RSTATE_DEAD_TO_START : RSTATE_DEAD_TO_STOP;
    s_current = cmd;
}

void Relay_Update(void)
{
    if (s_state != RSTATE_DEAD_TO_START && s_state != RSTATE_DEAD_TO_STOP) return;

    if (HAL_GetTick() - s_dead_tick < DEAD_TIME_MS) return;

    if (s_state == RSTATE_DEAD_TO_START) {
        set_outputs(true, false);
        s_state = RSTATE_START;
    } else {
        set_outputs(false, true);
        s_state = RSTATE_STOP;
    }
}

RelayCmd_t Relay_GetState(void)
{
    return s_current;
}
