# JY61P：STM32F407ZG 的 I2C2 读取与 USART1 阻塞输出

工程目录：`D:\codeprofiles\STM32\JY61P`。目标芯片为 **STM32F407ZGT6**，使用工程现有的 STM32F4 HAL、F407 启动文件及 Flash 链接脚本。

## 配置与接线

| 功能 | 当前配置 | 接线 |
| --- | --- | --- |
| I2C2 SDA | PF0、AF4、复用开漏、无内部上拉 | JY61P SDA |
| I2C2 SCL | PF1、AF4、复用开漏、无内部上拉 | JY61P SCL |
| USART1 TX | PA9、AF7 | USB-TTL RX |
| USART1 RX | PA10、AF7，保留原 TX/RX 配置 | 当前程序不接收，可不接 |
| GND | 共地 | STM32、JY61P、USB-TTL GND 相连 |
| IIC 上拉 | PF0/PF1 各自上拉到 3.3V | 已有上拉须连接到这两根线；短线可从每路约 4.7kΩ 开始 |

模块电源按实际模块规格供电；信号上拉使用 3.3V。JY61P 的 TX/RX 不用于这次 IIC 读取。

`JY61P.ioc` 已具备所需外设和引脚，显式保存 `I2C2.ClockSpeed=80000`、标准模式，同时将生成代码中的 I2C2 频率改为 80kHz。串口为 **115200、8 数据位、无校验、1 停止位、无流控**。

重新保存后曾重新生成默认 100kHz，因此在 `main.c` 的 `USER CODE BEGIN I2C2_Init 2` 中增加了频率检查：若生成值与 `JY61P_I2C_CLOCK_HZ` 不同，就在开始读取前重新初始化为 80kHz。该代码随用户代码区保留。以后修改频率时，同时修改 `.ioc` 和这个宏。外部修改 `.ioc` 后，已经打开的 CubeMX 编辑页可能仍保留旧配置，应重新打开文件后再保存。

时钟沿用 HSI 16MHz 经 PLL 产生 168MHz，APB1=42MHz、APB2=84MHz。当前重新保存的配置已启用 USART1 RX/TX 的 DMA2 Stream2/Stream7 和相关中断，配置及句柄连接已检查，继续保留。主程序调用的仍是 `HAL_UART_Transmit`，不会启动 DMA 发送；本次功能依旧为阻塞输出。SysTick 保留，用于等待和超时。工程当前 PLLQ=4，48MHz 分支为 84MHz，该分支不参与本程序；以后启用 USB/其他要求 48MHz 的外设时，应把 PLLQ 设为 7。

选择 80kHz 的依据是 [ST ES0182，第 2.10.4 节](https://www.st.com/resource/en/errata_sheet/dm00037591-stm32f405-407xx-and-stm32f415-417xx-device-limitations-stmicroelectronics.pdf)：F407 在 88–100kHz 标准模式下，特定上升沿或从机时钟拉伸条件会影响重复起始建立时间。PF0/PF1 的 AF4 映射可参见 [F407 数据手册](https://www.st.com/resource/en/datasheet/stm32f407zg.pdf)。

## 运行行为

初始化后等待 1.5 秒，然后立即读取；后续每秒报告一次。在 `Core/Src/main.c` 的 `JY61P_REPORT_PERIOD_MS` 修改报告间隔。

`Core/Src/jy61p.c` 调用 `HAL_I2C_Mem_Read` 两次：

- 七位地址 `0x50`，传入 HAL 的地址为 `0x50 << 1`；线上写地址字节为 `0xA0`、读为 `0xA1`。
- 从 `0x34` 起读取 12 字节，得到 AX/AY/AZ/GX/GY/GZ。
- 从 `0x3D` 起读取 6 字节，得到 Roll/Pitch/Yaw；跳过磁场区。
- 寄存器编号发送 8 位，每个寄存器数据为有符号 16 位，低字节在前。
- 两次读取均成功才输出；量程与成功的软件版一致：16g、2000°/s、180° 对应 32768。

随后调用 `HAL_UART_Transmit` 阻塞发送完整一行。每次 IIC 调用的超时参数为 100ms，串口发送超时参数也为 100ms。两个独立读事务不能保证数据来自同一个传感器更新时间。

启动行：

```text
JY61P I2C2 -> USART1, address=0x50
```

正常行格式如下，数值仅作示例：

```text
ACC(g)=0.000,0.000,1.000 GYRO(deg/s)=0.000,0.000,0.000 ANGLE(deg)=0.000,0.000,0.000
```

输出用整数千分之一单位换算并保留三位小数，因此**不用另外开启浮点 printf**。三位小数是显示格式，不代表模块具有相同精度。

## 失败输出

```text
JY61P I2C2 error: reg=0x34 status=1 err=0x00000004 SCL=1 SDA=1 reinit=0
```

| 字段 | 含义 |
| --- | --- |
| `reg=0x34` / `reg=0x3D` | 哪一段寄存器读取失败 |
| `status` | HAL 返回值：0=OK，1=ERROR，2=BUSY，3=TIMEOUT |
| `err` | HAL 错误位掩码，可能组合多个错误 |
| `0x00000004` | AF，无应答；先检查地址、供电、引脚、共地 |
| `0x00000020` | F4 HAL 的超时错误；检查卡低、波形、总线状态 |
| `0x00000200` | F4 HAL 的 WRONG_START；与旧工程的错误解释需分开核对 |
| `SCL` / `SDA` | 读取失败后、重新初始化前的 PF1/PF0 实际电平 |
| `reinit=0` | STM32 I2C 外设重新初始化成功，下个周期再读 |

失败时只重新初始化 STM32 的 I2C 外设，不更改传感器配置；这不能自动解除从机持续拉低 SDA/SCL 的情况，也不能保证下次读一定成功。没有加入旧工程的上电拉低线路检测。

## 文件与重新生成

新增 `Core/Inc/jy61p.h`、`Core/Src/jy61p.c`。`main.c` 的头文件引用、报告周期、上电等待和主循环调用都在 `USER CODE` 区。`.ioc` 已设置保留用户代码；CubeMX 重新生成时应继续启用该选项。I2C 频率在 `.ioc` 和当前 `main.c` 中保持一致。

CubeIDE 打开或刷新工程后直接构建即可；现有工程的 `Core` 源文件目录会包含新增的 `jy61p.c`。无需配置浮点格式化选项。已有 DMA 和 USART1 中断配置不会改变当前发送方式。

原 `D:\codeprofiles\STM32C\Gyroscope_hard` 只有工具设备设置改为 F407，编译目标仍是 `STM32F103xB`，启动文件、链接脚本和 HAL 也仍属 F1；不能把该固件用于 F407。它不是本次新增代码所在的工程。

## 本次验证

2026-10-06：用 GNU Arm GCC 14.3.1、Cortex-M4、硬浮点 ABI 和工程原有 F407 Flash 链接脚本编译并链接成功。新增应用代码没有编译警告；额外开启 `-Wextra` 时，原 ST HAL Flash 扩展文件有三条未使用参数警告。

用实际 `jy61p.c` 配合模拟 HAL 验证了 6 个场景，包括两段读取的地址/长度、正负数与边界换算、完整串口输出、两段读取失败、错误码在重新初始化后仍正确报告。模拟测试不能替代实物通信验证；本次未烧录或测试实物。
