#ifndef MAMBA_PWM_APP_H
#define MAMBA_PWM_APP_H

#include "stm32f4xx_hal.h"

void MambaPwmApp_Init(void);
void MambaPwmApp_Poll(void);
void MambaPwmApp_UartRxComplete(UART_HandleTypeDef *uart);
void MambaPwmApp_UartError(UART_HandleTypeDef *uart);
void MambaPwmApp_ForceSignalLow(void);

#endif
