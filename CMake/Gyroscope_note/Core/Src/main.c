/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* 阅读入口：本文件只负责“按什么顺序启动”和“何时读取”。
 * 传感器寄存器、数值换算和串口报告见 jy61p.c；
 * START/ACK/STOP 等软件 IIC 时序见 soft_i2c.c。 */
#include "main.h"
#include "clock.h"
#include "gpio.h"
#include "usart.h"
#include "soft_i2c.h"
#include "jy61p.h"

/* 两次报告的计划间隔为 1000ms；它不决定 IIC 的时钟速度，
 * 也不保证传感器每次都产生一组全新的测量值。 */
#define REPORT_PERIOD_MS 1000U

/* main 是复位后的应用入口：先初始化运行环境和外设，再循环读取传感器。
 * 读取与串口发送由 JY61P_Report 完成，main 不处理寄存器或字节。 */
int main(void)
{
  uint32_t last_report;

  /* HAL_Init：初始化 HAL，并建立 1ms 的 SysTick 时间基准。
   * 后面的 HAL_Delay、HAL_GetTick 和软件 IIC 超时都依赖这个计时。 */
  HAL_Init();

  /* SystemClock_Config：选择内部 HSI 8MHz，不启用 PLL；具体配置在 clock.c。
   * 必须先确定系统时钟，软件 IIC 才能按 CPU 周期计算等待时间。 */
  SystemClock_Config();

  /* MX_GPIO_Init：打开 GPIO 时钟，让 PB6/SCL、PB7/SDA 初始处于输入上拉状态。
   * 后续 SoftI2C_Init 和驱动会按需要释放线路或将其拉低。 */
  MX_GPIO_Init();

  /* MX_USART1_UART_Init：配置 USART1 为 115200、8N1，只用 PA9/TX 向电脑输出。
   * 传感器通过 PB6/PB7 的 IIC 读取，USART1 不是传感器通信接口。 */
  MX_USART1_UART_Init();

  /* HAL_Delay：沿用原程序的 1.5 秒上电等待，让传感器先准备好。 */
  HAL_Delay(1500U);

  /* SoftI2C_Init：启用 DWT 周期计数器，设置约 50µs 的边沿等待，
   * 释放 SCL/SDA 并尝试恢复总线；它本身不读取传感器数据。 */
  SoftI2C_Init();

  /* HAL_GetTick 返回当前毫秒数。回退一个周期，使第一次循环立即报告。 */
  last_report = HAL_GetTick() - REPORT_PERIOD_MS;

  /* 主循环是阻塞式轮询：每次取当前时间，到了报告周期再读取。 */
  while (1)
  {
    uint32_t now = HAL_GetTick();
    /* 无符号差值可跨越毫秒计数器回绕；这里没有使用定时器中断。 */
    if ((uint32_t)(now - last_report) >= REPORT_PERIOD_MS)
    {
      last_report = now;

      /* JY61P_Report：通过软件 IIC 读加速度、角速度和姿态角，
       * 再通过 USART1 输出数值；若读取失败则输出错误阶段。 */
      JY61P_Report();
    }
  }
}

/* Error_Handler：供时钟和串口初始化失败时调用。
 * 关中断后停在死循环，避免在外设配置错误时继续访问传感器；
 * 调试器可在此处暂停并检查调用栈和外设状态。 */
void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}
#ifdef USE_FULL_ASSERT

/* assert_failed：只在启用 USE_FULL_ASSERT 时由 HAL 的参数检查调用。
 * file、line 分别指出失败的源码文件与行号；当前实现为空，可在此设断点。 */
void assert_failed(uint8_t *file, uint32_t line)
{
}
#endif
