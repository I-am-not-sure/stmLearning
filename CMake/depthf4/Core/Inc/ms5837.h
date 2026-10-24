#ifndef MS5837_H
#define MS5837_H

#include "main.h"

#define MS5837_ADDR (0x76U << 1)
#define MS5837_D1_CMD 0x48U
#define MS5837_D2_CMD 0x58U

/* [主流程] 启动和周期测量共用一个驱动状态机，主循环逐步推进。 */
typedef enum
{
    MS5837_STOPPED = 0,
    MS5837_RESET_SEND,
    MS5837_RESET_WAIT_TX,
    MS5837_RESET_WAIT,
    MS5837_PROM_SEND,
    MS5837_PROM_WAIT_TX,
    MS5837_PROM_RECEIVE,
    MS5837_PROM_WAIT_RX,
    MS5837_PROM_CHECK_CRC,
    MS5837_STARTUP_WAIT,
    MS5837_CONV_SEND,
    MS5837_CONV_WAIT_TX,
    MS5837_CONV_WAIT,
    MS5837_ADC_SEND,
    MS5837_ADC_WAIT_TX,
    MS5837_ADC_RECEIVE,
    MS5837_ADC_WAIT_RX,
    MS5837_REFERENCE_GAP,
    MS5837_IDLE,
    MS5837_ERROR
} MS5837_State;

/* [失败处理] 区分通信故障、等待超时、标定校验失败及 ADC 数据异常。 */
typedef enum
{
    MS5837_FAULT_NONE = 0,
    MS5837_FAULT_I2C,
    MS5837_FAULT_TIMEOUT,
    MS5837_FAULT_CRC,
    MS5837_FAULT_ADC_ZERO,
    MS5837_FAULT_STATE
} MS5837_Fault;

/* [主流程] 一轮有效测量的数据；单位与原驱动保持一致。 */
typedef struct
{
    uint32_t d1;
    uint32_t d2;
    int32_t temperature_x100;  /* 0.01 摄氏度 */
    int32_t pressure_x100;     /* Pa，也就是 mbar x 100 */
    int32_t reference_x100;    /* 水面基准压力，Pa */
    int32_t depth_cm_x100;     /* cm x 100 */
} MS5837_Sample;

/* [失败处理/调试记录] state 是进入 ERROR 之前的出错状态。 */
typedef struct
{
    MS5837_State state;
    MS5837_Fault cause;
    HAL_StatusTypeDef status;
    uint32_t hal_error;
} MS5837_Error;

/* [主流程] 单传感器驱动；从 main() 启动一次，HAL_OK 只表示接受启动。
 * 传入的 IIC 句柄和驱动 DMA 缓冲区必须持续有效；故障后需复位再启动。 */
HAL_StatusTypeDef MS5837_StartDMA(
    I2C_HandleTypeDef *i2c, uint8_t reference_samples, uint32_t interval_ms);
/* [主流程] 每次主循环调用，不能只在输出周期到达时调用。 */
void MS5837_ProcessDMA(void);
/* [主流程] 取走一轮完成的数据；返回 1 表示成功，取走后允许下一轮。 */
uint8_t MS5837_TakeSample(MS5837_Sample *sample);
/* [调试记录] 基准建立后 IsReady 为 1；故障时 GetError 返回 1。 */
uint8_t MS5837_IsReady(void);
MS5837_State MS5837_GetState(void);
uint8_t MS5837_GetError(MS5837_Error *error);

/* [中断通知] 由 main.c 的 HAL 回调转发；只设置标志，不等待、不重启 IIC。 */
void MS5837_OnTxComplete(I2C_HandleTypeDef *i2c);
void MS5837_OnRxComplete(I2C_HandleTypeDef *i2c);
void MS5837_OnI2CError(I2C_HandleTypeDef *i2c);

/* [纯计算] 原有 CRC、温压补偿及水深算法，不包含通信等待。 */
uint8_t MS5837_CalculateCRC(const uint16_t prom[7]);
void MS5837_Calculate(uint32_t d1, uint32_t d2, const uint16_t prom[7],
    int32_t *temperature_x100, int32_t *pressure_x100);
int32_t MS5837_CalculateDepth(int32_t pressure_x100, int32_t reference_x100);

#endif
