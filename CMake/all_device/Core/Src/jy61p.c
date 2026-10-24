#include "jy61p.h"
#include "i2c.h"
#include <stddef.h>

/* 阅读路线：
 * 正常：StartReadData 申请 -> Process 推进两段 DMA -> GetReadResult 取走一次。
 * 中断：OnRxComplete/OnI2CError 只存事件，下一轮 Process 再决定走哪条路。
 * 故障：RecordFailure 保留原始错误 -> RecoverI2C 停 DMA/清除总线/重建外设。
 * 正常读取维持原流程；总线清除只在下面独立的故障恢复部分执行。
 */

/* 地址、寄存器、读取长度和换算系数全部沿用原程序。 */
#define JY61P_ADDRESS          (0x50U << 1)
#define JY61P_MOTION_REGISTER  0x34U
#define JY61P_ANGLE_REGISTER   0x3DU
#define JY61P_READ_TIMEOUT_MS  100U
#define JY61P_RECOVERY_RETRY_MS 1000U
#define JY61P_SCL_TIMEOUT_MS    2U
/* 与 i2c.c 的 I2C2 引脚配置一致：PF0=SDA，PF1=SCL，使用外部上拉。 */
#define JY61P_I2C_GPIO         GPIOF
#define JY61P_SDA_PIN          GPIO_PIN_0
#define JY61P_SCL_PIN          GPIO_PIN_1

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
static uint8_t recovery_attempted;
static uint32_t recovery_tick;
static JY61P_RecoveryInfo recovery_info;

/* volatile 提醒编译器：这些变量可能随时被中断回调更新。 */
static volatile uint8_t rx_complete;
static volatile uint8_t rx_error;
static volatile uint32_t rx_error_code;

/* 故障处理函数集中实现在文件末尾，正常流程只调用它们的入口。 */
static void JY61P_RecordFailure(HAL_StatusTypeDef status, uint32_t error);
static HAL_StatusTypeDef JY61P_RecoverI2C(void);
static HAL_StatusTypeDef JY61P_ClearBus(void);
static HAL_StatusTypeDef JY61P_WaitSclHigh(void);

/* ======================== 正常工作代码 ======================== */

/* 与原程序一致：低字节在前，合成为有符号 16 位原始测量值。 */
static int16_t JY61P_DecodeInt16(const uint8_t *bytes)
{
    uint16_t value = (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8);
    return (int16_t)value;
}

HAL_StatusTypeDef JY61P_StartReadData(void)
{
    /* [正常：只接受请求] 不在这里启动硬件 DMA，也不等待设备响应。
     * READY/FAILED 结果必须由应用取走后才回到 IDLE，因此不能覆盖旧事务。
     */
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
    /* [主状态机] 每次调用至多处理当前状态的一步。
     * 正常读取分支在前，RECOVER 故障分支单独列出；结果由 getter 消费。
     */
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

        case JY61P_WAIT_MOTION: /* 等第一段：加速度+角速度，共12字节。 */
        case JY61P_WAIT_ANGLES: /* 等第二段：三个姿态角，共6字节。 */
            /* [两种状态共用代码] 两个 case 之间没有 break，也没有独立处理语句，
             * 无论当前在等哪一段，都会进入下面这组 if / else if 检查。
             * state 仍保留各自的等待状态，所以正常完成后的下一状态可以不同。
             * rx_error / rx_complete 由中断回调置位，这里在主循环里读取和处理。
             */
            if (rx_error != 0U)
            {
                /* [故障A：接收报错] 放在最前面：错误与完成同时出现也按失败处理。
                 * RecordFailure 保存错误并把 state 切到 RECOVER；这里不输出数据。
                 * 实际停止DMA/清除总线等操作，由后续 Process 的 RECOVER 分支执行。
                 */
                JY61P_RecordFailure(HAL_ERROR, rx_error_code);
            }
            else if (rx_complete != 0U)
            {
                /* [正常主线：本段接收完成] 没有错误且收到完成通知，才进入这里。 */
                rx_complete = 0U; /* 消费本次完成标志，避免下一轮误用同一个通知。 */
                /* 三目表达式的意思：条件为真取 ? 后面的值，为假取 : 后面的值。
                 * 第一段 WAIT_MOTION 完成 -> START_ANGLES，准备读取第二段。
                 * 第二段 WAIT_ANGLES 完成 -> DATA_READY，两段数据可以一起取出。
                 * 这里只换状态；下一次 Process 才启动第二段，完整结果由 GetReadResult 取走。
                 */
                state = (state == JY61P_WAIT_MOTION)
                      ? JY61P_START_ANGLES : JY61P_DATA_READY;
            }
            else if ((uint32_t)(HAL_GetTick() - transfer_start_tick)
                     >= JY61P_READ_TIMEOUT_MS)
            {
                /* [故障B：等待超时] 没有错误通知，也没有完成通知，但已等至少100 ms。
                 * transfer_start_tick 在启动当前这段DMA时记录；这里计算已等待多久。
                 * 无符号减法可正确处理 HAL_GetTick 的32位回绕，避免永久卡在等待状态。
                 * | 是按位或：保留已有I2C错误位，同时加上 TIMEOUT 错误位。
                 * 同样只记录失败并切到 RECOVER，实际恢复在后续调用中执行。
                 */
                JY61P_RecordFailure(HAL_TIMEOUT,
                    HAL_I2C_GetError(&hi2c2) | HAL_I2C_ERROR_TIMEOUT);
            }
            /* [正常等待：以上条件都未满足] 保持原 WAIT 状态，结束本轮 switch；
             * 返回主循环，下次 Process 再检查。这里没有 while 循环卡住等待。
             * 上面任一分支执行后也从这里离开 switch，不会落入下一 case。
             */
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
            /* [故障] 首次立即尝试；仍失败则每秒再尝试，避免反复脉冲占用主循环。
             * 总线清除、DMA/I2C 重建都成功后才开放失败结果；下一次读取仍须
             * 独立成功才能产生新数据。SysTick、深度和串口在重试间隔继续运行。
             */
            if (recovery_attempted == 0U
                || (uint32_t)(HAL_GetTick() - recovery_tick) >= JY61P_RECOVERY_RETRY_MS)
            {
                recovery_attempted = 1U;
                recovery_tick = HAL_GetTick();
                ++recovery_info.attempts;
                recovery_info.lines_valid = 0U;
                recovery_info.pulses = 0U;
                recovery_info.status = JY61P_RecoverI2C();
                if (recovery_info.status == HAL_OK)
                {
                    /* 保留原始 read_result/saved_error，不能把恢复成功伪装成读取成功。 */
                    state = JY61P_FAILED;
                }
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
    /* [一次性结果接口] 返回值说明是否“完成”，status 说明是否“成功”。
     * 返回 1 后将状态置 IDLE；下次调用不会重复消费同一份 DMA 数据。
     * 失败也必须消费结果，但不覆盖 data，避免失败被伪装成全零新样本。
     */
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
void JY61P_OnRxComplete(I2C_HandleTypeDef *hi2c)
{
    if (hi2c == &hi2c2)
    {
        rx_complete = 1U;
    }
}

/* ======================== 故障处理代码 ========================
 * 这部分只在启动失败、传输错误或超时后运行。
 * 恢复不会生成新的业务数据；应用保存故障快照并每秒输出JY61P ERROR。
 */

/* 故障中断入口：先保存错误码，再置标志；不在中断中复位外设。 */
void JY61P_OnI2CError(I2C_HandleTypeDef *hi2c)
{
    if (hi2c == &hi2c2)
    {
        rx_error_code = HAL_I2C_GetError(hi2c);
        rx_error = 1U;
    }
}

/* [异常只读接口] 恢复过程中也允许应用报告错误，但不开放旧DMA缓冲区。
 * 返回0时不修改输出；返回1时状态与错误码已在恢复清码前保存。 */
uint8_t JY61P_GetFailure(HAL_StatusTypeDef *status, uint32_t *hal_error)
{
    if (status == NULL || hal_error == NULL ||
        (state != JY61P_RECOVER && state != JY61P_FAILED))
    {
        return 0U;
    }
    *status = read_result;
    *hal_error = saved_error_code;
    return 1U;
}

uint8_t JY61P_GetRecoveryInfo(JY61P_RecoveryInfo *info)
{
    if (info == NULL)
    {
        return 0U;
    }
    *info = recovery_info;
    return 1U;
}

static void JY61P_RecordFailure(HAL_StatusTypeDef status, uint32_t error)
{
    /* [故障入口] 这里只切换状态、保存错误，不在失败位置直接复位硬件。
     * 这样所有错误都进入统一恢复路径，且恢复清除 HAL 错误之前已有快照。
     */
    /* 只在主循环调用；恢复时 HAL 会清错误码，所以必须先保存快照。 */
    read_result = status;
    saved_error_code = error;
    recovery_attempted = 0U;
    recovery_info.status = HAL_BUSY;
    recovery_info.lines_valid = 0U;
    recovery_info.pulses = 0U;
    state = JY61P_RECOVER;
}

static HAL_StatusTypeDef JY61P_StopDma(DMA_HandleTypeDef *hdma)
{
    /* [故障清理] 要求流真正停下之后才能 DeInit；硬件仍运行则返回 BUSY。
     * 未停完的 DMA 可能还在写静态缓冲区，所以不能提前发布/复用结果。
     */
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

/* [故障工具] 写 SET 只会释放开漏输出；需读 IDR 确认线路真的被上拉拉高。
 * 从机延长低电平或线路被拉住时，2 ms 后返回，不会无限等待。
 */
static HAL_StatusTypeDef JY61P_WaitSclHigh(void)
{
    uint32_t started = HAL_GetTick();
    while (HAL_GPIO_ReadPin(JY61P_I2C_GPIO, JY61P_SCL_PIN) == GPIO_PIN_RESET)
    {
        if ((uint32_t)(HAL_GetTick() - started) >= JY61P_SCL_TIMEOUT_MS)
        {
            return HAL_TIMEOUT;
        }
    }
    return HAL_OK;
}

/* [故障工具：I2C bus clear]
 * 调用前必须停掉 I2C 请求和两路 DMA。这里临时使用开漏 GPIO：
 * 写 RESET=主动接地；写 SET=断开下拉，由已有外部上拉拉高，绝不推挽强拉高。
 * SDA 低时最多发 9 拍 SCL；释放后生成 STOP，再检查两线高。
 * 每半拍用 1 ms 延时，远大于 I2C 最小时序；只在故障恢复中短暂执行。
 * 所有出口均释放两根线。SCL 不升高/SDA 不释放时维持故障，不能宣称恢复。
 */
static HAL_StatusTypeDef JY61P_ClearBus(void)
{
    GPIO_InitTypeDef pins = {0};
    HAL_StatusTypeDef result = HAL_TIMEOUT;
    __HAL_RCC_GPIOF_CLK_ENABLE();

    recovery_info.sda_before = (HAL_GPIO_ReadPin(JY61P_I2C_GPIO, JY61P_SDA_PIN) == GPIO_PIN_SET);
    recovery_info.scl_before = (HAL_GPIO_ReadPin(JY61P_I2C_GPIO, JY61P_SCL_PIN) == GPIO_PIN_SET);
    recovery_info.lines_valid = 1U;
    recovery_info.pulses = 0U;

    /* 先预置 ODR 为高再切换模式，避免切为输出时瞬间拉低 SDA 产生假 START。 */
    HAL_GPIO_WritePin(JY61P_I2C_GPIO, JY61P_SDA_PIN | JY61P_SCL_PIN, GPIO_PIN_SET);
    pins.Pin = JY61P_SDA_PIN | JY61P_SCL_PIN;
    pins.Mode = GPIO_MODE_OUTPUT_OD;
    pins.Pull = GPIO_NOPULL; /* 使用用户已有的外部上拉。 */
    pins.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(JY61P_I2C_GPIO, &pins);

    if (JY61P_WaitSclHigh() != HAL_OK)
    {
        goto release;
    }
    while (HAL_GPIO_ReadPin(JY61P_I2C_GPIO, JY61P_SDA_PIN) == GPIO_PIN_RESET
           && recovery_info.pulses < 9U)
    {
        HAL_GPIO_WritePin(JY61P_I2C_GPIO, JY61P_SCL_PIN, GPIO_PIN_RESET);
        HAL_Delay(1U);
        HAL_GPIO_WritePin(JY61P_I2C_GPIO, JY61P_SCL_PIN, GPIO_PIN_SET);
        if (JY61P_WaitSclHigh() != HAL_OK)
        {
            goto release;
        }
        ++recovery_info.pulses;
        HAL_Delay(1U);
    }
    if (HAL_GPIO_ReadPin(JY61P_I2C_GPIO, JY61P_SDA_PIN) == GPIO_PIN_RESET)
    {
        goto release;
    }

    /* STOP：先在 SCL 低时拉低 SDA，释放 SCL 并确认高，再释放 SDA。
     * 只有 SCL 高时 SDA 从低到高才是 STOP，不能同时翻转两根线代替。
     */
    HAL_GPIO_WritePin(JY61P_I2C_GPIO, JY61P_SCL_PIN, GPIO_PIN_RESET);
    HAL_Delay(1U); /* 给实际 SCL 下降并稳定的时间，再改变 SDA，避免意外 START。 */
    HAL_GPIO_WritePin(JY61P_I2C_GPIO, JY61P_SDA_PIN, GPIO_PIN_RESET);
    HAL_Delay(1U);
    HAL_GPIO_WritePin(JY61P_I2C_GPIO, JY61P_SCL_PIN, GPIO_PIN_SET);
    if (JY61P_WaitSclHigh() != HAL_OK)
    {
        goto release;
    }
    HAL_Delay(1U);
    HAL_GPIO_WritePin(JY61P_I2C_GPIO, JY61P_SDA_PIN, GPIO_PIN_SET);
    HAL_Delay(1U);
    result = HAL_OK;

release:
    /* 包括失败路径：MCU 退出时不能自己继续把线拉低。 */
    HAL_GPIO_WritePin(JY61P_I2C_GPIO, JY61P_SDA_PIN | JY61P_SCL_PIN, GPIO_PIN_SET);
    recovery_info.sda_after = (HAL_GPIO_ReadPin(JY61P_I2C_GPIO, JY61P_SDA_PIN) == GPIO_PIN_SET);
    recovery_info.scl_after = (HAL_GPIO_ReadPin(JY61P_I2C_GPIO, JY61P_SCL_PIN) == GPIO_PIN_SET);
    if (recovery_info.sda_after == 0U || recovery_info.scl_after == 0U)
    {
        result = HAL_TIMEOUT;
    }
    return result;
}

static HAL_StatusTypeDef JY61P_RecoverI2C(void)
{
    /* [故障恢复：本地停机 -> 外部总线清除 -> 本地重建]
     * 顺序：关本外设中断 -> 禁 I2C DMA -> 停两路 DMA -> DeInit -> RCC 复位
     *     -> GPIO 总线清除 -> 清挂起事件 -> Init -> 检查 BUSY -> 开 DMA 中断。
     * GPIO 清除失败时两线保持释放、本地 I2C 暂不重建；上层 RECOVER 每秒再试。
     */
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

    HAL_StatusTypeDef bus_status = JY61P_ClearBus();
    if (bus_status != HAL_OK)
    {
        return bus_status;
    }

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
    if (__HAL_I2C_GET_FLAG(&hi2c2, I2C_FLAG_BUSY) != RESET)
    {
        return HAL_BUSY;
    }
    HAL_NVIC_EnableIRQ(DMA1_Stream2_IRQn);
    HAL_NVIC_EnableIRQ(DMA1_Stream7_IRQn);
    return HAL_OK;
}
