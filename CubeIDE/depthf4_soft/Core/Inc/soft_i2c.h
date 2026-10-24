#ifndef SOFT_I2C_H
#define SOFT_I2C_H

#include "main.h"

/* Half-period 20 us gives at most about 25 kHz, plus GPIO/software overhead. */
#define SOFT_I2C_DEFAULT_HALF_PERIOD_US 20U

typedef struct
{
    GPIO_TypeDef *scl_port;
    uint16_t scl_pin;
    GPIO_TypeDef *sda_port;
    uint16_t sda_pin;
    uint32_t half_period_us;
    uint32_t cycles_per_us;
    uint8_t initialized;
} SoftI2C_HandleTypeDef;

/* Configures open-drain GPIOs with internal pull-ups and a DWT timebase.
 * Call after SystemClock_Config. This bus is for one controller, used sequentially.
 * External pull-ups may still be needed for the actual wiring capacitance. */
HAL_StatusTypeDef SoftI2C_Init(SoftI2C_HandleTypeDef *bus);

/* address is the unshifted 7-bit address, e.g. 0x76 for MS5837.
 * Each call sends START, address, data, STOP. timeout_ms must be nonzero. */
HAL_StatusTypeDef SoftI2C_Master_Transmit(
    SoftI2C_HandleTypeDef *bus, uint16_t address,
    const uint8_t *data, uint16_t size, uint32_t timeout_ms);

HAL_StatusTypeDef SoftI2C_Master_Receive(
    SoftI2C_HandleTypeDef *bus, uint16_t address,
    uint8_t *data, uint16_t size, uint32_t timeout_ms);

#endif
