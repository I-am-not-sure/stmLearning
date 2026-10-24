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

/* 阅读入口：每秒读 JY61P 两段寄存器，经 USART1 输出九个物理量。
 * IIC 位时序在 soft_i2c.c；本文件负责传感器寄存器选择与数据换算。 */
#include "main.h"
#include "gpio.h"
#include "usart.h"

#include <stdio.h>
#include "soft_i2c.h"

typedef struct
{
  float acc[3];       /* X/Y/Z 加速度，单位 g */
  float gyro[3];      /* X/Y/Z 角速度，单位 °/s */
  float angle[3];     /* Roll/Pitch/Yaw 欧拉角，单位 °；模块已完成姿态解算 */
} JY61P_Data;

/* 传入未左移的七位设备地址 0x50；驱动内部转换为写 0xA0、读 0xA1。 */
#define JY61P_I2C_ADDR       0x50U
/* 寄存器编号不是字节偏移：每个编号对应一个 16 位数据。 */
#define JY61P_REG_AX         0x34U
#define JY61P_REG_ROLL       0x3DU
/* 报告周期由 CMake 的 REPORT_MS 指定；默认 20ms。 */
#ifndef GYRO_BENCHMARK
#define GYRO_BENCHMARK 0
#endif
#ifndef GYRO_REPORT_PERIOD_MS
#define GYRO_REPORT_PERIOD_MS 20U
#endif
#define REPORT_PERIOD_MS GYRO_REPORT_PERIOD_MS

void SystemClock_Config(void);

/* 同一个 16 位数据先收低字节，再收高字节；最高位为符号位。
 * 例如 00 F8 -> 0xF800 -> -2048，加速度换算后为 -1g。 */
static int16_t JY61P_DecodeInt16(const uint8_t *bytes)
{
  uint16_t value = (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8);
  return (int16_t)((value & 0x8000U) ? (int32_t)value - 65536 : value);
}

static SoftI2C_Status JY61P_Read(JY61P_Data *data)
{
  uint8_t motion[12]; /* 0x34~0x39：AX/AY/AZ/GX/GY/GZ，各占 2 字节 */
  uint8_t angles[6];  /* 0x3D~0x3F：Roll/Pitch/Yaw，各占 2 字节 */
  SoftI2C_Status status;

  /* length 是字节数：6 个寄存器必须读 12 字节，不是 6 字节。 */
  status = SoftI2C_ReadRegisters(JY61P_I2C_ADDR, JY61P_REG_AX,
                                 motion, sizeof(motion));
  if (status != SOFT_I2C_OK)
  {
    return status;
  }

  /* JY61P 当前不读 0x3A~0x3C 的磁场区域，另起一次事务读取角度。
   * 两次读取不保证属于同一个传感器更新时刻。 */
  status = SoftI2C_ReadRegisters(JY61P_I2C_ADDR, JY61P_REG_ROLL,
                                 angles, sizeof(angles));
  if (status != SOFT_I2C_OK)
  {
    return status;
  }

  /* 两段都成功后才换算，避免失败时输出未初始化/不完整的数据。
   * 系数沿用原工程及 IIC 协议：16g、2000°/s、180° 对应 32768。 */
  for (uint32_t axis = 0; axis < 3U; ++axis)
  {
    data->acc[axis] = JY61P_DecodeInt16(&motion[axis * 2U]) * (16.0f / 32768.0f);
    data->gyro[axis] = JY61P_DecodeInt16(&motion[6U + axis * 2U]) * (2000.0f / 32768.0f);
    data->angle[axis] = JY61P_DecodeInt16(&angles[axis * 2U]) * (180.0f / 32768.0f);
  }
  return SOFT_I2C_OK;
}

/* 串口只负责把 STM32 读到的数据送到电脑，不是传感器的通信接口。 */
#if !GYRO_BENCHMARK
static void JY61P_Report(void)
{
  JY61P_Data data;
  char line[192];
  int length;

  SoftI2C_Status status = JY61P_Read(&data);
  if (status == SOFT_I2C_OK)
  {
    length = snprintf(line, sizeof(line),
                      "ACC(g)=%.3f,%.3f,%.3f GYRO(deg/s)=%.3f,%.3f,%.3f "
                      "ANGLE(deg)=%.3f,%.3f,%.3f\r\n",
                      (double)data.acc[0], (double)data.acc[1], (double)data.acc[2],
                      (double)data.gyro[0], (double)data.gyro[1], (double)data.gyro[2],
                      (double)data.angle[0], (double)data.angle[1], (double)data.angle[2]);
  }
  else
  {
    /* 错误指出失败阶段；SCL/SDA 是 STOP 清理之后的电平快照。 */
    length = snprintf(line, sizeof(line),
                      "JY61P I2C read error: %s SCL=%u SDA=%u\r\n",
                      SoftI2C_ErrorText(status),
                      (unsigned int)HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_6),
                      (unsigned int)HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7));
  }

  if ((length > 0) && ((size_t)length < sizeof(line)))
  {
    (void)HAL_UART_Transmit(&huart1, (uint8_t *)line, (uint16_t)length, 100U);
  }
}

#endif

int main(void)
{
#if !GYRO_BENCHMARK
  uint32_t last_report;
#endif

  /* HAL 初始化 SysTick，后续 HAL_Delay/GetTick 和 IIC 超时均依赖它。 */
  HAL_Init();

  SystemClock_Config();

  MX_GPIO_Init();
  MX_USART1_UART_Init();

  /* 沿用原程序的上电等待，让模块准备好再发起总线访问。 */
  HAL_Delay(1500U);
  /* 初始化软件 IIC；边沿等待由 CMake 的 EDGE_US 指定。 */
  SoftI2C_Init();
  /* 减去一个周期，使主循环第一次进入时立即读取。 */
#if !GYRO_BENCHMARK
  last_report = HAL_GetTick() - REPORT_PERIOD_MS;
#endif

#if GYRO_BENCHMARK
  /* 连续读十秒；只在窗口结束时发一行统计，避免串口影响读取吞吐量。 */
  while (1)
  {
    uint32_t window_start = HAL_GetTick();
    uint32_t success = 0U, failure = 0U;
    uint32_t min_cycles = UINT32_MAX, max_cycles = 0U;
    uint64_t sum_cycles = 0U;
    SoftI2C_Status last_error = SOFT_I2C_OK;
    while ((uint32_t)(HAL_GetTick() - window_start) < 10000U)
    {
      JY61P_Data data;
      uint32_t begin = DWT->CYCCNT;
      SoftI2C_Status status = JY61P_Read(&data);
      uint32_t cycles = DWT->CYCCNT - begin;
      if (status == SOFT_I2C_OK)
      {
        ++success;
        sum_cycles += cycles;
        if (cycles < min_cycles) min_cycles = cycles;
        if (cycles > max_cycles) max_cycles = cycles;
      }
      else { ++failure; last_error = status; }
    }
    uint32_t elapsed = HAL_GetTick() - window_start;
    char line[150];
    int n = snprintf(line, sizeof(line),
                     "BENCH edge=%u us elapsed=%lu ms ok=%lu err=%lu min=%lu avg=%lu max=%lu us last=%s\r\n",
                     (unsigned int)SOFT_I2C_EDGE_US, (unsigned long)elapsed,
                     (unsigned long)success, (unsigned long)failure,
                     (unsigned long)(success ? min_cycles / (SystemCoreClock / 1000000U) : 0U),
                     (unsigned long)(success ? (sum_cycles / success) / (SystemCoreClock / 1000000U) : 0U),
                     (unsigned long)(success ? max_cycles / (SystemCoreClock / 1000000U) : 0U),
                     SoftI2C_ErrorText(last_error));
    if (n > 0 && (size_t)n < sizeof(line))
      (void)HAL_UART_Transmit(&huart1, (uint8_t *)line, (uint16_t)n, 200U);
  }
#else
  while (1)
  {
    uint32_t now = HAL_GetTick();
    /* 无符号差值可以跨越毫秒计数器回绕；这里是阻塞式轮询，不是定时中断。 */
    if ((uint32_t)(now - last_report) >= REPORT_PERIOD_MS)
    {
      last_report = now;
      JY61P_Report();
    }
  }
#endif
}

/* HSI 8MHz，无 PLL；AHB/APB1/APB2 均不分频。 */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/* 时钟或串口初始化失败时停在这里，方便调试器检查。 */
void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}
#ifdef USE_FULL_ASSERT

void assert_failed(uint8_t *file, uint32_t line)
{
}
#endif
