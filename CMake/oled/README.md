# oled：OLED 图形与文字

使用帧缓冲、字体、图形和文字接口驱动 OLED。

| 项目 | 内容 |
| --- | --- |
| MCU | `STM32F103C8T6` |
| 构建入口 | CMake |
| 学习内容 | I²C、OLED、帧缓冲、字库 |
| 代码入口 | [`Core/Src/main.c`](Core/Src/main.c) |
| 来源 | `stmLearning/oled` |

## 建议阅读顺序

1. [`Core/Src/main.c`](Core/Src/main.c)：初始化、主循环和中断回调。
2. [`Core/Inc/font.h`](Core/Inc/font.h)
3. [`Core/Inc/oled.h`](Core/Inc/oled.h)
4. [`Core/Src/font.c`](Core/Src/font.c)
5. [`Core/Src/oled.c`](Core/Src/oled.c)

外设和引脚配置见 [`oled.ioc`](oled.ioc)；实际启用行为以源码为准。

## CubeMX 配置信号索引

下面列出 `.ioc` 中的配置，包含可能尚未由应用启用的外设；这不是完整硬件接线图。

| MCU 引脚 | 配置信号 |
| --- | --- |
| `PB6` | `I2C1_SCL` |
| `PB7` | `I2C1_SDA` |

## 打开与构建

[公共构建与导入指南](../../docs/BUILDING.md) · [全部工程与外设索引](../../README.md)

本次整理的验证范围见[验证记录](../../docs/VALIDATION.md)。
