/*
 * soft_i2c.h
 * 软件模拟 I2C (bit-banging) 接口声明
 * 引脚: PB6 = SCL, PB7 = SDA
 */

#ifndef __SOFT_I2C_H
#define __SOFT_I2C_H

#include "main.h"

void     Soft_I2C_Init(void);
void     delay_us(uint32_t us);

void     Soft_I2C_Start(void);
void     Soft_I2C_Stop(void);

uint8_t  Soft_I2C_Wait_Ack(void);          /* 0=收到 ACK，1=无 ACK(超时) */
void     Soft_I2C_Ack(void);               /* 主机发 ACK(拉低 SDA) */
void     Soft_I2C_NAck(void);              /* 主机发 NACK(保持 SDA 高) */

void     Soft_I2C_Send_Byte(uint8_t byte); /* 发送 1 字节，高位在前 */
uint8_t  Soft_I2C_Read_Byte(uint8_t ack);  /* 读 1 字节；ack=1 读完发 ACK，0 发 NACK */

#endif
