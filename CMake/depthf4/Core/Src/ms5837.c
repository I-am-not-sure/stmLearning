/**
 * @file ms5837.c
 * @brief MS5837-30BA：DMA 状态机负责复位、PROM、基准和周期测量。
 * [主流程] main() 每轮调用 Process；中断只通知完成或错误。
 * [失败处理] 所有故障保留现场并停止新传输；本步不自动中止或恢复。
 */
#include "ms5837.h"
#include <string.h>

/* [配置] 沿用原驱动的等待时间，全部改为 HAL_GetTick 时间判断。 */
#define MS5837_RESET_WAIT_MS 20U
#define MS5837_CONVERSION_WAIT_MS 20U
#define MS5837_STARTUP_WAIT_MS 1000U
#define MS5837_REFERENCE_GAP_MS 20U
/* [失败处理配置] 单次通信/总线等待和一轮 D1+D2 测量的超时。 */
#define MS5837_TRANSFER_TIMEOUT_MS 100U
#define MS5837_CYCLE_TIMEOUT_MS 100U

/* [主流程] 持久上下文放在普通 SRAM，不能迁到 DMA 无法访问的 CCM。 */
typedef struct
{
    I2C_HandleTypeDef *i2c;
    MS5837_State state;
    uint32_t state_since;
    uint32_t cycle_since;
    uint32_t last_sample_start;
    uint32_t interval_ms;
    uint16_t prom[7];
    uint8_t prom_index;
    uint8_t reference_target;
    uint8_t reference_count;
    uint8_t reference_done;
    uint8_t read_d2;
    uint8_t cycle_active;
    int64_t pressure_sum;
    int32_t reference_x100;
    uint32_t d1;
    uint32_t d2;

    /* [主流程] 一条命令和最多 3 字节接收数据，当前事务结束前不复用。 */
    uint8_t tx_command;
    uint8_t rx_buffer[3];
    /* [中断通知] 只有这些字段由中断回调写入。 */
    volatile uint8_t tx_done;
    volatile uint8_t rx_done;
    volatile uint8_t error_pending;
    volatile uint32_t i2c_error;

    /* [主流程/调试记录] 计算结果由主循环生成和读取。 */
    uint8_t sample_ready;
    MS5837_Sample sample;
    MS5837_Error error;
} MS5837_Context;

static MS5837_Context ms5837;

/* [主流程] 每次进入状态，重新记录该步骤开始时间。 */
static void MS5837_Enter(MS5837_State state, uint32_t now)
{
    ms5837.state = state;
    ms5837.state_since = now;
}

/* [失败处理] 冻结状态和错误现场；不修改 DMA 命令/接收缓冲区。 */
static void MS5837_Fail(MS5837_Fault cause, HAL_StatusTypeDef status, uint32_t error)
{
    ms5837.error.state = ms5837.state;
    ms5837.error.cause = cause;
    ms5837.error.status = status;
    ms5837.error.hal_error = error;
    ms5837.sample_ready = 0U;
    ms5837.state = MS5837_ERROR;
}

/* [失败处理] 仅这些状态需要检查通信/总线等待超时，定时等待另行判断。 */
static uint8_t MS5837_IsTransferState(MS5837_State state)
{
    switch (state)
    {
        case MS5837_RESET_SEND:
        case MS5837_RESET_WAIT_TX:
        case MS5837_PROM_SEND:
        case MS5837_PROM_WAIT_TX:
        case MS5837_PROM_RECEIVE:
        case MS5837_PROM_WAIT_RX:
        case MS5837_CONV_SEND:
        case MS5837_CONV_WAIT_TX:
        case MS5837_ADC_SEND:
        case MS5837_ADC_WAIT_TX:
        case MS5837_ADC_RECEIVE:
        case MS5837_ADC_WAIT_RX:
            return 1U;
        default:
            return 0U;
    }
}

/* [失败处理] 将故障检查与正常采集 switch 分开，便于先阅读主流程。 */
static uint8_t MS5837_CheckFailure(uint32_t now)
{
    if (ms5837.error_pending != 0U)
    {
        MS5837_Fail(MS5837_FAULT_I2C, HAL_ERROR, ms5837.i2c_error);
        return 1U;
    }
    if ((MS5837_IsTransferState(ms5837.state) != 0U &&
         (uint32_t)(now - ms5837.state_since) >= MS5837_TRANSFER_TIMEOUT_MS) ||
        (ms5837.cycle_active != 0U &&
         (uint32_t)(now - ms5837.cycle_since) >= MS5837_CYCLE_TIMEOUT_MS))
    {
        MS5837_Fail(MS5837_FAULT_TIMEOUT, HAL_TIMEOUT,
            HAL_I2C_GetError(ms5837.i2c) | HAL_I2C_ERROR_TIMEOUT);
        return 1U;
    }
    return 0U;
}

/* [忙状态] HAL READY 和硬件 BUSY 清零必须同时满足，避免 HAL 入口忙等。 */
static uint8_t MS5837_BusReady(void)
{
    return (HAL_I2C_GetState(ms5837.i2c) == HAL_I2C_STATE_READY &&
            __HAL_I2C_GET_FLAG(ms5837.i2c, I2C_FLAG_BUSY) == RESET) ? 1U : 0U;
}

/* [主流程] 在持久缓冲区准备一条命令，启动后等待发送完成标志。 */
static void MS5837_SendCommand(uint8_t command, MS5837_State next, uint32_t now)
{
    /* [忙状态] STOP 尚未结束或句柄仍忙，下一轮再检查，不覆盖缓冲区。 */
    if (MS5837_BusReady() == 0U)
    {
        return;
    }
    ms5837.tx_command = command;
    ms5837.tx_done = 0U;
    HAL_StatusTypeDef status = HAL_I2C_Master_Transmit_DMA(
        ms5837.i2c, MS5837_ADDR, &ms5837.tx_command, 1U);
    if (status == HAL_OK)
    {
        MS5837_Enter(next, now);
    }
    else if (status != HAL_BUSY)
    {
        /* [失败处理] 启动失败；HAL_BUSY 保留原状态，等待下一轮重试。 */
        MS5837_Fail(MS5837_FAULT_I2C, status, HAL_I2C_GetError(ms5837.i2c));
    }
}

/* [主流程] PROM 接收 2 字节，ADC 接收 3 字节，使用同一个持久缓冲区。 */
static void MS5837_Receive(uint16_t length, MS5837_State next, uint32_t now)
{
    /* [忙状态] 等待上一条命令的 STOP 完成后，才启动新的接收事务。 */
    if (MS5837_BusReady() == 0U)
    {
        return;
    }
    ms5837.rx_done = 0U;
    HAL_StatusTypeDef status = HAL_I2C_Master_Receive_DMA(
        ms5837.i2c, MS5837_ADDR, ms5837.rx_buffer, length);
    if (status == HAL_OK)
    {
        MS5837_Enter(next, now);
    }
    else if (status != HAL_BUSY)
    {
        /* [失败处理] 接收启动失败；总线忙时留在原状态等待重试。 */
        MS5837_Fail(MS5837_FAULT_I2C, status, HAL_I2C_GetError(ms5837.i2c));
    }
}

/* [主流程] D1/D2 都使用同一组状态，开始新一轮时先选择 D1。 */
static void MS5837_BeginMeasurement(uint32_t now)
{
    ms5837.read_d2 = 0U;
    ms5837.d1 = 0U;
    ms5837.d2 = 0U;
    ms5837.cycle_active = 1U;
    ms5837.cycle_since = now;
    MS5837_Enter(MS5837_CONV_SEND, now);
}

/* [主流程] D1/D2 齐全后计算；基准阶段累加，正常阶段发布完整样本。 */
static void MS5837_FinishMeasurement(uint32_t now)
{
    int32_t temperature_x100 = 0;
    int32_t pressure_x100 = 0;
    MS5837_Calculate(ms5837.d1, ms5837.d2, ms5837.prom,
        &temperature_x100, &pressure_x100);
    ms5837.cycle_active = 0U;
    if (ms5837.reference_done == 0U)
    {
        ms5837.pressure_sum += pressure_x100;
        ++ms5837.reference_count;
        MS5837_Enter(MS5837_REFERENCE_GAP, now);
    }
    else
    {
        ms5837.sample.d1 = ms5837.d1;
        ms5837.sample.d2 = ms5837.d2;
        ms5837.sample.temperature_x100 = temperature_x100;
        ms5837.sample.pressure_x100 = pressure_x100;
        ms5837.sample.reference_x100 = ms5837.reference_x100;
        ms5837.sample.depth_cm_x100 = MS5837_CalculateDepth(
            pressure_x100, ms5837.reference_x100);
        ms5837.sample_ready = 1U;
        MS5837_Enter(MS5837_IDLE, now);
    }
}

/* [主流程] 只接受启动请求，不在此处等待复位、PROM 或基准完成。 */
HAL_StatusTypeDef MS5837_StartDMA(
    I2C_HandleTypeDef *i2c, uint8_t reference_samples, uint32_t interval_ms)
{
    /* [参数检查] DMA 句柄必须已绑定，采样次数和周期必须大于零。 */
    if (i2c == NULL || i2c->hdmatx == NULL || i2c->hdmarx == NULL ||
        reference_samples == 0U || interval_ms == 0U)
    {
        return HAL_ERROR;
    }
    /* [忙状态] 不覆盖已经启动或故障后可能仍被 DMA 占用的上下文。 */
    if (ms5837.state != MS5837_STOPPED)
    {
        return HAL_BUSY;
    }
    memset(&ms5837, 0, sizeof(ms5837));
    ms5837.i2c = i2c;
    ms5837.reference_target = reference_samples;
    ms5837.interval_ms = interval_ms;
    MS5837_Enter(MS5837_RESET_SEND, HAL_GetTick());
    return HAL_OK;
}

/* [主流程] 复位 -> PROM/CRC -> 等待稳定 -> 基准采样 -> 周期采样。 */
void MS5837_ProcessDMA(void)
{
    if (ms5837.state == MS5837_STOPPED || ms5837.state == MS5837_ERROR)
    {
        return;
    }
    uint32_t now = HAL_GetTick();
    /* [失败处理] 检查后立即返回；下面 switch 只描述正常状态推进。 */
    if (MS5837_CheckFailure(now) != 0U)
    {
        return;
    }

    switch (ms5837.state)
    {
        case MS5837_RESET_SEND:
            /* [主流程] 发送复位命令 0x1E。 */
            MS5837_SendCommand(0x1EU, MS5837_RESET_WAIT_TX, now);
            break;
        case MS5837_RESET_WAIT_TX:
            /* [主流程] 命令完成后，才开始复位等待计时。 */
            if (ms5837.tx_done != 0U)
            {
                ms5837.tx_done = 0U;
                MS5837_Enter(MS5837_RESET_WAIT, now);
            }
            break;
        case MS5837_RESET_WAIT:
            /* [主流程] 20 ms 内每次立即返回，等待传感器复位完成。 */
            if ((uint32_t)(now - ms5837.state_since) >= MS5837_RESET_WAIT_MS)
            {
                MS5837_Enter(MS5837_PROM_SEND, now);
            }
            break;
        case MS5837_PROM_SEND:
            /* [主流程] 逐个发送 C0..C6 的 PROM 读取命令。 */
            MS5837_SendCommand((uint8_t)(0xA0U + ms5837.prom_index * 2U),
                MS5837_PROM_WAIT_TX, now);
            break;
        case MS5837_PROM_WAIT_TX:
            if (ms5837.tx_done != 0U)
            {
                ms5837.tx_done = 0U;
                MS5837_Enter(MS5837_PROM_RECEIVE, now);
            }
            break;
        case MS5837_PROM_RECEIVE:
            /* [主流程] 每个系数为 16 位，通过 DMA 接收 2 字节。 */
            MS5837_Receive(2U, MS5837_PROM_WAIT_RX, now);
            break;
        case MS5837_PROM_WAIT_RX:
            if (ms5837.rx_done != 0U)
            {
                ms5837.rx_done = 0U;
                ms5837.prom[ms5837.prom_index] =
                    ((uint16_t)ms5837.rx_buffer[0] << 8) | ms5837.rx_buffer[1];
                ++ms5837.prom_index;
                MS5837_Enter((ms5837.prom_index < 7U) ?
                    MS5837_PROM_SEND : MS5837_PROM_CHECK_CRC, now);
            }
            break;
        case MS5837_PROM_CHECK_CRC:
            /* [主流程] 使用原 CRC 算法核对标定系数。 */
            if ((uint8_t)(ms5837.prom[0] >> 12) == MS5837_CalculateCRC(ms5837.prom))
            {
                MS5837_Enter(MS5837_STARTUP_WAIT, now);
            }
            else
            {
                /* [失败处理] CRC 不匹配，禁止使用错误标定系数计算。 */
                MS5837_Fail(MS5837_FAULT_CRC, HAL_ERROR, HAL_I2C_ERROR_NONE);
            }
            break;
        case MS5837_STARTUP_WAIT:
            /* [主流程] 沿用原来的 1 秒启动稳定时间，CPU 可以处理其他任务。
             * 启动及基准采集期间，传感器应保持在空气/水面参考位置。 */
            if ((uint32_t)(now - ms5837.state_since) >= MS5837_STARTUP_WAIT_MS)
            {
                MS5837_BeginMeasurement(now);
            }
            break;
        case MS5837_CONV_SEND:
            /* [主流程] 先 D1 后 D2，分别发送 0x48 和 0x58。 */
            MS5837_SendCommand((ms5837.read_d2 == 0U) ? MS5837_D1_CMD : MS5837_D2_CMD,
                MS5837_CONV_WAIT_TX, now);
            break;
        case MS5837_CONV_WAIT_TX:
            if (ms5837.tx_done != 0U)
            {
                ms5837.tx_done = 0U;
                MS5837_Enter(MS5837_CONV_WAIT, now);
            }
            break;
        case MS5837_CONV_WAIT:
            /* [主流程] 用时间差判断转换结束，代替阻塞延时。 */
            if ((uint32_t)(now - ms5837.state_since) >= MS5837_CONVERSION_WAIT_MS)
            {
                MS5837_Enter(MS5837_ADC_SEND, now);
            }
            break;
        case MS5837_ADC_SEND:
            /* [主流程] 转换完成后发送 ADC 读取命令 0x00。 */
            MS5837_SendCommand(0x00U, MS5837_ADC_WAIT_TX, now);
            break;
        case MS5837_ADC_WAIT_TX:
            if (ms5837.tx_done != 0U)
            {
                ms5837.tx_done = 0U;
                MS5837_Enter(MS5837_ADC_RECEIVE, now);
            }
            break;
        case MS5837_ADC_RECEIVE:
            /* [主流程] ADC 是 24 位，DMA 接收 3 字节。 */
            MS5837_Receive(3U, MS5837_ADC_WAIT_RX, now);
            break;
        case MS5837_ADC_WAIT_RX:
            if (ms5837.rx_done != 0U)
            {
                ms5837.rx_done = 0U;
                uint32_t adc = ((uint32_t)ms5837.rx_buffer[0] << 16) |
                               ((uint32_t)ms5837.rx_buffer[1] << 8) |
                               (uint32_t)ms5837.rx_buffer[2];
                /* [失败处理] 保留原驱动的全零 ADC 异常检查。 */
                if (adc == 0U)
                {
                    MS5837_Fail(MS5837_FAULT_ADC_ZERO, HAL_ERROR, HAL_I2C_ERROR_NONE);
                    break;
                }
                /* [主流程] 保存 D1 后再读 D2；两者齐全后完成本轮计算。 */
                if (ms5837.read_d2 == 0U)
                {
                    ms5837.d1 = adc;
                    ms5837.read_d2 = 1U;
                    MS5837_Enter(MS5837_CONV_SEND, now);
                }
                else
                {
                    ms5837.d2 = adc;
                    MS5837_FinishMeasurement(now);
                }
            }
            break;
        case MS5837_REFERENCE_GAP:
            /* [主流程] 基准样本间隔 20 ms，累计足够次数后取平均。 */
            if ((uint32_t)(now - ms5837.state_since) >= MS5837_REFERENCE_GAP_MS)
            {
                if (ms5837.reference_count < ms5837.reference_target)
                {
                    MS5837_BeginMeasurement(now);
                }
                else
                {
                    ms5837.reference_x100 =
                        (int32_t)(ms5837.pressure_sum / ms5837.reference_target);
                    ms5837.reference_done = 1U;
                    ms5837.last_sample_start = now - ms5837.interval_ms;
                    MS5837_Enter(MS5837_IDLE, now);
                }
            }
            break;
        case MS5837_IDLE:
            /* [主流程] 结果被取走且周期到达后开始新测量，第一轮立即开始。 */
            if (ms5837.sample_ready == 0U &&
                (uint32_t)(now - ms5837.last_sample_start) >= ms5837.interval_ms)
            {
                ms5837.last_sample_start = now;
                MS5837_BeginMeasurement(now);
            }
            break;
        default:
            /* [失败处理] 非预期状态，停止新传输并保留现场。 */
            MS5837_Fail(MS5837_FAULT_STATE, HAL_ERROR, HAL_I2C_GetError(ms5837.i2c));
            break;
    }
}

/* [主流程] 在主循环取走完成结果；计算结果不由中断修改。 */
uint8_t MS5837_TakeSample(MS5837_Sample *sample)
{
    if (sample == NULL || ms5837.sample_ready == 0U)
    {
        return 0U;
    }
    *sample = ms5837.sample;
    ms5837.sample_ready = 0U;
    return 1U;
}

/* [调试记录] 只读接口，便于在主循环观察启动进度和故障。 */
uint8_t MS5837_IsReady(void)
{
    return (ms5837.reference_done != 0U && ms5837.state != MS5837_ERROR) ? 1U : 0U;
}
MS5837_State MS5837_GetState(void)
{
    return ms5837.state;
}
uint8_t MS5837_GetError(MS5837_Error *error)
{
    if (error == NULL || ms5837.state != MS5837_ERROR)
    {
        return 0U;
    }
    *error = ms5837.error;
    return 1U;
}

/* [主流程：中断通知] 接收正确 IIC 句柄的完成事件，不推进状态。 */
void MS5837_OnTxComplete(I2C_HandleTypeDef *i2c)
{
    if (i2c != NULL && i2c == ms5837.i2c)
    {
        ms5837.tx_done = 1U;
    }
}
void MS5837_OnRxComplete(I2C_HandleTypeDef *i2c)
{
    if (i2c != NULL && i2c == ms5837.i2c)
    {
        ms5837.rx_done = 1U;
    }
}

/* [失败处理：中断通知] 先写错误码再写标志，由下一次 Process 处理。 */
void MS5837_OnI2CError(I2C_HandleTypeDef *i2c)
{
    if (i2c != NULL && i2c == ms5837.i2c)
    {
        ms5837.i2c_error = HAL_I2C_GetError(i2c);
        ms5837.error_pending = 1U;
    }
}


/* [纯计算] 以下函数保持原有算法和计算公式。 */
uint8_t MS5837_CalculateCRC(const uint16_t prom[7])
{
    uint16_t data[8] = {0};
    uint16_t remainder = 0;

    /* 拷贝 7 个系数到 data[0..6]，data[7] 保持 0 */
    for (uint8_t i = 0; i < 7; i++)
    {
        data[i] = prom[i];
    }

    /* 第一个字的高 4 位存放的是 CRC 本身，参与计算前清零 */
    data[0] &= 0x0FFFU;

    /* 按字节顺序（先高字节后低字节）处理 16 个字节 */
    for (uint8_t i = 0; i < 16; i++)
    {
        if (i % 2 == 0)
        {
            /* 偶数字节取高字节 */
            remainder ^= data[i / 2] >> 8;
        }
        else
        {
            /* 奇数字节取低字节 */
            remainder ^= data[i / 2] & 0x00FFU;
        }

        /* 每个字节处理 8 位 */
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

    /* 取结果高 4 位作为 CRC-4 */
    return (uint8_t)((remainder >> 12) & 0x0FU);
}

/* [纯计算] 以下函数保持原有算法和计算公式。 */
void MS5837_Calculate(
    uint32_t d1,
    uint32_t d2,
    const uint16_t prom[7],
    int32_t *temperature_x100,
    int32_t *pressure_x100
)
{
    /* dT：与参考温度的差值（D2 - C5*2^8） */
    int64_t dT =
        (int64_t)d2 - (int64_t)prom[5] * 256;

    /* 一阶温度：2000 + dT*C6/2^23，单位 0.01℃ */
    int64_t temp =
        2000 + dT * prom[6] / 8388608;

    /* 一阶压力偏置：C2*2^17 + C4*dT/2^6 */
    int64_t off =
        (int64_t)prom[2] * 131072
        + (int64_t)prom[4] * dT / 64;

    /* 一阶压力灵敏度：C1*2^16 + C3*dT/2^7 */
    int64_t sens =
        (int64_t)prom[1] * 65536
        + (int64_t)prom[3] * dT / 128;

    /* 二阶补偿项（仅低温时非零） */
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

    /* 叠加二阶补偿 */
    off = off - off_i;
    sens = sens - sens_i;

    /* 最终压力：P = (D1*SENS/2^21 - OFF)/2^15，单位 Pa */
    int64_t pressure =
        ((int64_t)d1 * sens / 2097152 - off) / 32768;

    *temperature_x100 = (int32_t)(temp - temp_i);
    *pressure_x100 = (int32_t)pressure;
}

/* [纯计算] 以下函数保持原有算法和计算公式。 */
int32_t MS5837_CalculateDepth(
    int32_t pressure_x100,
    int32_t reference_x100
)
{
    /* 压力差（Pa），正值表示位于水面以下 */
    int64_t delta_pa =
        (int64_t)pressure_x100 - (int64_t)reference_x100;

    /* Pa -> cm×100 */
    return (int32_t)(delta_pa * 1000000LL / 980665LL);
}