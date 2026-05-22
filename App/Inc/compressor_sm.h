#ifndef COMPRESSOR_SM_H_
#define COMPRESSOR_SM_H_

#include <stdint.h>
#include <stdbool.h>
#include "settings.h"

typedef enum {
    SM_IDLE     = 0,  /* waiting for operator to press START */
    SM_STARTING = 1,  /* relay energised; waiting for oil pressure switch to close */
    SM_RUNNING  = 2,  /* oil confirmed; monitoring pressure for cut-out or fault */
    SM_STOPPING = 3,  /* relay de-energised; waiting for pressure to drop to cut-in */
    SM_FAULT    = 4,  /* relay off; latched fault — requires operator RESET */
    SM_ESTOP    = 5,  /* relay off; e-stop active — requires pin reset AND operator RESET */
} SmState_t;

/* Initialise state machine with a pointer to the live settings struct.
   s must be non-NULL; call after Settings_Init(). */
void      SM_Init(Settings_t *s);

/* Evaluate one state-machine iteration with the latest pressure reading.
   Call every main-loop iteration; checks E-stop first, then per-state logic. */
void      SM_Update(uint16_t pressure_psi);

/* Request a compressor start (from UI touch handler, main-loop context) */
void      SM_RequestStart(void);

/* Request a fault/e-stop reset (from UI touch handler, main-loop context) */
void      SM_RequestReset(void);

/* Set the E-stop active flag — called from EXTI ISR or main loop.
   active=true forces the SM into SM_ESTOP on the next SM_Update() call. */
void      SM_SetEStop(bool active);

/* Update the oil-switch state — called from EXTI ISR or main loop.
   ok=true means the NC contact is closed (oil pressure present). */
void      SM_SetOilSwitch(bool ok);

/* Return the current state machine state */
SmState_t SM_GetState(void);

/* Return a short human-readable string for the current state (for the UI header) */
const char *SM_GetStateName(void);

/* Return true if the oil pressure switch is currently reporting pressure OK.
   Used by the UI to display the OIL OK / OIL WAIT / OIL LOW indicator. */
bool      SM_GetOilOk(void);

#endif /* COMPRESSOR_SM_H_ */
