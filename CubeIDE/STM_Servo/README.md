# STM_Servo：串口控制舵机

接收串口角度输入，进行范围限制并更新 PWM 比较值。

| 项目 | 内容 |
| --- | --- |
| MCU | `STM32F103C8T6` |
| 构建入口 | STM32CubeIDE |
| 学习内容 | UART、PWM、舵机、编码器实验 |
| 代码入口 | [`Core/Src/main.c`](Core/Src/main.c) |
| 来源 | `stmLearning/STM_Servo` |

## 建议阅读顺序

1. [`Core/Src/main.c`](Core/Src/main.c)：初始化、主循环和中断回调。

外设和引脚配置见 [`STM_Servo.ioc`](STM_Servo.ioc)；实际启用行为以源码为准。

## CubeMX 配置信号索引

下面列出 `.ioc` 中的配置，包含可能尚未由应用启用的外设；这不是完整硬件接线图。

| MCU 引脚 | 配置信号 |
| --- | --- |
| `PA2` | `USART2_TX` |
| `PA3` | `USART2_RX` |
| `PA8` | `S_TIM1_CH1` |
| `PA9` | `S_TIM1_CH2` |
| `PB8` | `S_TIM4_CH3` |

## 打开与构建

[公共构建与导入指南](../../docs/BUILDING.md) · [全部工程与外设索引](../../README.md)

本次整理的验证范围见[验证记录](../../docs/VALIDATION.md)。
