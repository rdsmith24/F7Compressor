#ifndef COMPRESSOR_SM_H_
#define COMPRESSOR_SM_H_

#include <stdint.h>
#include <stdbool.h>
#include "settings.h"

typedef enum {
    SM_IDLE     = 0,
    SM_STARTING = 1,
    SM_RUNNING  = 2,
    SM_STOPPING = 3,
    SM_FAULT    = 4,
    SM_ESTOP    = 5,
} SmState_t;

void      SM_Init(Settings_t *s);
void      SM_Update(uint16_t pressure_psi);
void      SM_RequestStart(void);
void      SM_RequestReset(void);
void      SM_SetEStop(bool active);
void      SM_SetOilSwitch(bool ok);
SmState_t SM_GetState(void);
const char *SM_GetStateName(void);

#endif /* COMPRESSOR_SM_H_ */
