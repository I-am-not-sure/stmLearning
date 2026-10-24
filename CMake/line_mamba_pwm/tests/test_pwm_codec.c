#include "mamba_pwm.h"

#include <assert.h>

int main(void)
{
  assert(MambaPwm_Pulse(1, 0U) == 1500U);
  assert(MambaPwm_Pulse(-1, 0U) == 1500U);
  assert(MambaPwm_Pulse(1, 50U) == 1750U);
  assert(MambaPwm_Pulse(-1, 50U) == 1250U);
  assert(MambaPwm_Pulse(1, 100U) == 2000U);
  assert(MambaPwm_Pulse(-1, 100U) == 1000U);
  assert(MambaPwm_Pulse(-1, 200U) == 1000U);
  return 0;
}
