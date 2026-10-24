/**
  ******************************************************************************
  * @file    gpio.c
  * @brief   This file provides code for the configuration
  *          of all used GPIO pins.
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

#include "gpio.h"

/* 端口时钟；PB6/PB7 的硬件 I2C 引脚配置由 i2c.c 完成。 */
void MX_GPIO_Init(void)
{
  /* I2C1 的 PB6/PB7 在 HAL_I2C_MspInit 中设为复用开漏。
   * 这里仅开启端口时钟，不能再把它们配置为输入上拉。 */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
}
