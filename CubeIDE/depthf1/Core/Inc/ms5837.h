#ifndef __MS5837_H_
#define __MS5837_H_

#include "main.h"
#include "i2c.h"

#define MS5837_MODEL_AUTO 0U
#define MS5837_MODEL_02BA 2U
#define MS5837_MODEL_30BA 30U

/* Confirm the sensor model before overriding an unknown PROM version. */
#ifndef MS5837_MODEL_OVERRIDE
#define MS5837_MODEL_OVERRIDE MS5837_MODEL_AUTO
#endif

/* Fresh water. Adjust this value for the liquid being measured. */
#ifndef MS5837_FLUID_DENSITY
#define MS5837_FLUID_DENSITY 1000.0f
#endif

typedef enum {
    MS5837_OK = 0,
    MS5837_ERROR_I2C,
    MS5837_ERROR_DATA,
    MS5837_ERROR_CRC,
    MS5837_ERROR_MODEL,
    MS5837_ERROR_NOT_INITIALIZED
} MS5837_StatusTypeDef;

typedef struct {
    const char *stage;
    HAL_StatusTypeDef hal_status;
    uint32_t hal_error;
    uint8_t prom_index;
    uint8_t version;
    uint8_t crc_expected;
    uint8_t crc_actual;
} MS5837_DiagnosticsTypeDef;

MS5837_StatusTypeDef MS5837_init(void);
MS5837_StatusTypeDef MS5837_Getdata(void);
const char *MS5837_StatusName(MS5837_StatusTypeDef status);
const char *MS5837_ModelName(void);

extern uint32_t Cal_C[7];
extern uint32_t D1_Pres, D2_Temp;
extern uint32_t Pressure;       /* Integer mbar, for existing callers. */
extern float Pressure_mbar;     /* Pressure retaining sensor resolution. */
extern float Atmosphere_mbar;   /* Reference measured during initialization. */
extern float Temperature;       /* Degrees Celsius. */
extern float Depth;             /* Centimetres relative to the startup reference. */
extern MS5837_DiagnosticsTypeDef MS5837_Diagnostics;

#endif
