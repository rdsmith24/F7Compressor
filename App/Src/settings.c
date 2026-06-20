#include "settings.h"
#include "main.h"
#include <string.h>

static Settings_t s_settings;   /* live RAM copy, always valid after Settings_Init() */

/*
 * Sum all persistent fields into a 16-bit additive checksum.
 * Covers every field except the checksum itself and the padding bytes.
 * Increment SETTINGS_MAGIC whenever Settings_T layout changes so stale
 * flash data is rejected rather than loaded with a wrong checksum match.
 */
static uint16_t compute_checksum(const Settings_t *s)
{
    uint16_t sum = 0;
    sum += (uint16_t)(s->magic >> 16);
    sum += (uint16_t)(s->magic & 0xFFFFu);
    sum += s->oil_pressure_delay_s;
    sum += s->pressure_min_psi;
    sum += s->pressure_max_psi;
    sum += s->pressure_span_psi;
    sum += s->auto_restart_en;
    sum += s->pressure_high_alarm_psi;
    sum += s->pressure_high_high_psi;
    return sum;
}

/*
 * Return true if the flash sector contains a valid settings struct:
 * magic word matches the compiled-in value AND checksum is correct.
 */
static bool validate(const Settings_t *s)
{
    return (s->magic == SETTINGS_MAGIC) && (s->checksum == compute_checksum(s));
}

static uint16_t clamp_u16(uint16_t v, uint16_t lo, uint16_t hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/*
 * Clamp every field to its valid range and enforce the ordering the state
 * machine relies on. The UI enforces these bounds when editing, but a
 * checksum-valid struct in flash (e.g. after a future layout change, or
 * corruption that happens to collide) could otherwise feed SM_Update() values
 * that underflow (max − span) or violate the high_high ≥ high_alarm ≥ max
 * priority assumption. Ranges mirror row_min/row_max in ui_settings.c.
 */
static void clamp_settings(Settings_t *s)
{
    s->oil_pressure_delay_s    = clamp_u16(s->oil_pressure_delay_s,    1u, 60u);
    s->pressure_min_psi        = clamp_u16(s->pressure_min_psi,        0u, 150u);
    s->pressure_max_psi        = clamp_u16(s->pressure_max_psi,        10u, 200u);
    s->pressure_span_psi       = clamp_u16(s->pressure_span_psi,       1u, 50u);
    s->auto_restart_en         = (s->auto_restart_en != 0u) ? 1u : 0u;
    s->pressure_high_alarm_psi = clamp_u16(s->pressure_high_alarm_psi, 10u, 220u);
    s->pressure_high_high_psi  = clamp_u16(s->pressure_high_high_psi,  10u, 230u);

    /* span must stay below max so cut-in = (max − span) cannot underflow */
    if (s->pressure_span_psi >= s->pressure_max_psi) {
        s->pressure_span_psi = (uint16_t)(s->pressure_max_psi - 1u);
    }
    /* SM_RUNNING checks assume high_high ≥ high_alarm ≥ max */
    if (s->pressure_high_alarm_psi < s->pressure_max_psi) {
        s->pressure_high_alarm_psi = s->pressure_max_psi;
    }
    if (s->pressure_high_high_psi < s->pressure_high_alarm_psi) {
        s->pressure_high_high_psi = s->pressure_high_alarm_psi;
    }
}

/*
 * Load settings from flash sector 7. If the sector is blank or the
 * checksum fails, the compiled-in defaults are used instead and the
 * settings are NOT written back to flash (that happens on first SAVE).
 */
void Settings_Init(void)
{
    const Settings_t *flash = (const Settings_t *)SETTINGS_FLASH_ADDR;

    if (validate(flash)) {
        memcpy(&s_settings, flash, sizeof(Settings_t));
    } else {
        Settings_t def = SETTINGS_DEFAULT;
        def.checksum   = compute_checksum(&def);
        s_settings     = def;
    }

    /* Guarantee the live config is self-consistent regardless of flash contents */
    clamp_settings(&s_settings);
}

/*
 * Return a pointer to the live settings struct in RAM.
 * Read freely from this pointer; do not write to it directly.
 * Use Settings_Save() to commit changes to flash.
 */
Settings_t *Settings_Get(void)
{
    return &s_settings;
}

/*
 * Persist settings to flash sector 7.
 *
 * Steps:
 *   1. Copy s into a 32-byte aligned buffer and recompute the checksum.
 *   2. Flush D-cache to ensure the flash controller sees the current buffer.
 *   3. Unlock flash, erase sector 7 (128 KB), program one FLASHWORD (32 bytes).
 *   4. Lock flash and invalidate D-cache so the next read reflects the new data.
 *   5. Update the in-RAM copy on success.
 *
 * Returns true on success, false if erase or program fails (flash locked on error).
 */
bool Settings_Save(const Settings_t *s)
{
    Settings_t buf __attribute__((aligned(32)));
    memcpy(&buf, s, sizeof(Settings_t));
    buf.checksum = compute_checksum(&buf);

    /* Flush D-cache before touching flash hardware */
    SCB_CleanInvalidateDCache_by_Addr((uint32_t *)SETTINGS_FLASH_ADDR,
                                       sizeof(Settings_t));

    HAL_FLASH_Unlock();

    FLASH_EraseInitTypeDef erase = {
        .TypeErase    = FLASH_TYPEERASE_SECTORS,
        .Banks        = FLASH_BANK_1,
        .Sector       = FLASH_SECTOR_7,
        .NbSectors    = 1u,
        .VoltageRange = FLASH_VOLTAGE_RANGE_3,
    };
    /* Refresh the watchdog: a 128 KB sector erase blocks long enough to matter */
    IWDG1->KR = 0x0000AAAAu;

    uint32_t sector_error = 0;
    if (HAL_FLASHEx_Erase(&erase, &sector_error) != HAL_OK) {
        HAL_FLASH_Lock();
        return false;
    }

    bool ok = (HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD,
                                  SETTINGS_FLASH_ADDR,
                                  (uint32_t)&buf) == HAL_OK);

    HAL_FLASH_Lock();

    /* Invalidate D-cache so the next read picks up the programmed data */
    SCB_InvalidateDCache_by_Addr((uint32_t *)SETTINGS_FLASH_ADDR,
                                  sizeof(Settings_t));

    if (ok) {
        memcpy(&s_settings, &buf, sizeof(Settings_t));
    }
    return ok;
}
