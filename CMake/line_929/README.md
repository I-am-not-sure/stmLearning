# line_929：GPIO/DWT 四路 DShot300

用 GPIO/DWT 生成 DShot300 帧，配合串口命令和协议测试。

| 项目 | 内容 |
| --- | --- |
| MCU | `STM32F407ZGT6` |
| 构建入口 | CMake |
| 学习内容 | DShot300、GPIO、DWT、UART、协议解析 |
| 代码入口 | [`Core/Src/main.c`](Core/Src/main.c) |
| 来源 | `STM32C/line_929` |

## 建议阅读顺序

1. [`Core/Src/main.c`](Core/Src/main.c)：初始化、主循环和中断回调。
2. [`Core/Inc/oneone_app.h`](Core/Inc/oneone_app.h)
3. [`Core/Inc/oneone_command.h`](Core/Inc/oneone_command.h)
4. [`Core/Inc/oneone_dshot.h`](Core/Inc/oneone_dshot.h)
5. [`Core/Inc/oneone_dshot_codec.h`](Core/Inc/oneone_dshot_codec.h)
6. [`Core/Src/oneone_app.c`](Core/Src/oneone_app.c)
7. [`Core/Src/oneone_command.c`](Core/Src/oneone_command.c)
8. [`Core/Src/oneone_dshot.c`](Core/Src/oneone_dshot.c)
9. [`Core/Src/oneone_dshot_codec.c`](Core/Src/oneone_dshot_codec.c)

外设和引脚配置见 [`line_929.ioc`](line_929.ioc)；实际启用行为以源码为准。

## CubeMX 配置信号索引

下面列出 `.ioc` 中的配置，包含可能尚未由应用启用的外设；这不是完整硬件接线图。

| MCU 引脚 | 配置信号 |
| --- | --- |
| `PA2` | `USART2_TX` |
| `PA3` | `USART2_RX` |
| `PA6` | `GPIO_Output` |
| `PA7` | `GPIO_Output` |
| `PB0` | `GPIO_Output` |
| `PB1` | `GPIO_Output` |

## 本工程说明

原预设使用 MinGW Makefiles 且记录本机 make 路径；跨电脑可按公共构建指南显式选择 Ninja。

## 打开与构建

[公共构建与导入指南](../../docs/BUILDING.md) · [全部工程与外设索引](../../README.md)

本次整理的验证范围见[验证记录](../../docs/VALIDATION.md)。

---

## 整理前的项目说明

原有说明保留如下。文中的固件产物、实测记录和本机路径属于原记录；本仓库发布源码，编译输出需自行生成。

# line_929：四合一电调 DShot300 控制与排查

目标硬件是 STM32F407ZGT6 与目前这块 MAMBA 四合一电调。此工程只控制 S1～S4，未接入旧 `one_one` 的 M5/M6、传感器或树莓派协议。

## 为什么这样实现

9 月初的 `one_one` 记录显示，同一块四合一电调曾由 TIM1 的 PE9/PE11/PE13/PE14 发送 DShot300 驱动，并报告 M1～M4 可反转；那不是本次 PA6/PA7/PB0/PB1 接线。9 月 28 日的 TIM3 DShot 与 PWM 测试没有确认电调输入端的实际波形，而且切换协议的 PWM 测试没有记下电调动力电源重新上电的步骤。因此本工程保留**当前接线**，用 GPIO 和 DWT 周期计数器生成 DShot300，避免依赖 TIM3 的逐位更新；帧格式与旧工程相同。

电调参数尚未通过配置器读回。固件启动后只发送 DShot 停转帧；收到 `arm` 时，先向四路电调发送与旧 `one_one` 相同的**临时 3D 模式请求**，然后允许 `p`、`n` 分别控制两个方向。每次重新 `arm` 都重发该请求，以覆盖电调单独断电重启的情况。工程没有 `SAVE_SETTINGS`，不会写电调永久设置。串口的 `OK`/`READY` 仅表示 STM32 状态或指令已发，不是电调的回执。

## 接线

| STM32 | 电调 | 用途 |
| --- | --- | --- |
| PA6 | S1 | 电机信号 1 |
| PA7 | S2 | 电机信号 2 |
| PB0 | S3 | 电机信号 3 |
| PB1 | S4 | 电机信号 4 |
| GND | 信号 GND／电池负极 | 共地 |
| PA2 (TX) | USB 转 TTL RX | 串口发送 |
| PA3 (RX) | USB 转 TTL TX | 串口接收 |

串口使用 3.3 V TTL、115200、8N1。电调动力端由适配的电池供电，STM32 由自己的电源供电。信号线连接到电调丝印的 **S1～S4**，不要把电池电压线接到 STM32 IO。旧接线 PE9/PE11/PE13/PE14 **不适用**于本固件。

当前实物使用 **8 针线束**，6S 电池和已焊接的 470 µF／35 V 输入电容。厂家接线图中的这一排接口还包含 `CURR`、`RX6`、`VCC` 和 `GND`，标注顺序为 `CURR RX6 S4 S3 S2 S1 VCC GND`。这是图纸中的标注，不能据此猜测手中插头从哪一面数第 1 针。**先断开电池和 STM32 电源**，用万用表逐根确认 PA6／PA7／PB0／PB1 分别导通到电调端 S1／S2／S3／S4，确认 GND 导通，并核对 VCC 不接到任何 STM32 IO；不要只按线色或插头位置推断。图纸：[MAMBA F405 MK2 V2 + F55_BLS 接线图](https://flymod.net/download/diagrams_parameters_mamba_f405_mk2_v2_z_esc_f55_6s_55a)。

**不要把 8 针线束的 VCC 接到 STM32 的 3.3 V、5 V 或 GPIO。** 这款 F55_BLS 的规格为 `BEC: NO`，VCC 不能当作稳压 5 V 输出；从厂家接线图推断，它很可能连接电池电压。当前 STM32 已由独立电源正常启动，控制电调只需 S1～S4 与公共 GND，VCC 可以悬空。若需验证，保持两板不相连，用万用表量电调 VCC 相对 GND 的实际直流电压，再决定是否需要适配 6S 的降压模块。[Diatone F55_BLS 规格](https://www.diatone.us/products/mamba-f55_bls-dshot600-4in1-esc-40a-6s-2)。

## 编译、烧录

在 `line_929` 目录使用 ARM GNU 工具链：

```text
cmake -S . -B build/Debug -G "MinGW Makefiles" -DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake -DCMAKE_BUILD_TYPE=Debug
cmake --build build/Debug -j 4
```

烧录 `build/Debug/line_929.hex` 或 `line_929.elf`。每次修改源码后重新编译、确认烧录路径，避免使用先前工程留下的旧 ELF。**当前 60 秒超时版已编译到 `build/Debug`，尚需重新烧录；启动串口应显示 `60s command timeout`。电调接收和电机旋转仍待实物验证。**

### ST-LINK 能识别但找不到 STM32

若显示 `Target no device found`／`Unable to get core ID`，表示失败发生在连接芯片、尚未写入本固件。2026-09-29 的复查中，电脑能识别同一只 ST-LINK，目标电压读数约 3.22 V，但 Normal 1 MHz、Under Reset 100 kHz、Hotplug 100 kHz 都读不到芯片 ID；而 9 月 28 日同一只探针和固件曾读到 F407 的 `Device ID 0x413`。因此先检查目标板实际 3.3 V、SWDIO／SWCLK、GND 与 NRST 到芯片的导通。队友此前烧录的程序若把 PA13／PA14 重配成 GPIO，也可能造成运行时无法连接；单纯在 CubeMX 未选择 Serial Wire、但程序未改动这两个引脚，不足以直接定因。

恢复时先断开电调 6S 电源。若 NRST 确实连接，可用 ST-LINK 的 Connect Under Reset；仍失败时，把 STM32F407 的 BOOT0 置 1、BOOT1 置 0，然后复位，让它从系统存储器启动，再尝试 SWD 连接与烧录。刷入本固件后把 BOOT0 恢复 0 并复位。**先确认芯片 ID，再执行下载**；不要在连接失败时擦除或修改保护选项。[ST-LINK/V2 手册](https://www.st.com/resource/en/user_manual/dm00026748.pdf)、[STM32F4 启动模式表](https://www.st.com/resource/en/reference_manual/dm00031020-stm32f4-discovery-board-technical-reference-manual-stmicroelectronics.pdf)。

## 首次试转顺序

1. 拆下全部螺旋桨，固定电机。让电调动力电源断开至少 5 秒；先烧录本固件并让 STM32 正常运行。
2. 给电调重新接上动力电源。保持 STM32 运行并持续发送停转帧；切换自 PWM 或其它 DShot 固件时，电调需要这次重新上电以重新识别当前输入。
3. 打开串口，若错过启动提示可发 `status`。STM32 启动约 5 秒后应回复 `READY DISARMED 3D pending on arm`。这不证明电调已经解锁。
4. 可先发 `beep 1`，只让 S1 请求诊断鸣叫；若有声音，确认确实来自 S1 电机。没有声音也不能单独判定引脚无信号，因为电调固件版本尚未读回。首次试转先发 `mode1`，收到回复后发 `arm`、`p 1 5`，观察 S1 是否持续转动。普通单向映射下 `p 1 5` 的 DShot 值约 148。测试后发 `disarm`。
5. S1 正常后逐一对 S2～S4 重复 `beep 2`、`p 2 5` 等。逐路核对实际电机编号和旋向。

每行命令须带回车或换行。`arm` 后电机仍停；`p` 才发送油门。`p 1 5` 中的 `5` 是指令行程的 5%，不是测量的转速。`arm` 后有 60 秒输入下一条控制命令；电机运行时若连续 60 秒没有新的控制命令，才会自动停机并解除 `arm`。每条有效油门命令重新计时；需要提前停转可发 `disarm`。

2026-09-29 的串口记录发现旧版在 `arm` 后约 35 ms 就错误打印 `DISARM command timeout`：发送 3D 请求期间时间向前走了，但主循环仍用发送前的旧时间计算无符号超时。当前源码已在解析指令后重新读取时钟，并重编译 `build/Debug/line_929.elf`／`.hex`。若设备仍出现 `OK armed` 紧接着 `DISARM command timeout`，说明烧入的还是旧固件。该记录还显示第一次 `status` 未附换行，下一条 `status\n` 与它拼成无效命令；务必让串口助手每次发送 CR 或 LF。

## 双向试转

拆桨、核对信号线，并确认电调上电后三声及识别零油门的两声。默认 `arm` 会自动发送临时 3D 请求，然后执行：

```text
arm
p 1 5
stop
n 1 5
disarm
```

`arm`／`mode3d` 向四路电调各发送 10 次带遥测位的 DShot `3D_MODE_ON` 请求，并保持电机停转。正反方向的物理含义以实际观察为准。该请求不写入电调设置；串口打印 `3D requested` 不是电调确认。若电调没有响应或出现异常声音，先 `disarm` 并断开电调动力电源。`mode1` 可发送临时 `3D_MODE_OFF` 请求，把后续 `arm` 切换成普通单向诊断；重新 `mode3d` 或重启 STM32 可恢复默认双向请求。

**首次试 3D 必须拆桨。** 如果电调没有接受 3D 请求，本程序发出的 3D 数值可能被仍处于单向模式的电调当成较高油门。`arm` 后先确认电机仍停，再从 `p 1 5` 开始；确认停机命令有效、方向对应正确之后才试 `n 1 5`。

## 其它命令和诊断

| 命令 | 行为 |
| --- | --- |
| `status` | 显示启动、解锁和已请求的模式；`3D requested` 不等于电调确认 |
| `beep 1`～`beep 4` | 在未解锁时请求指定通道鸣叫 |
| `arm` | 默认先请求临时 3D，再允许后续油门；当前仍为停转帧 |
| `p 1 5`～`p 4 100` | 默认 3D 模式的一个方向；`mode1` 后为普通单向油门 |
| `n 1 5`～`n 4 100` | 3D 请求后为另一个方向 |
| `stop` 或 `s` | 四路停转，保留解锁状态 |
| `disarm` | 四路停转并取消解锁 |
| `mode1` / `mode3d` | 未解锁时手动改变后续 `arm` 使用的模式 |

波形预期：每位约 3.33 µs；逻辑 0 的高电平约 1.25 µs，逻辑 1 约 2.50 µs；约每 1 ms 一帧。若串口收到 `OK` 而电机不动，在**电调端 S1 与 GND**量波形；只量 STM32 引脚或读取定时器寄存器不足以确认信号到了电调。再测加油门时电调动力端电压，并排查该路三根电机相线及接头。`line_929_boot_step`、`oneone_live.frames_sent`、`oneone_live.dshot_values[4]` 可由调试器查看，但也不能替代电调端实测。

BLHeli_S 的声音判据：**上电三声**表示电调上电；随后一声低音表示检测到输入信号，再一声高音表示看到了零油门并完成解锁。长时间零油门后的单声周期性蜂鸣可能是寻机蜂鸣。9 月 28 日普通 PWM 1000 µs 测试曾听到额外两声，这支持至少某一路识别了低油门信号，但当时没有逐路确认声音来自哪台电机。若连续重复完整的上电三声，应同步测电调电池焊盘电压，排查供电反复中断；不能单凭串口 `READY` 判断电调状态。[BLHeli_S 手册](https://bluerobotics.com/wp-content/uploads/2018/10/BLHeli_S-manual-SiLabs-Rev16.x.pdf)。

厂家明确列出 F55_BLS 支持 6S 和 DShot300／600，且要求 6S 使用输入电容；这些条件目前已满足。BLHeli_S 的默认方向是普通单向，3D 是单独的选项；本工程为双向需求在每次 `arm` 时发送临时 3D 请求。若要诊断未配置电调能否接受普通单向 DShot，请在断电重启电调后依次发送 `mode1`、`arm`、`p 1 5`；此时 `p 1 5` 为 DShot 值约 148。9 月 28 日 1000～1720 µs 的 PWM 试验中虽然听到两声，但仅抖动、鸣叫；记录没有确认 PWM 测试前电调曾断电重启，也没有在电调端测量信号，所以还不能由此判定电调或电机损坏。[Diatone F55_BLS 规格](https://www.diatone.us/products/mamba-f55_bls-dshot600-4in1-esc-40a-6s-2)、[BLHeli_S 源码默认设置](https://github.com/bitdump/BLHeli/blob/master/BLHeli_S%20SiLabs/BLHeli_S.asm)。
