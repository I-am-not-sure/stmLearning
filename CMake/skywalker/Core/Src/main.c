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
#include "tim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum
{
  APP_STARTUP_STOP = 0,
  APP_AUTO_RUN,
  APP_TEST_COMPLETE
} AppState;

typedef struct
{
  uint32_t uptime_ms;
  uint16_t motor_command;   /* 当前输出的油门脉宽 μs */
  AppState state;
} AppLiveValues;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* Skywalker V2 航模电调油门脉宽，单位 μs（出厂默认行程 1100~1940）
 * TIM3 计数频率 1 MHz（PSC=71、ARR=19999 → 20 ms / 50 Hz），CCR 数值 = 脉宽 μs */
#define ESC_PULSE_MIN         1100u  /* 最低油门：停止 */
#define ESC_PULSE_TEST        1200u  /* 测试油门：低速旋转；若不转可逐步试 1220/1240/1260 */

/* 无按键的一次性安全测试：上电先等待自检，然后低速运行 2 秒并永久停止。
 * 复位 MCU 可重新执行一次；不会循环自动启动电机。 */
#define STARTUP_STOP_MS       8000U  /* 持续最低油门，留出电调接电与自检时间 */
#define AUTO_TEST_RUN_MS      2000U  /* 只自动低速运行一次 */
#define LOOP_PERIOD_MS           1U  /* 主循环节拍 */

#define LED_PIN  GPIO_PIN_7    /* PA7：低电平点亮（运行时亮） */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
UART_HandleTypeDef huart2;

/* 调试时可在 CubeIDE Live Expressions 中观察 app_live */
volatile AppLiveValues app_live = {0};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void MX_USART2_UART_Init(void);
static void App_UartWrite(const char *text);
static void App_ReportState(AppState state);
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

  uint32_t startup_ms;
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
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */

  MX_USART2_UART_Init();

  App_UartWrite(
      "\r\nSTM32F103 + Skywalker V2 no-button test v2.0\r\n"
      "PA6->ESC white(signal), STM32 GND->ESC black\r\n"
      "1100us for 8s, 1200us for 2s, then 1100us permanently\r\n");

  /* 先设最低油门，再启动 PWM：PA6 上电即稳定输出 1100 μs，
   * 电调上电自检要求“油门归零”才能启动 */
  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, ESC_PULSE_MIN);
  if (HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }

  startup_ms = HAL_GetTick();
  App_ReportState(APP_STARTUP_STOP);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    uint32_t now = HAL_GetTick();
    uint32_t elapsed_ms = (uint32_t)(now - startup_ms);
    uint16_t motor = ESC_PULSE_MIN;
    AppState state;

    if (elapsed_ms < STARTUP_STOP_MS)
    {
      /* 上电先稳定发送最低油门，给电调充分时间接电和完成自检。 */
      state = APP_STARTUP_STOP;
    }
    else if (elapsed_ms < (STARTUP_STOP_MS + AUTO_TEST_RUN_MS))
    {
      motor = ESC_PULSE_TEST;
      state = APP_AUTO_RUN;
    }
    else
    {
      /* 一次测试结束后永久保持最低油门，直到 MCU 被复位。 */
      state = APP_TEST_COMPLETE;
    }

    /* 输出油门 */
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, motor);

    /* 先输出停止/新指令，再打印状态 */
    App_ReportState(state);

    /* 绿色 LED：仅在输出非最低油门时点亮 */
    HAL_GPIO_WritePin(GPIOA, LED_PIN,
                      (motor != ESC_PULSE_MIN) ? GPIO_PIN_RESET : GPIO_PIN_SET);

    app_live.uptime_ms = now;
    app_live.motor_command = motor;
    app_live.state = state;

    HAL_Delay(LOOP_PERIOD_MS);
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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/**
  * @brief USART2（PA2=TX, PA3=RX）115200 8-N-1，仅轮询发送，用于状态输出
  */
static void MX_USART2_UART_Init(void)
{
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
}

static void App_UartWrite(const char *text)
{
  size_t length = strlen(text);

  if (length != 0U)
  {
    (void)HAL_UART_Transmit(&huart2, (uint8_t *)text, (uint16_t)length, 200U);
  }
}

static void App_ReportState(AppState state)
{
  static AppState last_state = (AppState)0xFF;
  const char *message = NULL;

  if (state == last_state)
  {
    return;
  }
  last_state = state;

  switch (state)
  {
    case APP_STARTUP_STOP:
      message = "STATE,STARTUP_STOP,1100us; power ESC now\r\n";
      break;
    case APP_AUTO_RUN:
      message = "STATE,AUTO_RUN,1200us for 2s\r\n";
      break;
    case APP_TEST_COMPLETE:
      message = "STATE,TEST_COMPLETE,1100us until reset\r\n";
      break;
    default:
      message = "STATE,UNKNOWN\r\n";
      break;
  }

  App_UartWrite(message);
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* 出错时先关闭 TIM3 输出，再把 PA6 配成普通低电平输出。
   * 仅写 GPIOA->BRR 不能可靠覆盖仍由定时器控制的复用功能引脚。 */
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_TIM3_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  TIM3->CCER &= ~TIM_CCER_CC1E;
  TIM3->CR1 &= ~TIM_CR1_CEN;
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
  GPIO_InitStruct.Pin = GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
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
