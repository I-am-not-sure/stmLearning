/*
 * ms5837.c
 * MS5837-02BA 压力/深度传感器驱动（软件 I2C 版本）
 * 功能与 Depthsensor 工程的 ms5837.c 一致：读出压力(mbar)和深度(cm)
 * 注意：实测传感器是 02BA（量程 300~1200mbar），不是 30BA。02BA 与 30BA 的
 * 压力公式不同，若套用 30BA 公式结果会恰好偏大 20 倍。
 */

#include "ms5837.h"

/* 设备地址：MS5837 的 7 位地址固定为 0x76 */
#define MS5837_ADDR_WRITE 0xEC   /* 0x76 << 1     */
#define MS5837_ADDR_READ  0xED   /* 0x76 << 1 | 1 */

/* MS5837 命令 */
#define MS5837_RESET_CMD     0x1E   /* 复位 */
#define MS5837_ADC_READ_CMD  0x00   /* 读 ADC 结果 */
#define MS5837_PROM_READ_CMD 0xA0   /* 读 PROM 起始地址 */
#define MS5837_CONVERT_D1    0x4A   /* D1 压力转换，OSR=8192，约 17ms */
#define MS5837_CONVERT_D2    0x5A   /* D2 温度转换，OSR=8192，约 17ms */

/* 压力合理范围 (mbar)：02BA 量程 300~1200mbar（2bar 封顶） */
#define MIN_PRESSURE 300
#define MAX_PRESSURE 2000

/* 全局结果变量 */
uint32_t Cal_C[7];             /* C0..C6，计算用 C1..C6 */
uint32_t D1_Pres, D2_Temp;     /* 原始 ADC 值 */
float    Pressure;             /* 压力 mbar（带小数，保留亚 mbar 分辨率） */
float    Atmdsphere_Pressure;  /* 大气压参考 mbar */
float    Depth;                /* 深度 cm */

/* 中间变量（用有符号 64 位，避免温度/差值出现负数时溢出） */
static int64_t dT, TEMP;
static int64_t OFF_, SENS;

/* 发送一个命令字节（复位/转换/设 PROM 地址），0=成功，1=无应答 */
static uint8_t ms5837_send_cmd(uint8_t cmd)
{
    Soft_I2C_Start();
    Soft_I2C_Send_Byte(MS5837_ADDR_WRITE);
    if (Soft_I2C_Wait_Ack()) { Soft_I2C_Stop(); return 1; }
    Soft_I2C_Send_Byte(cmd);
    if (Soft_I2C_Wait_Ack()) { Soft_I2C_Stop(); return 1; }
    Soft_I2C_Stop();
    return 0;
}

/* 从从机连续读 n 字节，0=成功，1=无应答 */
static uint8_t ms5837_read_bytes(uint8_t *buf, uint8_t n)
{
    uint8_t i;
    Soft_I2C_Start();
    Soft_I2C_Send_Byte(MS5837_ADDR_READ);
    if (Soft_I2C_Wait_Ack()) { Soft_I2C_Stop(); return 1; }
    for (i = 0; i < n; i++) {
        buf[i] = Soft_I2C_Read_Byte(i < (n - 1));  /* 最后一字节发 NACK */
    }
    Soft_I2C_Stop();
    return 0;
}

/* 发起一次转换并读回 24 位结果，失败返回 0 */
static uint32_t ms5837_get_conversion(uint8_t convert_cmd)
{
    uint8_t buf[3] = {0};

    if (ms5837_send_cmd(convert_cmd))            /* 发起转换 */
        return 0;
    HAL_Delay(25);                               /* 等转换完成(OSR8192 约 17ms) */

    if (ms5837_send_cmd(MS5837_ADC_READ_CMD))    /* 切换到读结果 */
        return 0;
    if (ms5837_read_bytes(buf, 3))               /* 读 3 字节 */
        return 0;

    return ((uint32_t)buf[0] << 16) | ((uint32_t)buf[1] << 8) | (uint32_t)buf[2];
}

/* 校验 PROM 的 CRC4，0=通过，1=失败 */
static uint8_t ms5837_crc4(void)
{
    uint8_t  crc_read = (uint8_t)(Cal_C[0] >> 12);  /* C0 高 4 位是厂家写入的 CRC */
    uint16_t n_prom[8];
    uint16_t n_rem = 0;
    int cnt, bit;

    for (cnt = 0; cnt < 7; cnt++)
        n_prom[cnt] = (uint16_t)Cal_C[cnt];
    n_prom[0] &= 0x0FFF;   /* 掩掉 CRC 位 */
    n_prom[7]  = 0;

    for (cnt = 0; cnt < 16; cnt++) {
        if (cnt & 1)
            n_rem ^= (uint16_t)(n_prom[cnt >> 1] & 0x00FF);
        else
            n_rem ^= (uint16_t)(n_prom[cnt >> 1] >> 8);
        for (bit = 8; bit > 0; bit--) {
            if (n_rem & 0x8000)
                n_rem = (n_rem << 1) ^ 0x3000;
            else
                n_rem = (n_rem << 1);
        }
    }
    n_rem = (n_rem >> 12) & 0x000F;
    return (uint8_t)(n_rem ^ 0x00) == crc_read ? 0 : 1;
}

MS5837_StatusTypeDef MS5837_init(void)
{
    uint8_t buf[2];
    uint8_t i, valid;

    /* 1. 复位 */
    if (ms5837_send_cmd(MS5837_RESET_CMD))
        return MS5837_ERR_RESET;
    HAL_Delay(20);

    /* 2. 读 PROM C0..C6（C0 含 CRC，C1..C6 是校准系数） */
    for (i = 0; i < 7; i++) {
        if (ms5837_send_cmd(MS5837_PROM_READ_CMD + (i * 2)))
            return MS5837_ERR_PROM;
        if (ms5837_read_bytes(buf, 2))
            return MS5837_ERR_PROM;
        Cal_C[i] = ((uint16_t)buf[0] << 8) | buf[1];
        if (Cal_C[i] == 0 || Cal_C[i] == 0xFFFF)
            return MS5837_ERR_PROM;
    }

    /* 3. CRC4 校验（能抓住 I2C 时序不稳导致的 PROM 读错） */
    if (ms5837_crc4())
        return MS5837_ERR_CRC;

    /* 4. 采集大气压作为零位参考 */
    Atmdsphere_Pressure = 0;
    valid = 0;
    for (i = 0; i < 5; i++) {
        MS5837_Getdata();
        if (Pressure >= MIN_PRESSURE && Pressure <= MAX_PRESSURE) {
            Atmdsphere_Pressure += Pressure;
            valid++;
        }
        HAL_Delay(100);
    }
    if (valid > 0) {
        Atmdsphere_Pressure /= valid;   /* 按有效样本数取平均 */
        return MS5837_OK;
    }

    Atmdsphere_Pressure = 1013.0f;      /* 失败用标准大气压 */
    return MS5837_ERR_ZERO;
}

void MS5837_Getdata(void)
{
    /* 读原始值 */
    D1_Pres = ms5837_get_conversion(MS5837_CONVERT_D1);
    D2_Temp = ms5837_get_conversion(MS5837_CONVERT_D2);

    if (D1_Pres == 0 || D2_Temp == 0)
        return;   /* 读失败，保留上次结果 */

    /* 一阶温度补偿 (02BA) */
    dT   = (int64_t)D2_Temp - (int64_t)Cal_C[5] * 256;
    TEMP = 2000 + dT * (int64_t)Cal_C[6] / 8388608;                    /* /2^23 */
    OFF_ = (int64_t)Cal_C[2] * 131072 + (int64_t)Cal_C[4] * dT / 64;   /* *2^17, /2^6 */
    SENS = (int64_t)Cal_C[1] * 65536 + (int64_t)Cal_C[3] * dT / 128;   /* *2^16, /2^7 */

    /* 二阶补偿 (02BA，仅低温 <20°C 生效) */
    if (TEMP < 2000) {
        int64_t aux = (2000 - TEMP) * (2000 - TEMP);
        TEMP = TEMP - 11 * dT * dT / 34359738368LL;    /* Ti, /2^35 */
        OFF_ = OFF_ - 31 * aux / 8;
        SENS = SENS - 63 * aux / 32;
    }

    /* 压力：结果单位为 0.01mbar，除以 100 得 mbar (02BA)；用浮点保留亚 mbar 分辨率 */
    Pressure = (float)((int64_t)D1_Pres * SENS / 2097152 - OFF_) / 32768.0f / 100.0f;

    /* 压力异常时回退到大气压参考（此时深度为 0） */
    if (Pressure < MIN_PRESSURE || Pressure > MAX_PRESSURE)
        Pressure = Atmdsphere_Pressure;

    /* 深度：1mbar ≈ 0.983615cm，减去大气压后即水下深度 */
    Depth = 0.983615f * (Pressure - Atmdsphere_Pressure);
}
