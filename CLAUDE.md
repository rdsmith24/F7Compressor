# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

# F7Compressor Firmware

STM32H723ZG firmware for controlling a 220VAC air compressor with a Waveshare 3.2" touch LCD display.

## Build Commands

```bash
make -j4          # build (outputs build/F7Compressor.{elf,hex,bin})
make flash        # build then flash via OpenOCD + ST-Link
make clean        # remove build directory
```

`make flash` requires OpenOCD on PATH. Alternatively flash manually with STM32CubeProgrammer using `build/F7Compressor.hex`.

**Toolchain**: `arm-none-eabi-gcc` must be on `PATH`. Flags: `-mcpu=cortex-m7 -mfpu=fpv5-d16 -mfloat-abi=hard -Og -g -gdwarf-2`.

## VS Code Debug (needs fix)

`.vscode/launch.json` currently points to a different project (`H7Servo`). Before debugging, update:
- `executable` → `${workspaceRoot}/build/F7Compressor.elf`
- `svdFile` → `${workspaceRoot}/STM32H723.svd`

## Current Project Status

All bring-up steps are **implemented and compile cleanly** (~30KB flash). The firmware has not yet been flashed to hardware for validation.

**Completed:**
- Makefile USER CODE sections present; all BSP and App sources compile.
- `BSP/Inc/ili9341.h` modifications applied (`LCD_BASE1 = 0x60000002`, LL GPIO backlight macros).
- Full BSP layer: display, touch (XPT2046), pressure ADC, relay control, digital inputs.
- Full App layer: compressor state machine, alarm management, settings (flash storage), three UI screens.
- `Core/Src/main.c` and `Core/Src/stm32h7xx_it.c` USER CODE blocks filled.

**Pending (first-time hardware validation):**
- Flash and confirm display shows IDLE screen (FMC + TIM3 backlight + lcdInit).
- Verify PSI reading (apply known voltage to PA3).
- Tune touch calibration constants in `BSP/Inc/xpt2046_spi.h` (`XPT_CAL_*`, `XPT_SWAP_XY`, `XPT_FLIP_*`).
- Validate relay dead-time with oscilloscope on PG4/PG5.
- End-to-end state machine test with real I/O.
- Settings save/load across power cycle.

## Makefile USER CODE Sections

CubeMX preserves `/* USER CODE */` blocks on regeneration. After every regeneration, ensure these are present:

```makefile
# USER CODE BEGIN C_SOURCES
BSP/Src/ili9341.c \
BSP/Src/font8.c \
BSP/Src/font12.c \
BSP/Src/font16.c \
BSP/Src/font20.c \
BSP/Src/font24.c \
BSP/Src/xpt2046_spi.c \
BSP/Src/pressure_adc.c \
BSP/Src/relay.c \
BSP/Src/digital_in.c \
App/Src/compressor_sm.c \
App/Src/alarms.c \
App/Src/settings.c \
App/Src/ui_main.c \
App/Src/ui_settings.c \
App/Src/ui_alarms.c
# USER CODE END C_SOURCES

# USER CODE BEGIN C_INCLUDES
-IBSP/Inc \
-IApp/Inc
# USER CODE END C_INCLUDES
```

## Architecture

### Layering

Bare-metal superloop — no RTOS. Three layers:

- **`Core/`** (CubeMX-owned): peripheral init (`MX_*_Init()`). Add app code only in `/* USER CODE */` blocks. ISR hooks go in `Core/Src/stm32h7xx_it.c` USER CODE blocks.
- **`BSP/`**: hardware drivers for display, touch, ADC, relays, digital inputs. CubeMX never touches this directory.
- **`App/`**: application logic (state machine, alarms, settings, UI screens). CubeMX never touches this directory.

The main loop polls ADC (~10Hz) and touch panel every iteration, and redraws the UI at ~20Hz. ISRs handle E-Stop (EXTI2, PC2) and oil pressure switch (EXTI0, PC0); both delegate to `DigIn_*_IRQHandler()`. EXTI3 (PG3, TP_IRQ) calls `XPT2046_IRQHandler()` (sets a flag; actual SPI read happens in the main loop by polling the pin).

### Main Loop Structure (`Core/Src/main.c`)

```
Init:  TIM3 PWM start → lcdInit → XPT2046_Init → PADC_Init → Relay_Init → DigIn_Init
       → Alarms_Init → Settings_Init → SM_Init

Loop:  every 100ms: PADC_Sample + alarm update
       every iter:  Relay_Update, SM_SetOilSwitch/EStop, SM_Update
       every iter:  XPT2046_Read → UI touch dispatch
       every 50ms:  UI draw (active screen)
```

Screen routing uses a `Screen_t` enum (`SCREEN_MAIN`, `SCREEN_SETTINGS`, `SCREEN_ALARMS`) and a `s_full_redraw` flag. Navigation functions `UI_ShowMain/Settings/Alarms()` are defined in `main.c` USER CODE 0 and declared `extern` in each `ui_*.c`.

### HAL vs LL Driver Mixing

The project intentionally uses **both** HAL and LL:
- **FMC uses HAL**: no LL option exists for NOR/PSRAM init. This also makes `HAL_Delay()` and `HAL_GetTick()` available project-wide.
- **All other peripherals use LL** (SPI5, ADC1, TIM3, GPIO, RCC, CORTEX_M7): lower overhead, direct register access.

Both `USE_HAL_DRIVER` and `USE_FULL_LL_DRIVER` are defined project-wide.

### ILI9341 Library

Source: https://github.com/taburyak/STM32-ILI9341-320x240-FSMC-Library

Two changes applied to `BSP/Inc/ili9341.h`:

```c
// 1. A0 address bit, not A18:
#define LCD_BASE1  ((uint32_t)0x60000002)   // was 0x60080000

// 2. LL GPIO for backlight (PC6):
#define LCD_BL_ON()   LL_GPIO_SetOutputPin(GPIOC, LL_GPIO_PIN_6)
#define LCD_BL_OFF()  LL_GPIO_ResetOutputPin(GPIOC, LL_GPIO_PIN_6)
```

**Important:** `LCD_BL_ON/OFF` are ineffective because PC6 is in TIM3_CH1 alternate-function mode. Backlight is controlled by starting TIM3 PWM (`LL_TIM_EnableCounter` + `LL_TIM_CC_EnableChannel`). `lcdReset()` uses a software reset command only — the LCD_RST pin (PG2) is held HIGH by `MX_GPIO_Init` and never toggled by the library.

FMC address map: `0x60000000` = command (A0=0), `0x60000002` = data (A0=1). Only PF0 (FMC_A0) is wired.

### BSP Modules

#### `xpt2046_spi` — Touch driver
- SPI5 LL, polling, CS on PF10 (TP_CS). IRQ on PG3 (TP_IRQ), active-low.
- `XPT2046_Read()` polls TP_IRQ pin directly; returns false if not touched.
- 8-sample averaging per axis. Calibration constants in `BSP/Inc/xpt2046_spi.h`:
  - `XPT_CAL_X_MIN/MAX`, `XPT_CAL_Y_MIN/MAX` — raw ADC limits at screen edges.
  - `XPT_SWAP_XY` (default 1), `XPT_FLIP_X` (default 0), `XPT_FLIP_Y` (default 1) — orientation. **Tune these after first flash.**
- SPI5 transaction pattern: `SetTransferSize → Enable → StartMasterTransfer → xfer bytes → wait EOT → ClearFlag_EOT/TXTF → Disable`.

#### `pressure_adc` — 4-20mA pressure sensor
- ADC1 CH15 (PA3), 12-bit, software-triggered polled.
- `PADC_Init()` runs LL calibration then enables ADC. Call once at startup.
- `PADC_Sample()` triggers one conversion, waits for EOC, updates PSI. Call at ~10Hz.
- Calibration constants in `BSP/Inc/pressure_adc.h`: `PADC_ADC_4MA=744`, `PADC_ADC_20MA=3724`, `PADC_SENSOR_RANGE_PSI=200`.
- `PADC_IsFault()` returns true if reading is outside a 5% margin of the 4-20mA window.

#### `relay` — START/STOP relay outputs
- PG4 = START relay, PG5 = STOP relay. Both init LOW.
- `Relay_Request(cmd)` schedules a state change; de-asserts current relay, enters dead-time.
- `Relay_Update()` must be called every main-loop iteration to apply the transition after 50ms.
- `RELAY_CMD_IDLE` forces both LOW immediately (no dead-time).

#### `digital_in` — OIL_SW and E-STOP inputs
- PC0 (OIL_SW): NC contact to GND, pull-up. LOW = pressure OK, HIGH = fault. Both EXTI edges.
- PC2 (ESTOP): NC contact to GND, pull-up. LOW = normal, HIGH = e-stop active. Falling edge ISR detects reset (contact re-closes).
- `DigIn_ConsumeEStopReset()` returns true once when the e-stop NC contact closes again (physical reset allowed).

### App Modules

#### `settings` — Flash-backed configuration
- Stored at `0x080E0000` (sector 7, last 128KB). Magic `0xC0FFEE01`, uint16 checksum.
- `Settings_t` padded to 32 bytes for STM32H7 flash word (`FLASH_TYPEPROGRAM_FLASHWORD`).
- Uses `SCB_CleanInvalidateDCache_by_Addr` / `SCB_InvalidateDCache_by_Addr` around erase/program.
- Defaults applied if magic or checksum fails.

```c
typedef struct {
    uint32_t magic;                // 0xC0FFEE01
    uint16_t oil_pressure_delay_s; // default 5
    uint16_t pressure_min_psi;     // default 80
    uint16_t pressure_max_psi;     // default 100
    uint16_t pressure_span_psi;    // hysteresis deadband, default 10
    uint16_t auto_restart_en;      // 0=manual reset, 1=auto, default 1
    uint16_t checksum;
    uint8_t  _pad[16];             // pad to 32 bytes
} Settings_t;
```

#### `alarms` — Alarm management
- Bitmask: `ALARM_OIL_TIMEOUT=0x01`, `ALARM_PRESSURE_FAULT=0x02`, `ALARM_ESTOP=0x04`.
- `Alarms_Set()` marks unacknowledged. `Alarms_Acknowledge()` marks acknowledged (clears unack flag).
- `Alarms_GetUnacknowledged()` used by UI to show the `!` indicator.

#### `compressor_sm` — State machine

```
IDLE → STARTING → RUNNING → STOPPING → IDLE (or STARTING if auto_restart_en)
          │            │
      oil timeout   oil fault
          └──→ FAULT ←──┘
[any state] + estop pin HIGH → ESTOP
```

- `SM_Update(psi)` called every main-loop iteration. Reads oil/estop state via `SM_SetOilSwitch/EStop()`.
- Oil timeout: if oil switch stays open for `oil_pressure_delay_s` seconds after entering STARTING → FAULT + `ALARM_OIL_TIMEOUT`.
- Restart threshold (STOPPING → STARTING/IDLE): `pressure <= pressure_max_psi - pressure_span_psi`.
- E-stop reset requires both: pin goes LOW (`DigIn_ConsumeEStopReset`) AND `SM_RequestReset()` from UI.

### UI Screens (320×240 landscape)

All screens use `lcdFillRect`, `lcdSetTextFont`, `lcdSetCursor`, `lcdPrintf`. Screen switching sets `s_full_redraw = true`.

#### Main screen (`ui_main`)
- Header (Y 0–31): state name + PSI, colored by state (GREEN=running, YELLOW=starting, CYAN=stopping, RED=fault, MAGENTA=e-stop).
- Centre (Y 32–199): large PSI number or "SENSOR FAULT"; alarm banner at Y 172 if unacknowledged alarms exist.
- Button bar (Y 200–239): START/STOP (toggles) | RESET | ALARMS | SETTINGS.

#### Settings screen (`ui_settings`)
- `UI_Settings_Enter()` must be called before showing (copies current settings to local edit buffer).
- Five rows with [-]/[+] buttons: oil delay, min PSI, max PSI, hysteresis, auto-restart.
- CANCEL discards; SAVE calls `Settings_Save()` and returns to main.

#### Alarms screen (`ui_alarms`)
- Three rows: oil timeout, pressure fault, e-stop. Active+unacknowledged = red; active+acked = dark grey; inactive = black.
- Tap header to return to main. Tap bottom bar to acknowledge all.

### Compressor State Machine

**Relay rule**: never assert both relays simultaneously. De-assert one, wait 50ms dead-time, then assert the other.

| State | PG4 START relay | PG5 STOP relay |
|---|---|---|
| IDLE / STOPPED | LOW | LOW |
| STARTING / RUNNING | HIGH | LOW |
| STOPPING / FAULT / E-STOP | LOW | HIGH |

## Hardware Summary

- **MCU**: STM32H723ZGTx on NUCLEO-H723ZG, 480MHz (HSE 8MHz → PLL1, VOS0)
- **Power**: External 5VDC on CN9 pin 6 (E5V); JP3 jumper in E5V position (1-2)
- **Debug**: SWD only — USART3 is consumed by FMC (PD8/PD9 = FMC_D13/D14), no ST-Link VCP

### Key Pin Assignments

| Function | Pin | Notes |
|---|---|---|
| FMC_A0 (RS/DC) | PF0 | AF12; 0x60000000=cmd, 0x60000002=data |
| FMC_D13/D14 | PD8/PD9 | AF12; USART3 must be disabled |
| LCD_RST | PG2 | GPIO output, init HIGH; library uses soft-reset only |
| BL_CTRL | PC6 | TIM3_CH1 PWM AF2, 1kHz, 75% default; LCD_BL_ON/OFF ineffective in AF mode |
| TP_CS | PF10 | SPI5 touch chip select |
| TP_IRQ | PG3 | EXTI3 falling edge, pull-up; polled by pin state in XPT2046_Read |
| Oil pressure switch | PC0 | EXTI0, both edges, pull-up, NC contact |
| Emergency stop | PC2 | EXTI2, falling edge, pull-up, NC contact |
| START relay | PG4 | Output, init LOW |
| STOP relay | PG5 | Output, init LOW |
| Pressure 4-20mA | PA3 | ADC1_INP15; 150Ω shunt → 0.6–3.0V |

150Ω resistor between PA3 and GND (4mA→0.6V, 20mA→3.0V). Add 100nF decoupling cap.

## CubeMX Notes (for regeneration)

- FMC: **HAL** driver (no LL NOR/PSRAM option)
- SPI5, ADC1, TIM3, GPIO, RCC, CORTEX_M7: **LL** driver
- FMC address width: **1 bit** (A0 only, PF0); do not use A18
- USART3: **disabled** (frees PD8/PD9 for FMC_D13/D14)
- FMC timing at 240MHz AHB3: Address setup=1, Data setup=15, Bus turnaround=0, Mode A
- NVIC Priority Group: **4** (4 bits preemption, 0 sub-priority)
- ADC: polled in main loop; NVIC enabled by CubeMX but no ADC interrupt sources are enabled — ADC_IRQHandler is empty and harmless
- TIM3: prescaler=239 (1MHz at 240MHz PCLK1), ARR=999 (1kHz), CCR1=750 (75%)

## Bring-up Sequence

1. ✅ Add Makefile USER CODE blocks → verify compile
2. ✅ Apply `ili9341.h` modifications; start TIM3 backlight PWM; `lcdInit()` + landscape
3. ✅ Text rendering with fonts (library included)
4. ✅ XPT2046 touch driver — calibration constants need tuning on hardware
5. ✅ ADC pressure → PSI conversion + fault detection
6. ✅ Relay outputs + oil switch/E-Stop inputs with ISR handlers
7. ✅ Compressor state machine wired to real I/O
8. ✅ Flash settings read/write (sector 7, D-cache flushed)
9. ✅ Full UI integration (main, settings, alarms screens)
