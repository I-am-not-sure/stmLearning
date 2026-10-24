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
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "device_app.h"
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
  MX_TIM1_Init();
  MX_I2C1_Init();
  MX_I2C2_Init();
  MX_TIM8_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  /* [正常工作：原样保留] 以下启动序列来自line_ide_cmake，
   * 8路PWM启动、1470/1500设置、顺序及延时全部保持原样。 */

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

  
  /* [正常工作] 电机启动完成后再启动两个传感器，主循环开始持续推进。 */
  DevicesApp_Init();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* [正常工作：循环调度]
     * 1. 推进MS5837深度状态机；2. 推进JY61P姿态状态机；
     * 3. 每秒请求一次串口DMA输出，两路采样不会等待串口发送。
     * 具体正常转换和异常报错/恢复分区见device_app.c。
     * 原电机工程循环中的CH4测试值和HAL_Delay(2000)不加入调度，
     * PWM继续保持启动代码设置的值，等后续串口命令协议确定再控制。
     */
    DevicesApp_Process();
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
