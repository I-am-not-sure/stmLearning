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
#include "dma.h"
#include "i2c.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ms5837.h"
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define MS5837_REFERENCE_SAMPLES 10U
#define MS5837_OUTPUT_INTERVAL_MS 1000U
#define MS5837_UART_TIMEOUT_MS 100U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  uint16_t prom[7] = {0};
  int32_t reference_x100 = 0;
  HAL_StatusTypeDef init_status = MS5837_Init(&hi2c1, prom);
  HAL_StatusTypeDef reference_status = HAL_ERROR;
  char init_buffer[64];
  int init_length;

  if (init_status != HAL_OK)
  {
    init_length = snprintf(
        init_buffer, sizeof(init_buffer),
        "MS5837 INIT status=%u\r\n", (unsigned int)init_status);

    if (init_length > 0 && init_length < (int)sizeof(init_buffer))
    {
      HAL_UART_Transmit(&huart1, (uint8_t *)init_buffer,
                        (uint16_t)init_length, MS5837_UART_TIMEOUT_MS);
    }
  }
  else
  {
    /* Keep the sensor in air during startup: this pressure becomes zero depth. */
    HAL_Delay(1000);
    reference_status = MS5837_ReadReference(
        &hi2c1, prom, MS5837_REFERENCE_SAMPLES, &reference_x100);

    if (reference_status != HAL_OK)
    {
      init_length = snprintf(
          init_buffer, sizeof(init_buffer),
          "MS5837 REF status=%u REF_x100=%ld\r\n",
          (unsigned int)reference_status, (long)reference_x100);

      if (init_length > 0 && init_length < (int)sizeof(init_buffer))
      {
        HAL_UART_Transmit(&huart1, (uint8_t *)init_buffer,
                          (uint16_t)init_length, MS5837_UART_TIMEOUT_MS);
      }
    }
  }
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    char buffer[96];
    int length;

    if (init_status == HAL_OK && reference_status == HAL_OK)
    {
      uint32_t d1 = 0;
      uint32_t d2 = 0;
      int32_t temperature_x100 = 0;
      int32_t pressure_x100 = 0;

      HAL_StatusTypeDef read_status = MS5837_Read(
          &hi2c1, prom, &d1, &d2, &temperature_x100, &pressure_x100);

      if (read_status == HAL_OK)
      {
        int32_t depth_cm_x100 = MS5837_CalculateDepth(
            pressure_x100, reference_x100);

        /* Px100: mbar * 100 (Pa); Dx100: cm * 100. */
        length = snprintf(
            buffer, sizeof(buffer), "Px100=%ld Dx100=%ld\r\n",
            (long)pressure_x100, (long)depth_cm_x100);
      }
      else
      {
        length = snprintf(
            buffer, sizeof(buffer), "MS5837 READ FAIL status=%u\r\n",
            (unsigned int)read_status);
      }
    }
    else
    {
      length = snprintf(
          buffer, sizeof(buffer), "NOT READY init=%u ref=%u\r\n",
          (unsigned int)init_status, (unsigned int)reference_status);
    }

    if (length > 0 && length < (int)sizeof(buffer))
    {
      /* Blocking transmission, even though USART1 DMA is configured. */
      HAL_UART_Transmit(&huart1, (uint8_t *)buffer,
                        (uint16_t)length, MS5837_UART_TIMEOUT_MS);
    }

    HAL_Delay(MS5837_OUTPUT_INTERVAL_MS);
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

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
