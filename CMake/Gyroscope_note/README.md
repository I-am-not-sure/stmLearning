# Gyroscope_note：JY61P 软件 I²C 阅读版

把软件 I²C、JY61P 解码和时钟初始化分模块，保留逐步阅读指南。

| 项目 | 内容 |
| --- | --- |
| MCU | `STM32F103C8T6` |
| 构建入口 | CMake |
| 学习内容 | 软件 I²C、JY61P、USART1、模块化 |
| 代码入口 | [`Core/Src/main.c`](Core/Src/main.c) |
| 来源 | `STM32C/Gyroscope_note` |

## 建议阅读顺序

1. [`Core/Src/main.c`](Core/Src/main.c)：初始化、主循环和中断回调。
2. [`Core/Inc/clock.h`](Core/Inc/clock.h)
3. [`Core/Inc/jy61p.h`](Core/Inc/jy61p.h)
4. [`Core/Inc/soft_i2c.h`](Core/Inc/soft_i2c.h)
5. [`Core/Src/clock.c`](Core/Src/clock.c)
6. [`Core/Src/jy61p.c`](Core/Src/jy61p.c)
7. [`Core/Src/soft_i2c.c`](Core/Src/soft_i2c.c)

本目录没有 `.ioc`；引脚、时钟和外设初始化直接维护在源码中。

## 本工程说明

该版本没有 .ioc；CMake、启动文件、链接脚本和 HAL/CMSIS 均保留。

## 原有学习资料

- [阅读指南.md](阅读指南.md)

## 打开与构建

[公共构建与导入指南](../../docs/BUILDING.md) · [全部工程与外设索引](../../README.md)

本次整理的验证范围见[验证记录](../../docs/VALIDATION.md)。
