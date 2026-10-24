# stm32_hal_aht20：AHT20 基础驱动

初始化、触发测量并读取 AHT20 温湿度，串口输出结果。

| 项目 | 内容 |
| --- | --- |
| MCU | `STM32F103C8T6` |
| 构建入口 | CMake；同时保留 CubeIDE 元数据 |
| 学习内容 | I²C、AHT20、UART |
| 代码入口 | [`Core/Src/main.c`](Core/Src/main.c) |
| 来源 | `stmLearning/stm32_hal_aht20` |

## 建议阅读顺序

1. [`Core/Src/main.c`](Core/Src/main.c)：初始化、主循环和中断回调。
2. [`Core/Inc/aht20.h`](Core/Inc/aht20.h)
3. [`Core/Src/aht20.c`](Core/Src/aht20.c)

外设和引脚配置见 [`stm32_hal_aht20.ioc`](stm32_hal_aht20.ioc)；实际启用行为以源码为准。

## CubeMX 配置信号索引

下面列出 `.ioc` 中的配置，包含可能尚未由应用启用的外设；这不是完整硬件接线图。

| MCU 引脚 | 配置信号 |
| --- | --- |
| `PA2` | `USART2_TX` |
| `PA3` | `USART2_RX` |
| `PB6` | `I2C1_SCL` |
| `PB7` | `I2C1_SDA` |

## 本工程说明

同时保留 CMakeLists.txt 和 CubeIDE 的 .project/.cproject，按 CMake 主入口分类，也可用 CubeIDE 导入。CMake 预设使用小写：`cmake --preset debug`、`cmake --build --preset debug`，输出在 `build/debug/build/`。

原 CMake 源文件清单引用了两个不存在的 TIM 驱动文件；由于 `HAL_TIM_MODULE_ENABLED` 原本未启用，整理时只移除了这两条无效构建引用，未改变温湿度采集程序。

## 打开与构建

[公共构建与导入指南](../../docs/BUILDING.md) · [全部工程与外设索引](../../README.md)

本次整理的验证范围见[验证记录](../../docs/VALIDATION.md)。
