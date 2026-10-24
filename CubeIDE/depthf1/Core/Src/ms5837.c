#include "ms5837.h"

/* HAL needs the 7-bit address shifted left once, for both TX and RX. */
#define MS5837_ADDR (0x76U << 1)
#define MS5837_I2C hi2c1
#define MS5837_TIMEOUT_MS 100U
#define MS5837_RESET_CMD 0x1EU
#define MS5837_ADC_READ_CMD 0x00U
#define MS5837_PROM_READ_CMD 0xA0U
#define MS5837_D1_CMD 0x48U
#define MS5837_D2_CMD 0x58U

uint32_t Cal_C[7];
uint32_t D1_Pres, D2_Temp;
uint32_t Pressure;
float Pressure_mbar, Atmosphere_mbar, Temperature, Depth;
MS5837_DiagnosticsTypeDef MS5837_Diagnostics = { .stage = "not-initialized" };

static uint8_t calibration_ready;
static uint8_t reference_ready;
static uint8_t sensor_model;

static MS5837_StatusTypeDef CheckHAL(HAL_StatusTypeDef status)
{
    MS5837_Diagnostics.hal_status = status;
    MS5837_Diagnostics.hal_error = HAL_I2C_GetError(&MS5837_I2C);
    return status == HAL_OK ? MS5837_OK : MS5837_ERROR_I2C;
}

static MS5837_StatusTypeDef SendCommand(uint8_t command)
{
    return CheckHAL(HAL_I2C_Master_Transmit(&MS5837_I2C, MS5837_ADDR,
                                           &command, 1, MS5837_TIMEOUT_MS));
}

static uint8_t PromCRC(void)
{
    uint16_t remainder = 0;
    for (uint8_t i = 0; i < 16; ++i) {
        uint16_t word = i < 14 ? (uint16_t)Cal_C[i / 2] : 0;
        if (i < 2) word &= 0x0FFFU;
        remainder ^= (i & 1U) ? (word & 0xFFU) : (word >> 8);
        for (uint8_t bit = 0; bit < 8; ++bit) {
            remainder = (uint16_t)((remainder << 1) ^
                         ((remainder & 0x8000U) ? 0x3000U : 0U));
        }
    }
    return (uint8_t)(remainder >> 12);
}

static MS5837_StatusTypeDef ReadADC(uint8_t command, uint32_t *value)
{
    uint8_t bytes[3];
    *value = 0;
    MS5837_Diagnostics.stage = command == MS5837_D1_CMD ? "D1-start" : "D2-start";
    if (SendCommand(command) != MS5837_OK) return MS5837_ERROR_I2C;
    HAL_Delay(20); /* OSR 4096: both models finish within 10 ms. */

    MS5837_Diagnostics.stage = command == MS5837_D1_CMD ? "D1-command" : "D2-command";
    if (SendCommand(MS5837_ADC_READ_CMD) != MS5837_OK) return MS5837_ERROR_I2C;
    MS5837_Diagnostics.stage = command == MS5837_D1_CMD ? "D1-read" : "D2-read";
    if (CheckHAL(HAL_I2C_Master_Receive(&MS5837_I2C, MS5837_ADDR,
                                      bytes, 3, MS5837_TIMEOUT_MS)) != MS5837_OK) {
        return MS5837_ERROR_I2C;
    }
    *value = ((uint32_t)bytes[0] << 16) | ((uint32_t)bytes[1] << 8) | bytes[2];
    if (*value == 0 || *value == 0xFFFFFFU) return MS5837_ERROR_DATA;
    return MS5837_OK;
}

MS5837_StatusTypeDef MS5837_Getdata(void)
{
    MS5837_StatusTypeDef status;
    int64_t dT, temp, off, sens, ti = 0, offi = 0, sensi = 0;
    int64_t dt_squared, temp_squared, pressure_scaled;
    float next_pressure, next_temperature;

    if (!calibration_ready) {
        MS5837_Diagnostics.stage = "not-initialized";
        return MS5837_ERROR_NOT_INITIALIZED;
    }
    D1_Pres = D2_Temp = 0;
    status = ReadADC(MS5837_D1_CMD, &D1_Pres);
    if (status != MS5837_OK) return status;
    status = ReadADC(MS5837_D2_CMD, &D2_Temp);
    if (status != MS5837_OK) return status;

    /* Signed 64-bit arithmetic covers temperatures below zero as well. */
    dT = (int64_t)D2_Temp - (int64_t)Cal_C[5] * 256;
    temp = 2000 + dT * Cal_C[6] / 8388608LL;
    dt_squared = dT * dT;
    temp_squared = (temp - 2000) * (temp - 2000);

    if (sensor_model == MS5837_MODEL_02BA) {
        off = (int64_t)Cal_C[2] * 131072 + (int64_t)Cal_C[4] * dT / 64;
        sens = (int64_t)Cal_C[1] * 65536 + (int64_t)Cal_C[3] * dT / 128;
        if (temp < 2000) {
            ti = 11 * dt_squared / 34359738368LL;
            offi = 31 * temp_squared / 8;
            sensi = 63 * temp_squared / 32;
        }
        pressure_scaled = ((int64_t)D1_Pres * (sens - sensi) / 2097152LL -
                           (off - offi)) / 32768;
        next_pressure = (float)pressure_scaled / 100.0f;
    } else {
        off = (int64_t)Cal_C[2] * 65536 + (int64_t)Cal_C[4] * dT / 128;
        sens = (int64_t)Cal_C[1] * 32768 + (int64_t)Cal_C[3] * dT / 256;
        if (temp < 2000) {
            ti = 3 * dt_squared / 8589934592LL;
            offi = 3 * temp_squared / 2;
            sensi = 5 * temp_squared / 8;
            if (temp < -1500) {
                int64_t very_cold = (temp + 1500) * (temp + 1500);
                offi += 7 * very_cold;
                sensi += 4 * very_cold;
            }
        } else {
            ti = 2 * dt_squared / 137438953472LL;
            offi = temp_squared / 16;
        }
        pressure_scaled = ((int64_t)D1_Pres * (sens - sensi) / 2097152LL -
                           (off - offi)) / 8192;
        next_pressure = (float)pressure_scaled / 10.0f;
    }
    next_temperature = (float)(temp - ti) / 100.0f;

    MS5837_Diagnostics.stage = "calculate";
    if (next_pressure <= 0.0f ||
        next_pressure > (sensor_model == MS5837_MODEL_02BA ? 2000.0f : 30000.0f) ||
        next_temperature < -20.0f || next_temperature > 85.0f) {
        return MS5837_ERROR_DATA;
    }
    Pressure_mbar = next_pressure;
    Pressure = (uint32_t)next_pressure;
    Temperature = next_temperature;
    /* Convert mbar to Pa, then metres to centimetres. */
    Depth = reference_ready ? (Pressure_mbar - Atmosphere_mbar) *
            10000.0f / (MS5837_FLUID_DENSITY * 9.80665f) : 0.0f;
    MS5837_Diagnostics.stage = "ready";
    return MS5837_OK;
}

MS5837_StatusTypeDef MS5837_init(void)
{
    uint8_t bytes[2], valid_samples = 0;
    float pressure_sum = 0.0f;
    MS5837_StatusTypeDef status = MS5837_ERROR_DATA;

    calibration_ready = reference_ready = 0;
    sensor_model = MS5837_MODEL_AUTO;
    Pressure = D1_Pres = D2_Temp = 0;
    Pressure_mbar = Atmosphere_mbar = Temperature = Depth = 0.0f;
    for (uint8_t i = 0; i < 7; ++i) Cal_C[i] = 0;
    MS5837_Diagnostics = (MS5837_DiagnosticsTypeDef){ .stage = "probe" };

    if (CheckHAL(HAL_I2C_IsDeviceReady(&MS5837_I2C, MS5837_ADDR,
                                      2, MS5837_TIMEOUT_MS)) != MS5837_OK) {
        return MS5837_ERROR_I2C;
    }
    MS5837_Diagnostics.stage = "reset";
    if (SendCommand(MS5837_RESET_CMD) != MS5837_OK) return MS5837_ERROR_I2C;
    HAL_Delay(20);

    for (uint8_t i = 0; i < 7; ++i) {
        MS5837_Diagnostics.prom_index = i;
        MS5837_Diagnostics.stage = "PROM-command";
        if (SendCommand((uint8_t)(MS5837_PROM_READ_CMD + i * 2)) != MS5837_OK) {
            return MS5837_ERROR_I2C;
        }
        /* The datasheet uses separate command and receive transactions. */
        MS5837_Diagnostics.stage = "PROM-read";
        if (CheckHAL(HAL_I2C_Master_Receive(&MS5837_I2C, MS5837_ADDR,
                                          bytes, 2, MS5837_TIMEOUT_MS)) != MS5837_OK) {
            return MS5837_ERROR_I2C;
        }
        Cal_C[i] = ((uint32_t)bytes[0] << 8) | bytes[1];
        if (i > 0 && (Cal_C[i] == 0 || Cal_C[i] == 0xFFFFU)) return MS5837_ERROR_DATA;
    }

    MS5837_Diagnostics.version = (uint8_t)((Cal_C[0] >> 5) & 0x7FU);
    MS5837_Diagnostics.stage = "PROM-CRC";
    MS5837_Diagnostics.crc_expected = (uint8_t)(Cal_C[0] >> 12);
    MS5837_Diagnostics.crc_actual = PromCRC();
    if (MS5837_Diagnostics.crc_expected != MS5837_Diagnostics.crc_actual) {
        return MS5837_ERROR_CRC;
    }

    MS5837_Diagnostics.stage = "model";
    if (MS5837_MODEL_OVERRIDE == MS5837_MODEL_02BA ||
        MS5837_MODEL_OVERRIDE == MS5837_MODEL_30BA) {
        sensor_model = MS5837_MODEL_OVERRIDE;
    } else if (MS5837_Diagnostics.version == 0x00U ||
               MS5837_Diagnostics.version == 0x15U) {
        sensor_model = MS5837_MODEL_02BA;
    } else if (MS5837_Diagnostics.version == 0x1AU) {
        sensor_model = MS5837_MODEL_30BA;
    } else {
        return MS5837_ERROR_MODEL;
    }
    calibration_ready = 1;

    /* Power on in air; average only successful reference samples. */
    for (uint8_t i = 0; i < 5; ++i) {
        HAL_Delay(100);
        status = MS5837_Getdata();
        if (status != MS5837_OK) continue;
        pressure_sum += Pressure_mbar;
        ++valid_samples;
    }
    if (valid_samples == 0) {
        calibration_ready = 0;
        return status;
    }
    Atmosphere_mbar = pressure_sum / valid_samples;
    reference_ready = 1;
    Depth = (Pressure_mbar - Atmosphere_mbar) *
            10000.0f / (MS5837_FLUID_DENSITY * 9.80665f);
    MS5837_Diagnostics.stage = "ready";
    MS5837_Diagnostics.hal_status = HAL_OK;
    MS5837_Diagnostics.hal_error = 0;
    return MS5837_OK;
}

const char *MS5837_StatusName(MS5837_StatusTypeDef status)
{
    switch (status) {
        case MS5837_OK: return "OK";
        case MS5837_ERROR_I2C: return "I2C";
        case MS5837_ERROR_DATA: return "DATA";
        case MS5837_ERROR_CRC: return "CRC";
        case MS5837_ERROR_MODEL: return "MODEL";
        default: return "NOT_INITIALIZED";
    }
}

const char *MS5837_ModelName(void)
{
    if (sensor_model == MS5837_MODEL_02BA) return "02BA";
    if (sensor_model == MS5837_MODEL_30BA) return "30BA";
    return "unknown";
}
