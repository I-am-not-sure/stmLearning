#ifndef APP_H
#define APP_H

#include "jy61p.h"

/* 主循环持续调用：分别推进连续采样状态机和串口发送状态机。
 * 此函数不负责 1 秒打印周期，也不调用 HAL_Delay。
 * 不能放在中断回调中，因为 F4 HAL 的 IIC DMA 启动仍会等待地址阶段。
 */
void App_Process(void);

/* 请求打印一次最新采样结果，由主循环的计时判断每隔 1 秒调用。
 * 仅置请求标志；发送忙时保留一个请求，不堆积历史打印数据。
 */
void App_RequestPrint(void);

/* PID 等主循环任务可用此接口取得最近一次完整成功的采样。
 * 返回 1：data 已复制；sample_tick 非 NULL 时同时返回采样完成时刻。
 * 返回 0：尚无成功数据，或最近一次采样失败，data 保持不变。
 * sample_tick 的单位为毫秒，可用于计算 PID 的实际 dt / 数据年龄。
 * 数据由主循环更新，此接口也只允许在主循环调用，不能从 ISR 调用。
 */
uint8_t App_GetLatestData(JY61P_Data *data, uint32_t *sample_tick);

#endif
