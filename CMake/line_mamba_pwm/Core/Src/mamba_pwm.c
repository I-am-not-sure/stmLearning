#include "mamba_pwm.h"

#include "main.h"
#include "tim.h"

static uint8_t pwm_armed;

#if MAMBA_PWM_NORMAL_TEST
#define PWM_IDLE_US 1000U
#else
#define PWM_IDLE_US 1500U
#endif

void MambaPwm_Disarm(void)
{
  GPIO_InitTypeDef gpio = {0};

  pwm_armed = 0U;
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

void MambaPwm_Init(void)
{
  /* Original line clock is HSI 16 MHz, APB1 divider 1; TIM3 = 16 MHz. */
  if ((HAL_RCC_GetSysClockFreq() != 16000000U) ||
      (HAL_RCC_GetPCLK1Freq() != 16000000U))
  {
    Error_Handler();
  }
  MambaPwm_Disarm();
}

void MambaPwm_Arm(void)
{
  GPIO_InitTypeDef gpio = {0};

  if (pwm_armed != 0U)
  {
    return;
  }
  /* 16 MHz / (15 + 1) = 1 MHz; (19999 + 1) us = 20 ms = 50 Hz. */
  TIM3->PSC = 15U;
  TIM3->ARR = 19999U;
  TIM3->CNT = 0U;
  TIM3->CCMR1 |= TIM_CCMR1_OC1PE | TIM_CCMR1_OC2PE;
  TIM3->CCMR2 |= TIM_CCMR2_OC3PE | TIM_CCMR2_OC4PE;
  TIM3->CCR1 = PWM_IDLE_US;
  TIM3->CCR2 = PWM_IDLE_US;
  TIM3->CCR3 = PWM_IDLE_US;
  TIM3->CCR4 = PWM_IDLE_US;
  TIM3->EGR = TIM_EGR_UG;
  TIM3->SR &= ~TIM_SR_UIF;

  gpio.Pin = GPIO_PIN_6 | GPIO_PIN_7;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  gpio.Alternate = GPIO_AF2_TIM3;
  HAL_GPIO_Init(GPIOA, &gpio);
  gpio.Pin = GPIO_PIN_0 | GPIO_PIN_1;
  HAL_GPIO_Init(GPIOB, &gpio);

  TIM3->CCER |= TIM_CCER_CC1E | TIM_CCER_CC2E |
                TIM_CCER_CC3E | TIM_CCER_CC4E;
  TIM3->CR1 |= TIM_CR1_CEN;
  pwm_armed = 1U;
}

void MambaPwm_SetPulses(const uint16_t pulse_us[4])
{
  if ((pwm_armed == 0U) || (pulse_us == 0))
  {
    return;
  }
  TIM3->CCR1 = pulse_us[0];
  TIM3->CCR2 = pulse_us[1];
  TIM3->CCR3 = pulse_us[2];
  TIM3->CCR4 = pulse_us[3];
}
