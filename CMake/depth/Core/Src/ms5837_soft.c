/*
 * ms5837_soft.c
 * MS5837-02BA 驱动 — 软件 I2C 版本
 * 直接移植自: BlueRobots B02 例程 MS5837.c
 */

#include "ms5837_soft.h"

uint32_t MS5837_Cal[7];
double   MS5837_Temperature;
int32_t  MS5837_Pressure;
uint32_t MS5837_D1, MS5837_D2;

static int32_t  dT, TEMP;
static int64_t  OFF_, SENS;
static int32_t  OFFi, SENSi, Ti;
static int64_t  OFF2, SENS2;

void MS5837_Soft_Reset(void)
{
    Soft_I2C_Start();
    Soft_I2C_Send_Byte(MS5837_WRITE);
    Soft_I2C_Wait_Ack();
    Soft_I2C_Send_Byte(MS5837_RESET);
    Soft_I2C_Wait_Ack();
    Soft_I2C_Stop();
}

uint8_t MS5837_Soft_PROM(void)
{
    uint8_t inth, intl;
    MS5837_Soft_Reset();
    HAL_Delay(20);

    for (int i = 0; i < 7; i++) {
        Soft_I2C_Start();
        Soft_I2C_Send_Byte(MS5837_WRITE);
        Soft_I2C_Wait_Ack();
        Soft_I2C_Send_Byte(MS5837_PROM_RD + (i * 2));
        Soft_I2C_Wait_Ack();
        Soft_I2C_Stop();

        delay_us(5);

        Soft_I2C_Start();
        Soft_I2C_Send_Byte(MS5837_READ);
        delay_us(5);
        Soft_I2C_Wait_Ack();
        inth = Soft_I2C_Read_Byte(1);
        delay_us(5);
        intl = Soft_I2C_Read_Byte(0);
        Soft_I2C_Stop();

        MS5837_Cal[i] = (((uint16_t)inth << 8) | intl);
    }
    return !MS5837_Cal[0];
}

uint32_t MS5837_Soft_GetConversion(uint8_t cmd)
{
    uint32_t conv = 0;
    uint8_t temp[3];

    Soft_I2C_Start();
    Soft_I2C_Send_Byte(MS5837_WRITE);
    Soft_I2C_Wait_Ack();
    Soft_I2C_Send_Byte(cmd);
    Soft_I2C_Wait_Ack();
    Soft_I2C_Stop();

    HAL_Delay(10);

    Soft_I2C_Start();
    Soft_I2C_Send_Byte(MS5837_WRITE);
    Soft_I2C_Wait_Ack();
    Soft_I2C_Send_Byte(0);
    Soft_I2C_Wait_Ack();
    Soft_I2C_Stop();

    Soft_I2C_Start();
    Soft_I2C_Send_Byte(MS5837_READ);
    Soft_I2C_Wait_Ack();
    temp[0] = Soft_I2C_Read_Byte(1);
    temp[1] = Soft_I2C_Read_Byte(1);
    temp[2] = Soft_I2C_Read_Byte(0);
    Soft_I2C_Stop();

    conv = (uint32_t)temp[0] * 65536 + (uint32_t)temp[1] * 256 + (uint32_t)temp[2];
    return conv;
}

void MS5837_Soft_GetData(void)
{
    MS5837_D2 = MS5837_Soft_GetConversion(MS5837_D2_4096);
    HAL_Delay(20);
    MS5837_D1 = MS5837_Soft_GetConversion(MS5837_D1_4096);
    HAL_Delay(20);

    dT   = MS5837_D2 - (((uint32_t)MS5837_Cal[5]) * 256L);
    SENS = (int64_t)MS5837_Cal[1] * 65536L + ((int64_t)MS5837_Cal[3] * dT) / 128L;
    OFF_ = (int64_t)MS5837_Cal[2] * 131072L + ((int64_t)MS5837_Cal[4] * dT) / 64L;
    TEMP = 2000L + (int64_t)(dT)*MS5837_Cal[6] / 8388608LL;

    OFFi = 0; SENSi = 0; Ti = 0;
    if (TEMP < 2000) {
        Ti    = (11 * (int64_t)(dT) * (int64_t)(dT) / (34359738368LL));
        OFFi  = (31 * (TEMP - 2000) * (TEMP - 2000)) / 8;
        SENSi = (63 * (TEMP - 2000) * (TEMP - 2000)) / 32;
    }

    OFF2  = OFF_ - OFFi;
    SENS2 = SENS - SENSi;

    MS5837_Pressure    = ((MS5837_D1 * SENS2) / 2097152L - OFF2) / 32768L / 100;
    MS5837_Temperature = (TEMP - Ti) / 100.0;
}

uint8_t MS5837_Soft_Crc4(void)
{
    int cnt;
    unsigned int n_rem = 0;
    unsigned char n_bit;
    unsigned short n_prom[8];

    for (int t = 0; t < 7; t++)
        n_prom[t] = MS5837_Cal[t];

    n_prom[0] = ((n_prom[0]) & 0x0FFF);
    n_prom[7] = 0;

    for (cnt = 0; cnt < 16; cnt++) {
        if (cnt % 2 == 1)
            n_rem ^= (unsigned short)((n_prom[cnt >> 1]) & 0x00FF);
        else
            n_rem ^= (unsigned short)(n_prom[cnt >> 1] >> 8);

        for (n_bit = 8; n_bit > 0; n_bit--) {
            if (n_rem & 0x8000)
                n_rem = (n_rem << 1) ^ 0x3000;
            else
                n_rem = (n_rem << 1);
        }
    }

    n_rem = ((n_rem >> 12) & 0x000F);
    return (n_rem ^ 0x00);
}
