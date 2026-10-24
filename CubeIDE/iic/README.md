# iic：AHT20 I²C 采集

通过 AHT20 驱动采集温湿度，并通过串口输出。

| 项目 | 内容 |
| --- | --- |
| MCU | `STM32F103C8T6` |
| 构建入口 | STM32CubeIDE |
| 学习内容 | I²C、AHT20、异步采集、UART |
| 代码入口 | [`Core/Src/main.c`](Core/Src/main.c) |
| 来源 | `stmLearning/iic` |

## 建议阅读顺序

1. [`Core/Src/main.c`](Core/Src/main.c)：初始化、主循环和中断回调。
2. [`Core/Inc/aht20.h`](Core/Inc/aht20.h)
3. [`Core/Src/aht20.c`](Core/Src/aht20.c)

外设和引脚配置见 [`iic.ioc`](iic.ioc)；实际启用行为以源码为准。

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
