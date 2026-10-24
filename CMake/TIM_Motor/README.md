# TIM_Motor：电机驱动与编码器

使用 DRV8833 驱动电机，启动编码器接口并读取计数。

| 项目 | 内容 |
| --- | --- |
| MCU | `STM32F103C8T6` |
| 构建入口 | CMake |
| 学习内容 | DRV8833、GPIO、TIM 编码器 |
| 代码入口 | [`Core/Src/main.c`](Core/Src/main.c) |
| 来源 | `stmLearning/TIM_Motor` |

## 建议阅读顺序

1. [`Core/Src/main.c`](Core/Src/main.c)：初始化、主循环和中断回调。
2. [`Core/Inc/drv8833.h`](Core/Inc/drv8833.h)
3. [`Core/Src/drv8833.c`](Core/Src/drv8833.c)

外设和引脚配置见 [`TIM_Motor.ioc`](TIM_Motor.ioc)；实际启用行为以源码为准。

## CubeMX 配置信号索引

下面列出 `.ioc` 中的配置，包含可能尚未由应用启用的外设；这不是完整硬件接线图。

| MCU 引脚 | 配置信号 |
| --- | --- |
| `PA0-WKUP` | `S_TIM2_CH1_ETR` |
| `PA1` | `S_TIM2_CH2` |
| `PA8` | `S_TIM1_CH1` |
| `PA9` | `S_TIM1_CH2` |

## 本工程说明

与 CubeIDE/stm32_hal_drv8833 形成两种构建入口的对照。

## 打开与构建

[公共构建与导入指南](../../docs/BUILDING.md) · [全部工程与外设索引](../../README.md)

本次整理的验证范围见[验证记录](../../docs/VALIDATION.md)。
