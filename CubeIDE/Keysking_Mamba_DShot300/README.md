# Keysking_Mamba_DShot300：F103 双路 DShot300

使用 GPIO/DWT 发送两路 DShot300，按键控制并输出串口状态。

| 项目 | 内容 |
| --- | --- |
| MCU | `STM32F103C8T6` |
| 构建入口 | STM32CubeIDE |
| 学习内容 | DShot300、GPIO、DWT、按键、UART |
| 代码入口 | [`Core/Src/main.c`](Core/Src/main.c) |
| 来源 | `STM32/Keysking_Mamba_DShot300` |

## 建议阅读顺序

1. [`Core/Src/main.c`](Core/Src/main.c)：初始化、主循环和中断回调。
2. [`Core/Inc/dshot.h`](Core/Inc/dshot.h)
3. [`Core/Src/dshot.c`](Core/Src/dshot.c)

外设和引脚配置见 [`Keysking_Mamba_DShot300.ioc`](Keysking_Mamba_DShot300.ioc)；实际启用行为以源码为准。

## CubeMX 配置信号索引

下面列出 `.ioc` 中的配置，包含可能尚未由应用启用的外设；这不是完整硬件接线图。

| MCU 引脚 | 配置信号 |
| --- | --- |
| `PA2` | `USART2_TX` |
| `PA3` | `USART2_RX` |
| `PA7` | `GPIO_Output` |
| `PA12` | `GPIO_Output` |
| `PA15` | `GPIO_Output` |
| `PB12` | `GPIO_Input` |
| `PB13` | `GPIO_Input` |

## 本工程说明

保留原接线与烧录指南；没有重复导入 teammate_Mamba_DShot300 中相同的应用源码。

## 原有学习资料

- [README_接线烧录指南.md](README_接线烧录指南.md)

## 打开与构建

[公共构建与导入指南](../../docs/BUILDING.md) · [全部工程与外设索引](../../README.md)

本次整理的验证范围见[验证记录](../../docs/VALIDATION.md)。
