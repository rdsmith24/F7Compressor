#ifndef ALARMS_H_
#define ALARMS_H_

#include <stdint.h>
#include <stdbool.h>

/* Alarm bitmask constants — OR these together when calling Set/Clear/Acknowledge */
#define ALARM_NONE             0x00u
#define ALARM_OIL_TIMEOUT      0x01u  /* oil switch did not close within oil_pressure_delay_s */
#define ALARM_PRESSURE_FAULT   0x02u  /* ADC reading outside 4-20 mA window */
#define ALARM_ESTOP            0x04u  /* emergency stop contact opened */
#define ALARM_LOW_PRESSURE     0x08u  /* discharge pressure below min_psi at end of startup */
#define ALARM_HIGH_PRESSURE    0x10u  /* pressure exceeded high_alarm_psi (controlled stop) */
#define ALARM_HIGH_HIGH_PRES   0x20u  /* pressure exceeded high_high_psi (emergency shutdown) */

/* Clear all active and acknowledged bits; call once at startup */
void    Alarms_Init(void);

/* Assert one or more alarm bits; newly raised alarms are automatically unacknowledged */
void    Alarms_Set(uint8_t bits);

/* Clear one or more alarm bits from both the active and acknowledged masks */
void    Alarms_Clear(uint8_t bits);

/* Return the bitmask of all currently active alarms */
uint8_t Alarms_Get(void);

/* Mark active alarms as acknowledged — suppresses the main-screen banner.
   Has no effect on bits that are not currently active. */
void    Alarms_Acknowledge(uint8_t bits);

/* Return active alarms that have not yet been acknowledged.
   Used by the UI to decide whether to show the alarm banner. */
uint8_t Alarms_GetUnacknowledged(void);

/* Return true if any alarm bit is currently active */
bool    Alarms_AnyActive(void);

#endif /* ALARMS_H_ */
