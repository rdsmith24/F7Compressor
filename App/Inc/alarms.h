#ifndef ALARMS_H_
#define ALARMS_H_

#include <stdint.h>
#include <stdbool.h>

#define ALARM_NONE             0x00u
#define ALARM_OIL_TIMEOUT      0x01u
#define ALARM_PRESSURE_FAULT   0x02u
#define ALARM_ESTOP            0x04u
#define ALARM_LOW_PRESSURE     0x08u   /* discharge pressure low at end of startup delay */
#define ALARM_HIGH_PRESSURE    0x10u   /* pressure exceeded high alarm threshold */
#define ALARM_HIGH_HIGH_PRES   0x20u   /* pressure exceeded emergency shutdown threshold */

void     Alarms_Init(void);
void     Alarms_Set(uint8_t bits);
void     Alarms_Clear(uint8_t bits);
uint8_t  Alarms_Get(void);
void     Alarms_Acknowledge(uint8_t bits);
uint8_t  Alarms_GetUnacknowledged(void);
bool     Alarms_AnyActive(void);

#endif /* ALARMS_H_ */
