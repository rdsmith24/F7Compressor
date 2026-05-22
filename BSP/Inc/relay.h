#ifndef RELAY_H_
#define RELAY_H_

typedef enum {
    RELAY_CMD_IDLE  = 0,   /* de-energise immediately (used for E-stop) */
    RELAY_CMD_START = 1,   /* energise PG4 — compressor runs */
    RELAY_CMD_STOP  = 2,   /* de-energise PG4 — compressor stops */
} RelayCmd_t;

void       Relay_Init(void);              /* de-energise relay, reset state */
void       Relay_Request(RelayCmd_t cmd); /* change relay state; repeated same-cmd calls are no-ops */
void       Relay_Update(void);            /* no-op — retained for main-loop call-site compatibility */
RelayCmd_t Relay_GetState(void);          /* return last command passed to Relay_Request() */

#endif /* RELAY_H_ */
