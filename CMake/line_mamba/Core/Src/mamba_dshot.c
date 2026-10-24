#include "mamba_dshot.h"

#include "main.h"
#include "tim.h"

/* HSI/PLL=168 MHz, APB1=42 MHz, hence TIM3=84 MHz.
 * TIM3's four PWM channels share one counter; CCR preload aligns all bits. */
#if MAMBA_DSHOT_RATE_KHZ == 150
#define DSHOT_BIT_TICKS    560U
#define DSHOT_ZERO_TICKS   210U
#define DSHOT_ONE_TICKS    420U
#elif MAMBA_DSHOT_RATE_KHZ == 300
#define DSHOT_BIT_TICKS    280U
#define DSHOT_ZERO_TICKS   105U
#define DSHOT_ONE_TICKS    210U
#else
#error "MAMBA_DSHOT_RATE_KHZ must be 150 or 300"
#endif
#define DSHOT_3D_ON         10U
#define DSHOT_SAVE_SETTINGS 12U
#define DSHOT_BEEP1          1U

static void WaitUpdate(void)
{
  while ((TIM3->SR & TIM_SR_UIF) == 0U)
  {
    /* One bit period = 3.33 us. */
  }
}

static void SendPackets(const uint16_t values[4], uint8_t telemetry)
{
  uint16_t packets[4];
  uint16_t bit;
  uint32_t irq_state;
  uint32_t index;

  for (index = 0U; index < 4U; index++)
  {
    packets[index] = MambaDshot_Packet(values[index], telemetry);
  }

  /* A frame lasts about 57 us. This keeps bit boundaries free of ISR jitter. */
  irq_state = __get_PRIMASK();
  __disable_irq();
  TIM3->SR &= ~TIM_SR_UIF;
  WaitUpdate();
  for (bit = 0x8000U; bit != 0U; bit >>= 1)
  {
    TIM3->CCR1 = ((packets[0] & bit) != 0U) ? DSHOT_ONE_TICKS : DSHOT_ZERO_TICKS;
    TIM3->CCR2 = ((packets[1] & bit) != 0U) ? DSHOT_ONE_TICKS : DSHOT_ZERO_TICKS;
    TIM3->CCR3 = ((packets[2] & bit) != 0U) ? DSHOT_ONE_TICKS : DSHOT_ZERO_TICKS;
    TIM3->CCR4 = ((packets[3] & bit) != 0U) ? DSHOT_ONE_TICKS : DSHOT_ZERO_TICKS;
    TIM3->SR &= ~TIM_SR_UIF;
    WaitUpdate();
  }
  TIM3->CCR1 = 0U;
  TIM3->CCR2 = 0U;
  TIM3->CCR3 = 0U;
  TIM3->CCR4 = 0U;
  TIM3->SR &= ~TIM_SR_UIF;
  WaitUpdate();
  TIM3->SR &= ~TIM_SR_UIF;
  if (irq_state == 0U)
  {
    __enable_irq();
  }
}

void MambaDshot_Init(void)
{
  GPIO_InitTypeDef gpio = {0};

  if ((HAL_RCC_GetSysClockFreq() != 168000000U) ||
      (HAL_RCC_GetPCLK1Freq() != 42000000U))
  {
    Error_Handler();
  }

  /* Keep the four configured TIM3 pins and raise GPIO slew rate for DShot. */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  gpio.Pin = GPIO_PIN_6 | GPIO_PIN_7;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  gpio.Alternate = GPIO_AF2_TIM3;
  HAL_GPIO_Init(GPIOA, &gpio);
  gpio.Pin = GPIO_PIN_0 | GPIO_PIN_1;
  HAL_GPIO_Init(GPIOB, &gpio);

  TIM3->CR1 &= ~TIM_CR1_CEN;
  TIM3->PSC = 0U;
  TIM3->ARR = DSHOT_BIT_TICKS - 1U;
  TIM3->CNT = 0U;
  TIM3->CCMR1 |= TIM_CCMR1_OC1PE | TIM_CCMR1_OC2PE;
  TIM3->CCMR2 |= TIM_CCMR2_OC3PE | TIM_CCMR2_OC4PE;
  TIM3->CCR1 = 0U;
  TIM3->CCR2 = 0U;
  TIM3->CCR3 = 0U;
  TIM3->CCR4 = 0U;
  TIM3->EGR = TIM_EGR_UG;
  TIM3->SR &= ~TIM_SR_UIF;
  TIM3->CCER |= TIM_CCER_CC1E | TIM_CCER_CC2E |
                TIM_CCER_CC3E | TIM_CCER_CC4E;
  TIM3->CR1 |= TIM_CR1_CEN;
}

void MambaDshot_Send(const uint16_t values[4])
{
  SendPackets(values, 0U);
}

void MambaDshot_Beep(uint8_t channel)
{
  const uint16_t stopped[4] = {0U, 0U, 0U, 0U};
  uint16_t beep[4] = {0U, 0U, 0U, 0U};
  uint32_t repeat;

  if ((channel < 1U) || (channel > 4U))
  {
    return;
  }
  beep[channel - 1U] = DSHOT_BEEP1;
  SendPackets(stopped, 0U);
  HAL_Delay(10U);
  /* BLHeli_S BEEP1 requires at least 380 ms before another command. */
  for (repeat = 0U; repeat < 2U; repeat++)
  {
    SendPackets(beep, 1U);
    HAL_Delay(450U);
  }
  SendPackets(stopped, 0U);
}

uint8_t MambaDshot_ProbePins(void)
{
  uint8_t high_mask = 0U;
  uint8_t low_mask = 0U;
  uint32_t irq_state = __get_PRIMASK();
  uint32_t repeat;
  uint32_t pin_a;
  uint32_t pin_b;

  /* One PWM bit and one idle bit per sample; this is not a throttle packet. */
  __disable_irq();
  for (repeat = 0U; repeat < 8U; repeat++)
  {
    TIM3->CCR1 = DSHOT_ZERO_TICKS;
    TIM3->CCR2 = DSHOT_ZERO_TICKS;
    TIM3->CCR3 = DSHOT_ZERO_TICKS;
    TIM3->CCR4 = DSHOT_ZERO_TICKS;
    TIM3->SR &= ~TIM_SR_UIF;
    WaitUpdate();
    pin_a = GPIOA->IDR;
    pin_b = GPIOB->IDR;
    if ((pin_a & GPIO_PIN_6) != 0U) high_mask |= 0x01U;
    if ((pin_a & GPIO_PIN_7) != 0U) high_mask |= 0x02U;
    if ((pin_b & GPIO_PIN_0) != 0U) high_mask |= 0x04U;
    if ((pin_b & GPIO_PIN_1) != 0U) high_mask |= 0x08U;

    while (TIM3->CNT < (DSHOT_ZERO_TICKS +
                       (DSHOT_BIT_TICKS - DSHOT_ZERO_TICKS) / 2U)) { }
    pin_a = GPIOA->IDR;
    pin_b = GPIOB->IDR;
    if ((pin_a & GPIO_PIN_6) == 0U) low_mask |= 0x01U;
    if ((pin_a & GPIO_PIN_7) == 0U) low_mask |= 0x02U;
    if ((pin_b & GPIO_PIN_0) == 0U) low_mask |= 0x04U;
    if ((pin_b & GPIO_PIN_1) == 0U) low_mask |= 0x08U;

    TIM3->CCR1 = 0U;
    TIM3->CCR2 = 0U;
    TIM3->CCR3 = 0U;
    TIM3->CCR4 = 0U;
    TIM3->SR &= ~TIM_SR_UIF;
    WaitUpdate();
  }
  if (irq_state == 0U)
  {
    __enable_irq();
  }
  return (uint8_t)(high_mask & low_mask);
}

void MambaDshot_Enable3D(void)
{
  const uint16_t stopped[4] = {0U, 0U, 0U, 0U};
  const uint16_t enable[4] = {DSHOT_3D_ON, DSHOT_3D_ON,
                              DSHOT_3D_ON, DSHOT_3D_ON};
  uint32_t repeat;

  /* Volatile command only; no SAVE_SETTINGS or ESC flash write. */
  SendPackets(stopped, 0U);
  HAL_Delay(10U);
  for (repeat = 0U; repeat < 10U; repeat++)
  {
    SendPackets(enable, 1U);
    HAL_Delay(1U);
  }
  SendPackets(stopped, 0U);
}

void MambaDshot_SaveSettings(void)
{
  const uint16_t stopped[4] = {0U, 0U, 0U, 0U};
  const uint16_t save[4] = {DSHOT_SAVE_SETTINGS, DSHOT_SAVE_SETTINGS,
                            DSHOT_SAVE_SETTINGS, DSHOT_SAVE_SETTINGS};
  uint32_t repeat;

  /* Keep all channels stopped before requesting a nonvolatile ESC write. */
  for (repeat = 0U; repeat < 20U; repeat++)
  {
    SendPackets(stopped, 0U);
    HAL_Delay(5U);
  }
  /* BLHeli_S requires this command repeatedly with the telemetry bit set. */
  for (repeat = 0U; repeat < 10U; repeat++)
  {
    SendPackets(save, 1U);
    HAL_Delay(5U);
  }
  HAL_Delay(50U);
  SendPackets(stopped, 0U);
}

void MambaDshot_ForceSignalLow(void)
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_TIM3_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  TIM3->CCER &= ~(TIM_CCER_CC1E | TIM_CCER_CC2E |
                  TIM_CCER_CC3E | TIM_CCER_CC4E);
  TIM3->CR1 &= ~TIM_CR1_CEN;
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6 | GPIO_PIN_7, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0 | GPIO_PIN_1, GPIO_PIN_RESET);
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  gpio.Pin = GPIO_PIN_6 | GPIO_PIN_7;
  HAL_GPIO_Init(GPIOA, &gpio);
  gpio.Pin = GPIO_PIN_0 | GPIO_PIN_1;
  HAL_GPIO_Init(GPIOB, &gpio);
}
