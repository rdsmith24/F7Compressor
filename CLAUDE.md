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

The project is at **bring-up step 1**: CubeMX has generated `Core/` and `Drivers/`, but the following are not yet done:

- **Makefile**: USER CODE sections added; all BSP and App sources compile cleanly.
- **`BSP/Inc/ili9341.h`**: library modifications applied (`LCD_BASE1 = 0x60000002`, LL GPIO backlight macros).
- **`BSP/Inc/`**: `xpt2046_spi.h`, `pressure_adc.h`, `relay.h`, `digital_in.h` are empty stubs.
- **`BSP/Src/`**: `xpt2046_spi.c`, `pressure_adc.c`, `relay.c`, `digital_in.c` are empty stubs.
- **`App/Inc/`** and **`App/Src/`**: all files are empty stubs.

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

The main loop polls ADC (~10Hz) and the touch panel. ISRs handle E-Stop (EXTI2, PC2) and oil pressure switch (EXTI0, PC0).

### HAL vs LL Driver Mixing

The project intentionally uses **both** HAL and LL:
- **FMC uses HAL**: no LL option exists for NOR/PSRAM init. This also makes `HAL_Delay()` and `HAL_GetTick()` available project-wide, so `ili9341.c` can call them without modification.
- **All other peripherals use LL** (SPI5, ADC1, TIM3, GPIO, RCC, CORTEX_M7): lower overhead, direct register access.

Both `USE_HAL_DRIVER` and `USE_FULL_LL_DRIVER` are defined project-wide.

### ILI9341 Library Modifications

Source: https://github.com/taburyak/STM32-ILI9341-320x240-FSMC-Library

Two changes applied to `BSP/Inc/ili9341.h`:

```c
// 1. A0 address bit, not A18:
#define LCD_BASE1  ((uint32_t)0x60000002)   // was 0x60080000

// 2. LL GPIO for backlight (PC6):
#define LCD_BL_ON()   LL_GPIO_SetOutputPin(GPIOC, LL_GPIO_PIN_6)
#define LCD_BL_OFF()  LL_GPIO_ResetOutputPin(GPIOC, LL_GPIO_PIN_6)
```

FMC address map: `0x60000000` = command (A0=0), `0x60000002` = data (A0=1). Only PF0 (FMC_A0) is wired; do not use the A18/`0x60080000` approach.

Display must be initialized in Landscape:
```c
lcdInit();
lcdSetOrientation(LCD_ORIENTATION_LANDSCAPE);  // 320×240
```

### Compressor State Machine

```
IDLE → STARTING → RUNNING → STOPPING → IDLE
         │            │
         │ (oil timeout) │ (p>max or alarm)
         └──→ FAULT ←──┘
[any state] → ESTOP  (requires manual reset to return to IDLE)
```

**Relay rule**: never assert both relays simultaneously. De-assert one, wait 50ms dead-time, then assert the other.

| State | PG4 START relay | PG5 STOP relay |
|---|---|---|
| IDLE / STOPPED | LOW | LOW |
| STARTING / RUNNING | HIGH | LOW |
| STOPPING / FAULT / E-STOP | LOW | HIGH |

### Settings (persisted to flash)

```c
typedef struct {
    uint32_t magic;                // 0xC0FFEE01
    uint16_t oil_pressure_delay_s;
    uint16_t pressure_min_psi;
    uint16_t pressure_max_psi;
    uint16_t pressure_span_psi;    // hysteresis deadband
    uint16_t auto_restart_en;      // 0=manual reset, 1=auto
    uint16_t checksum;
} Settings_t;
```

## Hardware Summary

- **MCU**: STM32H723ZGTx on NUCLEO-H723ZG, 480MHz (HSE 8MHz → PLL1, VOS0)
- **Power**: External 5VDC on CN9 pin 6 (E5V); JP3 jumper in E5V position (1-2)
- **Debug**: SWD only — USART3 is consumed by FMC (PD8/PD9 = FMC_D13/D14), no ST-Link VCP

### Key Pin Assignments

| Function | Pin | Notes |
|---|---|---|
| FMC_A0 (RS/DC) | PF0 | AF12; 0x60000000=cmd, 0x60000002=data |
| FMC_D13/D14 | PD8/PD9 | AF12; USART3 must be disabled |
| LCD_RST | PG2 | GPIO output, init HIGH |
| BL_CTRL | PC6 | TIM3_CH1 PWM, 1kHz, 75% default |
| TP_CS | PF10 | SPI5 touch chip select |
| TP_IRQ | PG3 | EXTI falling edge, pull-up |
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
- ADC: polled in main loop, interrupt disabled
- TIM3: prescaler=119 (1MHz), ARR=999 (1kHz), CCR1=750 (75%)

## Bring-up Sequence

1. Add Makefile USER CODE blocks → verify compile
2. Apply `ili9341.h` modifications → FMC init → draw colored rectangle
3. Text rendering with fonts
4. XPT2046 touch → raw coordinates → calibration
5. ADC pressure → verify 0.6–3.0V maps to PSI
6. Relay outputs + oil switch/E-Stop inputs with debounce
7. Compressor state machine wired to real I/O
8. Flash settings read/write
9. Full UI integration
