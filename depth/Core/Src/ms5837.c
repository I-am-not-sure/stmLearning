/*
 * ms5837.c
 * MS5837-02BA 深度传感器驱动实现
 *
 * 协议参考: MS5837-02BA 数据手册 (TE Connectivity)
 *           BlueRobots B02 官方 STM32 例程
 *
 * I2C 地址: 7位 = 0x76,  HAL 传入左移1位 = 0xEC
 */

#include "ms5837.h"

/* ---- 命令定义 ---- */
#define MS5837_ADDR         0xEC
#define MS5837_RESET         0x1E
#define MS5837_ADC_READ      0x00
#define MS5837_PROM_READ     0xA0
#define MS5837_D1_CONVERT    0x48   /* D1 压力转换 OSR=4096 */
#define MS5837_D2_CONVERT    0x58   /* D2 温度转换 OSR=4096 */

/* PROM 读取最大重试次数 (参考 B02 例程) */
#define PROM_RETRY_MAX  3

/* ---- 静态变量 ---- */
static uint32_t Cal_C[7];
static int32_t Atmdsphere_Pressure;
static int32_t Pressure;

/* 调试用: 暴露原始值供 main.c 打印 */
uint32_t dbg_D1, dbg_D2;
uint32_t dbg_Cal[7];
uint8_t  dbg_init_ok = 0;
uint32_t dbg_i2c_err = 0;

/* ---- 内部函数 ---- */

static HAL_StatusTypeDef MS5837_Reset(void)
{
    uint8_t cmd = MS5837_RESET;
    HAL_StatusTypeDef status = HAL_I2C_Master_Transmit(&hi2c1, MS5837_ADDR, &cmd, 1, HAL_MAX_DELAY);
    if (status != HAL_OK) dbg_i2c_err++;
    HAL_Delay(20);
    return status;
}

static uint8_t MS5837_ReadProm(uint8_t prom_addr, uint16_t *val)
{
    uint8_t buf[2];

    if (HAL_I2C_Master_Transmit(&hi2c1, MS5837_ADDR, &prom_addr, 1, HAL_MAX_DELAY) != HAL_OK) {
        dbg_i2c_err++;
        return 0;
    }

    if (HAL_I2C_Master_Receive(&hi2c1, MS5837_ADDR, buf, 2, HAL_MAX_DELAY) != HAL_OK) {
        dbg_i2c_err++;
        return 0;
    }

    *val = ((uint16_t)buf[0] << 8) | buf[1];
    return 1;
}

static uint32_t MS5837_GetConversion(uint8_t cmd)
{
    uint8_t buf[3] = {0};

    if (HAL_I2C_Master_Transmit(&hi2c1, MS5837_ADDR, &cmd, 1, HAL_MAX_DELAY) != HAL_OK) {
        dbg_i2c_err++;
        return 0;
    }

    HAL_Delay(20);

    uint8_t adc_read = MS5837_ADC_READ;
    if (HAL_I2C_Master_Transmit(&hi2c1, MS5837_ADDR, &adc_read, 1, HAL_MAX_DELAY) != HAL_OK) {
        dbg_i2c_err++;
        return 0;
    }

    if (HAL_I2C_Master_Receive(&hi2c1, MS5837_ADDR, buf, 3, HAL_MAX_DELAY) != HAL_OK) {
        dbg_i2c_err++;
        return 0;
    }

    return ((uint32_t)buf[0] << 16) | ((uint32_t)buf[1] << 8) | buf[2];
}

/* ---- 对外函数 ---- */

void MS5837_Init(void)
{
    dbg_i2c_err = 0;

    /* 复位传感器 */
    HAL_StatusTypeDef rst = MS5837_Reset();

    /* 读取 PROM 校准系数 (带重试, 参考 B02 例程) */
    int prom_ok = 0;
    for (int retry = 0; retry < PROM_RETRY_MAX && !prom_ok; retry++) {
        if (retry > 0) {
            MS5837_Reset();
        }

        prom_ok = 1;
        for (int i = 0; i < 7; i++) {
            uint8_t prom_addr = MS5837_PROM_READ + (i * 2);
            uint16_t val;
            if (MS5837_ReadProm(prom_addr, &val)) {
                Cal_C[i] = val;
            } else {
                Cal_C[i] = 0;
                prom_ok = 0;
            }
        }
    }

    for (int i = 0; i < 7; i++)
        dbg_Cal[i] = Cal_C[i];

    /* 采集 5 组大气压参考值 */
    Atmdsphere_Pressure = 0;
    int valid_count = 0;
    float depth, temp;
    for (int i = 0; i < 5; i++) {
        HAL_Delay(100);
        MS5837_Read(&depth, &temp);
        if (Pressure >= 300 && Pressure <= 1200) {
            Atmdsphere_Pressure += Pressure;
            valid_count++;
        }
    }

    if (valid_count > 0) {
        Atmdsphere_Pressure /= valid_count;
        dbg_init_ok = 1;
    } else {
        Atmdsphere_Pressure = 1013;
        dbg_init_ok = 0;
    }
}

void MS5837_Read(float *depth, float *temperature)
{
    uint32_t D2_Temp = MS5837_GetConversion(MS5837_D2_CONVERT);
    HAL_Delay(20);
    uint32_t D1_Pres = MS5837_GetConversion(MS5837_D1_CONVERT);
    HAL_Delay(20);

    dbg_D1 = D1_Pres;
    dbg_D2 = D2_Temp;

    if (D1_Pres == 0 || D2_Temp == 0) {
        *depth = 0;
        *temperature = 0;
        return;
    }

    /* 一阶补偿 (数据手册公式) */
    int64_t dT   = (int64_t)D2_Temp - (int64_t)Cal_C[5] * 256;
    int64_t SENS = (int64_t)Cal_C[1] * 65536  + ((int64_t)Cal_C[3] * dT) / 128;
    int64_t OFF  = (int64_t)Cal_C[2] * 131072 + ((int64_t)Cal_C[4] * dT) / 64;
    int32_t TEMP = 2000 + (dT * Cal_C[6]) / 8388608;

    /* 二阶低温补偿 (TEMP < 20°C) */
    int32_t Ti = 0, OFFi = 0, SENSi = 0;
    if (TEMP < 2000) {
        Ti    = (11 * dT * dT) / 34359738368LL;
        OFFi  = (31 * (TEMP - 2000) * (TEMP - 2000)) / 8;
        SENSi = (63 * (TEMP - 2000) * (TEMP - 2000)) / 32;
    }

    int64_t OFF2  = OFF  - OFFi;
    int64_t SENS2 = SENS - SENSi;

    /* 压力 (mbar) = (D1*SENS2/2^21 - OFF2) / 2^15 / 100 */
    Pressure = (int32_t)(((D1_Pres * SENS2) / 2097152 - OFF2) / 32768) / 100;

    float temp_val = (TEMP - Ti) / 100.0f;

    /* 深度 (cm) = 系数 * (当前气压 - 水面大气压) */
    float depth_val = 0.983615f * ((float)Pressure - (float)Atmdsphere_Pressure);
    if (depth_val < 0) depth_val = 0;

    *depth = depth_val;
    *temperature = temp_val;
}
