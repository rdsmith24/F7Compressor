#include "compressor_sm.h"
#include "alarms.h"
#include "relay.h"
#include "main.h"
#include <stddef.h>

static Settings_t  *s_cfg = NULL;
static SmState_t    s_state = SM_IDLE;
static uint32_t     s_state_tick = 0;

/* Flags written by ISR/touch callbacks */
static volatile bool s_req_start   = false;
static volatile bool s_req_reset   = false;
static volatile bool s_estop       = false;
static volatile bool s_oil_ok      = true;

static void enter(SmState_t next)
{
    s_state      = next;
    s_state_tick = HAL_GetTick();

    switch (next) {
    case SM_IDLE:
        Relay_Request(RELAY_CMD_IDLE);
        Alarms_Clear(ALARM_OIL_TIMEOUT | ALARM_PRESSURE_FAULT);
        break;
    case SM_STARTING:
        Relay_Request(RELAY_CMD_START);
        break;
    case SM_RUNNING:
        Relay_Request(RELAY_CMD_START);
        break;
    case SM_STOPPING:
        Relay_Request(RELAY_CMD_STOP);
        break;
    case SM_FAULT:
        Relay_Request(RELAY_CMD_STOP);
        break;
    case SM_ESTOP:
        Relay_Request(RELAY_CMD_IDLE);
        Alarms_Set(ALARM_ESTOP);
        break;
    }
}

void SM_Init(Settings_t *s)
{
    s_cfg = s;
    s_state = SM_IDLE;
    s_state_tick = 0;
    s_req_start = s_req_reset = false;
    s_estop  = false;
    s_oil_ok = true;
}

void SM_Update(uint16_t pressure_psi)
{
    /* E-stop overrides everything */
    if (s_estop && s_state != SM_ESTOP) {
        enter(SM_ESTOP);
        s_req_start = s_req_reset = false;
        return;
    }

    uint32_t elapsed_s = (HAL_GetTick() - s_state_tick) / 1000u;

    switch (s_state) {

    case SM_IDLE:
        if (s_req_start) {
            s_req_start = false;
            enter(SM_STARTING);
        }
        break;

    case SM_STARTING:
        /* Oil pressure switch must close within oil_pressure_delay_s */
        if (s_oil_ok) {
            enter(SM_RUNNING);
        } else if (elapsed_s >= s_cfg->oil_pressure_delay_s) {
            Alarms_Set(ALARM_OIL_TIMEOUT);
            enter(SM_FAULT);
        }
        break;

    case SM_RUNNING:
        if (!s_oil_ok) {
            enter(SM_FAULT);
        } else if (pressure_psi >= s_cfg->pressure_max_psi) {
            enter(SM_STOPPING);
        }
        break;

    case SM_STOPPING:
        /* Restart threshold: max - hysteresis */
        if (pressure_psi <= (uint16_t)(s_cfg->pressure_max_psi - s_cfg->pressure_span_psi)) {
            if (s_cfg->auto_restart_en) {
                enter(SM_STARTING);
            } else {
                enter(SM_IDLE);
            }
        }
        break;

    case SM_FAULT:
        if (s_req_reset) {
            s_req_reset = false;
            Alarms_Clear(ALARM_OIL_TIMEOUT | ALARM_PRESSURE_FAULT);
            enter(SM_IDLE);
        }
        break;

    case SM_ESTOP:
        /* E-stop reset requires the physical switch to re-close AND a manual reset */
        if (!s_estop && s_req_reset) {
            s_req_reset = false;
            Alarms_Clear(ALARM_ESTOP);
            enter(SM_IDLE);
        }
        break;
    }

    s_req_start = false;  /* consume any stale start requests when not in IDLE */
}

void SM_RequestStart(void)
{
    s_req_start = true;
}

void SM_RequestReset(void)
{
    s_req_reset = true;
}

void SM_SetEStop(bool active)
{
    s_estop = active;
}

void SM_SetOilSwitch(bool ok)
{
    s_oil_ok = ok;
}

SmState_t SM_GetState(void)
{
    return s_state;
}

const char *SM_GetStateName(void)
{
    switch (s_state) {
    case SM_IDLE:     return "IDLE";
    case SM_STARTING: return "STARTING";
    case SM_RUNNING:  return "RUNNING";
    case SM_STOPPING: return "STOPPING";
    case SM_FAULT:    return "FAULT";
    case SM_ESTOP:    return "E-STOP";
    default:          return "UNKNOWN";
    }
}
