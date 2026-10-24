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
#include "stm32f4xx_hal.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
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
uint8_t  receivedata[64];
volatile uint8_t  rx_ready = 0;   // ISR 里置 1，主循环清 0
volatile uint16_t rx_len   = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/*
// ---- 旧代码（保留，已注释）----
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
  if(huart== &huart1)
  {
      HAL_UART_Transmit_DMA(&huart1, receivedata, Size);
//      __HAL_DMA_DISABLE_IT(&hdma_usart1_rx,DMA_IT_HT);
  }
}
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1) return;

    if (HAL_UARTEx_ReceiveToIdle_DMA(huart, receivedata,
                                    sizeof(receivedata)) != HAL_OK)
    {
        Error_Handler();
    }

    __HAL_DMA_DISABLE_IT(huart->hdmarx, DMA_IT_HT);
}
*/

void parse_command(char *cmd)
{
    int tim, ch, val;

    // 期望格式：p<1|8>c<1..4> <值>，例如 "p1c1 1600"
    if (sscanf(cmd, "p%dc%d %d", &tim, &ch, &val) != 3)
    {
        HAL_UART_Transmit(&huart1, (uint8_t*)"bad\r\n", 5, 100);
        return;
    }

    TIM_HandleTypeDef *htim;
    switch (tim)
    {
        case 1:  htim = &htim1; break;
        case 8:  htim = &htim8; break;
        default:
            HAL_UART_Transmit(&huart1, (uint8_t*)"bad tim\r\n", 9, 100);
            return;
    }

    uint32_t channel;
    switch (ch)
    {
        case 1: channel = TIM_CHANNEL_1; break;
        case 2: channel = TIM_CHANNEL_2; break;
        case 3: channel = TIM_CHANNEL_3; break;
        case 4: channel = TIM_CHANNEL_4; break;
        default:
            HAL_UART_Transmit(&huart1, (uint8_t*)"bad ch\r\n", 8, 100);
            return;
    }

    if (val < 0 || val > 19999) val = 1500;

    __HAL_TIM_SET_COMPARE(htim, channel, (uint32_t)val);

    char ack[32];
    int n = sprintf(ack, "p%dc%d -> %d\r\n", tim, ch, val);
    HAL_UART_Transmit(&huart1, (uint8_t*)ack, n, 100);
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart != &huart1) return;
    rx_len   = Size;
    rx_ready = 1;
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
  MX_TIM1_Init();
  MX_I2C1_Init();
  MX_I2C2_Init();
  MX_TIM8_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  // HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);   // 旧代码（保留，已注释）
  // HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
  // HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);
  // HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4);
  // HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_1);
  // HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_2);
  // HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_3);
  // HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_4);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4);
  HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_2);
  HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_3);
  HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_4);
  HAL_Delay(3000);
__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 1470);
 HAL_Delay(1000);
 __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 1500);
 HAL_Delay(1000);
 __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 1470);
 HAL_Delay(1000);
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, 1470);
// // //  HAL_Delay(1000);
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, 1470);
// // //  HAL_Delay(1000);
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, 1470);
// // //  HAL_Delay(1000);
  __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_1, 1470);
// // //  HAL_Delay(1000);
  __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_2, 1470);
// // //  HAL_Delay(1000);
  __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_3, 1470);
// //  HAL_Delay(1000);
  __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_4, 1470);
  HAL_Delay(3000);
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 1500);
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, 1500);
// // //  HAL_Delay(1000);
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, 1500);
// // //  HAL_Delay(1000);
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, 1500);
  __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_1, 1500);
// // //  HAL_Delay(1000);
  __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_2, 1500);
// // //  HAL_Delay(1000);
  __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_3, 1500);
// //  HAL_Delay(1000);
  __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_4, 1500);
  HAL_Delay(3000);
//  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, 1492);
//HAL_Delay(1000);
//for (uint16_t pwm = 1500; pwm <= 1600; pwm += 10)
//{
//  __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_1, pwm);
//  HAL_Delay(500);
//}
//  __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_1, 1470);
//  HAL_Delay(500);
////  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 2000);
////    HAL_Delay(2000);
////    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 1500);
////      HAL_Delay(2000);
////  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 1500);
////  HAL_Delay(3000);
//  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 1500);
//  HAL_Delay(2000);
//  GPIO_InitTypeDef GPIO_InitStruct = {0};
//
//  /* 先预置高电平，再切换为 GPIO 输出 */
//  HAL_GPIO_WritePin(GPIOE, GPIO_PIN_9, GPIO_PIN_SET);
//
//  GPIO_InitStruct.Pin = GPIO_PIN_9;
//  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
//  GPIO_InitStruct.Pull = GPIO_NOPULL;
//  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
//  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);


  //串口控制
  // const uint8_t hello[] = "hello\r\n";
  // HAL_UART_Transmit(&huart1, hello, sizeof(hello) - 1, 100);
  // HAL_UARTEx_ReceiveToIdle_DMA(&huart1, receivedata, sizeof(receivedata));
  // __HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    //串口控制
    // if (rx_ready)
    // {
    //     rx_ready = 0;

    //     // 回显收到的原始数据（阻塞式，避免和 RX 抢缓冲区）
    //     HAL_UART_Transmit(&huart1, receivedata, rx_len, 100);

    //     if (rx_len < sizeof(receivedata))
    //         receivedata[rx_len] = '\0';
    //     else
    //         receivedata[sizeof(receivedata) - 1] = '\0';

    //     parse_command((char *)receivedata);

    //     HAL_UARTEx_ReceiveToIdle_DMA(&huart1, receivedata, sizeof(receivedata));
    //     __HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT);
    // }

  // ---- 旧代码（保留，已注释）：原每 2 秒强制写 CH1，会覆盖串口设置 ----
  // __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 1700);
  // //  HAL_Delay(2000);
	// __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, 1700);
  // //   HAL_Delay(2000);
	// __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, 1700);
  // //   HAL_Delay(2000);
	// __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, 1700);
  //   HAL_Delay(2000);
	// __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_1, 1700);
  //   HAL_Delay(2000);l
	// __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_2, 1700);
  //   HAL_Delay(2000);
  // __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_3, 1700);
  //   HAL_Delay(2000);
	// __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_4, 1700);
	  HAL_Delay(2000);

//  __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_1, 1600);
//	HAL_Delay(500);
//  HAL_GPIO_WritePin(GPIOE, GPIO_PIN_9, GPIO_PIN_SET);
//    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 10000); 
//	HAL_Delay(2000);
//__HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_1, 1600);
//HAL_Delay(2000);
//__HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_2, 1600);
//HAL_Delay(2000);
//__HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_3, 1700);//未初始化成功
//HAL_Delay(2000);
//__HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_4, 1600);
//HAL_Delay(2000);

//  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 10000);  /* 2.0 ms，3D 模式下接近正向满油门 */
//	// __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, 1600U);
//	 __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 10000U);
//	// __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, 1600U);
//	HAL_Delay(100);
//  for (uint16_t pwm = 1000; pwm <= 2000; pwm += 100)
//  {
//	  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, pwm);
//	  HAL_Delay(2000);   // 每个 PWM 值保持 2 秒
//  }

//	  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, pwm);
//	  HAL_Delay(200);   // 每个 PWM 值保持 2 秒

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
