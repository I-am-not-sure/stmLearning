# depthf4_soft：F407 软件 I²C 深度采集

用软件 I²C 实现 MS5837 参考压力与深度采集流程。

| 项目 | 内容 |
| --- | --- |
| MCU | `STM32F407ZGT6` |
| 构建入口 | STM32CubeIDE |
| 学习内容 | 软件 I²C、MS5837、深度换算、UART |
| 代码入口 | [`Core/Src/main.c`](Core/Src/main.c) |
| 来源 | `STM32/depthf4_soft` |

## 建议阅读顺序

1. [`Core/Src/main.c`](Core/Src/main.c)：初始化、主循环和中断回调。
2. [`Core/Inc/ms5837.h`](Core/Inc/ms5837.h)
3. [`Core/Inc/soft_i2c.h`](Core/Inc/soft_i2c.h)
4. [`Core/Src/ms5837.c`](Core/Src/ms5837.c)
5. [`Core/Src/soft_i2c.c`](Core/Src/soft_i2c.c)

外设和引脚配置见 [`depthf4_soft.ioc`](depthf4_soft.ioc)；实际启用行为以源码为准。

## CubeMX 配置信号索引

下面列出 `.ioc` 中的配置，包含可能尚未由应用启用的外设；这不是完整硬件接线图。

| MCU 引脚 | 配置信号 |
| --- | --- |
| `PA10` | `USART1_RX` |
| `PA9` | `USART1_TX` |

## 本工程说明

与 CubeIDE/depthf4 对照软件/硬件 I²C 实现。

## 打开与构建

[公共构建与导入指南](../../docs/BUILDING.md) · [全部工程与外设索引](../../README.md)

本次整理的验证范围见[验证记录](../../docs/VALIDATION.md)。
