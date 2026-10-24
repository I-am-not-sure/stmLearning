#ifndef DEVICE_APP_H
#define DEVICE_APP_H

#include "ms5837.h"
#include "jy61p.h"
#include "attitude_math.h"

/* 阅读提示：应用层有“姿态读取”和“串口发送”两个状态机。
 * 数学模块的 ready/count 只控制标定阶段，独立于 I2C 状态。
 * Init/Process 推进流程；下面的 Get 接口只读结果，不推进流程。
 */

/* [正常工作] 电机原有启动代码执行完、CubeMX 外设初始化完后调用一次。
 * 深度传感器提交启动请求；JY61P 保留原来 1500 ms 的上电稳定等待。
 */
void DevicesApp_Init(void);

/* [正常工作] 每次主循环都调用：分别推进两个传感器和串口发送状态机。
 * 不用 HAL_Delay(1000) 控制输出，等待期间仍能处理其他传感器。
 * STM32F4 HAL 启动 I2C DMA 时仍有短暂地址阶段轮询，故只能在主循环调用。
 */
void DevicesApp_Process(void);

/* [正常工作] 供后续控制算法取最近一次有效完整采样；只允许主循环调用。
 * 返回 1：复制数据，sample_tick 非 NULL 时返回采样完成的毫秒时刻。
 * 返回 0：尚未完成首次采样或最近一次采样已报错，不修改输出数据。
 * 时间戳可用于判断数据年龄；输出周期与传感器采样周期相互独立。
 */
uint8_t DevicesApp_GetDepth(MS5837_Sample *sample, uint32_t *sample_tick);
uint8_t DevicesApp_GetMotion(JY61P_Data *data, uint32_t *sample_tick);

/* [正常结果接口] 机体姿态：FRONT 抬头为正，SIDE 左高为正，YAW 左转为正。
 * 标定完成、最近一次解算有效且原始姿态采样有效时才返回 1。
 * 每次启动需在水平支架上保持静止，至少 10 个间隔 100 ms 的有效样本。
 * 手持摆放时仍可完成平均，但水平基准与角速度零偏的精度会受晃动影响。
 * 返回 0 时不修改 data；返回 1 的 sample_tick 仍须由控制代码检查是否过旧。
 */
uint8_t DevicesApp_GetAttitude(AttitudeBody *data, uint32_t *sample_tick);

/* 主循环调用，在机器重新放稳后重新收集水平基准；不修改电机或传感器。 */
void DevicesApp_RestartAttitudeCalibration(void);

#endif
