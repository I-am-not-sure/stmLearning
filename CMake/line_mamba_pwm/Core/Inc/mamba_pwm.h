#ifndef MAMBA_PWM_H
#define MAMBA_PWM_H

#include <stdint.h>

void MambaPwm_Init(void);
void MambaPwm_Arm(void);
void MambaPwm_Disarm(void);
void MambaPwm_SetPulses(const uint16_t pulse_us[4]);

/* Pure 3D throttle conversion; percent is a command range, not measured RPM. */
uint16_t MambaPwm_Pulse(int8_t direction, uint8_t percent);

#endif
