#include "mamba_pwm.h"

uint16_t MambaPwm_Pulse(int8_t direction, uint8_t percent)
{
  uint16_t delta_us;

  if (percent > 100U)
  {
    percent = 100U;
  }
#if MAMBA_PWM_NORMAL_TEST
  delta_us = (uint16_t)((uint16_t)percent * 9U);
  (void)direction;
  return (uint16_t)(1000U + delta_us);
#else
  delta_us = (uint16_t)((uint16_t)percent * 5U);
  return (direction < 0) ? (uint16_t)(1500U - delta_us)
                         : (uint16_t)(1500U + delta_us);
#endif
}
