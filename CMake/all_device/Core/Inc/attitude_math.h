/*
 * 安装补偿数学模块的接口和数据结构。
 * 阅读顺序：Sample/Body 数据含义 -> Result 返回值 -> Math 保存的状态 -> 三个接口。
 * 正常流程：Init -> Process 累计 10 次有效样本 -> 建立矩阵 -> Process 每帧解算。
 * ready/calibration_count 只控制“未标定/已标定”两个数学阶段，
 * 与传感器驱动的 I2C/UART 状态机独立；本模块不处理总线收发或驱动重试。
 */
#ifndef ATTITUDE_MATH_H
#define ATTITUDE_MATH_H

#include <stdint.h>

/* 正常标定的样本数与最小取样间隔；两者不是晃动检测阈值。
 * 纯数学模块，不依赖 HAL，也不读 I2C、不打印、不修改电机。
 */
#define ATTITUDE_CALIBRATION_SAMPLES       10U
#define ATTITUDE_CALIBRATION_INTERVAL_MS   100U

/* Process 的输入：同一次完整测量中的三个角和两个三轴向量。
 * 这是传感器自身坐标系的数据，不能直接当作机器人坐标系的数据。
 */
typedef struct
{
    /* 原始顺序必须保持 {传感器X角, 传感器Y角, 传感器Z角}。
     * JY61P 协议将它们命名为 Roll/Pitch/Yaw，这是传感器欧拉角的名称。
     * 当前安装约为传感器X向右、Y向前：机体俯仰主要反映在原始X角，
     * 机体横滚主要反映在原始Y角；安装倾斜会引入耦合，不可直接当机体角。
     * 机体角应使用下面换算后的 front_deg/pitch、side_deg/roll、yaw_deg。
     * 不要因机体动作名称不同而交换这里的X/Y输入，矩阵按原始顺序解算。
     */
    float angles_deg[3];
    float gyro_deg_s[3]; /* 传感器自身 X/Y/Z 轴角速度。 */
    float acc_g[3];      /* 传感器自身 X/Y/Z 轴加速度。 */
} AttitudeSample;

/* Process 返回 READY 时的输出：机体姿态以及换算到机体坐标的向量。
 * 以下正方向是给机器人控制使用的约定，与原始 X/Y/Z 角名称区别开。
 */
typedef struct
{
    float front_deg; /* 机体俯仰/pitch：机头抬高为正，范围 [-90, 90]。 */
    float side_deg;  /* 机体横滚/roll：左侧抬高为正，范围 [-180, 180)。 */
    float yaw_deg;   /* 机头向左转为正，相对标定时的航向，范围 [-180, 180)。 */
    /* 下面两组向量仍用机体“右、前、上”顺序：
     * gyro[0] 绕右方轴，抬头方向为正；gyro[1] 绕前方轴，左高方向为正；
     * gyro[2] 绕上方轴，左转方向为正。这些角速度不等同于欧拉角的导数。
     */
    float gyro_deg_s[3];
    float acc_g[3];
} AttitudeBody;

/* 每一次 Process 调用的结果，调用方先检查它，再决定能否使用 output。
 * CALIBRATING 是正常等待阶段；INVALID/SINGULAR 才是本帧不可用的支线。
 * READY 表示本帧成功，不等同于 ctx.ready 这个“是否完成标定”的持久标志。
 */
typedef enum
{
    ATTITUDE_MATH_CALIBRATING = 0, /* 正常累计/等待样本间隔，未写 output。 */
    ATTITUDE_MATH_READY,          /* 本帧解算成功，已写 output。 */
    ATTITUDE_MATH_INVALID,        /* 参数或输入无效，未写 output。 */
    ATTITUDE_MATH_SINGULAR        /* 机头接近竖直，欧拉角不能唯一分解，未写 output。 */
} AttitudeMathResult;

/* 模块的持久上下文，由应用保留并传给每一次调用。
 * 标定后两套矩阵不再逐帧变化；原始数据和本帧输出放在另外两个结构体中。
 */
typedef struct
{
    uint8_t ready;                  /* 0=尚未建矩阵；1=已完成标定，可以解算。 */
    uint8_t calibration_count;      /* 已计入平均的有效样本数，完成时为 10。 */
    uint32_t calibration_tick;      /* 上一次计入平均的毫秒时刻，用于 100 ms 间隔。 */
    uint32_t calibration_restarts;  /* 故障导致未完成累计被清空的次数，不是 MCU 重启。 */
    float sensor_x_heading_offset_deg; /* 外部提供的平面内安装偏角，默认对齐时为 0。 */
    float angle_sin_sum[3];         /* 三个角的 sin 累加，用于跨 +/-180 度的环形平均。 */
    float angle_cos_sum[3];         /* 三个角的 cos 累加，与 sin 一起计算平均角。 */
    float gyro_sum[3];              /* 原始角速度累加，保留标定期间的平均统计。 */
    float reference_angles_deg[3];  /* 标定时原始 X/Y/Z 的平均值，供建矩阵和日志使用。 */
    float gyro_bias_deg_s[3];       /* 标定期间平均角速度；当前只保留记录，不从RATE中减去。 */
    float mount_sensor_to_body[3][3]; /* M：传感器向量 -> 机体向量的固定安装矩阵。 */
    float reference_world_to_initial[3][3]; /* 外部参考系 -> 初始航向参考系的矩阵。 */
} AttitudeMath;

/* 【正常入口 1】初始化/重新标定。
 * 输入：可写 ctx，以及 heading_offset_deg 安装偏角；无返回值/姿态输出。
 * 启动或主动重新标定时调用，清空全部 ctx；ctx 为 NULL 时直接返回。
 * 标定期间应让机体尽量水平、静止，支架可以提高平均基准的准确性。
 * 本模块不会因手扶晃动自动重启；晃动仍会影响基准角和角速度零偏的估计。
 * heading_offset 是传感器 +X 在水平面的投影，相对机体右方的安装偏角；
 * 从上方看逆时针为正。默认对齐时传 0；单个水平姿态无法自动测出此值。
 * 该函数不向 JY61P 写入校准或归零指令。
 */
void AttitudeMath_Init(AttitudeMath *ctx, float heading_offset_deg);

/* 【正常入口 2】每次收到新的完整成功采样时调用。
 * 输入：ctx、只读 sample 和采样完成的毫秒时刻 sample_tick。
 * 输出：返回本帧结果；仅 READY 会写 output；标定阶段会修改 ctx。
 * 标定时按至少 100 ms 间隔收集 10 个样本；中间的读数不计入平均值。
 * 当前 JY61P 应配置为至少 10 Hz 更新，否则需增大标定间隔。
 * 不根据角速度、加速度模长或角度波动自动重启标定；水平静止由操作保证。
 * 非有限/超出传感器量程的输入会清掉未完成累积；通信错误由调用方通知。
 * 返回 READY 才修改 output；第 10 个样本会建立矩阵并立即输出结果。
 * 其他返回值保持 output 不变，调用方不能将旧 output 当作本次有效结果。
 */
AttitudeMathResult AttitudeMath_Process(AttitudeMath *ctx,
    const AttitudeSample *sample, uint32_t sample_tick, AttitudeBody *output);

/* 【故障入口】调用方在通信故障时通知本模块丢弃未完成的标定。
 * 输入：可写 ctx；无返回值/姿态输出。ready=0 且 count>0 时清空累计并加 restart。
 * 已经建立好的固定安装矩阵继续保留；ctx 为 NULL 或无需丢弃时不做修改。
 * 本函数只管数学标定累计，不负责复位/恢复 JY61P 或 I2C/UART 通信。
 */
void AttitudeMath_DiscardPending(AttitudeMath *ctx);

#endif
