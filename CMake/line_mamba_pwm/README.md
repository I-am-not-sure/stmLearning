# line_mamba_pwm：TIM3 四路普通 PWM

使用 TIM3 输出传统 PWM，以串口命令调整四路电调指令。

| 项目 | 内容 |
| --- | --- |
| MCU | `STM32F407ZGT6` |
| 构建入口 | CMake |
| 学习内容 | PWM、TIM3、UART、命令协议 |
| 代码入口 | [`Core/Src/main.c`](Core/Src/main.c) |
| 来源 | `STM32C/line_mamba_pwm` |

## 建议阅读顺序

1. [`Core/Src/main.c`](Core/Src/main.c)：初始化、主循环和中断回调。
2. [`Core/Inc/mamba_command.h`](Core/Inc/mamba_command.h)
3. [`Core/Inc/mamba_pwm.h`](Core/Inc/mamba_pwm.h)
4. [`Core/Inc/mamba_pwm_app.h`](Core/Inc/mamba_pwm_app.h)
5. [`Core/Src/mamba_command.c`](Core/Src/mamba_command.c)
6. [`Core/Src/mamba_pwm.c`](Core/Src/mamba_pwm.c)
7. [`Core/Src/mamba_pwm_app.c`](Core/Src/mamba_pwm_app.c)
8. [`Core/Src/mamba_pwm_codec.c`](Core/Src/mamba_pwm_codec.c)

外设和引脚配置见 [`line_mamba_pwm.ioc`](line_mamba_pwm.ioc)；实际启用行为以源码为准。

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

与 line_mamba 的数字 DShot 分开：此程序使用普通脉宽 PWM；运行前阅读原文中的电调模式与中位条件。

## 打开与构建

[公共构建与导入指南](../../docs/BUILDING.md) · [全部工程与外设索引](../../README.md)

本次整理的验证范围见[验证记录](../../docs/VALIDATION.md)。

---

## 整理前的项目说明

原有说明保留如下。文中的固件产物、实测记录和本机路径属于原记录；本仓库发布源码，编译输出需自行生成。

# line_mamba_pwm：TIM3 四路普通 PWM 控制曼巴四合一电调

这是从 `line` 复制出的独立 STM32F407ZGT 工程，采用 `Senior` 项目中 50 Hz、1500 μs 中位的**传统舵机式 PWM**思路。它不发送 DShot 帧。已有的 `../line_mamba` 是 DShot300 版本，两个工程不要同时刷入同一块板。

**如果已经按 `line_mamba` 接好 PA6→S1、PA7→S2、PB0→S3、PB1→S4 和共地，切换到本工程不需要改线。** USART2 接电脑的 PA2/PA3 也完全相同。区别在固件发出的波形以及电调的双向模式配置。

**先决条件：**本工程假定四合一电调的**每一路**都已经启用双向 3D 模式，并把 1500 μs 确认为停转中位。未经这一步，`arm` 输出的 1500 μs 可能会让电机转动。只用现有 STM32 硬件时，可先刷 `../line_mamba`，在 `READY` 且四路停转后显式输入 `save3d` 请求 BLHeli_S 保存 3D 模式；但该命令无法读回或改写电调的 PWM 中心及行程。原版 BLHeli_S 默认中心约为 1488 μs，实际板上固件/设置未知，切换本 PWM 固件后必须拆桨验证 1500 μs 停转。F55 BLS 商品资料主要标 DShot，尚未在你们这块实物上验证普通 PWM 输入。`p/n` 的百分比是**指令行程比例**，不是闭环测得的额定转速比例；没有测速反馈，无法承诺 50% 指令对应 50% 转速。

## 硬件接线

| STM32F407ZGT 端 | 四合一电调端 | 用途 |
| --- | --- | --- |
| PA6 / TIM3_CH1 | S1（电机 1 信号） | 50 Hz PWM，1000–2000 μs |
| PA7 / TIM3_CH2 | S2（电机 2 信号） | 同上 |
| PB0 / TIM3_CH3 | S3（电机 3 信号） | 同上 |
| PB1 / TIM3_CH4 | S4（电机 4 信号） | 同上 |
| STM32 GND | ESC GND | **共地，必须连接** |
| PA2 / USART2_TX | USB 转 TTL 模块 RX | 给电脑回传状态 |
| PA3 / USART2_RX | USB 转 TTL 模块 TX | 接收电脑命令 |
| STM32 GND | USB 转 TTL 模块 GND | 共地 |

串口模块用 **3.3 V TTL 电平**，115200、8N1；不要直接接 RS-232 电平。电调的大电流电池线接与电调及电机匹配的电源，四组电机各接对应的三根相线。确认所购 F55 BLS 版本的电池电压与额定电流后再上电；不要由 STM32 或 USB 串口模块给电调主功率供电。STM32 用自己的稳定电源（开发板 USB 或合适的稳压器），并与电调共地。四合一电调的电源焊盘、S1–S4、GND 的实际位置，应对照**你们这块板的丝印或原理图**核对，不要只凭插头线色。其它在 `line` 中初始化的外设引脚没有用于本程序，不需要接线。

若使用电调随附的 8 针排线，只引出已核实的 **S1–S4 和 GND** 到 STM32。排线中可能还有电池原始电压、 电流检测或遥测脚；未确认引脚定义前不要把整束排线插到 STM32 的普通 IO 或 3.3 V 电源上。厂商为 F55 BLS 标注 **BEC: NO**，所以不能指望电调提供开发板电源。

在切换到此固件并给电调上电前，通过支持该型号 BLHeli_S 的配置器/飞控直通，逐路读取并设置电调：Motor Direction 选双向 3D（需要的话选择双向反转），核对 PWM 输入范围 1000/1500/2000 μs，写入后重新读取确认。若电调的双向中位或输入范围不同，先修改此工程的映射，不能直接测试。软件不能自动读取电调配置。也不要运行 `Senior` 里开机发送 2000→1000→1500 μs 的校准流程。

## 编译和操作

使用原 `line` 的 CMake + arm-none-eabi-gcc + Ninja 工具链：

```powershell
cd D:\codeprofiles\STM32C\line_mamba_pwm
cmake --preset Debug
cmake --build --preset Debug
```

固件生成在 `build/Debug/line_mamba_pwm.elf`。也可将 `Debug` 换成 `Release`。烧录方式沿用开发板原有 SWD 工具；烧录时先断开电调主电源、拆下螺旋桨。

如果在 VS Code 中调试，从“运行和调试”的配置列表选择 `STM32Cube: Debug PWM (Live Watch off)`。该配置在 `.vscode/launch.json` 中关闭 STM32 扩展的 Live Watch，仍会烧录固件并允许普通断点调试。

上电后四个 PWM 信号引脚保持低电平，串口显示 `PWM DISARMED`。确认配置和接线、移除螺旋桨后输入以下命令，每条以回车或换行结束：

```text
arm       # 开始连续输出 1500 μs 中位；等待 3 秒后收到 READY
p 2 50    # 2 号通道正向，输出 1750 μs
n 4 25    # 4 号通道反向，输出 1375 μs
p 2 0     # 2 号通道回到 1500 μs 中位
stop      # 四路全部回到 1500 μs；s 也可以
off       # 四路停止 PWM 输出，信号引脚拉低；再次运行需 arm
```

`p/n` 后的通道只能为 1–4，百分比只能为 0–100。代码计算 `p`：1500 + 5 × 百分比 μs；`n`：1500 − 5 × 百分比 μs。方向切换时先保持中位 300 ms 再输出反向指令。若最后一条有效控制命令过去 5 秒，四路自动回中位；上位机需周期性发送当前命令（建议小于 1 秒一次），否则不会持续转动。`stop` 是中位停转指令，`off` 才停止产生 PWM 脉冲。断开电调主功率电源是物理急停手段，软件命令不能代替它。

### 单向 PWM 接线诊断构建

当 DShot 输入没有被电调识别时，可以另建一个 CMake 构建目录并设置 `-DMAMBA_PWM_NORMAL_TEST=ON`。这一构建只用于拆桨台架诊断：`arm` 开始四路 1000 μs 低油门，`p 1 50` 让第 1 路输出 1450 μs，`p 1 80` 输出 1720 μs，`stop` 回到四路 1000 μs，`off` 拉低信号脚；不接受 `n`。测试上限为 1900 μs。默认构建仍是上文的双向 3D PWM，且上电均不自动输出脉冲。由于实际电调设置尚未读回，首次发送 `arm` 前须拆除螺旋桨并留意意外转动。

第一次台架测试建议先不接电机，用示波器/逻辑分析仪检查 CH1–CH4 是否为 20 ms 周期、`arm` 时 1500 μs、`p 1 10` 时 1550 μs、`n 1 10` 时 1450 μs。再只接一台电机低指令试转，核对 S1–S4 对应关系和实际方向。若电机不识别 PWM，保持断电排查，改用 `../line_mamba` 的 DShot300 固件，而不要盲目增大油门。

## 与学长工程的对应关系

`Senior/version_two` 与 `Senior/only_pwm` 均使用 50 Hz 定时器比较值表示 1000–2000 μs 指令。学长工程的定时器时钟和引脚不同；本工程沿用 `line` 的 HSI 16 MHz，设置 TIM3 PSC=15、ARR=19999，让计数单位为 1 μs、周期为 20 ms。没有复制传感器、姿态控制或自动油门流程，因为当前硬件没有连接这些信号。

参考资料：[Diatone F55 BLS 官方规格](https://www.diatone.us/products/mamba-f55_bls-dshot600-4in1-esc-40a-6s-2)、[BLHeli_S 源码中的普通 1–2 ms 输入说明](https://github.com/bitdump/BLHeli/blob/master/BLHeli_S%20SiLabs/BLHeli_S.asm)、[Betaflight 3D 模式配置说明](https://betaflight.com/docs/wiki/guides/current/3D-Setup)。这些资料说明了协议与配置原则，不能替代对你们具体电调固件及接线的实测。
