#ifndef MS5837_H
#define MS5837_H

#include "soft_i2c.h"

#define MS5837_ADDR 0x76U /* Software bus accepts an unshifted 7-bit address. */
#define MS5837_D1_CMD 0x48U
#define MS5837_D2_CMD 0x58U

HAL_StatusTypeDef MS5837_Reset(SoftI2C_HandleTypeDef *i2c);

HAL_StatusTypeDef MS5837_ReadPROM(
    SoftI2C_HandleTypeDef *i2c,
    uint8_t index,
    uint16_t *value
);
uint8_t MS5837_CalculateCRC(const uint16_t prom[7]);
HAL_StatusTypeDef MS5837_ReadADC(
    SoftI2C_HandleTypeDef *i2c,
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
    SoftI2C_HandleTypeDef *i2c,
    uint16_t prom[7]
);
HAL_StatusTypeDef MS5837_Read(
    SoftI2C_HandleTypeDef *i2c,
    const uint16_t prom[7],
    uint32_t *d1,
    uint32_t *d2,
    int32_t *temperature_x100,
    int32_t *pressure_x100
);
HAL_StatusTypeDef MS5837_ReadReference(
    SoftI2C_HandleTypeDef *i2c,
    const uint16_t prom[7],
    uint8_t sample_count,
    int32_t *reference_x100
);
int32_t MS5837_CalculateDepth(
    int32_t pressure_x100,
    int32_t reference_x100
);
#endif
