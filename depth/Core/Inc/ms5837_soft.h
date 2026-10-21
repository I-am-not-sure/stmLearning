/*
 * ms5837_soft.h
 * MS5837-02BA 驱动 — 软件 I2C 版本
 * 直接移植自: BlueRobots B02 例程 MS5837.h
 */

#ifndef __MS5837_SOFT_H
#define __MS5837_SOFT_H

#include "soft_i2c.h"

#define MS5837_WRITE   0xEC
#define MS5837_READ    0xED
#define MS5837_RESET   0x1E
#define MS5837_PROM_RD 0xA0
#define MS5837_ADC_RD  0x00
#define MS5837_D1_4096 0x48
#define MS5837_D2_4096 0x58

void     MS5837_Soft_Reset(void);
uint8_t  MS5837_Soft_PROM(void);
uint32_t MS5837_Soft_GetConversion(uint8_t cmd);
void     MS5837_Soft_GetData(void);
uint8_t  MS5837_Soft_Crc4(void);

extern double   MS5837_Temperature;
extern int32_t  MS5837_Pressure;
extern uint32_t MS5837_Cal[7];
extern uint32_t MS5837_D1, MS5837_D2;

#endif
