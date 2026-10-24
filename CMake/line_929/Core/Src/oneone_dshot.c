#include "oneone_dshot.h"
#include "main.h"

/* Only the original one_one TIM1 four-channel MAMBA group is ported:
 * M1/S1 -> PA6, M2/S2 -> PA7, M3/S3 -> PB0, M4/S4 -> PB1.
 * The original TIM2 two-channel independent-ESC group is not used.
 *
 * F407 HCLK = 168 MHz, so DShot300 uses 560 core cycles per bit.
 * GPIO/DWT waveform: teammate_Mamba_DShot300.
 * Packet, throttle map and 3D command sequence: one_one_failversion. */
#define BIT_CYCLES   560U
#define ZERO_CYCLES  210U
#define ONE_CYCLES   420U
#define A_PINS (GPIO_PIN_6 | GPIO_PIN_7)
#define B_PINS (GPIO_PIN_0 | GPIO_PIN_1)

static void WaitUntil(uint32_t deadline)
{
  while ((int32_t)(DWT->CYCCNT - deadline) < 0) { }
}

void OneOneDshot_Init(void)
{
  GPIO_InitTypeDef gpio = {0};
  uint32_t cycle_start;
  volatile uint32_t delay;
  if (HAL_RCC_GetHCLKFreq() != 168000000U) Error_Handler();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  GPIOA->BSRR = ((uint32_t)A_PINS << 16);
  GPIOB->BSRR = ((uint32_t)B_PINS << 16);
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  gpio.Pin = A_PINS;
  HAL_GPIO_Init(GPIOA, &gpio);
  gpio.Pin = B_PINS;
  HAL_GPIO_Init(GPIOB, &gpio);
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  if ((DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) == 0U) Error_Handler();
  cycle_start = DWT->CYCCNT;
  for (delay = 0U; delay < 64U; delay++) __NOP();
  if (DWT->CYCCNT == cycle_start) Error_Handler();
}

static void SendFourInternal(const uint16_t values[4], uint8_t telemetry)
{
  uint16_t packets[4];
  uint16_t mask;
  uint32_t next_start;
  uint32_t saved_primask;
  uint32_t motor;
  if (values == NULL) return;
  for (motor = 0U; motor < 4U; motor++)
  {
    packets[motor] = OneOneDshot_MakePacket(values[motor], telemetry);
  }
  saved_primask = __get_PRIMASK();
  __disable_irq();
  next_start = DWT->CYCCNT + 128U;
  for (mask = 0x8000U; mask != 0U; mask >>= 1)
  {
    uint32_t zero_a = 0U;
    uint32_t one_a = 0U;
    uint32_t zero_b = 0U;
    uint32_t one_b = 0U;
    if ((packets[0] & mask) != 0U) one_a |= GPIO_PIN_6;
    else zero_a |= GPIO_PIN_6;
    if ((packets[1] & mask) != 0U) one_a |= GPIO_PIN_7;
    else zero_a |= GPIO_PIN_7;
    if ((packets[2] & mask) != 0U) one_b |= GPIO_PIN_0;
    else zero_b |= GPIO_PIN_0;
    if ((packets[3] & mask) != 0U) one_b |= GPIO_PIN_1;
    else zero_b |= GPIO_PIN_1;

    WaitUntil(next_start);
    GPIOA->BSRR = A_PINS;
    GPIOB->BSRR = B_PINS;
    WaitUntil(next_start + ZERO_CYCLES);
    GPIOA->BSRR = zero_a << 16;
    GPIOB->BSRR = zero_b << 16;
    WaitUntil(next_start + ONE_CYCLES);
    GPIOA->BSRR = one_a << 16;
    GPIOB->BSRR = one_b << 16;
    next_start += BIT_CYCLES;
  }
  WaitUntil(next_start);
  GPIOA->BSRR = ((uint32_t)A_PINS << 16);
  GPIOB->BSRR = ((uint32_t)B_PINS << 16);
  if (saved_primask == 0U) __enable_irq();
}

void OneOneDshot_SendFour(const uint16_t values[4])
{
  SendFourInternal(values, 0U);
}

void OneOneDshot_SendCommandToAll(uint16_t command)
{
  const uint16_t stopped[4] = {0U, 0U, 0U, 0U};
  uint16_t commands[4];
  uint32_t motor;
  uint32_t repeat;
  if ((command == 0U) || (command > 47U)) return;
  for (motor = 0U; motor < 4U; motor++) commands[motor] = command;
  SendFourInternal(stopped, 0U);
  HAL_Delay(10U);
  for (repeat = 0U; repeat < 10U; repeat++)
  {
    SendFourInternal(commands, 1U);
    HAL_Delay(1U);
  }
  SendFourInternal(stopped, 0U);
  HAL_Delay(1U);
}

void OneOneDshot_Beep(uint8_t channel)
{
  const uint16_t stopped[4] = {0U, 0U, 0U, 0U};
  uint16_t commands[4] = {0U, 0U, 0U, 0U};
  uint32_t repeat;
  if ((channel < 1U) || (channel > 4U)) return;
  commands[channel - 1U] = ONEONE_DSHOT_BEEP1;
  SendFourInternal(stopped, 0U);
  HAL_Delay(10U);
  for (repeat = 0U; repeat < 2U; repeat++)
  {
    SendFourInternal(commands, 1U);
    HAL_Delay(450U);
  }
  SendFourInternal(stopped, 0U);
}

void OneOneDshot_ForceLow(void)
{
  GPIO_InitTypeDef gpio = {0};
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  GPIOA->BSRR = ((uint32_t)A_PINS << 16);
  GPIOB->BSRR = ((uint32_t)B_PINS << 16);
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  gpio.Pin = A_PINS;
  HAL_GPIO_Init(GPIOA, &gpio);
  gpio.Pin = B_PINS;
  HAL_GPIO_Init(GPIOB, &gpio);
}
