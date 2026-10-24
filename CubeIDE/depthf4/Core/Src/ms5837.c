#include "ms5837.h"
//发送0x1E复位传感器
HAL_StatusTypeDef MS5837_Reset(I2C_HandleTypeDef *i2c)
{
	uint8_t command = 0x1E;
	    HAL_StatusTypeDef status;

	    status = HAL_I2C_Master_Transmit(
	        i2c,
	        MS5837_ADDR,
	        &command,
	        1,
	        100
	    );//==>HAL_I2C_Master_Transmit(i2c,MS5837_ADDR,&command,1,100)
	    if (status != HAL_OK)
	        {
	            return status;
	        }

	        HAL_Delay(20);

	        return HAL_OK;
}

HAL_StatusTypeDef MS5837_ReadPROM(
    I2C_HandleTypeDef *i2c,
    uint8_t index,
    uint16_t *value
)
{
    uint8_t command;
    uint8_t data[2];
    HAL_StatusTypeDef status;

    if (index > 6 || value == NULL)
    {
        return HAL_ERROR;
    }

    command = 0xA0U + index * 2U;

    status = HAL_I2C_Master_Transmit(
        i2c, MS5837_ADDR, &command, 1, 100
    );

    if (status != HAL_OK)
    {
        return status;
    }

    status = HAL_I2C_Master_Receive(
        i2c, MS5837_ADDR, data, 2, 100
    );

    if (status != HAL_OK)
    {
        return status;
    }

    *value = ((uint16_t)data[0] << 8) | data[1];

    return HAL_OK;
}

uint8_t MS5837_CalculateCRC(const uint16_t prom[7])
{
    uint16_t data[8] = {0};
    uint16_t remainder = 0;

    for (uint8_t i = 0; i < 7; i++)
    {
        data[i] = prom[i];
    }

    data[0] &= 0x0FFFU;

    for (uint8_t i = 0; i < 16; i++)
    {
        if (i % 2 == 0)
        {
            remainder ^= data[i / 2] >> 8;
        }
        else
        {
            remainder ^= data[i / 2] & 0x00FFU;
        }

        for (uint8_t bit = 0; bit < 8; bit++)
        {
            if (remainder & 0x8000U)
            {
                remainder =
                    (uint16_t)((remainder << 1) ^ 0x3000U);
            }
            else
            {
                remainder = (uint16_t)(remainder << 1);
            }
        }
    }

    return (uint8_t)((remainder >> 12) & 0x0FU);
}

HAL_StatusTypeDef MS5837_ReadADC(
    I2C_HandleTypeDef *i2c,
    uint8_t command,
    uint32_t *value
)
{
    uint8_t read_command = 0x00;
    uint8_t data[3];
    HAL_StatusTypeDef status;

    if (value == NULL)
    {
        return HAL_ERROR;
    }

    status = HAL_I2C_Master_Transmit(
        i2c, MS5837_ADDR, &command, 1, 100
    );

    if (status != HAL_OK)
    {
        return status;
    }

    HAL_Delay(20);

    status = HAL_I2C_Master_Transmit(
        i2c, MS5837_ADDR, &read_command, 1, 100
    );

    if (status != HAL_OK)
    {
        return status;
    }

    status = HAL_I2C_Master_Receive(
        i2c, MS5837_ADDR, data, 3, 100
    );

    if (status != HAL_OK)
    {
        return status;
    }

    *value = ((uint32_t)data[0] << 16)
           | ((uint32_t)data[1] << 8)
           | data[2];

    if (*value == 0)
    {
        return HAL_ERROR;
    }

    return HAL_OK;
}
void MS5837_Calculate(
    uint32_t d1,
    uint32_t d2,
    const uint16_t prom[7],
    int32_t *temperature_x100,
    int32_t *pressure_x100
)
{
    int64_t dT =
        (int64_t)d2 - (int64_t)prom[5] * 256;

    int64_t temp =
        2000 + dT * prom[6] / 8388608;

    int64_t off =
        (int64_t)prom[2] * 131072
        + (int64_t)prom[4] * dT / 64;

    int64_t sens =
        (int64_t)prom[1] * 65536
        + (int64_t)prom[3] * dT / 128;

    int64_t temp_i = 0;
    int64_t off_i = 0;
    int64_t sens_i = 0;

    if (temp < 2000)
    {
        int64_t temp_diff = temp - 2000;

        temp_i = 11 * dT * dT / 34359738368LL;
        off_i = 31 * temp_diff * temp_diff / 8;
        sens_i = 63 * temp_diff * temp_diff / 32;
    }

    off = off - off_i;
    sens = sens - sens_i;

    int64_t pressure =
        ((int64_t)d1 * sens / 2097152 - off) / 32768;

    *temperature_x100 = (int32_t)(temp - temp_i);
    *pressure_x100 = (int32_t)pressure;
}

HAL_StatusTypeDef MS5837_Init(
    I2C_HandleTypeDef *i2c,
    uint16_t prom[7]
)
{
    if (i2c == NULL || prom == NULL)
    {
        return HAL_ERROR;
    }

    HAL_StatusTypeDef status = MS5837_Reset(i2c);

    if (status != HAL_OK)
    {
        return status;
    }

    for (uint8_t i = 0; i < 7; i++)
    {
        status = MS5837_ReadPROM(i2c, i, &prom[i]);

        if (status != HAL_OK)
        {
            return status;
        }
    }

    uint8_t stored_crc = prom[0] >> 12;
    uint8_t calculated_crc = MS5837_CalculateCRC(prom);

    if (stored_crc != calculated_crc)
    {
        return HAL_ERROR;
    }

    return HAL_OK;
}
HAL_StatusTypeDef MS5837_Read(
    I2C_HandleTypeDef *i2c,
    const uint16_t prom[7],
    uint32_t *d1,
    uint32_t *d2,
    int32_t *temperature_x100,
    int32_t *pressure_x100
)
{
    if (i2c == NULL || prom == NULL ||
        d1 == NULL || d2 == NULL ||
        temperature_x100 == NULL || pressure_x100 == NULL)
    {
        return HAL_ERROR;
    }

    uint32_t next_d1 = 0;
    uint32_t next_d2 = 0;
    HAL_StatusTypeDef status;

    status = MS5837_ReadADC(i2c, MS5837_D1_CMD, &next_d1);

    if (status != HAL_OK)
    {
        return status;
    }

    status = MS5837_ReadADC(i2c, MS5837_D2_CMD, &next_d2);

    if (status != HAL_OK)
    {
        return status;
    }

    MS5837_Calculate(
        next_d1,
        next_d2,
        prom,
        temperature_x100,
        pressure_x100
    );

    *d1 = next_d1;
    *d2 = next_d2;

    return HAL_OK;
}
HAL_StatusTypeDef MS5837_ReadReference(
    I2C_HandleTypeDef *i2c,
    const uint16_t prom[7],
    uint8_t sample_count,
    int32_t *reference_x100
)
{
    if (i2c == NULL || prom == NULL ||
        reference_x100 == NULL || sample_count == 0)
    {
        return HAL_ERROR;
    }

    int64_t pressure_sum = 0;

    for (uint8_t i = 0; i < sample_count; i++)
    {
        uint32_t d1 = 0;
        uint32_t d2 = 0;
        int32_t temperature_x100 = 0;
        int32_t pressure_x100 = 0;

        HAL_StatusTypeDef status = MS5837_Read(
            i2c,
            prom,
            &d1,
            &d2,
            &temperature_x100,
            &pressure_x100
        );

        if (status != HAL_OK)
        {
            return status;
        }

        pressure_sum += pressure_x100;

        HAL_Delay(20);
    }

    *reference_x100 =
        (int32_t)(pressure_sum / sample_count);

    return HAL_OK;
}
int32_t MS5837_CalculateDepth(
    int32_t pressure_x100,
    int32_t reference_x100
)
{
    int64_t delta_pa =
        (int64_t)pressure_x100 - (int64_t)reference_x100;

    return (int32_t)(delta_pa * 1000000LL / 980665LL);
}
