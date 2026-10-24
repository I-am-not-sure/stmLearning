#ifndef JY61P_H
#define JY61P_H

/* 完成一次传感器读取，并通过 USART1 输出结果或错误原因。
 * 调用前先完成 GPIO、串口和 SoftI2C_Init() 初始化。 */
void JY61P_Report(void);

#endif
