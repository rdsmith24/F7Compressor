# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

# F7Compressor Firmware

STM32H723ZG firmware for controlling a 220VAC air compressor with a Waveshare 3.2" touch LCD display.

## Build Commands

```bash
make -j4          # build (outputs build/F7Compressor.{elf,hex,bin})
make flash        # build then flash+verify+reset via J-Link
make clean        # remove build directory
```

`make flash` uses `JLinkExe` at `/Applications/SEGGER/JLink_V946/JLinkExe`. Override if needed:

```bash
make flash JLINK=/path/to/JLinkExe
make flash JLINK_IF=JTAG      # default SWD
make flash JLINK_SPD=1000     # default 4000 kHz
```

**Toolchain**: `arm-none-eabi-gcc` must be on `PATH`. Flags: `-mcpu=cortex-m7 -mfpu=fpv5-d16 -mfloat-abi=hard -Og -g -gdwarf-2`.

## VS Code Debug

`.vscode/launch.json` is configured for **J-Link** (`serverpath: /Applications/SEGGER/JLink_V946/JLinkGDBServerCLExe`, `device: STM32H723ZG`, `interface: swd`). Connect J-Link EDU Mini to CN4 (20-pin ARM debug connector). Do not connect CN1 (ST-Link USB) at the same time — there are no CN2 jumpers on this board; ST-Link and J-Link would contend on the SWD lines. JP3 (T_NRST, 2-pin) must remain installed.

**Debug output**: ST-Link VCP is unavailable (USART3/PD8/PD9 consumed by FMC). Options:
- J-Link RTT (preferred — no UART, no pins, zero overhead)
- USART1 (PA9=TX, PA10=RX) + USB-serial adapter (shows as `/dev/ttyUSB*` on Linux)

## Current Project Status

All firmware is **implemented and compiles cleanly** (~30KB flash). First flash to hardware confirmed successful via OpenOCD + ST-Link SWD.

**Pending (hardware validation):**
- Confirm display shows IDLE screen (FMC + TIM3 backlight + lcdInit). Heartbeat LED (LD1, PB0) blinks at 1Hz to confirm firmware is running.
- Verify PSI reading (apply known voltage to PA3).
- Tune touch calibration constants in `BSP/Inc/xpt2046_spi.h` (`XPT_CAL_*`, `XPT_SWAP_XY`, `XPT_FLIP_*`).
- Validate relay output on PG4 with oscilloscope (PG5 unused).
- End-to-end state machine test with real I/O.
- Settings save/load across power cycle.

## Makefile USER CODE Sections

CubeMX preserves `/* USER CODE */` blocks on regeneration. After every regeneration, ensure these are present:

```makefile
# USER CODE BEGIN C_SOURCES
C_SOURCES += \
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

Note: `BSP/Src/example.c` is NOT compiled — it is the original library example file, kept for reference only.

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

Loop:  every 500ms: toggle LD1 (PB0) heartbeat LED
       every 100ms: PADC_Sample + alarm update
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

#### `relay` — Single relay output
- PG4 = RUN relay (energised = compressor runs, de-energised = compressor stops). PG5 unused.
- `Relay_Request(RELAY_CMD_START)` energises PG4; any other command de-energises it.
- `Relay_Update()` is a no-op retained for API compatibility — no dead-time required.

#### `digital_in` — OIL_SW and E-STOP inputs
- PC0 (OIL_SW): NC contact to GND, pull-up. LOW = pressure OK, HIGH = fault. Both EXTI edges.
- PC2 (ESTOP): NC contact to GND, pull-up. LOW = normal, HIGH = e-stop active. Falling edge ISR detects reset (contact re-closes).
- `DigIn_ConsumeEStopReset()` returns true once when the e-stop NC contact closes again (physical reset allowed).

### App Modules

#### `settings` — Flash-backed configuration
- Stored at `0x080E0000` (sector 7, last 128KB). Magic `0xC0FFEE02`, uint16 checksum.
- `Settings_t` padded to 32 bytes for STM32H7 flash word (`FLASH_TYPEPROGRAM_FLASHWORD`).
- Uses `SCB_CleanInvalidateDCache_by_Addr` / `SCB_InvalidateDCache_by_Addr` around erase/program.
- Defaults applied if magic or checksum fails. **Magic must be incremented whenever the struct layout changes.**

```c
typedef struct {
    uint32_t magic;                   // 0xC0FFEE02
    uint16_t oil_pressure_delay_s;    // default 5
    uint16_t pressure_min_psi;        // default 80  (low pressure alarm threshold)
    uint16_t pressure_max_psi;        // default 100 (normal cut-out)
    uint16_t pressure_span_psi;       // hysteresis deadband, default 10
    uint16_t auto_restart_en;         // 0=manual reset, 1=auto, default 1
    uint16_t pressure_high_alarm_psi; // default 110 (alarm, enter STOPPING)
    uint16_t pressure_high_high_psi;  // default 120 (emergency shutdown, enter FAULT)
    uint16_t checksum;
    uint8_t  _pad[12];                // pad to 32 bytes
} Settings_t;
```

#### `alarms` — Alarm management
- Bitmask (uint8_t, 6 bits used):

| Bit | Constant | Cleared by |
|---|---|---|
| 0x01 | `ALARM_OIL_TIMEOUT` | FAULT reset / enter IDLE |
| 0x02 | `ALARM_PRESSURE_FAULT` | FAULT reset / enter IDLE |
| 0x04 | `ALARM_ESTOP` | E-stop reset path |
| 0x08 | `ALARM_LOW_PRESSURE` | FAULT reset / enter IDLE |
| 0x10 | `ALARM_HIGH_PRESSURE` | enter IDLE (auto-cleared when pressure normalises) |
| 0x20 | `ALARM_HIGH_HIGH_PRES` | FAULT reset / enter IDLE |

- `Alarms_Set()` marks unacknowledged. `Alarms_Acknowledge()` marks acknowledged (clears unack flag).
- `Alarms_GetUnacknowledged()` used by UI to show the alarm indicator.

#### `compressor_sm` — State machine

```
IDLE → STARTING → RUNNING → STOPPING → IDLE (or STARTING if auto_restart_en)
          │            │         │
     oil timeout   oil fault  high-high → FAULT
     low pressure  hi alarm → STOPPING
          └──→ FAULT ←──────────┘
[any state] + estop pin HIGH → ESTOP
```

Full SM transition logic:

| State | Condition | Action |
|---|---|---|
| STARTING | oil switch closes | → RUNNING |
| STARTING | elapsed ≥ oil_delay AND !oil_ok | ALARM_OIL_TIMEOUT; if psi < min_psi: ALARM_LOW_PRESSURE; → FAULT |
| RUNNING | !oil_ok | → FAULT |
| RUNNING | psi ≥ high_high_psi | ALARM_HIGH_HIGH_PRES → FAULT |
| RUNNING | psi ≥ high_alarm_psi | ALARM_HIGH_PRESSURE → STOPPING |
| RUNNING | psi ≥ max_psi | → STOPPING (normal cycle) |
| RUNNING | psi < high_alarm_psi | Alarms_Clear(HIGH_PRESSURE) |
| STOPPING | psi ≥ high_high_psi | ALARM_HIGH_HIGH_PRES → FAULT |
| STOPPING | psi ≤ max_psi − span_psi | → STARTING (auto) or IDLE (manual) |
| FAULT | SM_RequestReset() | clear oil/pressure/low/hihi alarms → IDLE |
| ESTOP | pin clears AND reset | clear ESTOP alarm → IDLE |

- `SM_GetOilOk()` exposes the internal `s_oil_ok` flag (used by UI oil pressure indicator).
- E-stop reset requires both: pin goes LOW (`DigIn_ConsumeEStopReset`) AND `SM_RequestReset()` from UI.

### UI Screens (320×240 landscape)

All screens use `lcdFillRect`, `lcdSetTextFont`, `lcdSetCursor`, `lcdPrintf`. Screen switching sets `s_full_redraw = true`.

#### Main screen (`ui_main`)
- **Header (Y 0–31)**: state name + PSI, colored by state (GREEN=running, YELLOW=starting, CYAN=stopping, RED=fault, MAGENTA=e-stop).
- **Centre (Y 32–171)**: large PSI number; below it in Font12 cyan: `CUT-IN: NNN PSI` / `CUT-OUT: NNN PSI` (live from settings). Shows `SENSOR FAULT` in red if ADC fault.
- **Status strip (Y 172–197)**: split across full width:
  - Left 240px: alarm banner (red `! ALARM - tap ALARMS`) or black.
  - Right 80px (above SETTINGS button): oil pressure indicator — GREEN `OIL OK` / YELLOW `OIL WAIT` (SM_STARTING) / RED `OIL LOW`.
- **Button bar (Y 200–239)**: START/STOP (toggles label+color) | RESET | ALARMS | SETTINGS.

#### Settings screen (`ui_settings`)
- `UI_Settings_Enter()` must be called before showing (copies current settings to local edit buffer).
- **Seven rows** with [-]/[+] buttons (ROW_H=24px): oil delay, min PSI, max PSI, hysteresis, auto-restart, high alarm PSI, high-high stop PSI.
- CANCEL discards; SAVE calls `Settings_Save()` and returns to main.

#### Alarms screen (`ui_alarms`)
- **Six rows** (ROW_H=26px): oil timeout, low discharge pressure, pressure sensor fault, high pressure alarm, high-high shutdown, emergency stop.
- Active+unacknowledged = red; active+acknowledged = dark grey; inactive = black.
- Tap header to return to main. Tap bottom bar to acknowledge all.

### Compressor State Machine

**Relay rule**: PG4 HIGH = compressor runs; PG4 LOW = compressor stops. PG5 is unused.

| State | PG4 RUN relay |
|---|---|
| IDLE / STOPPED | LOW |
| STARTING / RUNNING | HIGH |
| STOPPING / FAULT / E-STOP | LOW |

## Hardware Summary

- **MCU**: STM32H723ZGTx on NUCLEO-H723ZG, 480MHz (HSE 8MHz → PLL1, VOS0)
- **Power**: USB via CN1 (ST-Link connector) powers the board by default (JP2 pins 1-2, STLK). For external 5V, connect to CN11 pin 6 and move JP2 to pins 5-6 (EXT). JP3 is the 2-pin T_NRST jumper (keep installed). JP5 pins 1-2 sets VDD = 3.3V (default).
- **Debug**: SWD only — USART3 is consumed by FMC (PD8/PD9 = FMC_D13/D14), no ST-Link VCP
- **Heartbeat LED**: LD1 (Green, PB0) toggled at 1Hz in the main loop — confirms firmware is running after first flash.

### Key Pin Assignments

CN7–CN10 are the Zio/Arduino female sockets (populated). CN11/CN12 are the Morpho 2×38 footprints (★ = unpopulated from factory — solder a male pin header to access).

| Function | Pin | NUCLEO Connector | Notes |
|---|---|---|---|
| FMC_A0 (RS/DC) | PF0 | CN9 pin 21 | AF12; 0x60000000=cmd, 0x60000002=data |
| FMC_NE1 (LCD CS) | PD7 | CN9 pin 2 | AF12; Bank 1 chip select |
| FMC_NOE (RD) | PD4 | CN9 pin 8 | AF12; read strobe |
| FMC_NWE (WR) | PD5 | CN9 pin 6 | AF12; write strobe |
| FMC_D0, D1 | PD14, PD15 | CN7 pin 14, 16 | AF12; 16-bit data bus |
| FMC_D2, D3 | PD0, PD1 | CN9 pin 23, 25 | AF12 |
| FMC_D4–D12 | PE7–PE15 | CN10 P20, P18, P4, P24, P6, P26, P10, P8, P30 | AF12 (D4=PE7=P20 … D12=PE15=P30) |
| FMC_D13–D15 | PD8–PD10 | CN12-P10★, CN11-P69★, CN12-P65★ | AF12; USART3 must be disabled |
| LCD_RST | PG2 | CN8 pin 14 | GPIO output, init HIGH; library uses soft-reset only |
| BL_CTRL | PC6 | CN7 pin 1 | TIM3_CH1 PWM AF2, 1kHz, 75% default; LCD_BL_ON/OFF ineffective in AF mode |
| TP_CS | PF10 | CN9 pin 11 | SPI5 touch chip select |
| TP_IRQ | PG3 | CN8 pin 16 | EXTI3 falling edge, pull-up; polled by pin state in XPT2046_Read |
| SPI5_SCK | PF7 | CN9 pin 26 | SPI5 AF5 |
| SPI5_MISO | PF8 | CN9 pin 24 | SPI5 AF5 |
| SPI5_MOSI | PF9 | CN9 pin 28 | SPI5 AF5 |
| Oil pressure switch | PC0 | CN9 pin 3 | EXTI0, both edges, pull-up, NC contact |
| Emergency stop | PC2 | CN9 pin 9 | EXTI2, falling edge, pull-up, NC contact |
| RUN relay | PG4 | CN12 pin 69★ | Output, init LOW; HIGH = compressor runs |
| (unused) | PG5 | CN12 pin 68★ | Output, init LOW |
| Pressure 4-20mA | PA3 | CN9 pin 1 | ADC1_INP15; 150Ω shunt → 0.6–3.0V |
| Heartbeat LED | PB0 | LD1 (on-board) | LD1 Green (NUCLEO built-in); toggled 1Hz in firmware |

150Ω resistor between PA3 and GND (4mA→0.6V, 20mA→3.0V). Add 100nF decoupling cap.

## CubeMX Notes (for regeneration)

- FMC: **HAL** driver (no LL NOR/PSRAM option)
- SPI5, ADC1, TIM3, GPIO, RCC, CORTEX_M7: **LL** driver
- FMC address width: **1 bit** (A0 only, PF0); do not use A18
  - Note: generated `fmc.c` configures PD13 as `FMC_A18` — this is harmless; all LCD accesses use addresses 0x60000000/0x60000002 which keep A18=0
- USART3: **disabled** (frees PD8/PD9 for FMC_D13/D14)
- FMC timing at 240MHz AHB3: Address setup=1, Data setup=15, Bus turnaround=0, Mode A
- NVIC Priority Group: **4** (4 bits preemption, 0 sub-priority)
- ADC: polled in main loop; NVIC enabled by CubeMX but no ADC interrupt sources are enabled — ADC_IRQHandler is empty and harmless
- TIM3: prescaler=239 (1MHz at 240MHz PCLK1), ARR=999 (1kHz), CCR1=750 (75%)

## Reference Documents

| File | Contents |
|---|---|
| `ui_mockup.html` | Interactive 2× scale UI mockup of all three screens with live controls |
| `flowchart.html` | State machine flowchart — Mermaid diagram, threshold table, alarm table |
| `wiring_diagram.html` | Full SVG wiring diagram — all pin connections, relay module, power, J-Link |
| `Documentation/3.2inch-320-240-Touch-LCD-SCH-2.pdf` | LCD module schematic |
| `Documentation/XPT2046-EN.pdf` | Touch controller datasheet |
| `Documentation/ILI9325_datasheet.pdf` | Display controller datasheet |
| `Documentation/NUCLEO-H723ZG-e01_schematic.pdf` | NUCLEO board schematic |
| `Documentation/STM32H723zg Datasheet.pdf` | MCU datasheet |
| `Documentation/STM32 H723 Reference Manaul.pdf` | MCU reference manual |
