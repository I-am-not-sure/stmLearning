#ifndef ONEONE_APP_H
#define ONEONE_APP_H

#include "stm32f4xx_hal.h"

typedef struct
{
  uint32_t frames_sent;
  uint32_t rx_bytes;
  uint32_t uart_errors;
  uint16_t dshot_values[4];
  uint8_t ready;
  uint8_t armed;
  uint8_t mode_3d_requested; /* Request sent; the ESC does not acknowledge it. */
} OneOneLive;

extern volatile OneOneLive oneone_live;

void OneOneApp_Init(void);
void OneOneApp_Poll(void);
void OneOneApp_UartRxComplete(UART_HandleTypeDef *uart);
void OneOneApp_UartError(UART_HandleTypeDef *uart);
void OneOneApp_ForceSignalLow(void);

#endif
