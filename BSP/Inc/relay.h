#ifndef RELAY_H_
#define RELAY_H_

typedef enum {
    RELAY_CMD_IDLE  = 0,
    RELAY_CMD_START = 1,
    RELAY_CMD_STOP  = 2,
} RelayCmd_t;

void Relay_Init(void);
void Relay_Request(RelayCmd_t cmd);
void Relay_Update(void);       /* call every main-loop iteration */
RelayCmd_t Relay_GetState(void);

#endif /* RELAY_H_ */
