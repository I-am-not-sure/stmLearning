# STM32 外设学习工程

按构建入口整理的 STM32F103 / STM32F407 学习记录。每个子目录是一个独立工程，保留自己的 HAL/CMSIS、启动文件和链接脚本。

当前共 **36 个工程：21 个 CMake、15 个 STM32CubeIDE**。其中本轮新增 20 个；原有 16 个工程已迁移分类。

```text
stmLearning/
├── CMake/       # 以 CMakeLists.txt 为构建入口
├── CubeIDE/     # 以 .project / .cproject 导入 CubeIDE
└── docs/        # 构建、筛选和验证记录
```

- [打开与构建](docs/BUILDING.md)
- [项目筛选记录](docs/SELECTION.md)
- [本次验证范围](docs/VALIDATION.md)
- [MS5837-02BA 数据手册要点](docs/MS5837-02BA_数据手册关键内容总结.md)

分类以实际工程文件为准：`.ioc` 只是 CubeMX 配置，不代表一定是 CubeIDE 工程。`CMake/stm32_hal_aht20` 同时保留两种入口，按 CMake 主入口归类。

## 从外设开始找工程

| 主题 | 推荐起点 |
| --- | --- |
| GPIO / 按键 / EXTI | [点灯](CubeIDE/blink/README.md)、[消抖](CubeIDE/K/README.md)、[外部中断](CubeIDE/interrupt/README.md) |
| 定时器 | [更新中断](CMake/TIM/README.md)、[外部计数](CMake/TIM_ExternalInput/README.md)、[输入捕获测距](CMake/TIM_InputCapture/README.md) |
| UART / DMA | [串口基础](CubeIDE/serial/README.md)、[F407 DMA 回显](CMake/line_serial/README.md) |
| AHT20 / OLED | [AHT20](CMake/stm32_hal_aht20/README.md)、[OLED 绘图](CMake/oled/README.md)、[温湿度计](CMake/hygro_thermometer_/README.md)、[小恐龙](CubeIDE/I2C_OLED_Dino/README.md) |
| MS5837 / I²C | [F103 软件 I²C](CMake/depth/README.md)、[F407 硬件 I²C](CubeIDE/depthf4/README.md)、[I²C DMA](CMake/depthf4/README.md) |
| JY61P / I²C | [软件 I²C 阅读版](CMake/Gyroscope_note/README.md)、[硬件 I²C 对照](CMake/Gyroscope_hard/README.md)、[I²C DMA](CMake/JY6P/README.md) |
| PWM / 电机 / 编码器 | [调光](CubeIDE/pwm_light/README.md)、[舵机](CubeIDE/STM_Servo/README.md)、[DRV8833](CMake/TIM_Motor/README.md)、[四路 PWM](CMake/line_mamba_pwm/README.md) |
| DShot | [F103 双路](CubeIDE/Keysking_Mamba_DShot300/README.md)、[TIM3 四路](CMake/line_mamba/README.md)、[GPIO/DWT 四路](CMake/line_929/README.md) |
| 多外设组合 | [MS5837 + JY61P 异步采集](CMake/all_device/README.md) |

## CMake 工程

| 工程 | MCU | 外设 / 学习主题 | 实际用途 |
| --- | --- | --- | --- |
| [all_device](CMake/all_device/README.md)（新增） | STM32F407ZGT6 | MS5837、JY61P、I²C DMA、UART DMA、状态机 | 组合 MS5837 与 JY61P 状态机，进行姿态数学处理和串口 DMA 输出。 |
| [depth](CMake/depth/README.md) | STM32F103C8T6 | 软件 I²C、MS5837、UART | 通过软件 I²C 读取 MS5837 的 PROM 和测量数据，并串口输出。 |
| [depthf4](CMake/depthf4/README.md)（新增） | STM32F407ZGT6 | I²C DMA、MS5837、UART DMA、状态机 | 使用 I²C DMA 回调推进 MS5837 采样，并通过串口 DMA 输出数据。 |
| [Gyroscope_fast](CMake/Gyroscope_fast/README.md)（新增） | STM32F103C8T6 | 软件 I²C、JY61P、时序测量、UART | 以软件 I²C 读取 JY61P，保留读取时序和吞吐量对比记录。 |
| [Gyroscope_hard](CMake/Gyroscope_hard/README.md)（新增） | STM32F103C8T6 | 硬件 I²C、JY61P、总线恢复、UART | 用 I2C1 读取 JY61P，包含线路诊断、BUSY 恢复和错误输出。 |
| [Gyroscope_note](CMake/Gyroscope_note/README.md)（新增） | STM32F103C8T6 | 软件 I²C、JY61P、USART1、模块化 | 把软件 I²C、JY61P 解码和时钟初始化分模块，保留逐步阅读指南。 |
| [hygro_thermometer_](CMake/hygro_thermometer_/README.md) | STM32F103C8T6 | I²C、AHT20、OLED | 把 AHT20 采样与 OLED 图文显示组合成温湿度计。 |
| [JY6P](CMake/JY6P/README.md)（新增） | STM32F407ZGT6 | I²C DMA、JY61P、UART DMA、状态机 | 通过 I²C Mem_Read_DMA 采样 JY61P，结合串口 DMA 与错误恢复。 |
| [line_929](CMake/line_929/README.md)（新增） | STM32F407ZGT6 | DShot300、GPIO、DWT、UART、协议解析 | 用 GPIO/DWT 生成 DShot300 帧，配合串口命令和协议测试。 |
| [line_ide_cmake](CMake/line_ide_cmake/README.md)（新增） | STM32F407ZGT6 | UART DMA、命令解析、TIM1/TIM8、PWM | 接收串口空闲 DMA 数据，解析命令并设置 TIM1/TIM8 的 PWM 比较值。 |
| [line_mamba](CMake/line_mamba/README.md)（新增） | STM32F407ZGT6 | DShot300、TIM3、UART、命令协议 | 用 TIM3 产生 DShot 位脉冲，解析串口命令并控制四路电调。 |
| [line_mamba_pwm](CMake/line_mamba_pwm/README.md)（新增） | STM32F407ZGT6 | PWM、TIM3、UART、命令协议 | 使用 TIM3 输出传统 PWM，以串口命令调整四路电调指令。 |
| [line_serial](CMake/line_serial/README.md)（新增） | STM32F407ZGT6 | UART、DMA、Receive-to-idle、回调 | 通过 USART1 的空闲接收和 DMA 发送实现数据回显与接收重启。 |
| [ms_soft](CMake/ms_soft/README.md)（新增） | STM32F407ZGT6 | 软件 I²C、MS5837、UART | 分离软件 I²C 与 MS5837 驱动，周期采集后经串口报告。 |
| [oled](CMake/oled/README.md) | STM32F103C8T6 | I²C、OLED、帧缓冲、字库 | 使用帧缓冲、字体、图形和文字接口驱动 OLED。 |
| [skywalker](CMake/skywalker/README.md)（新增） | STM32F103C8T6 | TIM3、PWM、ESC、UART | 使用 TIM3 输出油门 PWM，并通过串口报告启动、短时运行和停止状态。 |
| [stm32_hal_aht20](CMake/stm32_hal_aht20/README.md) | STM32F103C8T6 | I²C、AHT20、UART | 初始化、触发测量并读取 AHT20 温湿度，串口输出结果。 |
| [TIM](CMake/TIM/README.md) | STM32F103C8T6 | TIM、更新中断、UART | 启动定时器更新中断，在回调中配合串口观察计数行为。 |
| [TIM_ExternalInput](CMake/TIM_ExternalInput/README.md) | STM32F103C8T6 | TIM、外部时钟/计数、OLED | 配置外部输入计数，并在 OLED 上显示计数结果。 |
| [TIM_InputCapture](CMake/TIM_InputCapture/README.md)（新增） | STM32F103C8T6 | 输入捕获、超声波测距、GPIO、OLED | 产生 Trig 脉冲，使用 TIM1 双通道捕获 Echo 边沿，计算距离并显示到 OLED。 |
| [TIM_Motor](CMake/TIM_Motor/README.md) | STM32F103C8T6 | DRV8833、GPIO、TIM 编码器 | 使用 DRV8833 驱动电机，启动编码器接口并读取计数。 |

## STM32CubeIDE 工程

| 工程 | MCU | 外设 / 学习主题 | 实际用途 |
| --- | --- | --- | --- |
| [blink](CubeIDE/blink/README.md) | STM32F103C8T6 | GPIO、HAL 延时 | 通过 GPIO 输出和延时控制 LED。 |
| [depthf1](CubeIDE/depthf1/README.md)（新增） | STM32F103C8T6 | 硬件 I²C、MS5837、PROM/CRC、UART | 读取 MS5837 的 PROM/CRC 和深度测量，串口输出初始化与错误状态。 |
| [depthf4](CubeIDE/depthf4/README.md)（新增） | STM32F407ZGT6 | 硬件 I²C、MS5837、深度换算、UART | 读取 MS5837，采集参考压力并换算深度，通过串口输出。 |
| [depthf4_soft](CubeIDE/depthf4_soft/README.md)（新增） | STM32F407ZGT6 | 软件 I²C、MS5837、深度换算、UART | 用软件 I²C 实现 MS5837 参考压力与深度采集流程。 |
| [I2C_OLED_Dino](CubeIDE/I2C_OLED_Dino/README.md)（新增） | STM32F103C8T6 | I²C、OLED、按键、游戏状态 | 用 OLED 帧缓冲、字库和按键实现小恐龙游戏交互。 |
| [iic](CubeIDE/iic/README.md) | STM32F103C8T6 | I²C、AHT20、异步采集、UART | 通过 AHT20 驱动采集温湿度，并通过串口输出。 |
| [interrupt](CubeIDE/interrupt/README.md) | STM32F103C8T6 | GPIO、EXTI | 使用按键/外部中断配合 LED 输出，比较主循环与中断响应。 |
| [JY61P](CubeIDE/JY61P/README.md) | STM32F407ZGT6 | I²C、JY61P、UART、错误恢复 | 通过 I²C 读取 JY61P 并报告数据，保留总线恢复和错误诊断逻辑。 |
| [K](CubeIDE/K/README.md) | STM32F103C8T6 | GPIO 输入/输出、按键消抖 | 轮询按键并进行软件消抖，翻转输出电平。 |
| [Keysking_Mamba_DShot300](CubeIDE/Keysking_Mamba_DShot300/README.md)（新增） | STM32F103C8T6 | DShot300、GPIO、DWT、按键、UART | 使用 GPIO/DWT 发送两路 DShot300，按键控制并输出串口状态。 |
| [line_serial](CubeIDE/line_serial/README.md)（新增） | STM32F407ZGT6 | UART、DMA、Receive-to-idle、回调 | 通过 USART1 的空闲接收和 DMA 发送实现数据回显与接收重启。 |
| [pwm_light](CubeIDE/pwm_light/README.md) | STM32F103C8T6 | 定时器、PWM、按键 | 启动定时器 PWM，通过按键和比较值调整输出。 |
| [serial](CubeIDE/serial/README.md) | STM32F103C8T6 | UART、DMA、中断、空闲接收 | 包含串口轮询、中断、DMA 与 Receive-to-idle 接收的学习代码。 |
| [stm32_hal_drv8833](CubeIDE/stm32_hal_drv8833/README.md) | STM32F103C8T6 | DRV8833、GPIO、TIM 编码器 | 封装 DRV8833 的前进、后退接口，并读取编码器计数。 |
| [STM_Servo](CubeIDE/STM_Servo/README.md) | STM32F103C8T6 | UART、PWM、舵机、编码器实验 | 接收串口角度输入，进行范围限制并更新 PWM 比较值。 |

## 使用范围

这些是学习与调试工程，目录归档不等同于统一完成硬件验收。传感器地址、串口、定时器时钟和电调协议均需按各工程配置确认；原有调试状态在项目 README 中保留。

每个工程单独编译和烧录。仓库根目录没有聚合固件目标，不应把全部 `main.c` 链接进同一程序。

第三方 HAL/CMSIS 与其他驱动保留原许可文件和版权声明；仓库没有为这些第三方文件重新指定许可证。
