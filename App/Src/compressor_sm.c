/*
 * compressor_sm.c — Air compressor state machine
 *
 * Controls a single-relay motor starter (PG4 energised = run, de-energised = stop).
 * The SM runs entirely
 * in the main loop via SM_Update(); ISR callbacks write the volatile flags
 * that SM_Update() reads on the next iteration.
 *
 * State diagram:
 *
 *   IDLE ──(start request)──► STARTING ──(oil switch closes)──► RUNNING
 *                                  │                                │
 *                           oil timeout                      oil lost / hi-hi ──► FAULT
 *                           or low psi ──► FAULT             high alarm ──► STOPPING
 *                                                            cut-out psi ──► STOPPING
 *                                                                │
 *   IDLE ◄──(manual, psi≤cut-in)──────────────── STOPPING ◄────┘
 *   STARTING ◄──(auto, psi≤cut-in)──────────────────┘
 *   FAULT ◄──(hi-hi while stopping)─────────────────┘
 *
 *   Any state + E-stop pin ──► ESTOP
 *   FAULT + reset request  ──► IDLE
 *   ESTOP + pin re-closes + reset request ──► IDLE
 */
#include "compressor_sm.h"
#include "alarms.h"
#include "relay.h"
#include "main.h"
#include <stddef.h>

/* Live configuration pointer — set once at SM_Init, never NULL after that */
static Settings_t  *s_cfg = NULL;

/* Current state and the HAL tick when we entered it (used for timeouts) */
static SmState_t    s_state = SM_IDLE;
static uint32_t     s_state_tick = 0;

/*
 * Flags written by ISR/touch callbacks and consumed by SM_Update().
 * volatile because they cross the ISR→main boundary.
 */
static volatile bool s_req_start   = false;  /* operator pressed START */
static volatile bool s_req_reset   = false;  /* operator pressed RESET */
static volatile bool s_estop       = false;  /* E-stop NC contact is open (fault) */
static volatile bool s_oil_ok      = true;   /* oil pressure switch: true = pressure present */

/*
 * enter() — transition to a new state.
 *
 * Records the entry tick for timeout calculations, issues the appropriate
 * relay command, and performs any entry-action side-effects (alarm management).
 * Never call Relay_Request() directly from SM_Update(); go through here so
 * relay actions stay in sync with state transitions.
 */
static void enter(SmState_t next)
{
    s_state      = next;
    s_state_tick = HAL_GetTick();

    switch (next) {
    case SM_IDLE:
        /* Both relays off; clear all non-E-stop alarms so the display is clean */
        Relay_Request(RELAY_CMD_IDLE);
        Alarms_Clear(ALARM_OIL_TIMEOUT | ALARM_PRESSURE_FAULT |
                     ALARM_LOW_PRESSURE | ALARM_HIGH_PRESSURE | ALARM_HIGH_HIGH_PRES);
        break;
    case SM_STARTING:
        /* Assert START relay; oil switch must close before oil_pressure_delay_s expires */
        Relay_Request(RELAY_CMD_START);
        break;
    case SM_RUNNING:
        /* Keep START relay asserted; motor is confirmed running */
        Relay_Request(RELAY_CMD_START);
        break;
    case SM_STOPPING:
        /* De-energize relay; compressor stops */
        Relay_Request(RELAY_CMD_STOP);
        break;
    case SM_FAULT:
        /* Motor off, relay de-energized; operator must reset */
        Relay_Request(RELAY_CMD_STOP);
        break;
    case SM_ESTOP:
        /* Emergency: de-energize relay immediately */
        Relay_Request(RELAY_CMD_IDLE);
        Alarms_Set(ALARM_ESTOP);
        break;
    }
}

/*
 * SM_Init — must be called once at startup before SM_Update().
 * Assumes s is non-NULL and points to valid, loaded settings.
 */
void SM_Init(Settings_t *s)
{
    s_cfg = s;
    s_state = SM_IDLE;
    s_state_tick = 0;
    s_req_start = s_req_reset = false;
    s_estop  = false;
    s_oil_ok = true;  /* assume oil ok until the switch tells us otherwise */
}

/*
 * SM_Update — called every main-loop iteration with the latest pressure reading.
 *
 * Checks E-stop first (overrides any other state), then dispatches to the
 * per-state handler. Call Relay_Update() separately each loop to apply relay
 * transitions after the 50ms dead-time.
 */
void SM_Update(uint16_t pressure_psi)
{
    /* E-stop overrides everything — transition immediately regardless of current state */
    if (s_estop && s_state != SM_ESTOP) {
        enter(SM_ESTOP);
        s_req_start = s_req_reset = false;  /* discard any queued commands */
        return;
    }

    /* Seconds spent in the current state — used for the oil-delay timeout in STARTING */
    uint32_t elapsed_s = (HAL_GetTick() - s_state_tick) / 1000u;

    switch (s_state) {

    case SM_IDLE:
        /* Wait for the operator to press START */
        if (s_req_start) {
            s_req_start = false;
            enter(SM_STARTING);
        }
        break;

    case SM_STARTING:
        /*
         * The motor is energised but we wait for the oil pressure switch to
         * confirm lube pressure before counting the compressor as "running".
         * If the switch doesn't close within oil_pressure_delay_s seconds we
         * fault out — continuing to run without oil would destroy the compressor.
         */
        if (s_oil_ok) {
            enter(SM_RUNNING);
        } else if (elapsed_s >= s_cfg->oil_pressure_delay_s) {
            Alarms_Set(ALARM_OIL_TIMEOUT);
            /* Also flag low discharge pressure if we never built any pressure */
            if (pressure_psi < s_cfg->pressure_min_psi) {
                Alarms_Set(ALARM_LOW_PRESSURE);
            }
            enter(SM_FAULT);
        }
        break;

    case SM_RUNNING:
        /*
         * Normal running. Pressure thresholds checked in priority order:
         *   1. Oil lost        → hard fault (motor damage risk)
         *   2. High-high psi   → hard fault (safety shutdown, needs manual reset)
         *   3. High alarm psi  → controlled stop with alarm (operator alert)
         *   4. Cut-out psi     → normal cycle stop (no alarm)
         *   5. Below alarm     → auto-clear the high-pressure alarm if it was set
         */
        if (!s_oil_ok) {
            enter(SM_FAULT);
        } else if (pressure_psi >= s_cfg->pressure_high_high_psi) {
            Alarms_Set(ALARM_HIGH_HIGH_PRES);
            enter(SM_FAULT);
        } else if (pressure_psi >= s_cfg->pressure_high_alarm_psi) {
            Alarms_Set(ALARM_HIGH_PRESSURE);
            enter(SM_STOPPING);
        } else if (pressure_psi >= s_cfg->pressure_max_psi) {
            enter(SM_STOPPING);          /* normal cut-out, no alarm */
        } else {
            Alarms_Clear(ALARM_HIGH_PRESSURE);   /* pressure back in range */
        }
        break;

    case SM_STOPPING:
        /*
         * STOP relay is asserted. Wait for pressure to bleed down to the
         * cut-in threshold (max_psi − span_psi) before restarting or idling.
         *
         * If pressure keeps rising even with the motor stopped, the unloader
         * valve or check valve has failed — treat it as a hard fault.
         */
        if (pressure_psi >= s_cfg->pressure_high_high_psi) {
            Alarms_Set(ALARM_HIGH_HIGH_PRES);
            enter(SM_FAULT);
            break;
        }
        /* Cut-in threshold = cut-out minus the hysteresis band */
        if (pressure_psi <= (uint16_t)(s_cfg->pressure_max_psi - s_cfg->pressure_span_psi)) {
            if (s_cfg->auto_restart_en) {
                enter(SM_STARTING);   /* automatic duty-cycle mode */
            } else {
                enter(SM_IDLE);       /* manual mode: wait for operator to press START */
            }
        }
        break;

    case SM_FAULT:
        /*
         * Motor is stopped. Operator must acknowledge the alarm and press RESET.
         * HIGH_PRESSURE is not cleared here — it auto-clears when pressure drops
         * below high_alarm_psi in SM_RUNNING after the next start.
         */
        if (s_req_reset) {
            s_req_reset = false;
            Alarms_Clear(ALARM_OIL_TIMEOUT | ALARM_PRESSURE_FAULT |
                         ALARM_LOW_PRESSURE | ALARM_HIGH_HIGH_PRES);
            enter(SM_IDLE);
        }
        break;

    case SM_ESTOP:
        /*
         * Two conditions must both be true before we allow a reset:
         *   1. The physical E-stop NC contact has re-closed (s_estop = false),
         *      set by DigIn_ConsumeEStopReset() in the main loop.
         *   2. The operator has pressed RESET on the UI.
         * This prevents an automatic restart if the E-stop is bypassed.
         */
        if (!s_estop && s_req_reset) {
            s_req_reset = false;
            Alarms_Clear(ALARM_ESTOP);
            enter(SM_IDLE);
        }
        break;
    }

    /*
     * Any start request that arrived while we were not in IDLE is stale —
     * discard it so it doesn't fire unexpectedly the next time we reach IDLE.
     */
    s_req_start = false;
}

/* Called from UI touch handler (main loop context, not ISR) */
void SM_RequestStart(void)
{
    s_req_start = true;
}

void SM_RequestReset(void)
{
    s_req_reset = true;
}

/* Called from EXTI ISR — sets flag only, SM_Update() acts on it next iteration */
void SM_SetEStop(bool active)
{
    s_estop = active;
}

/* Called from EXTI ISR or main loop after DigIn_ConsumeEStopReset() */
void SM_SetOilSwitch(bool ok)
{
    s_oil_ok = ok;
}

SmState_t SM_GetState(void)
{
    return s_state;
}

/* Exposes internal oil flag so the UI can show OIL OK / OIL WAIT / OIL LOW */
bool SM_GetOilOk(void)
{
    return s_oil_ok;
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
