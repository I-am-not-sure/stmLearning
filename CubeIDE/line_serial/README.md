# line_serial：F407 串口 DMA 回显

通过 USART1 的空闲接收和 DMA 发送实现数据回显与接收重启。

| 项目 | 内容 |
| --- | --- |
| MCU | `STM32F407ZGT6` |
| 构建入口 | STM32CubeIDE |
| 学习内容 | UART、DMA、Receive-to-idle、回调 |
| 代码入口 | [`Core/Src/main.c`](Core/Src/main.c) |
| 来源 | `STM32/line_serial` |

## 建议阅读顺序

1. [`Core/Src/main.c`](Core/Src/main.c)：初始化、主循环和中断回调。

外设和引脚配置见 [`line_serial.ioc`](line_serial.ioc)；实际启用行为以源码为准。

## CubeMX 配置信号索引

下面列出 `.ioc` 中的配置，包含可能尚未由应用启用的外设；这不是完整硬件接线图。

| MCU 引脚 | 配置信号 |
| --- | --- |
| `PA10` | `USART1_RX` |
| `PA9` | `USART1_TX` |

## 本工程说明

应用源码与 CMake/line_serial 相同，分别保留各自工程构建入口。

## 打开与构建

[公共构建与导入指南](../../docs/BUILDING.md) · [全部工程与外设索引](../../README.md)

本次整理的验证范围见[验证记录](../../docs/VALIDATION.md)。
