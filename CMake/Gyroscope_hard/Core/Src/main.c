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

/* 硬件 I2C1 版本：读取、换算、串口格式及 1 秒周期保持与软件版一致。 */
#include "main.h"
#include "gpio.h"
#include "usart.h"

#include <stdio.h>
#include "i2c.h"

typedef struct
{
  float acc[3];       /* X/Y/Z 加速度，单位 g */
  float gyro[3];      /* X/Y/Z 角速度，单位 °/s */
  float angle[3];     /* Roll/Pitch/Yaw 欧拉角，单位 °；模块已完成姿态解算 */
} JY61P_Data;

/* HAL_I2C_Mem_Read 要求七位地址左移，0x50 -> 0xA0；读位由外设设置。 */
#define JY61P_I2C_ADDR       (0x50U << 1)
/* 寄存器编号不是字节偏移：每个编号对应一个 16 位数据。 */
#define JY61P_REG_AX         0x34U
#define JY61P_REG_ROLL       0x3DU
/* 保持原来的 1 秒输出周期；改为 10ms 并不能加快软件 IIC。 */
#define JY61P_I2C_TIMEOUT_MS 100U
#define REPORT_PERIOD_MS    1000U

void SystemClock_Config(void);

/* 同一个 16 位数据先收低字节，再收高字节；最高位为符号位。
 * 例如 00 F8 -> 0xF800 -> -2048，加速度换算后为 -1g。 */
static int16_t JY61P_DecodeInt16(const uint8_t *bytes)
{
  uint16_t value = (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8);
  return (int16_t)((value & 0x8000U) ? (int32_t)value - 65536 : value);
}

static HAL_StatusTypeDef JY61P_Read(JY61P_Data *data)
{
  uint8_t motion[12]; /* 0x34~0x39：AX/AY/AZ/GX/GY/GZ，各占 2 字节 */
  uint8_t angles[6];  /* 0x3D~0x3F：Roll/Pitch/Yaw，各占 2 字节 */
  HAL_StatusTypeDef status;

  /* length 是字节数：6 个寄存器必须读 12 字节，不是 6 字节。 */
  status = HAL_I2C_Mem_Read(&hi2c1, JY61P_I2C_ADDR, JY61P_REG_AX,
                            I2C_MEMADD_SIZE_8BIT, motion, sizeof(motion),
                            JY61P_I2C_TIMEOUT_MS);
  if (status != HAL_OK)
  {
    return status;
  }

  /* JY61P 当前不读 0x3A~0x3C 的磁场区域，另起一次事务读取角度。
   * 两次读取不保证属于同一个传感器更新时刻。 */
  status = HAL_I2C_Mem_Read(&hi2c1, JY61P_I2C_ADDR, JY61P_REG_ROLL,
                            I2C_MEMADD_SIZE_8BIT, angles, sizeof(angles),
                            JY61P_I2C_TIMEOUT_MS);
  if (status != HAL_OK)
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
  return HAL_OK;
}

/* 串口只负责把 STM32 读到的数据送到电脑，不是传感器的通信接口。 */
static void JY61P_Report(void)
{
  JY61P_Data data;
  char line[192];
  int length;

  HAL_StatusTypeDef status = JY61P_Read(&data);
  if (status == HAL_OK)
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
    /* 在恢复总线前保存故障现场；BUSY 与线路电平有助于区分上拉/外设问题。 */
    uint32_t error = HAL_I2C_GetError(&hi2c1);
    uint32_t sr1 = hi2c1.Instance->SR1;
    uint32_t sr2 = hi2c1.Instance->SR2;
    uint32_t scl = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_6);
    uint32_t sda = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7);
    length = snprintf(line, sizeof(line),
                      "JY61P I2C read error: status=%u err=0x%08lX SR1=0x%04lX SR2=0x%04lX SCL=%lu SDA=%lu\r\n",
                      (unsigned int)status, (unsigned long)error,
                      (unsigned long)sr1, (unsigned long)sr2,
                      (unsigned long)scl, (unsigned long)sda);
    (void)I2C1_Recover();
  }

  if ((length > 0) && ((size_t)length < sizeof(line)))
  {
    (void)HAL_UART_Transmit(&huart1, (uint8_t *)line, (uint16_t)length, 100U);
  }
}

int main(void)
{
  uint32_t last_report;

  /* HAL 初始化 SysTick，后续 HAL_Delay/GetTick 和 IIC 超时均依赖它。 */
  HAL_Init();

  SystemClock_Config();

  MX_GPIO_Init();
  MX_USART1_UART_Init();
  I2C1_DiagnosePullups();
  MX_I2C1_Init();

  /* 沿用原程序的上电等待，让模块准备好再发起总线访问。 */
  HAL_Delay(1500U);
  /* 硬件 I2C1 初始化由 HAL 完成；若上电时 BUSY 锁住则尝试清除。 */
  if (__HAL_I2C_GET_FLAG(&hi2c1, I2C_FLAG_BUSY) != RESET)
  {
    (void)I2C1_Recover();
  }
  /* 减去一个周期，使主循环第一次进入时立即读取。 */
  last_report = HAL_GetTick() - REPORT_PERIOD_MS;

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
