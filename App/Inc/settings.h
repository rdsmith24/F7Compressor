#ifndef SETTINGS_H_
#define SETTINGS_H_

#include <stdint.h>
#include <stdbool.h>

/* Increment this magic value whenever the Settings_t struct layout changes.
   Mismatched magic causes the flash sector to be treated as blank (defaults used). */
#define SETTINGS_MAGIC  0xC0FFEE02UL

/* STM32H723ZG flash sector 7 — last 128 KB, safely above the code region */
#define SETTINGS_FLASH_ADDR  0x080E0000UL

typedef struct {
    uint32_t magic;                    /* must equal SETTINGS_MAGIC */
    uint16_t oil_pressure_delay_s;     /* seconds to wait for oil switch after start */
    uint16_t pressure_min_psi;         /* low-pressure alarm threshold at startup */
    uint16_t pressure_max_psi;         /* normal cut-out pressure */
    uint16_t pressure_span_psi;        /* hysteresis: cut-in = max_psi − span_psi */
    uint16_t auto_restart_en;          /* 1 = automatic duty-cycle; 0 = manual restart */
    uint16_t pressure_high_alarm_psi;  /* controlled stop with alarm above this threshold */
    uint16_t pressure_high_high_psi;   /* emergency shutdown above this threshold */
    uint16_t checksum;                 /* additive checksum over all fields above */
    uint8_t  _pad[12];                 /* pad struct to 32 bytes (one STM32H7 flash word) */
} Settings_t;

/* Compiled-in defaults — applied when flash is blank or checksum fails */
#define SETTINGS_DEFAULT { \
    .magic                    = SETTINGS_MAGIC, \
    .oil_pressure_delay_s     = 5,   \
    .pressure_min_psi         = 80,  \
    .pressure_max_psi         = 100, \
    .pressure_span_psi        = 10,  \
    .auto_restart_en          = 1,   \
    .pressure_high_alarm_psi  = 110, \
    .pressure_high_high_psi   = 120, \
    .checksum                 = 0,   \
    ._pad                     = {0}  \
}

/* Load settings from flash; apply defaults if the sector is blank or invalid.
   Call once at startup before SM_Init(). */
void       Settings_Init(void);

/* Return a pointer to the live in-RAM settings struct.
   Read directly; do not write — use Settings_Save() to persist changes. */
Settings_t *Settings_Get(void);

/* Write settings to flash sector 7 (erase + program one flash word).
   Handles D-cache flush/invalidate around the operation.
   Returns true on success; false if erase or program fails. */
bool       Settings_Save(const Settings_t *s);

#endif /* SETTINGS_H_ */
