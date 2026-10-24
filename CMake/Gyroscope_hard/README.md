# Gyroscope_hard：JY61P 硬件 I²C 对照

用 I2C1 读取 JY61P，包含线路诊断、BUSY 恢复和错误输出。

| 项目 | 内容 |
| --- | --- |
| MCU | `STM32F103C8T6` |
| 构建入口 | CMake |
| 学习内容 | 硬件 I²C、JY61P、总线恢复、UART |
| 代码入口 | [`Core/Src/main.c`](Core/Src/main.c) |
| 来源 | `STM32C/Gyroscope_hard` |

## 建议阅读顺序

1. [`Core/Src/main.c`](Core/Src/main.c)：初始化、主循环和中断回调。

本目录没有 `.ioc`；引脚、时钟和外设初始化直接维护在源码中。

## 本工程说明

原记录明确存在上拉与硬件读数验证事项，保留为对照/诊断实验，不宣称已完成当前硬件验证。该版本没有 .ioc。

## 打开与构建

[公共构建与导入指南](../../docs/BUILDING.md) · [全部工程与外设索引](../../README.md)

本次整理的验证范围见[验证记录](../../docs/VALIDATION.md)。

---

## 整理前的项目说明

原有说明保留如下。文中的固件产物、实测记录和本机路径属于原记录；本仓库发布源码，编译输出需自行生成。

# Gyroscope_hard：JY61P 硬件 I2C1 实验

这是独立的 STM32F103C8T6 工程。PB6 为 I2C1_SCL，PB7 为 I2C1_SDA，PA9 为 USART1_TX。它与 `Gyroscope_note` 一样，每秒从七位地址 `0x50` 读取 `0x34` 起 12 字节、`0x3D` 起 6 字节，按低字节在前的有符号 16 位格式换算加速度、角速度和 Roll/Pitch/Yaw，并通过 USART1 以相同成功格式输出。硬件 I2C 使用 HAL 的 `HAL_I2C_Mem_Read`；传入 HAL 的地址为 `0x50 << 1`，存储地址长度为 8 位。读取失败时输出 HAL 错误码、外设寄存器和 PB6/PB7 实际电平，再尝试恢复总线。

## 本次实测结论

**当前接线下硬件 I2C1 不能稳定读取 JY61P。** 这不是编译或烧录问题：工程已用 GNU Arm GCC 编译，STM32CubeProgrammer 经 ST-LINK 烧录并校验成功；烧录前 COM10 能连续看到软件版的有效九项读数。硬件版在 **100 kHz** 和降速的 **10 kHz** 下连续报告 `HAL_I2C_ERROR_TIMEOUT (0x00000200)`，没有一帧成功。故障快照中 SDA 有时被读为低，`SR2` 有时包含 BUSY。

一次性开漏上升检查给出了直接线索：分别把 PB6/PB7 拉低后释放，在 **2 ms 测量期限内两根线都未上升到高电平**；切换到 STM32 的输入弱上拉后，两根线均读到高电平。串口结果为：

```text
I2C pullup test: OD rise SCL=2008us SDA=2008us (2000=timeout), input pullup SCL=1 SDA=1
```

这里的 `2008us` 是超时后的计数值，**不是**测得的真实上升时间。两根线在复用开漏模式下缺少有效上拉，是本次无法形成正常硬件 I2C 波形的直接原因。软件版通过在“输入弱上拉”和“开漏拉低”之间切换来运行；STM32F1 的硬件 I2C 必须把引脚设为复用开漏，不能同时使用该输入模式的弱上拉。这与 [ST RM0008 的 GPIO 模式表](https://www.st.com/resource/en/reference_manual/cd00171190-stm32f101-103-105-107-stm32f100-series-armbased-32bit-mcus-stmicroelectronics.pdf)相符。板上是否装有电阻不能仅凭软件确定；测量能确定的是现有接线**没有形成有效上拉**。

下一次硬件验证，需要确认 PB6/SCL 和 PB7/SDA **各自**通过合适电阻上拉到 **3.3V**；短线标准模式可从每路约 **4.7 kΩ** 开始，若模块或开发板已经有合适且连通的上拉，不必重复加。然后烧录本工程 `firmware/Gyroscope_hard.elf`，在 COM10 以 115200、8N1 观察是否出现 `ACC(...) GYRO(...) ANGLE(...)` 连续读数。只验证“空闲时为高”仍不够，还应观察启动时的一次性 `OD rise` 检查和连续读取结果。4.7 kΩ 是 [ST I2C 入门资料](https://wiki.st.com/stm32mcu/wiki/Getting_started_with_I2C)列出的常用值，具体仍受连线电容和模块电路影响。

STM32F103 还有 [ES096 第 2.8.7 节 I2C BUSY 锁定勘误](https://www.st.com/resource/en/errata_sheet/es096-stm32f101x8b-stm32f102x8b-and-stm32f103x8b-mediumdensity-device-limitations-stmicroelectronics.pdf)。本工程保留归档版的最多 9 个恢复时钟、STOP、SWRST 和重新初始化逻辑；它无法替代上拉。若补足上拉之后仍失败，再检查波形和这个勘误，不能把本次超时直接全归因于勘误。

**试验结束后，已把板上 Flash 从试验前备份恢复，串口再次验证软件版读数。** 原板上备份保存在 `firmware/before_test.bin`；该文件是实际板子的 64KB Flash 快照，既便于复原，也不参与本工程编译。

## 代码入口与构建

| 文件 | 用途 |
| --- | --- |
| [main.c](Core/Src/main.c) | 保持原读取位置、换算公式、成功输出与一秒周期；失败时输出寄存器快照 |
| [i2c.c](Core/Src/i2c.c) | I2C1 初始化、PB6/PB7 复用开漏、总线恢复、开漏上升检查 |
| [gpio.c](Core/Src/gpio.c) | 开启端口时钟；不再将 I2C 引脚配为输入上拉 |
| [stm32f1xx_hal_conf.h](Core/Inc/stm32f1xx_hal_conf.h) | 启用硬件 I2C HAL |
| [CMakeLists.txt](CMakeLists.txt) | 编译独立工程、输出 ELF/HEX/BIN |

在已有 CMake、Ninja 和 GNU Arm 工具链的终端里，从本目录执行：

```powershell
cmake --preset Debug
cmake --build --preset Debug
```

输出在 `build/Debug/Gyroscope_hard.elf`、`.hex` 和 `.bin`。ELF/HEX 含烧录地址；裸 BIN 的烧录起点是 `0x08000000`。本次交付的 `firmware` 内保留了最终 **100 kHz** 编译产物；10 kHz 仅用于诊断，未作为交付配置。连接 ST-LINK 后可用 STM32CubeProgrammer 选择 ELF 下载、校验并复位。该目录自带所需 HAL/CMSIS、启动文件、链接脚本，不依赖旁边的 `Gyroscope` 或 `Gyroscope_note` 源目录。
