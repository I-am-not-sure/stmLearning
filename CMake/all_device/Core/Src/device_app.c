/**
 * @file device_app.c
 * @brief 两个传感器独立采样，共用 USART1 每秒发送一个包含两者结果的数据包。
 *
 * [正常状态转换]
 * 深度：见 ms5837.c；本文件每轮推进驱动并取走完整样本。
 * 姿态：BOOT_WAIT --1500 ms--> START_READ --请求受理--> WAIT_READ
 *                 WAIT_READ --完整结果取走--> START_READ，持续采样。
 * 串口：IDLE --1秒请求--> START_TX --DMA启动成功--> WAIT_TX
 *                 WAIT_TX --USART TC 完成回调--> IDLE。
 *
 * [异常状态转换]
 * 姿态：读取失败 --> RETRY_WAIT --1000 ms--> START_READ；恢复见 jy61p.c。
 * 深度：驱动进入 ERROR 后冻结现场，持续报告；沿用原来的复位后重启策略。
 * 串口：启动失败/错误中断/100 ms 超时 --> RECOVER
 *                 RECOVER --确认DMA停止并恢复成功--> IDLE。
 * 正常/异常文本分别在独立函数中生成；一个传感器故障不停止另一个状态机。
 *
 * [主循环与中断分工]
 * 状态、格式化、数据换算、故障恢复均由主循环处理。
 * HAL 回调只转发事件；串口缓冲区在 DMA 完成或确认中止前禁止修改。
 *
 * [建议阅读顺序]
 * 1. DevicesApp_Init：清标志、设置初始状态，申请深度传感器启动。
 * 2. DevicesApp_Process：每轮推进深度、姿态、串口；这是应用总入口。
 * 3. App_ProcessMotion 的正常分支：申请读取 -> 等待 -> 取到完整数据。
 * 4. App_UpdateAttitude：只有读取成功才进入数学模块，先标定再解算。
 * 5. App_FormatMotionNormal/App_FormatAttitude：只打印已有结果，不采样。
 * 6. 文件后半段的故障函数与中断转发：先理解正常流程，再读这一部分。
 *
 * [三个不同的“状态”不要混淆]
 * motion_state：应用是否在等启动、读数据、等失败后的重试。
 * jy61p.c 的 state：底层两段 I2C DMA 事务进展到哪一步。
 * attitude_math.ready/count：10 次平均是否完成；它不是通信状态机。
 * 日志 JY61P ERROR state=3 指这里的 MOTION_RETRY_WAIT，不是 MCU 重启。
 */
#include "device_app.h"
#include "i2c.h"
#include "usart.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define DEPTH_REFERENCE_SAMPLES 10U
/* [正常采样配置] 0表示连续采样：完整结果取走后尽快开始下一轮。
 * D1/D2各自的20ms转换等待仍由驱动保留，不受此参数影响。
 * 若以后填正数，则限制两轮采样开始之间的最小间隔，单位ms。
 * 串口报告周期独立由REPORT_INTERVAL_MS控制，仍为1000ms。
 */
#define DEPTH_SAMPLE_INTERVAL_MS 0U
#define MOTION_STARTUP_WAIT_MS 1500U
#define MOTION_RETRY_INTERVAL_MS 1000U
#define REPORT_INTERVAL_MS 1000U
#define UART_TX_TIMEOUT_MS 100U
#define UART_RECOVERY_RETRY_MS 1000U
#define UART_PACKET_CAPACITY 768U
/* 单个水平姿态无法测出平面内安装偏角。默认传感器 +X 水平投影向右。
 * 若安装定位/已知动作测得偏角，在这里填入，俯视逆时针为正。
 */
#define SENSOR_X_HEADING_OFFSET_DEG 0.0f

typedef enum
{
    MOTION_BOOT_WAIT,   /* 正常：等待原有上电稳定时间，到期才提交首轮采样。 */
    MOTION_START_READ,  /* 正常：申请两段 I2C2 DMA 读取，不等待数据。 */
    MOTION_WAIT_READ,   /* 正常：驱动完成两段读取后才能更新完整姿态数据。 */
    MOTION_RETRY_WAIT   /* 异常：本轮失败，等待1秒重试，避免持续占用故障总线。 */
} MotionState;

typedef enum
{
    UART_IDLE,        /* 正常：缓冲区可写，等待主循环每秒置发送请求。 */
    UART_START_TX,    /* 正常：数据包已完整生成，准备交给 DMA。 */
    UART_WAIT_TX,     /* 正常：缓冲区只读，等待最后一个字节真正发送结束。 */
    UART_RECOVER     /* 异常：先停止旧 DMA，再允许缓冲区复用。 */
} UartState;

typedef enum
{
    UART_FAULT_START = 1,   /* HAL_UART_Transmit_DMA 未能启动。 */
    UART_FAULT_INTERRUPT,  /* DMA/USART 错误中断。 */
    UART_FAULT_TIMEOUT,    /* 100 ms 内没有收到真正发送完成的通知。 */
    UART_FAULT_FORMAT      /* 文本格式化失败或发送缓冲区不足。 */
} UartFaultCause;

/* [正常工作数据] 只有完整成功的采样才能覆盖最近一次数据。
 * 故障时保留数值便于调试，但 valid 清零，getter/串口不会伪装为有效新数据。
 */
static MS5837_Sample depth_data;
static JY61P_Data motion_data;
static uint32_t depth_tick;
static uint32_t motion_tick;
static uint8_t depth_valid;
static uint8_t motion_valid;
/* [新增正常数据] 原始 motion_data 与机体 attitude_data 分开保存。
 * ready 是“曾完成标定”；attitude_valid 是“这次数据可用”，不能只看 ready。
 * I2C 出错时保留数值便于调试，但 getter 会返回 0，拒绝交出旧的有效标志。
 */
static AttitudeMath attitude_math;
static AttitudeBody attitude_data;
static AttitudeMathResult attitude_result;
static uint8_t attitude_valid;
static uint32_t attitude_tick;
static MotionState motion_state;
static uint32_t motion_state_tick;

/* [异常现场] 深度沿用冻结故障策略；姿态错误码在驱动恢复清码前快照。
 * motion_has_fault 在下一轮尚未成功时继续保留，避免重试被显示成正常。
 */
static HAL_StatusTypeDef depth_start_status;
static MS5837_Error depth_error;
static uint8_t depth_has_fault;
static HAL_StatusTypeDef motion_status;
static uint32_t motion_error;
static uint8_t motion_has_fault;

/* [串口状态与持久缓冲区] 放在普通 SRAM，不能放入 F407 的 CCM RAM。
 * pending 仅保留一个请求；忙时不堆积历史数据，发送下一包时取最新快照。
 */
static UartState uart_state;
static uint8_t report_pending;
static uint32_t last_report_tick;
static uint32_t uart_start_tick;
static char uart_packet[UART_PACKET_CAPACITY];
static uint16_t uart_packet_length;
static volatile uint8_t uart_tx_complete;
static volatile uint8_t uart_tx_error;
static volatile uint32_t uart_async_error;

/* [串口异常报错] 发送故障时串口自身可能不可用，只能先保存。
 * 恢复后将 UART1 RECOVERED 诊断附在下一包，发送完成才确认报告已送出。
 */
static uint32_t uart_fault_count;
static uint32_t uart_reported_fault_count;
static uint32_t uart_packet_fault_count;
static UartFaultCause uart_last_cause;
static HAL_StatusTypeDef uart_last_status;
static uint32_t uart_last_error;
static uint32_t uart_recovery_tick;
static uint8_t uart_recovery_attempted;

static void App_ProcessDepth(void);
static void App_ProcessMotion(void);
static void App_ProcessUart(void);
static uint8_t App_FormatPacket(void);
static uint8_t App_Append(size_t *used, const char *format, ...);
static uint8_t App_FormatDepthNormal(size_t *used);
static uint8_t App_FormatMotionNormal(size_t *used);
static uint8_t App_FormatAttitude(size_t *used);
static void App_UpdateAttitude(void);
static uint8_t App_FormatDepthDiagnostic(size_t *used);
static uint8_t App_FormatMotionDiagnostic(size_t *used);
static void App_UpdateDepthFault(void);
static void App_UpdateMotionFault(void);
static void App_RecordUartFault(UartFaultCause cause, HAL_StatusTypeDef status,
                                uint32_t hal_error);
static HAL_StatusTypeDef App_RecoverUart(void);

/* ======================== 正常工作的主要代码 ======================== */

/* [正常入口：仅启动一次]
 * 调用时机：main.c 完成外设和原电机启动序列后。
 * 这里只设置应用状态并发起请求，不等 10 次平均，也不在这里读完整数据。
 */
void DevicesApp_Init(void)
{
    uint32_t now = HAL_GetTick();
    /* 此时电机验证过的完整启动序列已执行完；不在此改变电机 PWM。 */
    depth_valid = 0U;
    motion_valid = 0U;
    DevicesApp_RestartAttitudeCalibration();
    depth_has_fault = 0U;
    motion_has_fault = 0U;
    motion_status = HAL_OK;
    motion_error = HAL_I2C_ERROR_NONE;
    motion_state = MOTION_BOOT_WAIT;
    motion_state_tick = now;
    uart_state = UART_IDLE;
    report_pending = 0U;
    last_report_tick = now;
    uart_tx_complete = 0U;
    uart_tx_error = 0U;
    uart_async_error = HAL_UART_ERROR_NONE;
    /* 复位、读PROM、CRC、1秒稳定等待、10次水面基准，都由深度状态机完成。
     * 建立基准时保持在空气/水面参考位置；HAL_OK只代表启动请求被接受。
     */
    depth_start_status = MS5837_StartDMA(&hi2c1, DEPTH_REFERENCE_SAMPLES,
                                        DEPTH_SAMPLE_INTERVAL_MS);
}

/* [正常入口：主循环每轮调用]
 * 每个子状态机只推进当前能做的一步；等待期间返回到主循环。
 * 1500 ms、1000 ms 等等待通过时间差判断，不把主循环停在 HAL_Delay 中。
 * 注意：HAL 启动 I2C DMA 时仍有短暂轮询，所以不能称为完全无阻塞。
 */
void DevicesApp_Process(void)
{
    /* 每轮各推进一次，不能把两个采样状态机放进每秒输出的 if 里面。 */
    App_ProcessDepth();
    App_ProcessMotion();

    /* 用无符号时间差计时，跨越 HAL_GetTick 的32位回绕仍然有效。
     * 1000 ms 控制的是串口请求节奏，不是整个主循环的执行间隔。
     */
    uint32_t now = HAL_GetTick();
    if ((uint32_t)(now - last_report_tick) >= REPORT_INTERVAL_MS)
    {
        last_report_tick = now;
        report_pending = 1U;
    }
    App_ProcessUart();
}

/* [正常采样：深度] 完整样本由驱动准备好；本函数取走它并记录完成时刻。
 * 末尾的故障检查只更新有效标志，不影响后面姿态/串口继续处理。
 */
static void App_ProcessDepth(void)
{
    MS5837_ProcessDMA();
    if (MS5837_TakeSample(&depth_data) != 0U)
    {
        /* 正常：D1+D2齐全才取样；取走后，驱动在后续主循环尽快开始下一轮。
         * 不在此处或中断内直接重启DMA；转换等待和总线忙判断仍由驱动负责。
         */
        depth_tick = HAL_GetTick();
        depth_valid = 1U;
    }
    App_UpdateDepthFault(); /* 异常检查独立实现，正常数据不会当作故障文本。 */
}

/* [正常主线：姿态采样]
 * BOOT_WAIT -> START_READ -> WAIT_READ -> 成功后 START_READ。
 * WAIT_READ 里“结果已完成”和“结果成功”是两个判断：有结果也可能是失败。
 * 失败分支才清有效标志、记错误并进 RETRY_WAIT；数学模块在成功分支之后。
 */
static void App_ProcessMotion(void)
{
    JY61P_Process(); /* I2C2 两段DMA读取及故障恢复，由驱动状态机推进。 */
    App_UpdateMotionFault();

    switch (motion_state)
    {
        case MOTION_BOOT_WAIT:
            /* 正常转换：上电稳定1500 ms后开始采样；期间深度/串口仍工作。 */
            if ((uint32_t)(HAL_GetTick() - motion_state_tick) >= MOTION_STARTUP_WAIT_MS)
            {
                motion_state = MOTION_START_READ;
            }
            break;
        case MOTION_START_READ:
        {
            HAL_StatusTypeDef status = JY61P_StartReadData();
            if (status == HAL_OK || status == HAL_BUSY)
            {
                /* [正常请求状态] 此处 BUSY 来自 JY61P_StartReadData，表示
                 * 驱动已在处理一个请求，转等待即可。它不同于最终结果中的
                 * HAL_BUSY + 0x20：后者是底层 HAL 等总线空闲超时的故障。
                 */
                motion_state = MOTION_WAIT_READ;
            }
            else
            {
                /* 异常转换：申请失败，保存现场后延后重试，其他任务继续运行。 */
                motion_valid = 0U;
                attitude_valid = 0U;
                AttitudeMath_DiscardPending(&attitude_math);
                motion_has_fault = 1U;
                motion_status = status;
                motion_error = JY61P_GetError();
                motion_state_tick = HAL_GetTick();
                motion_state = MOTION_RETRY_WAIT;
            }
            break;
        }
        case MOTION_WAIT_READ:
            if (JY61P_GetReadResult(&motion_data, &motion_status) != 0U)
            {
                motion_error = JY61P_GetError();
                if (motion_status == HAL_OK)
                {
                    /* 正常转换：取出完整ACC/GYRO/ANGLE，再立即申请下一轮。
                     * 串口是否正在发送不会影响这里更新最新数据。
                     */
                    motion_tick = HAL_GetTick();
                    motion_valid = 1U;
                    motion_has_fault = 0U;
                    /* 新增解算的位置：两个 DMA 读取都成功、数据取出以后。
                     * 读不到数据时不会进入这里，公式本身不操作 I2C 引脚。
                     */
                    App_UpdateAttitude();
                    motion_state = MOTION_START_READ;
                }
                else
                {
                    /* 异常转换：驱动已确认旧DMA停止，取走失败结果后等1秒重试。 */
                    motion_valid = 0U;
                    attitude_valid = 0U;
                    AttitudeMath_DiscardPending(&attitude_math);
                    motion_has_fault = 1U;
                    motion_state_tick = HAL_GetTick();
                    motion_state = MOTION_RETRY_WAIT;
                }
            }
            break;
        case MOTION_RETRY_WAIT:
            /* 异常重试计时：不是HAL_Delay，故障等待期间另一传感器仍可采样。 */
            if ((uint32_t)(HAL_GetTick() - motion_state_tick) >= MOTION_RETRY_INTERVAL_MS)
            {
                motion_state = MOTION_START_READ;
            }
            break;
        default:
            break;
    }
}

/* [正常主线：串口发送]
 * IDLE 取最新数据组包 -> START_TX 发起 DMA -> WAIT_TX 等真正发送完 -> IDLE。
 * START_TX/WAIT_TX 失败才走 RECOVER。正在发送的缓冲区不能重新格式化。
 */
static void App_ProcessUart(void)
{
    switch (uart_state)
    {
        case UART_IDLE:
            if (report_pending != 0U)
            {
                report_pending = 0U;
                /* 缓冲区此时已不被DMA占用；同一主循环中格式化两个最新结果。
                 * 采样数据仅在主循环修改，因此两行不会被中断改成半新半旧。
                 */
                 //组装
                if (App_FormatPacket() != 0U)
                {
                    uart_state = UART_START_TX;
                }
                else
                {
                    /* 异常：不发送截断的数据；保存错误，下一周期再报告。 */
                    App_RecordUartFault(UART_FAULT_FORMAT, HAL_ERROR, HAL_UART_ERROR_NONE);
                }
            }
            break;
        case UART_START_TX:
        {
            uart_tx_complete = 0U;
            uart_tx_error = 0U;
            uart_async_error = HAL_UART_ERROR_NONE;
            uart_start_tick = HAL_GetTick();
            /* 先进入等待再启动DMA，极快完成的中断也只置标志，不会丢失。 */
            uart_state = UART_WAIT_TX;
            //发送
            HAL_StatusTypeDef status = HAL_UART_Transmit_DMA(&huart1,
                                      (uint8_t *)uart_packet, uart_packet_length);
            if (status != HAL_OK)
            {
                App_RecordUartFault(UART_FAULT_START, status, HAL_UART_GetError(&huart1));
                uart_state = UART_RECOVER;
            }
            break;
        }
        case UART_WAIT_TX:
            /* 错误先于完成处理，避免把异常的数据包当作成功发送。 */
            if (uart_tx_error != 0U)
            {
                App_RecordUartFault(UART_FAULT_INTERRUPT, HAL_ERROR, uart_async_error);
                uart_state = UART_RECOVER;
            }
            else if (uart_tx_complete != 0U)
            {
                /* 普通DMA模式下是USART TC回调，最后一个字节已发完才复用内存。 */
                uart_reported_fault_count = uart_packet_fault_count;
                uart_state = UART_IDLE;
            }
            else if ((uint32_t)(HAL_GetTick() - uart_start_tick) >= UART_TX_TIMEOUT_MS)
            {
                App_RecordUartFault(UART_FAULT_TIMEOUT, HAL_TIMEOUT,
                                    HAL_UART_GetError(&huart1));
                uart_state = UART_RECOVER;
            }
            break;
        case UART_RECOVER:
            /* 异常恢复可以重试，硬件未停时维持旧缓冲区，不影响传感器处理。 */
            if (uart_recovery_attempted == 0U ||
                (uint32_t)(HAL_GetTick() - uart_recovery_tick) >= UART_RECOVERY_RETRY_MS)
            {
                uart_recovery_attempted = 1U;
                uart_recovery_tick = HAL_GetTick();
                if (App_RecoverUart() == HAL_OK)
                {
                    uart_state = UART_IDLE;
                }
            }
            break;
        default:
            break;
    }
}

/* [正常工作：将一段格式化文字追加到 uart_packet]
 * 这个函数只拼接文字，不读取传感器，也不启动串口发送。
 * 所有日志共用同一个缓冲区，通过used记录已经写了多少字节。
 *
 * [函数声明怎么读]
 * static：函数只供当前.c文件内部使用，不表示局部变量会一直保存。
 * uint8_t：返回8位无符号数；本函数约定1=拼接成功，0=失败。
 * size_t：用于表示大小/字节数的无符号整数类型，sizeof的结果也是这种类型。
 * size_t *used：指向调用者的计数变量。used是地址，*used是该地址里保存的数量。
 *               修改*used，就能更新App_FormatPacket中共用的计数。
 * const char *format：指向格式字符串，例如"YAW=%.3f"；不能通过format修改字符。
 *                     const修饰字符，format这个指针变量本身不是const。
 * ...：可变参数，允许在format之后继续传入不同数量的数值，像printf一样。
 *      格式字符串里的%u、%lu、%.3f等，应与后续参数的数量和类型对应。
 *
 * [调用关系]
 * App_FormatPacket创建size_t used=0，然后通过&used把计数地址传给各个格式化函数。
 * 格式化函数再把同一个used地址传到这里，确保各行依次追加，不从数组开头覆盖。
 * 当前调用者保证used指针有效、*used在缓冲区容量内；本函数没有新增参数检查。
 */
static uint8_t App_Append(size_t *used, const char *format, ...)
{
    /* [第1步：准备读取可变参数]
     * va_list是<stdarg.h>提供的参数读取类型，args用来访问...里传入的参数。
     * 比如format="X=%.3f"，后面传了一个double，vsnprintf就通过args取到它。
     */
    va_list args;
    /* format是最后一个固定形参；va_start从它之后的可变参数建立读取环境。 */
    va_start(args, format);

    /* [第2步：格式化并写入缓冲区] vsnprintf的四个参数分别是：
     * &uart_packet[*used]：写入起点。比如*used=20，就从uart_packet[20]开始写。
     * sizeof(uart_packet)-*used：剩余容量，包含字符串末尾'\0'所需的一个字节。
     * format：文字模板和占位符。
     * args：占位符对应的可变参数。
     *
     * length是容量充足时需要的字节数，不包含末尾'\0'。
     * 即使空间不足而内容被截断，length仍是完整内容需要的长度，不是实际写入数。
     * 此处不会打印到串口；文字只被写入uart_packet数组。
     */
    int length = vsnprintf(&uart_packet[*used], sizeof(uart_packet) - *used, format, args);

    /* [第3步：结束可变参数读取] 与va_start配对；所有return之前都先执行这一步。 */
    va_end(args);

    /* ==================== 异常/无有效输出：拼接失败 ====================
     * length<0：格式化出错；length==0：空输出，本实现也视为失败。
     * length>=剩余容量：完整文字连同最后的'\0'放不下，已经发生截断。
     * 必须用>=而不是>：即使文字长度刚好等于容量，也没有位置放'\0'。
     * ||是逻辑或，左侧为真就不算右侧，因此负数不会再转换成size_t去比较。
     */
    if (length <= 0 || (size_t)length >= sizeof(uart_packet) - *used)
    {
        /* 失败时不增加*used，并把0交给上层，让上层放弃发送这一整包。
         * 注意vsnprintf可能已经写入了截断文字；不是说失败就完全没改缓冲区。
         */
        return 0U;
    }

    /* ==================== 正常主线：更新拼接位置 ====================
     * 到这里说明文字完整写入，length为正且没有截断，可以安全转为size_t累加。
     * *used += length修改的是调用者的计数；不把末尾'\0'算进待发送长度。
     * 例如先写"ABC"，*used变成3；再写"DEF"就从下标3继续，得到"ABCDEF"。
     * 下一次追加会覆盖上一段末尾的'\0'，新字符串末尾仍由vsnprintf写入'\0'。
     */
    *used += (size_t)length;
    return 1U; /* 文字拼接成功，不代表传感器正常，也不代表串口已经发送完成。 */
}

static uint8_t App_FormatPacket(void)
{
    size_t used = 0U;
    uint8_t depth_ok = (depth_valid != 0U)
        ? App_FormatDepthNormal(&used) : App_FormatDepthDiagnostic(&used);
    uint8_t motion_ok = (motion_valid != 0U)
        ? App_FormatMotionNormal(&used) : App_FormatMotionDiagnostic(&used);
    if (depth_ok == 0U || motion_ok == 0U)
    {
        return 0U;
    }
    uart_packet_fault_count = uart_fault_count;
    if (uart_fault_count != uart_reported_fault_count)
    {
        /* 异常报错：只有上一次失败尚未成功报告时，追加恢复诊断。 */
        if (App_Append(&used, "UART1 RECOVERED count=%lu cause=%u status=%u err=0x%08lX\r\n",
            (unsigned long)uart_fault_count, (unsigned int)uart_last_cause,
            (unsigned int)uart_last_status, (unsigned long)uart_last_error) == 0U)
        {
            return 0U;
        }
    }
    uart_packet_length = (uint16_t)used;
    return 1U;
}

static uint8_t App_FormatDepthNormal(size_t *used)
{
    /* Px100=Pa（mbar×100），Dx100=cm×100，Tx100=摄氏度×100，单位沿用原算法。 */
    return App_Append(used, "MS5837 t=%lu Px100=%ld Dx100=%ld Tx100=%ld\r\n",
        (unsigned long)depth_tick, (long)depth_data.pressure_x100,
        (long)depth_data.depth_cm_x100, (long)depth_data.temperature_x100);
}

static uint8_t App_FormatMotionNormal(size_t *used)
{
    /* [正常显示] 先保留原始传感器一行，再追加机体一行；两套角度各有含义。
     * 本函数只有格式化功能，调用多少次都不会增加标定样本数。
     */
    if (App_Append(used,
        "JY61P t=%lu ACC(g)=%.3f,%.3f,%.3f GYRO(deg/s)=%.3f,%.3f,%.3f ANGLE(deg)=%.3f,%.3f,%.3f\r\n",
        (unsigned long)motion_tick,
        (double)motion_data.acc[0], (double)motion_data.acc[1], (double)motion_data.acc[2],
        (double)motion_data.gyro[0], (double)motion_data.gyro[1], (double)motion_data.gyro[2],
        (double)motion_data.angles.roll, (double)motion_data.angles.pitch,
        (double)motion_data.angles.yaw) == 0U)
    {
        return 0U;
    }
    //组装转换
    return App_FormatAttitude(used);
}

/* ==================== 新增姿态数学的正常接入代码 ==================== */

/* [正常：每次完整采样成功后的数学入口]
 * 调用位置：App_ProcessMotion 的 MOTION_WAIT_READ 成功分支。
 * 此时两段DMA都成功，motion_data 是本次完整ACC/GYRO/ANGLE，motion_tick是完成时刻。
 * 本函数不读I2C、不启动DMA、不等待10次，也不修改外层 motion_state。
 * 工作顺序：准备本次 sample -> 交给数学模块 -> 检查返回结果 -> 更新有效标志/时间。
 *
 * [10次标定在哪里完成]
 * AttitudeMath_Process 使用持久对象 attitude_math 保存 count、平均累计和矩阵。
 * ready=0：第一个有效样本直接计入；以后与上次计入的样本至少相隔100 ms才计入。
 *          因此调用本函数10次，不一定累计了10次；间隔太短只等待，不清已有累计。
 * count从9加到10的那一次 Process：FinishCalibration建矩阵、保存角速度平均记录，
 *          设置ready=1，再立刻Solve本帧；正常情况下这一次就返回READY并输出机体姿态。
 * ready=1：后续Process跳过累计，使用已有矩阵逐帧Solve，count保留为10。
 * 通信故障/无效输入可清掉尚未完成的累计；已有固定矩阵不会因此被丢弃。
 *
 * [不要混淆这三个量]
 * attitude_math.ready：是否已经完成标定；0=未完成，1=已完成。
 * attitude_result：本次调用结果；CALIBRATING=正常等待/累计，READY=本帧有有效输出，
 *                  INVALID=参数/数据无效，SINGULAR=机头相对基准近竖直，欧拉角不唯一。
 * attitude_valid：本次机体姿态能否使用，只在返回READY时为1。
 * 标定完成不保证每一帧都有效；即使ready=1，本帧仍可能SINGULAR或INVALID。
 * 外层motion_state在每次读取成功后都回MOTION_START_READ，继续采样，不等待count到10。
 */
static void App_UpdateAttitude(void)
{
    /* [第1步：准备本帧输入] sample是局部结构，每次调用都填入本次新数据。
     * 累计10次的计数和矩阵存在全局 attitude_math 中，不会随着这个局部变量消失。
     */
    AttitudeSample sample;

    /* [第2步：按传感器原始顺序填写三个角]
     * 驱动字段roll/pitch/yaw是传感器X/Y/Z的协议名称，不能当成机器动作名称。
     * 当前安装下，机体抬头主要体现在原始X，左右侧倾主要体现在原始Y。
     * 这里仍必须按X/Y/Z输入，安装补偿和机体角分解由数学模块完成。
     */
    sample.angles_deg[0] = motion_data.angles.roll;  /* 原始传感器X角，单位deg。 */
    sample.angles_deg[1] = motion_data.angles.pitch; /* 原始传感器Y角，单位deg。 */
    sample.angles_deg[2] = motion_data.angles.yaw;   /* 原始传感器Z角，单位deg。 */

    /* [第3步：复制本次三轴向量] memcpy一次复制整个三元素数组，sizeof给出字节数。
     * gyro单位deg/s：保留标定期间平均记录；当前直接换到机体坐标，不减均值。
     * acc单位g：检查数据有效性，标定后换到机体坐标；不参与建立安装矩阵。
     */
    memcpy(sample.gyro_deg_s, motion_data.gyro, sizeof(sample.gyro_deg_s));
    memcpy(sample.acc_g, motion_data.acc, sizeof(sample.acc_g));

    /* [第4步：让数学模块处理这一帧] 四个参数分别是：
     * &attitude_math：持久上下文，保存累计次数/平均/矩阵；标定时会更新它。
     * &sample：本帧原始输入，只读。
     * motion_tick：本帧采样完成时间，用于判断与上次计入是否间隔至少100 ms。
     * &attitude_data：机体输出，只有返回READY才写入新结果；其他结果保留旧数值。
     * 第10次计入时，建矩阵与首次解算都在这个函数调用内部完成，不用等下一轮。
     */
    attitude_result = AttitudeMath_Process(&attitude_math, &sample, motion_tick, &attitude_data);

    /* [第5步：标记本帧能否使用] 三目表达式：READY赋1，其他结果赋0。
     * CALIBRATING是正常启动过程，valid=0不代表I2C故障，只表示机体姿态还没准备好。
     * INVALID/SINGULAR也置0，不能把attitude_data中保留的旧数值当成本帧新结果。
     */
    attitude_valid = (attitude_result == ATTITUDE_MATH_READY) ? 1U : 0U;
    if (attitude_valid != 0U)
    {
        /* 仅成功得到本帧机体输出时更新时间戳；未输出时保留旧时间但valid为0。
         * 供DevicesApp_GetAttitude的调用方检查姿态数据的时间。
         */
        attitude_tick = motion_tick;
    }
}

/* [显示入口：把机体姿态追加到串口缓冲区]
 * App_FormatMotionNormal 先写入原始 JY61P 一行，再调用本函数追加 BODY 一行。
 * used指向缓冲区已写的字节数；App_Append从这个位置继续写，并更新*used。
 * 三条路径互斥：未标定 -> BODY CAL；已标定但本帧无效 -> BODY INVALID；有效 -> BODY OK。
 * 本函数只读取已有姿态/标定状态，不采样、不解算、不发送串口、不执行故障恢复。
 * 返回值沿用App_Append：1=文字拼装成功，0=格式化失败/空间不足，上层不发送这包。
 * 注意：返回1表示文字写入成功，不表示姿态有效；CAL/INVALID日志也可以拼装成功。
 * I2C通信故障通常由上层App_FormatMotionDiagnostic显示JY61P ERROR，不走本函数。
 */
static uint8_t App_FormatAttitude(size_t *used)
{
    /* ==================== 正常启动：标定进度显示 ====================
     * ready=0表示还没完成10次标定，暂时没有机体姿态，显示进度即可。
     * n=已累计样本数，分母=目标10次；restart=未完成累计被清空的次数，不是MCU重启。
     * 故障后重新累计也沿用这个显示；本分支优先于下面的本帧有效性判断。
     * return结束本函数，所以输出CAL后不会继续输出INVALID或OK。
     */
    if (attitude_math.ready == 0U)
    {
        return App_Append(used, "BODY CAL n=%u/%u restart=%lu\r\n",
            (unsigned int)attitude_math.calibration_count,
            (unsigned int)ATTITUDE_CALIBRATION_SAMPLES,
            (unsigned long)attitude_math.calibration_restarts);
    }

    /* ==================== 异常/特殊姿态：诊断输出 ====================
     * 能到这里说明ready=1，矩阵已建立；但valid=0表示本帧没有可用的机体姿态。
     * 例如数学输入无效(INVALID)，或机头相对基准近竖直、欧拉角不唯一(SINGULAR)。
     * status是本次数学处理返回值attitude_result，不是HAL I2C状态或I2C错误码。
     * 这里只输出诊断，不输出保留的旧姿态，也不重标定或复位传感器。
     */
    if (attitude_valid == 0U)
    {
        return App_Append(used, "BODY INVALID status=%u\r\n", (unsigned int)attitude_result);
    }

    /* ==================== 正常运行：输出有效机体姿态 ====================
     * 前面两条return都未执行：已经标定，且本帧解算成功，可以输出BODY OK。
     * t：这份姿态对应的采样时间戳，单位ms。
     * FRONT/SIDE/YAW：机体俯仰(抬头正)/横滚(左高正)/相对初始航向(左转正)，单位deg。
     * RATE：原始GYRO换到机体右/前/上三轴后的角速度，不减静止均值；不是欧拉角导数。
     * REF：标定时10次原始传感器X/Y/Z角的平均值，是固定基准，不是实时机体角。
     * ACC_BODY：安装补偿后的机体右/前/上三轴加速度，单位g，来自attitude_data.acc_g。
     * 保留重力对应读数：水平静止且标定正确时约(0,0,+1)g；倾斜静止时X/Y也会有分量。
     * %.3f将浮点数显示为小数点后三位；App_Append负责实际写入uart_packet。
     */
    return App_Append(used,
        "BODY OK t=%lu FRONT(deg)=%.3f SIDE(deg)=%.3f YAW(deg)=%.3f RATE(deg/s)=%.3f,%.3f,%.3f REF(deg)=%.3f,%.3f,%.3f ACC_BODY(g)=%.3f,%.3f,%.3f\r\n",
        (unsigned long)attitude_tick,
        (double)attitude_data.front_deg, (double)attitude_data.side_deg,
        (double)attitude_data.yaw_deg,
        (double)attitude_data.gyro_deg_s[0], (double)attitude_data.gyro_deg_s[1],
        (double)attitude_data.gyro_deg_s[2],
        (double)attitude_math.reference_angles_deg[0],
        (double)attitude_math.reference_angles_deg[1],
        (double)attitude_math.reference_angles_deg[2],
        (double)attitude_data.acc_g[0], (double)attitude_data.acc_g[1],
        (double)attitude_data.acc_g[2]);
}

/* [正常：主动要求重新标定]
 * 启动时调用一次；之后也可在主循环、机器重新放稳后主动调用。
 * 它清掉数学基准和机体有效标志，下一次成功采样开始新的 10 次平均。
 * 与故障 DiscardPending 不同，这里连已有基准也会清掉；不改 I2C/电机。
 */
void DevicesApp_RestartAttitudeCalibration(void)
{
    AttitudeMath_Init(&attitude_math, SENSOR_X_HEADING_OFFSET_DEG);
    attitude_result = ATTITUDE_MATH_CALIBRATING;
    attitude_valid = 0U;
    attitude_tick = 0U;
}

/* [正常：供控制代码取结果]
 * 返回 1 才复制数据；返回 0 不修改调用者的旧变量。
 * 原始采样和本次解算都必须有效。调用者还应检查时间戳，避免长期未更新。
 * getter 只读最近结果，不触发采样、不累计次数、不执行数学公式。
 */
uint8_t DevicesApp_GetAttitude(AttitudeBody *data, uint32_t *sample_tick)
{
    if (data == NULL || motion_valid == 0U || attitude_valid == 0U)
    {
        return 0U;
    }
    *data = attitude_data;
    if (sample_tick != NULL)
    {
        *sample_tick = attitude_tick;
    }
    return 1U;
}

uint8_t DevicesApp_GetDepth(MS5837_Sample *sample, uint32_t *sample_tick)
{
    if (sample == NULL || depth_valid == 0U)
    {
        return 0U;
    }
    *sample = depth_data;
    if (sample_tick != NULL)
    {
        *sample_tick = depth_tick;
    }
    return 1U;
}

uint8_t DevicesApp_GetMotion(JY61P_Data *data, uint32_t *sample_tick)
{
    if (data == NULL || motion_valid == 0U)
    {
        return 0U;
    }
    *data = motion_data;
    if (sample_tick != NULL)
    {
        *sample_tick = motion_tick;
    }
    return 1U;
}

/* ======================== 工作异常的报错/恢复代码 ======================== */

static void App_UpdateDepthFault(void)
{
    if (depth_start_status != HAL_OK || MS5837_GetError(&depth_error) != 0U)
    {
        depth_valid = 0U;
        depth_has_fault = 1U;
    }
}

/* [故障：尽早使旧数据失效]
 * 驱动正在 RECOVER、还没开放最终结果时，也能先取得错误快照。
 * 已完成的安装标定保留；未完成的 10 次平均会丢弃，等待新的有效采样。
 * 此函数不修复总线，真正的恢复步骤在 jy61p.c 中。
 */
static void App_UpdateMotionFault(void)
{
    /* 恢复尚未完成时也立即使旧数据失效，不能一直显示上一次正常姿态。 */
    if (JY61P_GetFailure(&motion_status, &motion_error) != 0U)
    {
        motion_valid = 0U;
        attitude_valid = 0U;
        AttitudeMath_DiscardPending(&attitude_math);
        motion_has_fault = 1U;
    }
}

static uint8_t App_FormatDepthDiagnostic(size_t *used)
{
    if (depth_start_status != HAL_OK)
    {
        return App_Append(used, "MS5837 ERROR start_status=%u\r\n",
                          (unsigned int)depth_start_status);
    }
    if (depth_has_fault != 0U)
    {
        return App_Append(used, "MS5837 ERROR state=%u cause=%u status=%u err=0x%08lX\r\n",
            (unsigned int)depth_error.state, (unsigned int)depth_error.cause,
            (unsigned int)depth_error.status, (unsigned long)depth_error.hal_error);
    }
    /* INIT是正常启动进度，尚未有完整数据，不能输出全零假样本。 */
    return App_Append(used, "MS5837 INIT state=%u\r\n", (unsigned int)MS5837_GetState());
}

static uint8_t App_FormatMotionDiagnostic(size_t *used)
{
    if (motion_has_fault != 0U)
    {
        return App_Append(used, "JY61P ERROR state=%u status=%u err=0x%08lX\r\n",
            (unsigned int)motion_state, (unsigned int)motion_status, (unsigned long)motion_error);
    }
    return App_Append(used, "JY61P INIT state=%u\r\n", (unsigned int)motion_state);
}

static void App_RecordUartFault(UartFaultCause cause, HAL_StatusTypeDef status,
                                uint32_t hal_error)
{
    ++uart_fault_count;
    uart_last_cause = cause;
    uart_last_status = status;
    uart_last_error = hal_error;
    uart_recovery_attempted = 0U;
}

static HAL_StatusTypeDef App_RecoverUart(void)
{
    /* 仅关闭发送相关中断，两个I2C和SysTick继续运行；恢复成功前保持关闭。
     * 当前阶段未启动RX；后续加入串口命令时需把RX错误恢复一并设计。
     */
    HAL_NVIC_DisableIRQ(USART1_IRQn);
    HAL_NVIC_DisableIRQ(DMA2_Stream7_IRQn);
    if (HAL_UART_AbortTransmit(&huart1) != HAL_OK)
    {
        return HAL_BUSY;
    }
    if (HAL_DMA_GetState(huart1.hdmatx) == HAL_DMA_STATE_BUSY &&
        HAL_DMA_Abort(huart1.hdmatx) != HAL_OK)
    {
        return HAL_BUSY;
    }
    __HAL_DMA_DISABLE(huart1.hdmatx);
    if ((huart1.hdmatx->Instance->CR & DMA_SxCR_EN) != 0U)
    {
        /* DMA硬件尚未停止：继续保留缓冲区，下次恢复重试，不强行覆盖。 */
        return HAL_BUSY;
    }
    if (HAL_DMA_GetState(huart1.hdmatx) != HAL_DMA_STATE_READY)
    {
        if (HAL_DMA_DeInit(huart1.hdmatx) != HAL_OK || HAL_DMA_Init(huart1.hdmatx) != HAL_OK)
        {
            return HAL_ERROR;
        }
    }
    HAL_NVIC_ClearPendingIRQ(USART1_IRQn);
    HAL_NVIC_ClearPendingIRQ(DMA2_Stream7_IRQn);
    uart_tx_complete = 0U;
    uart_tx_error = 0U;
    uart_async_error = HAL_UART_ERROR_NONE;
    HAL_NVIC_EnableIRQ(USART1_IRQn);
    HAL_NVIC_EnableIRQ(DMA2_Stream7_IRQn);
    return HAL_OK;
}

/* ======================== HAL中断回调：只通知，不等待 ======================== */

/* [正常完成] 深度使用Master收发，姿态使用Mem读取；两种回调不能混淆。 */
void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    MS5837_OnTxComplete(hi2c); /* 驱动内部核对句柄，只接收I2C1的事件。 */
}

void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    MS5837_OnRxComplete(hi2c);
}

void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    JY61P_OnRxComplete(hi2c); /* 驱动内部核对I2C2句柄。 */
}

/* [异常通知] 两个原工程各有同名HAL回调，合并后统一分发，避免链接重名。 */
void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
    MS5837_OnI2CError(hi2c);
    JY61P_OnI2CError(hi2c);
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart1)
    {
        uart_tx_complete = 1U;
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart1)
    {
        uart_async_error = HAL_UART_GetError(huart);
        uart_tx_error = 1U;
    }
}
