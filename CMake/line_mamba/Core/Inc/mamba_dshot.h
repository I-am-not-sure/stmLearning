#ifndef MAMBA_DSHOT_H
#define MAMBA_DSHOT_H

#include <stdint.h>

void MambaDshot_Init(void);
void MambaDshot_Send(const uint16_t values[4]);
void MambaDshot_Beep(uint8_t channel);
uint8_t MambaDshot_ProbePins(void);
void MambaDshot_Enable3D(void);
void MambaDshot_SaveSettings(void);
void MambaDshot_ForceSignalLow(void);

uint16_t MambaDshot_Packet(uint16_t value, uint8_t telemetry);
uint16_t MambaDshot_Throttle(int8_t direction, uint8_t percent);

#endif
