# depthf4：MS5837 I²C DMA 状态机

使用 I²C DMA 回调推进 MS5837 采样，并通过串口 DMA 输出数据。

| 项目 | 内容 |
| --- | --- |
| MCU | `STM32F407ZGT6` |
| 构建入口 | CMake |
| 学习内容 | I²C DMA、MS5837、UART DMA、状态机 |
| 代码入口 | [`Core/Src/main.c`](Core/Src/main.c) |
| 来源 | `STM32C/cmake_enemy/depthf4` |

## 建议阅读顺序

1. [`Core/Src/main.c`](Core/Src/main.c)：初始化、主循环和中断回调。
2. [`Core/Inc/ms5837.h`](Core/Inc/ms5837.h)
3. [`Core/Src/ms5837.c`](Core/Src/ms5837.c)

外设和引脚配置见 [`depthf4.ioc`](depthf4.ioc)；实际启用行为以源码为准。

## CubeMX 配置信号索引

下面列出 `.ioc` 中的配置，包含可能尚未由应用启用的外设；这不是完整硬件接线图。

| MCU 引脚 | 配置信号 |
| --- | --- |
| `PA10` | `USART1_RX` |
| `PA9` | `USART1_TX` |
| `PB6` | `I2C1_SCL` |
| `PB7` | `I2C1_SDA` |

## 本工程说明

与 CubeIDE/depthf4 区分：本版本包含 Master_Transmit_DMA/Master_Receive_DMA 及其完成/错误回调。

## 打开与构建

[公共构建与导入指南](../../docs/BUILDING.md) · [全部工程与外设索引](../../README.md)

本次整理的验证范围见[验证记录](../../docs/VALIDATION.md)。
