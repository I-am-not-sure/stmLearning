# all_device：双传感器异步采集

组合 MS5837 与 JY61P 状态机，进行姿态数学处理和串口 DMA 输出。

| 项目 | 内容 |
| --- | --- |
| MCU | `STM32F407ZGT6` |
| 构建入口 | CMake |
| 学习内容 | MS5837、JY61P、I²C DMA、UART DMA、状态机 |
| 代码入口 | [`Core/Src/main.c`](Core/Src/main.c) |
| 来源 | `STM32C/cmake_enemy/all_device` |

## 建议阅读顺序

1. [`Core/Src/main.c`](Core/Src/main.c)：初始化、主循环和中断回调。
2. [`Core/Inc/attitude_math.h`](Core/Inc/attitude_math.h)
3. [`Core/Inc/device_app.h`](Core/Inc/device_app.h)
4. [`Core/Inc/jy61p.h`](Core/Inc/jy61p.h)
5. [`Core/Inc/ms5837.h`](Core/Inc/ms5837.h)
6. [`Core/Src/attitude_math.c`](Core/Src/attitude_math.c)
7. [`Core/Src/device_app.c`](Core/Src/device_app.c)
8. [`Core/Src/jy61p.c`](Core/Src/jy61p.c)
9. [`Core/Src/ms5837.c`](Core/Src/ms5837.c)

外设和引脚配置见 [`all_device.ioc`](all_device.ioc)；实际启用行为以源码为准。

## CubeMX 配置信号索引

下面列出 `.ioc` 中的配置，包含可能尚未由应用启用的外设；这不是完整硬件接线图。

| MCU 引脚 | 配置信号 |
| --- | --- |
| `PA10` | `USART1_RX` |
| `PA9` | `USART1_TX` |
| `PB6` | `I2C1_SCL` |
| `PB7` | `I2C1_SDA` |
| `PC6` | `S_TIM8_CH1` |
| `PC7` | `S_TIM8_CH2` |
| `PC8` | `S_TIM8_CH3` |
| `PC9` | `S_TIM8_CH4` |
| `PE11` | `S_TIM1_CH2` |
| `PE13` | `S_TIM1_CH3` |
| `PE14` | `S_TIM1_CH4` |
| `PE9` | `S_TIM1_CH1` |
| `PF0` | `I2C2_SDA` |
| `PF1` | `I2C2_SCL` |

## 本工程说明

初始化代码会启动 TIM1/TIM8 PWM 并写比较值；它并非只读传感器日志的程序。

## 原有学习资料

- [ATTITUDE.md](ATTITUDE.md)
- [STATE_MACHINE_REVIEW.md](STATE_MACHINE_REVIEW.md)
- [README.md](tests/ms5837_continuous/README.md)

## 打开与构建

[公共构建与导入指南](../../docs/BUILDING.md) · [全部工程与外设索引](../../README.md)

本次整理的验证范围见[验证记录](../../docs/VALIDATION.md)。
