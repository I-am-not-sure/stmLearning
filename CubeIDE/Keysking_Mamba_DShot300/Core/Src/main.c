/*
 * KeysKing STM32F103C8T6 + MAMBA F55_BLS two-thruster bench test.
 *
 * Safety behaviour:
 *   - Always sends DShot stop packets during the first 3 seconds.
 *   - Requires both keys released continuously for 1 second before enabling.
 *   - K1/PB12 drives ESC S1; K2/PB13 drives ESC S2.
 *   - Releasing a key immediately commands stop.
 *   - A continuous run is limited to 3 seconds, then release is required.
 */

#include "main.h"
#include "dshot.h"

#include <stdio.h>
#include <string.h>

#define STARTUP_STOP_MS          3000U
#define RELEASE_TO_ARM_MS        1000U
#define BUTTON_DEBOUNCE_MS         20U
#define MAX_CONTINUOUS_RUN_MS    3000U
#define DSHOT_FRAME_PERIOD_MS       1U

/* DShot throttle is 48..2047. 150 is about 5.1% of the usable range. */
#define TEST_THROTTLE_VALUE       150U

#define KEY1_PIN GPIO_PIN_12
#define KEY2_PIN GPIO_PIN_13
#define LED_PIN  GPIO_PIN_7

typedef enum
{
  APP_STARTUP_STOP = 0,
  APP_WAIT_RELEASE,
  APP_READY,
  APP_MOTOR1,
  APP_MOTOR2,
  APP_BOTH,
  APP_TIMEOUT_LOCKOUT
} AppState;

typedef struct
{
  uint32_t uptime_ms;
  uint16_t motor1_command;
  uint16_t motor2_command;
  uint8_t keys;
  uint8_t armed;
  uint8_t timeout_latched;
  AppState state;
  uint32_t frames_sent;
} AppLiveValues;

UART_HandleTypeDef huart2;

/* Add app_live or its fields to CubeIDE Live Expressions while debugging. */
volatile AppLiveValues app_live = {0};

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void App_UartWrite(const char *text);
static uint8_t App_ReadDebouncedKeys(uint32_t now);
static void App_ReportState(AppState state);

static void App_UartWrite(const char *text)
{
  size_t length = strlen(text);

  if (length != 0U)
  {
    (void)HAL_UART_Transmit(&huart2, (uint8_t *)text, (uint16_t)length, 200U);
  }
}

static uint8_t App_ReadDebouncedKeys(uint32_t now)
{
  static uint8_t raw_previous = 0U;
  static uint8_t stable = 0U;
  static uint32_t raw_changed_ms = 0U;
  uint8_t raw = 0U;

  if (HAL_GPIO_ReadPin(GPIOB, KEY1_PIN) == GPIO_PIN_RESET)
  {
    raw |= 0x01U;
  }
  if (HAL_GPIO_ReadPin(GPIOB, KEY2_PIN) == GPIO_PIN_RESET)
  {
    raw |= 0x02U;
  }

  if (raw != raw_previous)
  {
    raw_previous = raw;
    raw_changed_ms = now;
  }
  else if ((uint32_t)(now - raw_changed_ms) >= BUTTON_DEBOUNCE_MS)
  {
    stable = raw;
  }

  return stable;
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
      message = "STATE,STARTUP_STOP,release both keys\r\n";
      break;
    case APP_WAIT_RELEASE:
      message = "STATE,WAIT_RELEASE,keep both keys released for 1s\r\n";
      break;
    case APP_READY:
      message = "STATE,READY,K1=S1 K2=S2 hold-to-run\r\n";
      break;
    case APP_MOTOR1:
      message = "STATE,RUN,S1 low throttle\r\n";
      break;
    case APP_MOTOR2:
      message = "STATE,RUN,S2 low throttle\r\n";
      break;
    case APP_BOTH:
      message = "STATE,RUN,S1+S2 low throttle\r\n";
      break;
    case APP_TIMEOUT_LOCKOUT:
      message = "STATE,STOP,3s limit reached; release both keys\r\n";
      break;
    default:
      message = "STATE,UNKNOWN\r\n";
      break;
  }

  App_UartWrite(message);
}

int main(void)
{
  uint32_t startup_ms;
  uint32_t release_started_ms = 0U;
  uint32_t run_started_ms = 0U;
  uint32_t last_frame_ms = 0U;
  uint8_t armed = 0U;
  uint8_t timeout_latched = 0U;

  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  DShot_Init();

  startup_ms = HAL_GetTick();
  App_UartWrite(
      "\r\nKeysKing + MAMBA F55_BLS DShot300 safe test v1.0\r\n"
      "PA12->S1, PA15->S2, GND->GND; ESC VCC MUST remain disconnected\r\n"
      "K1 controls S1, K2 controls S2; 3-second low-throttle limit\r\n");
  App_ReportState(APP_STARTUP_STOP);

  while (1)
  {
    uint32_t now = HAL_GetTick();
    uint8_t keys = App_ReadDebouncedKeys(now);
    uint16_t motor1 = DSHOT_COMMAND_STOP;
    uint16_t motor2 = DSHOT_COMMAND_STOP;
    AppState state;

    if ((uint32_t)(now - startup_ms) < STARTUP_STOP_MS)
    {
      state = APP_STARTUP_STOP;
      release_started_ms = 0U;
    }
    else if (armed == 0U)
    {
      state = APP_WAIT_RELEASE;
      if (keys == 0U)
      {
        if (release_started_ms == 0U)
        {
          release_started_ms = now;
        }
        else if ((uint32_t)(now - release_started_ms) >= RELEASE_TO_ARM_MS)
        {
          armed = 1U;
          state = APP_READY;
        }
      }
      else
      {
        release_started_ms = 0U;
      }
    }
    else if (keys == 0U)
    {
      timeout_latched = 0U;
      run_started_ms = 0U;
      state = APP_READY;
    }
    else if (timeout_latched != 0U)
    {
      state = APP_TIMEOUT_LOCKOUT;
    }
    else
    {
      if (run_started_ms == 0U)
      {
        run_started_ms = now;
      }

      if ((uint32_t)(now - run_started_ms) >= MAX_CONTINUOUS_RUN_MS)
      {
        timeout_latched = 1U;
        state = APP_TIMEOUT_LOCKOUT;
      }
      else
      {
        motor1 = ((keys & 0x01U) != 0U) ? TEST_THROTTLE_VALUE
                                        : DSHOT_COMMAND_STOP;
        motor2 = ((keys & 0x02U) != 0U) ? TEST_THROTTLE_VALUE
                                        : DSHOT_COMMAND_STOP;
        state = (keys == 0x01U) ? APP_MOTOR1
              : (keys == 0x02U) ? APP_MOTOR2
                                : APP_BOTH;
      }
    }

    if (((uint32_t)(now - last_frame_ms) >= DSHOT_FRAME_PERIOD_MS) ||
        (motor1 != app_live.motor1_command) ||
        (motor2 != app_live.motor2_command))
    {
      last_frame_ms = now;
      DShot_SendPair(motor1, motor2);
      app_live.frames_sent++;
    }

    /* Stop/new command is already on the wire before UART state reporting. */
    App_ReportState(state);

    /* The green board LED is on only while a motor command is non-zero. */
    HAL_GPIO_WritePin(GPIOA, LED_PIN,
                      ((motor1 != 0U) || (motor2 != 0U))
                          ? GPIO_PIN_RESET : GPIO_PIN_SET);

    app_live.uptime_ms = now;
    app_live.motor1_command = motor1;
    app_live.motor2_command = motor2;
    app_live.keys = keys;
    app_live.armed = armed;
    app_live.timeout_latched = timeout_latched;
    app_live.state = state;
  }
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

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

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

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

static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_AFIO_CLK_ENABLE();

  /* Establish safe levels before changing PA12/PA15 to outputs. */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12 | GPIO_PIN_15, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOA, LED_PIN, GPIO_PIN_SET);

  GPIO_InitStruct.Pin = GPIO_PIN_12 | GPIO_PIN_15 | LED_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = KEY1_PIN | KEY2_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

void Error_Handler(void)
{
  __HAL_RCC_GPIOA_CLK_ENABLE();
  GPIOA->BRR = GPIO_PIN_12 | GPIO_PIN_15;
  __disable_irq();
  while (1)
  {
  }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  (void)file;
  (void)line;
}
#endif
