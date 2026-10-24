# JY6P：JY61P I²C DMA 状态机

通过 I²C Mem_Read_DMA 采样 JY61P，结合串口 DMA 与错误恢复。

| 项目 | 内容 |
| --- | --- |
| MCU | `STM32F407ZGT6` |
| 构建入口 | CMake |
| 学习内容 | I²C DMA、JY61P、UART DMA、状态机 |
| 代码入口 | [`Core/Src/main.c`](Core/Src/main.c) |
| 来源 | `STM32C/cmake_enemy/JY6P` |

## 建议阅读顺序

1. [`Core/Src/main.c`](Core/Src/main.c)：初始化、主循环和中断回调。
2. [`Core/Inc/app.h`](Core/Inc/app.h)
3. [`Core/Inc/jy61p.h`](Core/Inc/jy61p.h)
4. [`Core/Src/jy61p.c`](Core/Src/jy61p.c)

外设和引脚配置见 [`JY6P.ioc`](JY6P.ioc)；实际启用行为以源码为准。

## CubeMX 配置信号索引

下面列出 `.ioc` 中的配置，包含可能尚未由应用启用的外设；这不是完整硬件接线图。

| MCU 引脚 | 配置信号 |
| --- | --- |
| `PA10` | `USART1_RX` |
| `PA9` | `USART1_TX` |
| `PF0` | `I2C2_SDA` |
| `PF1` | `I2C2_SCL` |

## 本工程说明

目录名 JY6P 保留原名称；实际传感器接口和文件为 JY61P。

## 打开与构建

[公共构建与导入指南](../../docs/BUILDING.md) · [全部工程与外设索引](../../README.md)

本次整理的验证范围见[验证记录](../../docs/VALIDATION.md)。
