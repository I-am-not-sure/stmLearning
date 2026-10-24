#include "mamba_dshot.h"

uint16_t MambaDshot_Packet(uint16_t value, uint8_t telemetry)
{
  uint16_t payload;
  uint16_t check;

  if (value > 2047U)
  {
    value = 2047U;
  }
  payload = (uint16_t)((value << 1) | ((telemetry != 0U) ? 1U : 0U));
  check = (uint16_t)(payload ^ (payload >> 4) ^ (payload >> 8));
  return (uint16_t)((payload << 4) | (check & 0x0FU));
}

uint16_t MambaDshot_Throttle(int8_t direction, uint8_t percent)
{
  uint16_t step;

  if (percent == 0U)
  {
    return 0U;
  }
  if (percent > 100U)
  {
    percent = 100U;
  }
#if MAMBA_DSHOT_NORMAL_TEST
  /* Normal mode uses the full 48..2047 DShot throttle range. */
  (void)direction;
  step = (uint16_t)(((uint32_t)percent * 1999U + 50U) / 100U);
  return (uint16_t)(48U + step);
#else
  /* In BLHeli_S 3D mode, 48..1047 and 1048..2047 are the two
   * directions. This scales command range, not measured motor RPM. */
  step = (uint16_t)(((uint32_t)percent * 999U + 50U) / 100U);
  return (uint16_t)(((direction < 0) ? 48U : 1048U) + step);
#endif
}
