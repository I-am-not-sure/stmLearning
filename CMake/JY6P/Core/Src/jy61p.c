#include "jy61p.h"
#include "i2c.h"
#include <stddef.h>

/* 地址、寄存器、读取长度和换算系数全部沿用原程序。 */
#define JY61P_ADDRESS          (0x50U << 1)
#define JY61P_MOTION_REGISTER  0x34U
#define JY61P_ANGLE_REGISTER   0x3DU
#define JY61P_READ_TIMEOUT_MS  100U

/* ======================== 状态机工作逻辑 ========================
 * 正常：IDLE -> START_MOTION -> WAIT_MOTION -> START_ANGLES
 *            -> WAIT_ANGLES -> DATA_READY -> 取走结果 -> IDLE
 * 故障：启动失败/接收错误/100 ms 超时 -> RECOVER -> FAILED
 *                                  -> 取走失败结果 -> IDLE
 *
 * state 只由主循环修改，中断只写下方的事件标志。
 * 中断里不启动下一次 IIC、不换算浮点数、不打印、不延时。
 */
typedef enum
{
    JY61P_IDLE,           /* 空闲，允许应用申请采样 */
    JY61P_START_MOTION,   /* 主循环准备读取加速度和角速度 */
    JY61P_WAIT_MOTION,    /* DMA 正在接收 12 字节，等待完成标志 */
    JY61P_START_ANGLES,   /* 主循环准备读取姿态角 */
    JY61P_WAIT_ANGLES,    /* DMA 正在接收 6 字节，等待完成标志 */
    JY61P_DATA_READY,     /* 两段接收成功，等待应用取走完整数据 */
    JY61P_RECOVER,        /* 故障状态：停止 DMA，恢复 IIC 外设 */
    JY61P_FAILED          /* 故障已清理，等待应用取走失败状态 */
} JY61P_State;

/* DMA 是异步操作，不能继续使用函数内的临时数组。
 * 这两个静态数组在整个采样期间一直有效；默认 .bss 位于普通 SRAM，
 * 不要改放到 STM32F407 的 CCM RAM，DMA 无法访问 CCM RAM。
 */
static uint8_t motion_bytes[12];
static uint8_t angle_bytes[6];
static JY61P_State state = JY61P_IDLE;
static uint32_t transfer_start_tick;
static HAL_StatusTypeDef read_result = HAL_OK;
static uint32_t saved_error_code;

/* volatile 提醒编译器：这些变量可能随时被中断回调更新。 */
static volatile uint8_t rx_complete;
static volatile uint8_t rx_error;
static volatile uint32_t rx_error_code;

/* 故障处理函数集中实现在文件末尾，正常流程只调用它们的入口。 */
static void JY61P_RecordFailure(HAL_StatusTypeDef status, uint32_t error);
static HAL_StatusTypeDef JY61P_RecoverI2C(void);

/* ======================== 正常工作代码 ======================== */

/* 与原程序一致：低字节在前，合成为有符号 16 位原始测量值。 */
static int16_t JY61P_DecodeInt16(const uint8_t *bytes)
{
    uint16_t value = (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8);
    return (int16_t)value;
}

HAL_StatusTypeDef JY61P_StartReadData(void)
{
    if (state != JY61P_IDLE)
    {
        /* 尚未完成或尚未取走结果时，禁止覆盖 DMA 缓冲区。 */
        return HAL_BUSY;
    }

    rx_complete = 0U;
    rx_error = 0U;
    rx_error_code = HAL_I2C_ERROR_NONE;
    saved_error_code = HAL_I2C_ERROR_NONE;
    read_result = HAL_OK;
    state = JY61P_START_MOTION;
    return HAL_OK;
}

void JY61P_Process(void)
{
    HAL_StatusTypeDef status;

    switch (state)
    {
        case JY61P_START_MOTION:
            /* 第一次读取：0x34 起的 12 字节，即原有 ACC 和 GYRO 数据。
             * 先设置等待状态，再启动 DMA，接收很快完成也不会漏掉事件。
             */
            rx_complete = 0U;
            transfer_start_tick = HAL_GetTick();
            state = JY61P_WAIT_MOTION;
            status = HAL_I2C_Mem_Read_DMA(&hi2c2, JY61P_ADDRESS,
                                         JY61P_MOTION_REGISTER,
                                         I2C_MEMADD_SIZE_8BIT,
                                         motion_bytes, sizeof(motion_bytes));
            if (status != HAL_OK)
            {
                /* 故障分支：调用成功仅代表启动成功，不代表数据已到齐。 */
                JY61P_RecordFailure(status, HAL_I2C_GetError(&hi2c2));
            }
            break;

        case JY61P_WAIT_MOTION:
        case JY61P_WAIT_ANGLES:
            /* 两个等待状态共用事件检查；只有收到完成中断才前进。
             * 错误优先于完成处理，避免输出一次失败采样的部分数据。
             */
            if (rx_error != 0U)
            {
                JY61P_RecordFailure(HAL_ERROR, rx_error_code);
            }
            else if (rx_complete != 0U)
            {
                rx_complete = 0U;
                state = (state == JY61P_WAIT_MOTION)
                      ? JY61P_START_ANGLES : JY61P_DATA_READY;
            }
            else if ((uint32_t)(HAL_GetTick() - transfer_start_tick)
                     >= JY61P_READ_TIMEOUT_MS)
            {
                /* 故障分支：DMA 启动后也必须有超时，不能永远等中断。
                 * 无符号减法可正确处理 HAL_GetTick 的 32 位回绕。
                 */
                JY61P_RecordFailure(HAL_TIMEOUT,
                    HAL_I2C_GetError(&hi2c2) | HAL_I2C_ERROR_TIMEOUT);
            }
            break;

        case JY61P_START_ANGLES:
            /* 第二次读取：仍从 0x3D 单独取 6 字节姿态角。
             * 保持原来的两段读取，不将寄存器间的其他数据混入结果。
             */
            rx_complete = 0U;
            transfer_start_tick = HAL_GetTick();
            state = JY61P_WAIT_ANGLES;
            status = HAL_I2C_Mem_Read_DMA(&hi2c2, JY61P_ADDRESS,
                                         JY61P_ANGLE_REGISTER,
                                         I2C_MEMADD_SIZE_8BIT,
                                         angle_bytes, sizeof(angle_bytes));
            if (status != HAL_OK)
            {
                JY61P_RecordFailure(status, HAL_I2C_GetError(&hi2c2));
            }
            break;

        /* -------------------- 故障恢复状态 -------------------- */
        case JY61P_RECOVER:
            if (JY61P_RecoverI2C() == HAL_OK)
            {
                /* 确认 DMA 已停止后才开放结果，防止下次采样复用旧传输。 */
                state = JY61P_FAILED;
            }
            break;

        /* 空闲以及已完成状态不再操作外设，等待应用发起/取走结果。 */
        case JY61P_IDLE:
        case JY61P_DATA_READY:
        case JY61P_FAILED:
        default:
            break;
    }
}

uint8_t JY61P_GetReadResult(JY61P_Data *data, HAL_StatusTypeDef *status)
{
    if ((data == NULL) || (status == NULL))
    {
        return 0U;
    }
    if (state == JY61P_FAILED)
    {
        /* 故障结果也要取走一次；不修改应用原来的 data。 */
        *status = read_result;
        state = JY61P_IDLE;
        return 1U;
    }
    if (state != JY61P_DATA_READY)
    {
        return 0U;
    }

    /* 两段 DMA 都成功后才换算：单位和量程与原程序完全一致。 */
    const float acc_scale = 16.0f / 32768.0f;
    const float gyro_scale = 2000.0f / 32768.0f;
    const float angle_scale = 180.0f / 32768.0f;
    for (uint32_t axis = 0U; axis < 3U; ++axis)
    {
        data->acc[axis] =
            JY61P_DecodeInt16(&motion_bytes[axis * 2U]) * acc_scale;
        data->gyro[axis] =
            JY61P_DecodeInt16(&motion_bytes[6U + axis * 2U]) * gyro_scale;
    }
    data->angles.roll = JY61P_DecodeInt16(&angle_bytes[0]) * angle_scale;
    data->angles.pitch = JY61P_DecodeInt16(&angle_bytes[2]) * angle_scale;
    data->angles.yaw = JY61P_DecodeInt16(&angle_bytes[4]) * angle_scale;

    state = JY61P_IDLE;
    *status = HAL_OK;
    return 1U;
}

uint32_t JY61P_GetError(void)
{
    return saved_error_code;
}

/* 正常接收完成中断入口：只置标志，由下一次主循环决定后续动作。
 * Mem_Read_DMA 对应的是 MemRxCpltCallback，不是 MasterRxCpltCallback。
 */
void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c == &hi2c2)
    {
        rx_complete = 1U;
    }
}

/* ======================== 故障处理代码 ========================
 * 这部分只在启动失败、传输错误或超时后运行。
 * 恢复不会生成新的业务数据；应用仍输出原有 I2C error 格式。
 */

/* 故障中断入口：先保存错误码，再置标志；不在中断中复位外设。 */
void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c == &hi2c2)
    {
        rx_error_code = HAL_I2C_GetError(hi2c);
        rx_error = 1U;
    }
}

static void JY61P_RecordFailure(HAL_StatusTypeDef status, uint32_t error)
{
    /* 只在主循环调用；恢复时 HAL 会清错误码，所以必须先保存快照。 */
    read_result = status;
    saved_error_code = error;
    state = JY61P_RECOVER;
}

static HAL_StatusTypeDef JY61P_StopDma(DMA_HandleTypeDef *hdma)
{
    if (hdma == NULL)
    {
        return HAL_OK;
    }
    if (HAL_DMA_GetState(hdma) == HAL_DMA_STATE_BUSY)
    {
        /* CubeMX 的 HAL_I2C_MspDeInit 仅调用 HAL_DMA_DeInit。
         * HAL_DMA_DeInit 遇到 BUSY 会直接返回，故必须先终止活动传输。
         * 此处在主循环执行，SysTick 仍能运行，Abort 的超时有效。
         */
        if (HAL_DMA_Abort(hdma) != HAL_OK)
        {
            return HAL_BUSY;
        }
    }

    /* 处理已发生错误/正在异步终止的 DMA；确认硬件也停止再复用内存。
     * 停止尚未生效时让下一次主循环重试，不强行宣告恢复成功。
     */
    __HAL_DMA_DISABLE(hdma);
    if ((hdma->Instance->CR & DMA_SxCR_EN) != 0U)
    {
        return HAL_BUSY;
    }
    return HAL_DMA_DeInit(hdma);
}

static HAL_StatusTypeDef JY61P_RecoverI2C(void)
{
    /* 只关闭本外设相关中断，SysTick 和 USART1 继续工作。
     * 防止清理过程中残留 DMA 回调又修改事件标志。
     */
    HAL_NVIC_DisableIRQ(I2C2_EV_IRQn);
    HAL_NVIC_DisableIRQ(I2C2_ER_IRQn);
    HAL_NVIC_DisableIRQ(DMA1_Stream2_IRQn);
    HAL_NVIC_DisableIRQ(DMA1_Stream7_IRQn);
    __HAL_I2C_DISABLE_IT(&hi2c2, I2C_IT_EVT | I2C_IT_BUF | I2C_IT_ERR);
    CLEAR_BIT(hi2c2.Instance->CR2, I2C_CR2_DMAEN | I2C_CR2_LAST);
    __HAL_I2C_DISABLE(&hi2c2);

    if ((JY61P_StopDma(hi2c2.hdmarx) != HAL_OK)
        || (JY61P_StopDma(hi2c2.hdmatx) != HAL_OK))
    {
        return HAL_BUSY;
    }
    if (HAL_I2C_DeInit(&hi2c2) != HAL_OK)
    {
        return HAL_ERROR;
    }
    __HAL_RCC_I2C2_FORCE_RESET();
    __HAL_RCC_I2C2_RELEASE_RESET();

    /* 清除上次传输的挂起中断，并清事件；保存的故障信息不清除。 */
    HAL_NVIC_ClearPendingIRQ(I2C2_EV_IRQn);
    HAL_NVIC_ClearPendingIRQ(I2C2_ER_IRQn);
    HAL_NVIC_ClearPendingIRQ(DMA1_Stream2_IRQn);
    HAL_NVIC_ClearPendingIRQ(DMA1_Stream7_IRQn);
    rx_complete = 0U;
    rx_error = 0U;
    rx_error_code = HAL_I2C_ERROR_NONE;

    /* Init 沿用 hi2c2.Init 中 CubeMX 配好的参数，包括 80 kHz 和引脚。
     * MspInit 会恢复 GPIO、DMA 配置和 I2C 中断；DMA NVIC 在下方恢复。
     * 若恢复失败，继续留在 RECOVER，下次主循环重试，缓冲区不开放。
     */
    if (HAL_I2C_Init(&hi2c2) != HAL_OK)
    {
        return HAL_ERROR;
    }
    HAL_NVIC_EnableIRQ(DMA1_Stream2_IRQn);
    HAL_NVIC_EnableIRQ(DMA1_Stream7_IRQn);
    return HAL_OK;
}
