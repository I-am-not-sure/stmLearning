/*
 * ms5837.h
 * MS5837-02BA 压力/深度传感器驱动接口（软件 I2C 版本）
 * 功能与 Depthsensor 工程的 ms5837.h 一致
 */

#ifndef __MS5837_H
#define __MS5837_H

#include "stm32f4xx_hal.h"
#include "soft_i2c.h"

/* 初始化状态码 */
typedef enum {
    MS5837_OK        = 0,   /* 初始化成功 */
    MS5837_ERR_RESET = 1,   /* 复位无应答 */
    MS5837_ERR_PROM  = 2,   /* 读 PROM 失败 */
    MS5837_ERR_CRC   = 3,   /* PROM CRC4 校验失败 */
    MS5837_ERR_ZERO  = 4    /* 大气压零位校准失败 */
} MS5837_StatusTypeDef;

MS5837_StatusTypeDef MS5837_init(void);
void MS5837_Getdata(void);

extern uint32_t Cal_C[7];             /* PROM 系数 C0..C6，计算用 C1..C6 */
extern uint32_t D1_Pres, D2_Temp;     /* 原始 ADC 值 */
extern float    Pressure;             /* 压力 mbar（带小数，保留亚 mbar 分辨率） */
extern float    Atmdsphere_Pressure;  /* 大气压参考 mbar */
extern float    Depth;                /* 深度 cm */

#endif
