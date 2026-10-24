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
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* [主流程] 传感器状态类型已移到 ms5837.h，由驱动统一管理。 */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* [配置] 驱动内部完成 10 次基准采集，然后每秒测量一次。 */
#define MS5837_REFERENCE_SAMPLES 10U
#define MS5837_OUTPUT_INTERVAL_MS 1000U
/* [失败处理配置] 出错时每秒尝试输出一次故障信息。 */
#define MS5837_ERROR_REPORT_INTERVAL_MS 1000U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* [主流程] 串口 DMA 保持独立缓冲区，发送完成前不能覆盖。 */
static uint8_t uart1_tx_buffer[96];
static volatile uint8_t uart1_tx_busy = 0U;
static volatile uint32_t uart1_tx_error = HAL_UART_ERROR_NONE;
/* [调试记录] 启动请求结果以及最近一次串口发送请求结果。 */
static volatile HAL_StatusTypeDef uart1_tx_status = HAL_OK;
static HAL_StatusTypeDef sensor_start_status = HAL_OK;
static MS5837_Sample sensor_sample;
static MS5837_Error sensor_error;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* [主流程] 仅由 main() 调用；复制数据后启动串口 DMA。 */
static HAL_StatusTypeDef UART1_SendDMA(const char *data, uint16_t length)
{
    /* [参数检查] 输入和 DMA 缓冲区容量必须合法。 */
    if (data == NULL || length == 0U || length > sizeof(uart1_tx_buffer))
    {
        return HAL_ERROR;
    }
    /* [失败处理] 已记录异步错误时暂停发送，尚未实现自动恢复。 */
    if (uart1_tx_error != HAL_UART_ERROR_NONE)
    {
        return HAL_ERROR;
    }
    /* [忙状态] 上一包未发完，本次不覆盖缓冲区、不入队。 */
    if (uart1_tx_busy != 0U)
    {
        return HAL_BUSY;
    }

    /* [主流程] 先设置忙标志，再启动 DMA，避免完成回调的时序竞态。 */
    memcpy(uart1_tx_buffer, data, length);
    uart1_tx_busy = 1U;
    HAL_StatusTypeDef status = HAL_UART_Transmit_DMA(
        &huart1, uart1_tx_buffer, length);

    /* [失败处理] 启动失败不会产生正常完成回调，立即解除忙状态。 */
    if (status != HAL_OK)
    {
        uart1_tx_busy = 0U;
    }
    return status;
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
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  /* [主流程] 只提交启动请求，复位/PROM/CRC/基准采集由驱动逐步执行。
   * 基准建立期间，传感器仍需保持在空气/水面参考位置。 */
  sensor_start_status = MS5837_StartDMA(
      &hi2c1, MS5837_REFERENCE_SAMPLES, MS5837_OUTPUT_INTERVAL_MS);
  /* [失败处理计时] 允许第一次循环立即检查启动请求是否失败。 */
  uint32_t last_error_report = HAL_GetTick() - MS5837_ERROR_REPORT_INTERVAL_MS;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* [主流程] 每轮推进传感器状态；所有等待都由驱动中的时间判断实现。 */
    MS5837_ProcessDMA();

    /* [主流程] 有一轮完整结果时，取出并通过串口 DMA 输出。 */
    if (MS5837_TakeSample(&sensor_sample) != 0U)
    {
      char buffer[96];
      // int length = snprintf(buffer, sizeof(buffer), "Px100=%ld Dx100=%ld\r\n",
      //     (long)sensor_sample.pressure_x100, (long)sensor_sample.depth_cm_x100);
      int length = snprintf(buffer, sizeof(buffer),
    "Px100=%ld Dx100=%ld Tx100=%ld\r\n",
    (long)sensor_sample.pressure_x100,
    (long)sensor_sample.depth_cm_x100,
    (long)sensor_sample.temperature_x100);
      /* [结果检查] 只提交完整、合法的格式化文本。 */
      if (length > 0 && length < (int)sizeof(buffer))
      {
        uart1_tx_status = UART1_SendDMA(buffer, (uint16_t)length);
      }
    }

    /* [失败处理] 与上方正常测量输出分开：每秒检查并报告一次故障。 */
    uint32_t now = HAL_GetTick();
    if ((uint32_t)(now - last_error_report) >= MS5837_ERROR_REPORT_INTERVAL_MS)
    {
      last_error_report = now;
      char buffer[96];
      int length = 0;
      if (sensor_start_status != HAL_OK)
      {
        length = snprintf(buffer, sizeof(buffer), "MS5837 START FAIL status=%u\r\n",
            (unsigned int)sensor_start_status);
      }
      else if (MS5837_GetError(&sensor_error) != 0U)
      {
        length = snprintf(buffer, sizeof(buffer),
            "MS5837 FAIL state=%u cause=%u status=%u err=0x%08lX\r\n",
            (unsigned int)sensor_error.state, (unsigned int)sensor_error.cause,
            (unsigned int)sensor_error.status, (unsigned long)sensor_error.hal_error);
      }
      if (length > 0 && length < (int)sizeof(buffer))
      {
        uart1_tx_status = UART1_SendDMA(buffer, (uint16_t)length);
      }
    }

    /* [主流程] 其他任务放这里；无需等待初始化、转换或采样周期。 */
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
/* [主流程：中断回调] 转发 IIC 完成通知，标志由传感器驱动管理。 */
void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    MS5837_OnTxComplete(hi2c);
}
void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    MS5837_OnRxComplete(hi2c);
}

/* [失败处理：中断回调] 仅转发错误通知，不在中断中恢复或等待。 */
void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
    MS5837_OnI2CError(hi2c);
}

/* [主流程：中断回调] 串口整包发送完成，允许缓冲区复用。 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart1)
    {
        uart1_tx_busy = 0U;
    }
}

/* [失败处理：中断回调] 保存串口错误，沿用暂停后续发送的策略。 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart1)
    {
        uart1_tx_error = HAL_UART_GetError(huart);
    }
}
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
