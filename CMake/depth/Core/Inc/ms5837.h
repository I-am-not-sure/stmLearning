/*
 * ms5837.h
 * MS5837-02BA 深度传感器驱动头文件
 * 适用芯片: STM32F103C8T6
 * 接口: I2C1 (PB6-SCL, PB7-SDA)
 * 地址: 0xEC (7位地址 0x76 左移1位)
 *
 * 参考来源:
 * - MS5837-02BA 数据手册 ENG_DS_MS5837-02BA01_A8.pdf
 * - BlueRobots B02 深度传感器官方例程
 */

#ifndef __MS5837_H_
#define __MS5837_H_

#include "i2c.h"

/* 初始化传感器: 复位 -> 读取PROM校准系数 -> 采集大气压参考值 */
void MS5837_Init(void);

/* 读取深度(cm)和温度(°C)，通过指针返回 */
void MS5837_Read(float *depth, float *temperature);

/* 调试变量: 查看原始数据排查问题 */
extern uint32_t dbg_D1;          /* 原始压力值 D1 */
extern uint32_t dbg_D2;          /* 原始温度值 D2 */
extern uint32_t dbg_Cal[7];      /* PROM 校准系数 C0~C6 */
extern uint8_t  dbg_init_ok;     /* 初始化是否成功 (1=成功, 0=失败) */
extern uint32_t dbg_i2c_err;     /* I2C 通信错误计数 */

#endif
