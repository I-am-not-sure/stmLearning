#ifndef JY61P_H
#define JY61P_H

#include "stm32f4xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 主循环调用：阻塞读 I2C2，随后阻塞发送 USART1。
 * 需要先完成外设初始化和模块的上电等待；仅支持单调用者。
 * 失败时输出寄存器编号、HAL 错误及 PF1/SCL、PF0/SDA 电平。 */
void JY61P_Report(I2C_HandleTypeDef *i2c, UART_HandleTypeDef *uart);

#ifdef __cplusplus
}
#endif

#endif
