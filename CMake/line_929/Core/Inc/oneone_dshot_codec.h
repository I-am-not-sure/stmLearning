#ifndef ONEONE_DSHOT_CODEC_H
#define ONEONE_DSHOT_CODEC_H

#include <stdint.h>

uint16_t OneOneDshot_MakePacket(uint16_t value, uint8_t telemetry);
uint16_t OneOneDshot_Throttle(int8_t direction, uint8_t percent,
                              uint8_t mode_3d);

#endif
