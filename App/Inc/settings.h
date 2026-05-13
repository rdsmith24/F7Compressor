#ifndef SETTINGS_H_
#define SETTINGS_H_

#include <stdint.h>
#include <stdbool.h>

#define SETTINGS_MAGIC  0xC0FFEE01UL

/* STM32H723ZG sector 7 — last 128KB, safe from code overlap */
#define SETTINGS_FLASH_ADDR  0x080E0000UL

typedef struct {
    uint32_t magic;
    uint16_t oil_pressure_delay_s;
    uint16_t pressure_min_psi;
    uint16_t pressure_max_psi;
    uint16_t pressure_span_psi;
    uint16_t auto_restart_en;
    uint16_t checksum;
    uint8_t  _pad[16];          /* pad struct to 32 bytes (1 flash word) */
} Settings_t;

/* Defaults applied when flash is erased or checksum fails */
#define SETTINGS_DEFAULT { \
    .magic               = SETTINGS_MAGIC, \
    .oil_pressure_delay_s = 5, \
    .pressure_min_psi    = 80, \
    .pressure_max_psi    = 100, \
    .pressure_span_psi   = 10, \
    .auto_restart_en     = 1, \
    .checksum            = 0, \
    ._pad                = {0} \
}

void      Settings_Init(void);
Settings_t *Settings_Get(void);
bool      Settings_Save(const Settings_t *s);

#endif /* SETTINGS_H_ */
