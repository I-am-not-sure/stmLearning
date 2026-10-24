#include "i2c.h"
#include "usart.h"
#include <stdio.h>

/* 硬件 I2C1：PB6=SCL、PB7=SDA，100kHz，HAL 阻塞读取。
 * GPIO 复用开漏时 STM32F1 的内部输入上拉不起作用。
 * 线路需要模块自带或板上外部上拉；恢复函数无法代替上拉。 */
I2C_HandleTypeDef hi2c1;

void MX_I2C1_Init(void)
{
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000; /* 标准模式；须先确保 SCL/SDA 有有效上拉。 */
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK) Error_Handler();
}

void HAL_I2C_MspInit(I2C_HandleTypeDef *i2cHandle)
{
  if (i2cHandle->Instance == I2C1)
  {
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();
    gpio.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    gpio.Mode = GPIO_MODE_AF_OD;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &gpio);
    __HAL_RCC_I2C1_CLK_ENABLE();
  }
}

void HAL_I2C_MspDeInit(I2C_HandleTypeDef *i2cHandle)
{
  if (i2cHandle->Instance == I2C1)
  {
    __HAL_RCC_I2C1_CLK_DISABLE();
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_6 | GPIO_PIN_7);
  }
}

static HAL_StatusTypeDef I2C1_WaitLine(uint16_t pin, GPIO_PinState state)
{
  uint32_t start = HAL_GetTick();
  while (HAL_GPIO_ReadPin(GPIOB, pin) != state)
  {
    if ((uint32_t)(HAL_GetTick() - start) >= 10U) return HAL_TIMEOUT;
  }
  HAL_Delay(1U);
  return HAL_OK;
}

HAL_StatusTypeDef I2C1_Recover(void)
{
  GPIO_InitTypeDef gpio = {0};
  HAL_StatusTypeDef status = HAL_TIMEOUT;

  /* STM32F103 ES096 section 2.8.7: release analog-filter BUSY lock.
     The open-drain lines need working external pull-ups. */
  __HAL_I2C_DISABLE(&hi2c1);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6 | GPIO_PIN_7, GPIO_PIN_SET);
  gpio.Pin = GPIO_PIN_6 | GPIO_PIN_7;
  gpio.Mode = GPIO_MODE_OUTPUT_OD;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &gpio);

  if (I2C1_WaitLine(GPIO_PIN_6, GPIO_PIN_SET) != HAL_OK) goto restore;
  /* Up to nine clocks release a slave interrupted partway through a byte. */
  for (uint32_t pulse = 0U;
       (pulse < 9U) && (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7) == GPIO_PIN_RESET);
       ++pulse)
  {
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);
    if (I2C1_WaitLine(GPIO_PIN_6, GPIO_PIN_RESET) != HAL_OK) goto restore;
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
    if (I2C1_WaitLine(GPIO_PIN_6, GPIO_PIN_SET) != HAL_OK) goto restore;
  }
  if (I2C1_WaitLine(GPIO_PIN_7, GPIO_PIN_SET) != HAL_OK) goto restore;

  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET);
  if (I2C1_WaitLine(GPIO_PIN_7, GPIO_PIN_RESET) != HAL_OK) goto restore;
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);
  if (I2C1_WaitLine(GPIO_PIN_6, GPIO_PIN_RESET) != HAL_OK) goto restore;
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
  if (I2C1_WaitLine(GPIO_PIN_6, GPIO_PIN_SET) != HAL_OK) goto restore;
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET);
  status = I2C1_WaitLine(GPIO_PIN_7, GPIO_PIN_SET);

restore:
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6 | GPIO_PIN_7, GPIO_PIN_SET);
  gpio.Mode = GPIO_MODE_AF_OD;
  HAL_GPIO_Init(GPIOB, &gpio);
  SET_BIT(hi2c1.Instance->CR1, I2C_CR1_SWRST);
  CLEAR_BIT(hi2c1.Instance->CR1, I2C_CR1_SWRST);
  if (HAL_I2C_Init(&hi2c1) != HAL_OK) return HAL_ERROR;
  return status;
}


/* 只在外设初始化前执行一次。释放开漏线后测量 2ms 内能否真正升高；
 * 随后切输入上拉做对照，再由 HAL_I2C_MspInit 还原为 I2C 复用开漏。
 * 这个诊断不向传感器发送 I2C 数据，不改变传感器配置。 */
void I2C1_DiagnosePullups(void)
{
  GPIO_InitTypeDef gpio = {0};
  uint32_t rise[2];
  uint32_t weak[2];
  const uint16_t pins[2] = {GPIO_PIN_6, GPIO_PIN_7};
  char line[120];

  __HAL_RCC_GPIOB_CLK_ENABLE();
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  gpio.Pin = GPIO_PIN_6 | GPIO_PIN_7;
  gpio.Mode = GPIO_MODE_OUTPUT_OD;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_WritePin(GPIOB, gpio.Pin, GPIO_PIN_SET);
  HAL_GPIO_Init(GPIOB, &gpio);

  for (uint32_t i = 0; i < 2U; ++i)
  {
    HAL_GPIO_WritePin(GPIOB, pins[i], GPIO_PIN_RESET);
    HAL_Delay(1U);
    uint32_t begin = DWT->CYCCNT;
    HAL_GPIO_WritePin(GPIOB, pins[i], GPIO_PIN_SET);
    while ((HAL_GPIO_ReadPin(GPIOB, pins[i]) == GPIO_PIN_RESET) &&
           ((uint32_t)(DWT->CYCCNT - begin) < SystemCoreClock / 500U)) {}
    rise[i] = (uint32_t)(DWT->CYCCNT - begin) / (SystemCoreClock / 1000000U);

    gpio.Pin = pins[i];
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &gpio);
    HAL_Delay(1U);
    weak[i] = HAL_GPIO_ReadPin(GPIOB, pins[i]);
    gpio.Mode = GPIO_MODE_OUTPUT_OD;
    HAL_GPIO_Init(GPIOB, &gpio);
  }

  int n = snprintf(line, sizeof(line),
                   "I2C pullup test: OD rise SCL=%luus SDA=%luus (>=2000=timeout), input pullup SCL=%lu SDA=%lu\r\n",
                   (unsigned long)rise[0], (unsigned long)rise[1],
                   (unsigned long)weak[0], (unsigned long)weak[1]);
  if ((n > 0) && ((size_t)n < sizeof(line)))
  {
    (void)HAL_UART_Transmit(&huart1, (uint8_t *)line, (uint16_t)n, 100U);
  }
}

