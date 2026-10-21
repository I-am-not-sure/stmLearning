/*
 * soft_i2c.h
 * 软件模拟 I2C (bit-banging)，适配 STM32F103C8T6
 * 移植自: BlueRobots B02 例程 myiic.c
 * 引脚: PB6 = SCL, PB7 = SDA  (与原 HAL I2C1 引脚相同)
 */

#ifndef __SOFT_I2C_H
#define __SOFT_I2C_H

#include "main.h"

void delay_us(uint32_t n);
void Soft_I2C_Init(void);
void Soft_I2C_Start(void);
void Soft_I2C_Stop(void);
uint8_t Soft_I2C_Wait_Ack(void);
void Soft_I2C_Ack(void);
void Soft_I2C_NAck(void);
void Soft_I2C_Send_Byte(uint8_t txd);
uint8_t Soft_I2C_Read_Byte(uint8_t ack);

#endif
