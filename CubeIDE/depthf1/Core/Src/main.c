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

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static uint8_t sensor_initialized;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static HAL_StatusTypeDef UART2_SendDepth(MS5837_StatusTypeDef status)
{
    char buf[224];
    int len;

    if (status == MS5837_OK) {
        len = snprintf(buf, sizeof(buf),
                       "OK model=%s Pressure=%.2f mbar Temp=%.2f C Depth=%.2f cm Ref=%.2f D1=%lu D2=%lu\r\n",
                       MS5837_ModelName(), (double)Pressure_mbar,
                       (double)Temperature, (double)Depth, (double)Atmosphere_mbar,
                       (unsigned long)D1_Pres, (unsigned long)D2_Temp);
    } else {
        len = snprintf(buf, sizeof(buf),
                       "ERR %s step=%s HAL=%u I2C=0x%08lX SCL=%u SDA=%u BUSY=%u PROM[%u] ver=0x%02X CRC=%u/%u D1=%lu D2=%lu\r\n",
                       MS5837_StatusName(status), MS5837_Diagnostics.stage,
                       (unsigned int)MS5837_Diagnostics.hal_status,
                       (unsigned long)MS5837_Diagnostics.hal_error,
                       (unsigned int)HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_6),
                       (unsigned int)HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7),
                       (unsigned int)(__HAL_I2C_GET_FLAG(&hi2c1, I2C_FLAG_BUSY) != RESET),
                       (unsigned int)MS5837_Diagnostics.prom_index,
                       (unsigned int)MS5837_Diagnostics.version,
                       (unsigned int)MS5837_Diagnostics.crc_actual,
                       (unsigned int)MS5837_Diagnostics.crc_expected,
                       (unsigned long)D1_Pres, (unsigned long)D2_Temp);
    }

    if (len <= 0 || len >= (int)sizeof(buf)) {
        return HAL_ERROR;
    }

    return HAL_UART_Transmit(&huart2,
                             (uint8_t *)buf,
                             (uint16_t)len,
                             100);
}

static void UART2_SendPROM(void)
{
    char buf[160];
    int len = snprintf(buf, sizeof(buf),
                       "PROM C0=%04lX C1=%lu C2=%lu C3=%lu C4=%lu C5=%lu C6=%lu\r\n",
                       (unsigned long)Cal_C[0], (unsigned long)Cal_C[1],
                       (unsigned long)Cal_C[2], (unsigned long)Cal_C[3],
                       (unsigned long)Cal_C[4], (unsigned long)Cal_C[5],
                       (unsigned long)Cal_C[6]);
    if (len > 0 && len < (int)sizeof(buf)) {
        HAL_UART_Transmit(&huart2, (uint8_t *)buf, (uint16_t)len, 100);
    }
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
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  static const char banner[] = "MS5837 diag-v2 I2C1 PB6/PB7 addr=0x76 UART2=115200\r\n";
  HAL_Delay(100);
  HAL_UART_Transmit(&huart2, (uint8_t *)banner, sizeof(banner) - 1, 100);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    MS5837_StatusTypeDef status;
    if (!sensor_initialized) {
      status = MS5837_init();
      sensor_initialized = status == MS5837_OK;
      if (Cal_C[0] != 0 || Cal_C[1] != 0) {
        UART2_SendPROM();
      }
      if (sensor_initialized) {
        status = MS5837_Getdata();
      }
    } else {
      status = MS5837_Getdata();
    }
    UART2_SendDepth(status);
    HAL_Delay(1000);
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

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
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
