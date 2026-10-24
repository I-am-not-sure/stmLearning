#ifndef MAMBA_APP_H
#define MAMBA_APP_H

#include "stm32f4xx_hal.h"

void MambaApp_Init(void);
void MambaApp_Poll(void);
void MambaApp_UartRxComplete(UART_HandleTypeDef *uart);
void MambaApp_UartError(UART_HandleTypeDef *uart);
void MambaApp_ForceSignalLow(void);

#endif
