/* 主机专用HAL桩：只提供MS5837驱动实际使用的类型和入口。
 * 预先定义CubeMX main.h的保护宏，避免主机测试引入真实STM32寄存器。
 * 编译时仍通过-I使用实际ms5837.h和ms5837.c，不复制或修改生产驱动。
 */
#ifndef DEPTH_CONTINUOUS_HAL_STUB_H
#define DEPTH_CONTINUOUS_HAL_STUB_H
#define __MAIN_H

#include <stdint.h>

typedef enum {HAL_OK = 0, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT} HAL_StatusTypeDef;
typedef enum {HAL_I2C_STATE_READY = 0, HAL_I2C_STATE_BUSY} HAL_I2C_StateTypeDef;
typedef struct {uint32_t unused;} DMA_HandleTypeDef;
typedef struct
{
    DMA_HandleTypeDef *hdmatx;
    DMA_HandleTypeDef *hdmarx;
    HAL_I2C_StateTypeDef state;
    uint32_t error;
} I2C_HandleTypeDef;

#define RESET 0U
#define I2C_FLAG_BUSY 1U
#define HAL_I2C_ERROR_NONE 0U
#define HAL_I2C_ERROR_TIMEOUT 0x20U
#define __HAL_I2C_GET_FLAG(handle, flag) TestHal_GetFlag((handle), (flag))

uint32_t HAL_GetTick(void);
HAL_I2C_StateTypeDef HAL_I2C_GetState(I2C_HandleTypeDef *handle);
uint32_t HAL_I2C_GetError(I2C_HandleTypeDef *handle);
uint32_t TestHal_GetFlag(I2C_HandleTypeDef *handle, uint32_t flag);
HAL_StatusTypeDef HAL_I2C_Master_Transmit_DMA(I2C_HandleTypeDef *handle,
    uint16_t address, uint8_t *data, uint16_t length);
HAL_StatusTypeDef HAL_I2C_Master_Receive_DMA(I2C_HandleTypeDef *handle,
    uint16_t address, uint8_t *data, uint16_t length);

#endif
