#ifndef JY61P_H
#define JY61P_H

#include "main.h"

/* 三个成员的单位都是度 */
typedef struct
{
    float roll;
    float pitch;
    float yaw;
} JY61P_Angles;

typedef struct
{
    float acc[3];          /* X/Y/Z 加速度，单位 g */
    float gyro[3];         /* X/Y/Z 角速度，单位 deg/s */
    JY61P_Angles angles;    /* 三个姿态角，单位 deg */
} JY61P_Data;

/* ======================== DMA 采样状态机接口 ========================
 * 本驱动独占 I2C2；应用只在主循环中使用下面三个接口。
 *
 * 正常流程：
 * StartReadData -> Process 启动 0x34 的 12 字节 DMA 读取
 *              -> 接收完成中断只置标志
 *              -> Process 启动 0x3D 的 6 字节 DMA 读取
 *              -> 接收完成中断只置标志
 *              -> GetReadResult 换算并取出完整数据。
 *
 * 故障流程：任一读取失败/超时 -> Process 停止 DMA 并恢复 I2C2
 *                            -> GetReadResult 返回本次失败状态。
 */

/* 申请一次完整采样，不等待数据。
 * HAL_OK：申请已受理，需持续调用 Process，再用 GetReadResult 取结果。
 * HAL_BUSY：前一次采样或未取走的结果仍占用驱动，不能重复启动。
 */
HAL_StatusTypeDef JY61P_StartReadData(void);

/* 推进状态机：每次主循环调用一次，不可从中断回调中调用。
 * 本工程的 F4 HAL 在启动 IIC DMA 时仍会等待地址阶段；放在主循环
 * 可让优先级 15 的 SysTick 正常运行，保证地址阶段的超时有效。
 */
void JY61P_Process(void);

/* 获取并消费一次结果：返回 0 表示尚未完成，返回 1 表示已取出结果。
 * 返回 1 时，status 才有效：
 * HAL_OK：两段数据都成功，data 得到完整的加速度、角速度、姿态角。
 * 其他值：本次采样失败，data 保持不变；错误详情见 GetError。
 * 将“是否完成”和“HAL 结果”分开，是因为 HAL_BUSY 也可能表示
 * 本次启动失败，不能把这种已完成的失败结果误当成“还在等待”。
 * 注意：成功和失败结果都只能取一次，取走后驱动才允许下次采样。
 * data/status 为 NULL 时返回 0，不消费结果。
 */
uint8_t JY61P_GetReadResult(JY61P_Data *data, HAL_StatusTypeDef *status);

/* 返回最近一次采样保存的 HAL IIC 错误码。
 * 错误码在外设恢复前保存，避免被 HAL_I2C_Init 清零。
 */
uint32_t JY61P_GetError(void);
#endif
