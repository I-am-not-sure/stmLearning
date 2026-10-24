# TIM_InputCapture：超声波输入捕获测距

产生 Trig 脉冲，使用 TIM1 双通道捕获 Echo 边沿，计算距离并显示到 OLED。

| 项目 | 内容 |
| --- | --- |
| MCU | `STM32F103C8T6` |
| 构建入口 | CMake |
| 学习内容 | 输入捕获、超声波测距、GPIO、OLED |
| 代码入口 | [`Core/Src/main.c`](Core/Src/main.c) |
| 来源 | `STM32C/TIM_InputCapture` |

## 建议阅读顺序

1. [`Core/Src/main.c`](Core/Src/main.c)：初始化、主循环和中断回调。
2. [`Core/Inc/font.h`](Core/Inc/font.h)
3. [`Core/Inc/oled.h`](Core/Inc/oled.h)
4. [`Core/Src/font.c`](Core/Src/font.c)
5. [`Core/Src/oled.c`](Core/Src/oled.c)

外设和引脚配置见 [`TIM_InputCapture.ioc`](TIM_InputCapture.ioc)；实际启用行为以源码为准。

## CubeMX 配置信号索引

下面列出 `.ioc` 中的配置，包含可能尚未由应用启用的外设；这不是完整硬件接线图。

| MCU 引脚 | 配置信号 |
| --- | --- |
| `PA10` | `S_TIM1_CH3` |
| `PA11` | `GPIO_Output` |
| `PB6` | `I2C1_SCL` |
| `PB7` | `I2C1_SDA` |

## 本工程说明

距离表达式位于 HAL_TIM_IC_CaptureCallback；数值换算依赖定时器计数分辨率，不能直接套用到不同定时器配置。

## 打开与构建

[公共构建与导入指南](../../docs/BUILDING.md) · [全部工程与外设索引](../../README.md)

本次整理的验证范围见[验证记录](../../docs/VALIDATION.md)。
