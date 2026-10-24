#ifndef DSHOT_H
#define DSHOT_H

#include "stm32f1xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DSHOT_COMMAND_STOP 0U
#define DSHOT_MIN_THROTTLE 48U
#define DSHOT_MAX_THROTTLE 2047U

void DShot_Init(void);
uint16_t DShot_MakePacket(uint16_t value, uint8_t request_telemetry);
void DShot_SendPair(uint16_t motor1_value, uint16_t motor2_value);

#ifdef __cplusplus
}
#endif

#endif
