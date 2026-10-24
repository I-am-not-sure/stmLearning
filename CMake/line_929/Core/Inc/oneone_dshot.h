#ifndef ONEONE_DSHOT_H
#define ONEONE_DSHOT_H

#include "stm32f4xx_hal.h"
#include "oneone_dshot_codec.h"

#define ONEONE_DSHOT_STOP          0U
#define ONEONE_DSHOT_BEEP1         1U
#define ONEONE_DSHOT_3D_MODE_OFF   9U
#define ONEONE_DSHOT_3D_MODE_ON   10U
#define ONEONE_DSHOT_MAX        2047U

void OneOneDshot_Init(void);
void OneOneDshot_SendFour(const uint16_t values[4]);
void OneOneDshot_SendCommandToAll(uint16_t command);
void OneOneDshot_Beep(uint8_t channel);
void OneOneDshot_ForceLow(void);

#endif
