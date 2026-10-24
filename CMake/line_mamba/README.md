# line_mamba：TIM3 四路 DShot300

用 TIM3 产生 DShot 位脉冲，解析串口命令并控制四路电调。

| 项目 | 内容 |
| --- | --- |
| MCU | `STM32F407ZGT6` |
| 构建入口 | CMake |
| 学习内容 | DShot300、TIM3、UART、命令协议 |
| 代码入口 | [`Core/Src/main.c`](Core/Src/main.c) |
| 来源 | `STM32C/line_mamba` |

## 建议阅读顺序

1. [`Core/Src/main.c`](Core/Src/main.c)：初始化、主循环和中断回调。
2. [`Core/Inc/mamba_app.h`](Core/Inc/mamba_app.h)
3. [`Core/Inc/mamba_command.h`](Core/Inc/mamba_command.h)
4. [`Core/Inc/mamba_dshot.h`](Core/Inc/mamba_dshot.h)
5. [`Core/Src/mamba_app.c`](Core/Src/mamba_app.c)
6. [`Core/Src/mamba_command.c`](Core/Src/mamba_command.c)
7. [`Core/Src/mamba_dshot.c`](Core/Src/mamba_dshot.c)
8. [`Core/Src/mamba_dshot_codec.c`](Core/Src/mamba_dshot_codec.c)

外设和引脚配置见 [`line_mamba.ioc`](line_mamba.ioc)；实际启用行为以源码为准。

## CubeMX 配置信号索引

下面列出 `.ioc` 中的配置，包含可能尚未由应用启用的外设；这不是完整硬件接线图。

| MCU 引脚 | 配置信号 |
| --- | --- |
| `PA2` | `USART2_TX` |
| `PA3` | `USART2_RX` |
| `PA6` | `S_TIM3_CH1` |
| `PA7` | `S_TIM3_CH2` |
| `PA8` | `I2C3_SCL` |
| `PB0` | `S_TIM3_CH3` |
| `PB1` | `S_TIM3_CH4` |
| `PC12` | `UART5_TX` |
| `PC9` | `I2C3_SDA` |
| `PD2` | `UART5_RX` |
| `PE11` | `S_TIM1_CH2` |
| `PE13` | `S_TIM1_CH3` |
| `PE14` | `S_TIM1_CH4` |
| `PE9` | `S_TIM1_CH1` |

## 本工程说明

保留原 README 中电调模式、命令超时和接线说明。

## 打开与构建

[公共构建与导入指南](../../docs/BUILDING.md) · [全部工程与外设索引](../../README.md)

本次整理的验证范围见[验证记录](../../docs/VALIDATION.md)。

---

## 整理前的项目说明

原有说明保留如下。文中的固件产物、实测记录和本机路径属于原记录；本仓库发布源码，编译输出需自行生成。

# line_mamba：TIM3 控制 MAMBA F55_BLS 四合一电调

此工程从 `line` 复制，保留 STM32F407ZG、HAL 和 CMake 构建链。只有 TIM3 的四路输出和 USART2 用于本程序；TIM1、I2C3、UART5 虽仍在 CubeMX 基础配置中初始化，但本程序不启动它们的输出或读取它们的数据。

## 为什么选 DShot300

`Senior` 中的 PWM 是约 50 Hz、1000～2000 μs 的传统油门脉冲，依赖电调预先配置的双向模式才能表示反转。`one_one_failversion` 已记录 MAMBA 四合一电调通过 DShot300 临时开启 3D 模式后实现正反转，所以本工程直接沿用这条经过项目验证的路径。

**TIM3 的 PWM 模式在这里用于产生 DShot 每一位的脉冲，不是传统 1000～2000 μs 油门 PWM。** CPU 时钟为 168 MHz、TIM3 时钟为 84 MHz；一个 DShot300 位是 280 个 TIM3 tick（约 3.33 μs）。高电平 105 tick 表示 0、210 tick 表示 1。每帧 16 位：11 位指令值、1 位遥测请求位、4 位异或校验。四个通道同步发帧，每 1 ms 更新一次，与已验证的 `one_one_failversion` 相同。

构建变量 `MAMBA_DSHOT_RATE_KHZ` 默认为 `300`；排查长线或信号完整性问题时可设为 `150`，此时位时长 560 个 TIM3 tick，高电平 210/420 tick。两种速率都是数字 DShot，不改变接线或串口命令。

还可另建诊断构建目录并设 `-DMAMBA_DSHOT_NORMAL_TEST=ON`：此构建不发送 3D 设置命令，只接受 `p` 单向油门，百分比映射到 DShot 48～2047；例如 `p 1 5` 对应约 148，与参考工程测试值 150 接近。`n` 和 `save3d` 会被拒绝。默认构建仍保持双向 3D 行为。

BLHeli_S 3D 模式中，0 表示停转；48～1047 和 1048～2047 分别表示两个旋转方向。程序上电后连续发停转帧，等待 3 秒，然后发送 10 次带遥测请求位的 `3D_MODE_ON`（指令值 10）。**默认仅临时生效**；串口出现 `READY` 只表示控制板发出了设置命令，电调没有回传确认。若要将当前双向方向设置写入电调，请按下文明确发送一次 `save3d`。此命令会请求四路电调写入非易失存储器，固件不自动执行。

## 接线

| 串口/电机通道 | STM32 引脚 | 定时器通道 | 电调信号端 |
| --- | --- | --- | --- |
| 1 | PA6 | TIM3_CH1 | S1 |
| 2 | PA7 | TIM3_CH2 | S2 |
| 3 | PB0 | TIM3_CH3 | S3 |
| 4 | PB1 | TIM3_CH4 | S4 |

电脑串口使用 USART2：PA2 是 STM32 TX，PA3 是 STM32 RX，115200、8N1。STM32 与电调信号地必须共地。F55_BLS 没有 BEC，STM32 板和电调动力侧需按各自要求供电。这里的 1～4 是接到 S1～S4 的**信号通道**；电机实际对应关系应对照线束和电调丝印确认。

## 串口命令

每条命令以回车或换行结束，使用小写 ASCII。例如：

```text
p 2 50
n 4 20
p 2 0
stop
beep 1
probe
save3d
```

`p`、`n` 分别选择 DShot 3D 的两段油门范围；第二个数字为通道 1～4；最后一个数字为 0～100 的**油门指令百分比**。`p 2 50` 对应 DShot 值 1548，表示第 2 路正向约 50% 的可用指令范围，不保证实际转速正好是额定转速的 50%。`n 4 20` 让第 4 路进入反向指令范围。0 停止所选通道；`stop` 或 `s` 停止全部四路。修改一条通道不会改变其他通道。

`beep 1`～`beep 4` 是诊断指令，仅在四路停转时向指定电调发送两次 DShot BEEP1（间隔 450 ms）。串口回应只表示 STM32 已发出命令；需听对应电机是否发声，才能判断电调是否收到了信号。该指令不会保存电调设置。

`probe` 仅在四路停转时发出短脉冲，返回 `PIN_TEST abcd`，依次代表 PA6、PA7、PB0、PB1 是否在引脚上检测到高、低电平变化。它只能验证 STM32 引脚，不验证电调端收到正确的 DShot 帧。

`save3d` 仅在 `READY` 后且四路均停转时接受。它先连续发停转帧，再向 S1～S4 各发送 10 次带遥测请求位的 DShot `SAVE_SETTINGS`（指令值 12），请求 BLHeli_S 保存当前设置。串口回应 `SAVE_SETTINGS sent...` **不代表电调已确认写入**；必须给电调彻底断电、重上电后验证。BLHeli_S 的 DShot 配置命令自 Rev16.7 起提供，实物固件版本尚未读回。原版 BLHeli_S 的出厂默认油门中心为约 1488 μs，最小/最大油门为约 1148/1832 μs，并不一定等于 PWM 工程假设的 1500/1000/2000 μs；DShot 保存命令只保存现有参数，不能在此工程里读取或改写这些数值。因此切换 `../line_mamba_pwm` 前必须拆桨验证 1500 μs 确实停转，并核对方向、行程。若实物装载了不同固件，或者需要精确配置中心/行程，仍需使用 BLHeli_S 配置器和兼容接口。

程序返回 `OK`、`OK stopped` 或 `ERR ...`。正反切换先停转 300 ms 再给新方向。上电等待 `READY` 之前只接受停转命令。为了处理电脑断线，**5 秒内没有有效命令时四路全部停转**；需要连续运行时，电脑应至少每 5 秒重发一条有效命令。程序启动后不会自动发出非零油门，初始化错误时强制拉低四路信号。首次通电验证请卸下螺旋桨，从小百分比逐路检查旋向。

## 构建

在安装了 Arm GNU Toolchain、CMake 和 Ninja 的环境中，在此目录运行：

```text
cmake --preset Debug
cmake --build --preset Debug
```

输出为 `build/Debug/line_mamba.elf`。`line_mamba.ioc` 同步记录了 PLL 168 MHz、TIM3 周期 279 和 USART2 中断配置。若重新生成 CubeMX 代码，要保留 `Core/Src/mamba_*.c`、`Core/Inc/mamba_*.h`、`main.c` 的应用调用及中断回调、`stm32f4xx_it.c` 中的 USART2 中断函数，以及 CMakeLists.txt 中的源文件列表。

参考资料：[DIATONE F55_BLS 页面](https://www.diatone.us/products/mamba-f55_bls-dshot600-4in1-esc-40a-6s-2)、[厂家接线/参数图](https://flymod.net/download/diagrams_parameters_mamba_f405_mk2_v2_z_esc_f55_6s_55a)、[BLHeli_S DShot 指令定义](https://github.com/bitdump/BLHeli/blob/master/BLHeli_S%20SiLabs/Dshotprog%20spec%20BLHeli_S.txt)、[BLHeli_S 原版实现](https://github.com/bitdump/BLHeli/blob/master/BLHeli_S%20SiLabs/BLHeli_S.asm)。
