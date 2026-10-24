#ifndef MS5837_H
#define MS5837_H

#include "main.h"

#define MS5837_ADDR (0x76U << 1)
#define MS5837_D1_CMD 0x48U
#define MS5837_D2_CMD 0x58U

HAL_StatusTypeDef MS5837_Reset(I2C_HandleTypeDef *i2c);

HAL_StatusTypeDef MS5837_ReadPROM(
    I2C_HandleTypeDef *i2c,
    uint8_t index,
    uint16_t *value
);
uint8_t MS5837_CalculateCRC(const uint16_t prom[7]);
HAL_StatusTypeDef MS5837_ReadADC(
    I2C_HandleTypeDef *i2c,
    uint8_t command,
    uint32_t *value
);
void MS5837_Calculate(
    uint32_t d1,
    uint32_t d2,
    const uint16_t prom[7],
    int32_t *temperature_x100,
    int32_t *pressure_x100
);
HAL_StatusTypeDef MS5837_Init(
    I2C_HandleTypeDef *i2c,
    uint16_t prom[7]
);
HAL_StatusTypeDef MS5837_Read(
    I2C_HandleTypeDef *i2c,
    const uint16_t prom[7],
    uint32_t *d1,
    uint32_t *d2,
    int32_t *temperature_x100,
    int32_t *pressure_x100
);
HAL_StatusTypeDef MS5837_ReadReference(
    I2C_HandleTypeDef *i2c,
    const uint16_t prom[7],
    uint8_t sample_count,
    int32_t *reference_x100
);
int32_t MS5837_CalculateDepth(
    int32_t pressure_x100,
    int32_t reference_x100
);
#endif
