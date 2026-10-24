# skywalker：Skywalker 电调 PWM 实验

使用 TIM3 输出油门 PWM，并通过串口报告启动、短时运行和停止状态。

| 项目 | 内容 |
| --- | --- |
| MCU | `STM32F103C8T6` |
| 构建入口 | CMake |
| 学习内容 | TIM3、PWM、ESC、UART |
| 代码入口 | [`Core/Src/main.c`](Core/Src/main.c) |
| 来源 | `STM32/skywalker` |

## 建议阅读顺序

1. [`Core/Src/main.c`](Core/Src/main.c)：初始化、主循环和中断回调。

外设和引脚配置见 [`skywalker.ioc`](skywalker.ioc)；实际启用行为以源码为准。

## CubeMX 配置信号索引

下面列出 `.ioc` 中的配置，包含可能尚未由应用启用的外设；这不是完整硬件接线图。

| MCU 引脚 | 配置信号 |
| --- | --- |
| `PA6` | `S_TIM3_CH1` |
| `PA2` | `USART2_TX` |
| `PA3` | `USART2_RX` |
| `PA7` | `GPIO_Output` |

## 本工程说明

上电流程包含自动短时运行；实测前阅读原接线与排障说明中的适配电调、电源及负载条件。

## 原有学习资料

- [README_接线与排障.md](README_接线与排障.md)
- [电机.md](source/电机.md)

## 打开与构建

[公共构建与导入指南](../../docs/BUILDING.md) · [全部工程与外设索引](../../README.md)

本次整理的验证范围见[验证记录](../../docs/VALIDATION.md)。
