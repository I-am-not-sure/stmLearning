/*
 * 姿态安装补偿：本文件只做数学运算，不收发 I2C/UART，也不驱动电机。
 *
 * 阅读路线（正常运行的主线）：
 *   1. AttitudeMath_Init：清空 ctx，开始标定。
 *   2. AttitudeMath_Process：按至少 100 ms 间隔累计 10 次有效样本。
 *   3. FinishCalibration：只在累计完成时调用一次，建立固定安装/航向矩阵。
 *   4. Solve：之后每个有效采样都调用，输出补偿后的机体姿态和向量。
 *
 * ready/count 只表示数学模块的标定阶段：
 *   ready=0、count=0..9：正在累计；ready=1：矩阵已建立，可以解算。
 * 它不是 JY61P 的 I2C/UART 通信状态机；通信重试和重新初始化在驱动中完成。
 *
 * 故障/特殊情况支线：
 *   无效输入 -> 不输出结果，必要时丢弃尚未完成的标定累计；
 *   通信故障 -> 调用方通知 DiscardPending，不清掉已经建立的安装矩阵；
 *   机头接近竖直 -> 欧拉角分解有奇异性，返回 SINGULAR，不发布输出。
 * 正常的手扶晃动不会触发自动清空；本文件没有运动阈值重启逻辑。
 */
#include "attitude_math.h"
#include <math.h>
#include <stddef.h>
#include <string.h>

#define DEG_TO_RAD 0.01745329251994329577f
#define RAD_TO_DEG 57.29577951308232088f
#define EULER_MIN_COS_PITCH 0.0001f

/* 【共享数学工具】输入任意角度，返回 [-180, 180) 内的等价角度。
 * 用于平均后的参考航向和最终侧倾/航向；不修改 ctx，也不写 output。
 */
static float WrapDegrees(float value)
{
    float wrapped = fmodf(value + 180.0f, 360.0f);
    if (wrapped < 0.0f)
    {
        wrapped += 360.0f;
    }
    return wrapped - 180.0f;
}

/* 【共享数学工具】输入 angles_deg={X,Y,Z} 欧拉角，写出 3x3 姿态矩阵。
 * 标定建矩阵、每帧解算时调用；只写 matrix，不修改输入或 ctx。
 * R = Rz(z) Ry(y) Rx(x)，将局部坐标的列向量换到外部参考坐标。
 * 每一列是局部 X/Y/Z 单位轴在外部参考系中的方向。
 */
static void EulerToMatrix(const float angles_deg[3], float matrix[3][3])
{
    float sx = sinf(angles_deg[0] * DEG_TO_RAD);
    float cx = cosf(angles_deg[0] * DEG_TO_RAD);
    float sy = sinf(angles_deg[1] * DEG_TO_RAD);
    float cy = cosf(angles_deg[1] * DEG_TO_RAD);
    float sz = sinf(angles_deg[2] * DEG_TO_RAD);
    float cz = cosf(angles_deg[2] * DEG_TO_RAD);

    matrix[0][0] = cz * cy;
    matrix[0][1] = cz * sy * sx - sz * cx;
    matrix[0][2] = cz * sy * cx + sz * sx;
    matrix[1][0] = sz * cy;
    matrix[1][1] = sz * sy * sx + cz * cx;
    matrix[1][2] = sz * sy * cx - cz * sx;
    matrix[2][0] = -sy;
    matrix[2][1] = cy * sx;
    matrix[2][2] = cy * cx;
}

/* 【共享数学工具】输入两个矩阵，计算 output=a*b。
 * 解算时组合安装补偿和航向参考；只写 output，不修改 a/b 或 ctx。
 * 对列向量来说右侧矩阵先作用，因此交换 a/b 会改变旋转的含义。
 */
static void MatrixMultiply(const float a[3][3], const float b[3][3],
                           float output[3][3])
{
    /* 调用处使用独立的临时矩阵，output 不与 a/b 重叠。 */
    for (uint32_t row = 0U; row < 3U; ++row)
    {
        for (uint32_t col = 0U; col < 3U; ++col)
        {
            output[row][col] = 0.0f;
            for (uint32_t k = 0U; k < 3U; ++k)
            {
                output[row][col] += a[row][k] * b[k][col];
            }
        }
    }
}

/* 【共享数学工具】把 input 向量按 matrix 变换，写入独立的 output。
 * 解算角速度/加速度时调用；只写 output，不修改输入或 ctx。
 * 向量换坐标可直接乘矩阵，不能把姿态角的减偏移方式用于三轴向量。
 */
static void MatrixVector(const float matrix[3][3], const float input[3],
                         float output[3])
{
    for (uint32_t row = 0U; row < 3U; ++row)
    {
        output[row] = matrix[row][0] * input[0]
                    + matrix[row][1] * input[1]
                    + matrix[row][2] * input[2];
    }
}

/* 【故障恢复工具】只清掉 ctx 中尚未完成的样本计数和三个累计数组。
 * DiscardPending 通过它重新开始累计；不改现有矩阵、ready 或结果输出。
 */
static void ResetAccumulation(AttitudeMath *ctx)
{
    ctx->calibration_count = 0U;
    memset(ctx->angle_sin_sum, 0, sizeof(ctx->angle_sin_sum));
    memset(ctx->angle_cos_sum, 0, sizeof(ctx->angle_cos_sum));
    memset(ctx->gyro_sum, 0, sizeof(ctx->gyro_sum));
}

/* 【正常主线 1：初始化/重新标定】输入 ctx 和平面内安装偏角（度）。
 * 启动或主动重新标定时调用；清空整个 ctx，再保存安装偏角。
 * 不产生姿态输出，不向传感器发送指令；NULL 输入直接返回。
 */
void AttitudeMath_Init(AttitudeMath *ctx, float heading_offset_deg)
{
    if (ctx == NULL)
    {
        return;
    }
    memset(ctx, 0, sizeof(*ctx));
    ctx->sensor_x_heading_offset_deg = heading_offset_deg;
}

/* 【故障支线：丢弃未完成标定】输入 ctx；通信失败或发现无效数据时调用。
 * 仅当 ready=0 且已有累计时清空累计，并令 calibration_restarts 加 1。
 * NULL、尚未累计或已经标定完成时无操作；不写姿态输出。
 * restart 是“标定累计重新开始次数”，不是 MCU 重启次数。
 */
void AttitudeMath_DiscardPending(AttitudeMath *ctx)
{
    if (ctx != NULL && ctx->ready == 0U && ctx->calibration_count != 0U)
    {
        ResetAccumulation(ctx);
        ++ctx->calibration_restarts;
    }
}

/* 【故障检查工具】输入非 NULL 样本，返回 1=有效、0=非有限数/超量程。
 * Process 累计或解算前调用；只读取样本，不修改 ctx 或 output。
 * 量程检查是数据有效性检查，不是用于限制手扶晃动的稳定性阈值。
 */
static uint8_t SampleIsFinite(const AttitudeSample *sample)
{
    for (uint32_t axis = 0U; axis < 3U; ++axis)
    {
        if (!isfinite(sample->angles_deg[axis])
            || fabsf(sample->angles_deg[axis]) > 180.01f
            || !isfinite(sample->gyro_deg_s[axis])
            || fabsf(sample->gyro_deg_s[axis]) > 2000.1f
            || !isfinite(sample->acc_g[axis])
            || fabsf(sample->acc_g[axis]) > 16.01f)
        {
            return 0U;
        }
    }
    return 1U;
}

/* 【正常主线 3：建立矩阵】输入已累计 10 个样本的 ctx；无直接返回值。
 * Process 在第 10 个有效样本到达时调用一次，之后不会逐帧重建矩阵。
 * 修改 ctx：计算参考角/角速度平均值，建立两套矩阵，最后置 ready=1。
 * 不写 AttitudeBody 输出；本帧随后的 Solve 才会产生姿态结果。
 */
static void FinishCalibration(AttitudeMath *ctx)
{
    for (uint32_t axis = 0U; axis < 3U; ++axis)
    {
        /* 环形平均，避免 +179.9 和 -179.9 平均成 0 度。 */
        ctx->reference_angles_deg[axis] = atan2f(ctx->angle_sin_sum[axis],
                                               ctx->angle_cos_sum[axis]) * RAD_TO_DEG;
        /* 保留前10次角速度平均记录；当前RATE已停用对此均值的扣除。 */
        ctx->gyro_bias_deg_s[axis] = ctx->gyro_sum[axis]
                                 / (float)ATTITUDE_CALIBRATION_SAMPLES;
    }

    /* 水平机体的 X/Y 欧拉角为 0：
     * R_sensor(0) = Rz(初始机体航向) * M。
     * 水平测量给出 M 的 X/Y 倾角；M 的 Z 安装偏角必须由安装定位提供。
     * 不能直接把含任意初始航向的 R_sensor(0) 当作安装矩阵。
     */
    const float mount_angles[3] = {ctx->reference_angles_deg[0],
                                  ctx->reference_angles_deg[1],
                                  ctx->sensor_x_heading_offset_deg};
    EulerToMatrix(mount_angles, ctx->mount_sensor_to_body);

    /* 初始航向归零改变的是外部参考系，故在解算时从左侧乘这个矩阵。
     * 它与固定安装补偿分开保存，原始 Z 姿态角仍参与完整的矩阵换算。
     */
    const float reference_angles[3] = {0.0f, 0.0f,
        -WrapDegrees(ctx->reference_angles_deg[2] - ctx->sensor_x_heading_offset_deg)};
    EulerToMatrix(reference_angles, ctx->reference_world_to_initial);
    ctx->ready = 1U;
}

/* 【正常主线 4：逐帧解算】输入已 ready 的只读 ctx 和有效传感器样本。
 * Process 在标定完成的第 10 帧及其后每个有效采样时调用。
 * READY：把本帧结果写入 output；SINGULAR：保留旧 output，不发布新结果。
 * 本函数不修改 ctx；先在局部 solved 中计算，成功后再整体提交给 output。
 */
static AttitudeMathResult Solve(const AttitudeMath *ctx,
                               const AttitudeSample *sample, AttitudeBody *output)
{
    float sensor[3][3];
    float mount_inverse[3][3];
    float world_body[3][3];
    float initial_body[3][3];
    EulerToMatrix(sample->angles_deg, sensor);
    /* 旋转矩阵的逆等于转置：M 把传感器向量换到机体坐标，
     * 此处 M^T 从传感器姿态矩阵中消除固定的安装旋转。
     */
    for (uint32_t row = 0U; row < 3U; ++row)
    {
        for (uint32_t col = 0U; col < 3U; ++col)
        {
            mount_inverse[row][col] = ctx->mount_sensor_to_body[col][row];
        }
    }

    /* 固定安装补偿乘在右边，航向参考系的换算乘在左边：
     * R_body = R_reference * R_sensor * transpose(M)。
     * 三个原始姿态角都参与换算，不直接逐项相减或只把原始 Z 清零。
     */
    /* C11 中二维数组增加 const 限定需要显式转换；不改变矩阵内容。 */
    MatrixMultiply((const float (*)[3])sensor,
                   (const float (*)[3])mount_inverse, world_body);
    MatrixMultiply(ctx->reference_world_to_initial,
                   (const float (*)[3])world_body, initial_body);

    /* 内部矩阵的轴顺序是“右、前、上”。输出使用“前、左、上”的
     * Z-Y-X 欧拉角约定，再将俯仰取反，使 FRONT 抬头为正。
     * 相当于对 initial_body 做 C^T R C，C=Rz(90 deg)，再取欧拉角。
     * 这样复合倾斜时 YAW 表示机头的水平朝向，而不是右方轴的朝向。
     */
    float sin_front = initial_body[2][1];
    if (sin_front > 1.0f) sin_front = 1.0f;
    if (sin_front < -1.0f) sin_front = -1.0f;
    float cos_front = hypotf(initial_body[0][1], initial_body[1][1]);
    if (cos_front < EULER_MIN_COS_PITCH)
    {
        /* 【特殊姿态支线】机头竖直时侧倾/航向不能唯一分解。
         * 此处提前返回，output 尚未写入；调用方应把本帧标为无效。
         * 这不是通信故障，也不会重新开始标定。
         */
        return ATTITUDE_MATH_SINGULAR;
    }

    AttitudeBody solved;
    solved.front_deg = asinf(sin_front) * RAD_TO_DEG;
    solved.side_deg = WrapDegrees(atan2f(-initial_body[2][0], initial_body[2][2]) * RAD_TO_DEG);
    solved.yaw_deg = WrapDegrees(atan2f(-initial_body[0][1], initial_body[1][1]) * RAD_TO_DEG);

    /* 正常向量处理：用固定安装矩阵 M 将角速度和加速度换到机体“右、前、上”。
     * 当前 RATE 直接转换本帧原始 GYRO，不减去前10次角速度平均值。
     * 此处得到本体角速度，不是 FRONT/SIDE/YAW 欧拉角的时间导数。
     */
    /* [按用户要求停用角速度零偏扣除，保留旧代码便于阅读]
    float unbiased_gyro[3];
    for (uint32_t axis = 0U; axis < 3U; ++axis)
    {
        unbiased_gyro[axis] = sample->gyro_deg_s[axis] - ctx->gyro_bias_deg_s[axis];
    }
    MatrixVector(ctx->mount_sensor_to_body, unbiased_gyro, solved.gyro_deg_s);
    */
    MatrixVector(ctx->mount_sensor_to_body, sample->gyro_deg_s, solved.gyro_deg_s);
    MatrixVector(ctx->mount_sensor_to_body, sample->acc_g, solved.acc_g);
    /* 到这里本帧才完全有效，统一提交输出。 */
    *output = solved;
    return ATTITUDE_MATH_READY;
}

/* 【正常主线入口/阶段分支】输入 ctx、完整样本及采样完成的毫秒时刻。
 * 每个新采样调用一次，返回本帧处于标定、成功、无效或奇异姿态哪种情况。
 * 标定阶段会修改 ctx；正常解算阶段只读取 ctx。
 * 只有返回 READY 才写 output，其他返回值不能把旧 output 当成本帧结果。
 */
AttitudeMathResult AttitudeMath_Process(AttitudeMath *ctx,
    const AttitudeSample *sample, uint32_t sample_tick, AttitudeBody *output)
{
    /* 【故障支线 A：参数错误】没有可用对象，直接返回；不修改任何对象。 */
    if (ctx == NULL || sample == NULL || output == NULL)
    {
        return ATTITUDE_MATH_INVALID;
    }
    /* 【故障支线 B：数据无效】不把坏数据加入平均，也不发布姿态。
     * 未标定完成时清掉已有累计；已完成的固定矩阵保留。
     */
    if (!isfinite(ctx->sensor_x_heading_offset_deg) || SampleIsFinite(sample) == 0U)
    {
        AttitudeMath_DiscardPending(ctx);
        return ATTITUDE_MATH_INVALID;
    }
    /* 【正常主线 2：累计阶段】ready=0 才进来；ready=1 则直接到最后的 Solve。 */
    if (ctx->ready == 0U)
    {
        if (ctx->calibration_count != 0U
            && (uint32_t)(sample_tick - ctx->calibration_tick)
                < ATTITUDE_CALIBRATION_INTERVAL_MS)
        {
            /* 时间间隔不足只跳过本次累计，不清空之前的样本，不增加 restart。 */
            return ATTITUDE_MATH_CALIBRATING;
        }
        /* 按约定只累计前 10 次有效测量，不用运动阈值清空累计。
         * 机体是否水平静止由标定时的支架与操作保证。
         */
        for (uint32_t axis = 0U; axis < 3U; ++axis)
        {
            ctx->angle_sin_sum[axis] += sinf(sample->angles_deg[axis] * DEG_TO_RAD);
            ctx->angle_cos_sum[axis] += cosf(sample->angles_deg[axis] * DEG_TO_RAD);
            ctx->gyro_sum[axis] += sample->gyro_deg_s[axis];
        }
        ctx->calibration_tick = sample_tick;
        ++ctx->calibration_count;
        if (ctx->calibration_count < ATTITUDE_CALIBRATION_SAMPLES)
        {
            return ATTITUDE_MATH_CALIBRATING;
        }
        /* 第 10 次有效累计：只在这里建立矩阵；随后继续解算这一帧。 */
        FinishCalibration(ctx);
    }
    return Solve(ctx, sample, output);
}
