#ifndef SOFT_I2C_H
#define SOFT_I2C_H

#include "main.h"

/* 区分总线电气状态、超时和不同发送阶段的无应答，便于定位故障。 */
typedef enum
{
  SOFT_I2C_OK = 0,
  SOFT_I2C_BUS_LOW,
  SOFT_I2C_TIMEOUT,
  SOFT_I2C_ADDRESS_WRITE_NACK,
  SOFT_I2C_REGISTER_NACK,
  SOFT_I2C_ADDRESS_READ_NACK,
  SOFT_I2C_INVALID_ARGUMENT
} SoftI2C_Status;

/* PB6=SCL，PB7=SDA；先完成系统时钟/HAL 初始化，再初始化软件 IIC。 */
void SoftI2C_Init(void);
/* address：未左移的七位地址（0x50）；reg：8 位寄存器编号。
 * data：至少 length 字节的输出缓冲区；length：字节数，不是寄存器个数。
 * 阻塞调用；失败时缓冲区可能只填了一部分，只能在返回 OK 后使用数据。
 * 必须先调用 SoftI2C_Init；依赖 SysTick，不可在关中断时调用。 */
SoftI2C_Status SoftI2C_ReadRegisters(uint8_t address, uint8_t reg,
                                   uint8_t *data, uint16_t length);
const char *SoftI2C_ErrorText(SoftI2C_Status status);

#endif
