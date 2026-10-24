# line_ide_cmake：串口命令控制多路 PWM

接收串口空闲 DMA 数据，解析命令并设置 TIM1/TIM8 的 PWM 比较值。

| 项目 | 内容 |
| --- | --- |
| MCU | `STM32F407ZGT6` |
| 构建入口 | CMake |
| 学习内容 | UART DMA、命令解析、TIM1/TIM8、PWM |
| 代码入口 | [`Core/Src/main.c`](Core/Src/main.c) |
| 来源 | `STM32C/cmake_enemy/line_ide_cmake` |

## 建议阅读顺序

1. [`Core/Src/main.c`](Core/Src/main.c)：初始化、主循环和中断回调。

外设和引脚配置见 [`line_ide_cmake.ioc`](line_ide_cmake.ioc)；实际启用行为以源码为准。

## CubeMX 配置信号索引

下面列出 `.ioc` 中的配置，包含可能尚未由应用启用的外设；这不是完整硬件接线图。

| MCU 引脚 | 配置信号 |
| --- | --- |
| `PA10` | `USART1_RX` |
| `PA9` | `USART1_TX` |
| `PB6` | `I2C1_SCL` |
| `PB7` | `I2C1_SDA` |
| `PC6` | `S_TIM8_CH1` |
| `PC7` | `S_TIM8_CH2` |
| `PC8` | `S_TIM8_CH3` |
| `PC9` | `S_TIM8_CH4` |
| `PE11` | `S_TIM1_CH2` |
| `PE13` | `S_TIM1_CH3` |
| `PE14` | `S_TIM1_CH4` |
| `PE9` | `S_TIM1_CH1` |
| `PF0` | `I2C2_SDA` |
| `PF1` | `I2C2_SCL` |

## 本工程说明

源码保留早期 PWM 实验注释，实际接线与初始化以 .ioc 和 tim.c 为准。

## 打开与构建

[公共构建与导入指南](../../docs/BUILDING.md) · [全部工程与外设索引](../../README.md)

本次整理的验证范围见[验证记录](../../docs/VALIDATION.md)。
