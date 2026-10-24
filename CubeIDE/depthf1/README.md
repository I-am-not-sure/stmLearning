# depthf1：F103 硬件 I²C 深度诊断

读取 MS5837 的 PROM/CRC 和深度测量，串口输出初始化与错误状态。

| 项目 | 内容 |
| --- | --- |
| MCU | `STM32F103C8T6` |
| 构建入口 | STM32CubeIDE |
| 学习内容 | 硬件 I²C、MS5837、PROM/CRC、UART |
| 代码入口 | [`Core/Src/main.c`](Core/Src/main.c) |
| 来源 | `STM32/depthf1` |

## 建议阅读顺序

1. [`Core/Src/main.c`](Core/Src/main.c)：初始化、主循环和中断回调。
2. [`Core/Inc/ms5837.h`](Core/Inc/ms5837.h)
3. [`Core/Src/ms5837.c`](Core/Src/ms5837.c)

外设和引脚配置见 [`depthf1.ioc`](depthf1.ioc)；实际启用行为以源码为准。

## CubeMX 配置信号索引

下面列出 `.ioc` 中的配置，包含可能尚未由应用启用的外设；这不是完整硬件接线图。

| MCU 引脚 | 配置信号 |
| --- | --- |
| `PA2` | `USART2_TX` |
| `PA3` | `USART2_RX` |
| `PB6` | `I2C1_SCL` |
| `PB7` | `I2C1_SDA` |

## 打开与构建

[公共构建与导入指南](../../docs/BUILDING.md) · [全部工程与外设索引](../../README.md)

本次整理的验证范围见[验证记录](../../docs/VALIDATION.md)。
