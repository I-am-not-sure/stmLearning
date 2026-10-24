#include "mamba_dshot.h"

#include <assert.h>

int main(void)
{
  assert(MambaDshot_Packet(0U, 0U) == 0x0000U);
  assert(MambaDshot_Packet(10U, 1U) == 0x0154U);
  assert(MambaDshot_Packet(1548U, 0U) == 0xC185U);
  assert(MambaDshot_Throttle(1, 0U) == 0U);
  assert(MambaDshot_Throttle(1, 50U) == 1548U);
  assert(MambaDshot_Throttle(-1, 50U) == 548U);
  assert(MambaDshot_Throttle(1, 100U) == 2047U);
  assert(MambaDshot_Throttle(-1, 100U) == 1047U);
  return 0;
}
