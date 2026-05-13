/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "spi.h"
#include "tim.h"
#include "gpio.h"
#include "fmc.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ili9341.h"
#include "xpt2046_spi.h"
#include "pressure_adc.h"
#include "relay.h"
#include "digital_in.h"
#include "settings.h"
#include "alarms.h"
#include "compressor_sm.h"
#include "ui_main.h"
#include "ui_settings.h"
#include "ui_alarms.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
typedef enum { SCREEN_MAIN, SCREEN_SETTINGS, SCREEN_ALARMS } Screen_t;
static Screen_t s_screen = SCREEN_MAIN;
static bool     s_full_redraw = true;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void PeriphCommonClock_Config(void);
static void MPU_Config(void);
/* USER CODE BEGIN PFP */
void UI_ShowMain(void);
void UI_ShowSettings(void);
void UI_ShowAlarms(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void UI_ShowMain(void)
{
    s_screen = SCREEN_MAIN;
    s_full_redraw = true;
}

void UI_ShowSettings(void)
{
    UI_Settings_Enter();
    s_screen = SCREEN_SETTINGS;
    s_full_redraw = true;
}

void UI_ShowAlarms(void)
{
    s_screen = SCREEN_ALARMS;
    s_full_redraw = true;
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* Configure the peripherals common clocks */
  PeriphCommonClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_FMC_Init();
  MX_SPI5_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */

  /* --- BSP init --- */
  /* Backlight: start TIM3 PWM on PC6 (AF2, already configured by MX_TIM3_Init) */
  LL_TIM_EnableCounter(TIM3);
  LL_TIM_CC_EnableChannel(TIM3, LL_TIM_CHANNEL_CH1);

  /* Display */
  lcdInit();
  lcdSetOrientation(LCD_ORIENTATION_LANDSCAPE);

  /* Touch */
  XPT2046_Init();

  /* Pressure ADC: calibrate + enable */
  PADC_Init();

  /* Relay outputs */
  Relay_Init();

  /* Digital inputs: read initial states */
  DigIn_Init();

  /* --- App init --- */
  Alarms_Init();
  Settings_Init();
  SM_Init(Settings_Get());

  /* Sync SM with actual input states */
  SM_SetOilSwitch(DigIn_IsOilPressureOK());
  SM_SetEStop(DigIn_IsEStopActive());

  /* Initial screen */
  s_screen = SCREEN_MAIN;
  s_full_redraw = true;

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  uint32_t adc_tick = 0;
  uint32_t ui_tick  = 0;

  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    /* ADC at ~10 Hz */
    if (HAL_GetTick() - adc_tick >= 100u) {
        adc_tick = HAL_GetTick();
        PADC_Sample();
        if (PADC_IsFault()) Alarms_Set(ALARM_PRESSURE_FAULT);
        else                Alarms_Clear(ALARM_PRESSURE_FAULT);
    }

    /* Relay dead-time management */
    Relay_Update();

    /* Synchronise digital-input state to SM */
    SM_SetOilSwitch(DigIn_IsOilPressureOK());
    SM_SetEStop(DigIn_IsEStopActive());
    if (DigIn_ConsumeEStopReset()) {
        /* Physical e-stop released — allow manual reset via UI */
    }

    /* State machine */
    SM_Update(PADC_GetPSI());

    /* Touch */
    Touch_t touch = {0};
    if (XPT2046_Read(&touch) && touch.pressed) {
        switch (s_screen) {
        case SCREEN_MAIN:     UI_Main_Touch(touch.x, touch.y);     break;
        case SCREEN_SETTINGS: UI_Settings_Touch(touch.x, touch.y); break;
        case SCREEN_ALARMS:   UI_Alarms_Touch(touch.x, touch.y);   break;
        }
    }

    /* UI at ~20 Hz */
    if (HAL_GetTick() - ui_tick >= 50u) {
        ui_tick = HAL_GetTick();
        switch (s_screen) {
        case SCREEN_MAIN:     UI_Main_Draw(s_full_redraw);     break;
        case SCREEN_SETTINGS: UI_Settings_Draw(s_full_redraw); break;
        case SCREEN_ALARMS:   UI_Alarms_Draw(s_full_redraw);   break;
        }
        s_full_redraw = false;
    }

  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  LL_FLASH_SetLatency(LL_FLASH_LATENCY_3);
  while(LL_FLASH_GetLatency()!= LL_FLASH_LATENCY_3)
  {
  }
  LL_PWR_ConfigSupply(LL_PWR_LDO_SUPPLY);
  LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE0);
  while (LL_PWR_IsActiveFlag_VOS() == 0)
  {
  }
  LL_RCC_HSE_Enable();

   /* Wait till HSE is ready */
  while(LL_RCC_HSE_IsReady() != 1)
  {

  }
  LL_RCC_PLL_SetSource(LL_RCC_PLLSOURCE_HSE);
  LL_RCC_PLL1P_Enable();
  LL_RCC_PLL1R_Enable();
  LL_RCC_PLL1_SetVCOInputRange(LL_RCC_PLLINPUTRANGE_8_16);
  LL_RCC_PLL1_SetVCOOutputRange(LL_RCC_PLLVCORANGE_WIDE);
  LL_RCC_PLL1_SetM(1);
  LL_RCC_PLL1_SetN(60);
  LL_RCC_PLL1_SetP(1);
  LL_RCC_PLL1_SetQ(4);
  LL_RCC_PLL1_SetR(2);
  LL_RCC_PLL1_Enable();

   /* Wait till PLL is ready */
  while(LL_RCC_PLL1_IsReady() != 1)
  {
  }

   /* Intermediate AHB prescaler 2 when target frequency clock is higher than 80 MHz */
   LL_RCC_SetAHBPrescaler(LL_RCC_AHB_DIV_2);

  LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL1);

   /* Wait till System clock is ready */
  while(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL1)
  {

  }
  LL_RCC_SetSysPrescaler(LL_RCC_SYSCLK_DIV_1);
  LL_RCC_SetAHBPrescaler(LL_RCC_AHB_DIV_2);
  LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_2);
  LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_2);
  LL_RCC_SetAPB3Prescaler(LL_RCC_APB3_DIV_2);
  LL_RCC_SetAPB4Prescaler(LL_RCC_APB4_DIV_2);
  LL_SetSystemCoreClock(480000000);

   /* Update the time base */
  if (HAL_InitTick (TICK_INT_PRIORITY) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief Peripherals Common Clock Configuration
  * @retval None
  */
void PeriphCommonClock_Config(void)
{
  LL_RCC_PLL2P_Enable();
  LL_RCC_PLL2_SetVCOInputRange(LL_RCC_PLLINPUTRANGE_8_16);
  LL_RCC_PLL2_SetVCOOutputRange(LL_RCC_PLLVCORANGE_WIDE);
  LL_RCC_PLL2_SetM(1);
  LL_RCC_PLL2_SetN(24);
  LL_RCC_PLL2_SetP(2);
  LL_RCC_PLL2_SetQ(2);
  LL_RCC_PLL2_SetR(2);
  LL_RCC_PLL2_Enable();

   /* Wait till PLL is ready */
  while(LL_RCC_PLL2_IsReady() != 1)
  {
  }

}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{

  /* Disables the MPU */
  LL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  LL_MPU_ConfigRegion(LL_MPU_REGION_NUMBER0, 0x87, 0x0, LL_MPU_REGION_SIZE_4GB|LL_MPU_TEX_LEVEL0|LL_MPU_REGION_NO_ACCESS|LL_MPU_INSTRUCTION_ACCESS_DISABLE|LL_MPU_ACCESS_SHAREABLE|LL_MPU_ACCESS_NOT_CACHEABLE|LL_MPU_ACCESS_NOT_BUFFERABLE);
  /* Enables the MPU */
  LL_MPU_Enable(LL_MPU_CTRL_PRIVILEGED_DEFAULT);

}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
