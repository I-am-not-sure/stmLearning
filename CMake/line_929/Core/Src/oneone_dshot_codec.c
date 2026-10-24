#include "oneone_dshot_codec.h"

uint16_t OneOneDshot_MakePacket(uint16_t value, uint8_t telemetry)
{
  uint16_t payload;
  uint16_t checksum;

  if (value > 2047U) value = 2047U;
  payload = (uint16_t)((value << 1) | ((telemetry != 0U) ? 1U : 0U));
  checksum = (uint16_t)(payload ^ (payload >> 4) ^ (payload >> 8));
  return (uint16_t)((payload << 4) | (checksum & 0x0FU));
}

uint16_t OneOneDshot_Throttle(int8_t direction, uint8_t percent,
                              uint8_t mode_3d)
{
  if (percent == 0U) return 0U;
  if (percent > 100U) percent = 100U;
  if (mode_3d == 0U)
  {
    return (uint16_t)(48U + (((uint32_t)percent * 1999U + 50U) / 100U));
  }
  if (direction < 0)
  {
    return (uint16_t)(48U + (((uint32_t)percent * 999U + 50U) / 100U));
  }
  return (uint16_t)(1048U + (((uint32_t)percent * 999U + 50U) / 100U));
}
