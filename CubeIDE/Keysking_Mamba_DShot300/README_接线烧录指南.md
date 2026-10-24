# Keysking STM32F103C8T6 + MAMBA F55_BLS 双推进器安全测试

本工程用 Keysking `STM32_KIT` 学习板通过 **DShot300** 控制 MAMBA
F55_BLS 四合一电调的 1、2 通道。电调官方规格是 3–6S（16.8–25.2 V）、
无 BEC、DShot300/600，因此 22.5 V 位于允许输入范围内，但不能给开发板供电。

本机已用 STM32CubeIDE 1.19.0 / GNU Tools for STM32 13.3.rel1 执行 Clean Build。
编译结果和固件校验值见 `BUILD_VERIFICATION.txt`。

## 1. 最终接线

**全部断电后操作。** 电调照片中右侧小焊盘从上到下为：
`CUR / NC / S4 / S3 / S2 / S1 / VCC / GND`。

| Keysking 开发板 | MAMBA F55_BLS | 用途 |
|---|---|---|
| 自定义 IO 排针 `PA12` | `S1` | 一号电调 DShot300 信号 |
| 自定义 IO 排针 `PA15` | `S2` | 二号电调 DShot300 信号 |
| 任一 `GND` | `GND` | 信号共地，必须接 |
| Type-C 5 V | 不接电调 | 开发板独立供电 |
| 不接 | `VCC` | 保持悬空并绝缘；电调无 BEC |
| 不接 | `CUR/NC/S3/S4` | 本测试不使用 |

强烈建议 PA12–S1、PA15–S2 各串联一个 100–220 Ω 电阻，信号线与 GND 线绞合，
并与电机三相线和电池大电流线保持距离。3.3 V DShot 高电平通常可由飞控电调识别；
若实测不识别，再加 74AHCT125 等 3.3→5 V 缓冲器，不要把 5 V 直接送入 STM32。

### 绝对禁止

- 禁止将 22.5 V 接到开发板的 `5V`、`3V3`、`VSWD` 或 GPIO。
- 禁止把开发板 `5V/3V3` 接到电调 `VCC`。该型号官方标注 `BEC: NO`；
  `VCC` 应视为电池电压检测/堆叠接口，照片不足以证明其电平，故本工程不用它。
- 禁止在推进器露出水面、有人手或异物在导流罩附近时运行。
- 禁止热插拔信号线或电机三相线。

### 电源主回路

22.5 V 电源正极 → 电调主电源 `+`；电源负极 → 电调主电源 `-`。
建议主回路串保险丝并使用可快速断开的 XT60/急停开关。电源应具备足够电流能力；
两台推进器同时测试时先设置较低限流值，再逐步确认。

## 2. 程序的安全行为

- 上电前 3 秒持续发送 DShot 停止帧。
- 两个按键必须连续松开 1 秒才解锁。
- 按住板载 `K1`（PB12）：一号推进器低速运行；松手立即停。
- 按住板载 `K2`（PB13）：二号推进器低速运行；松手立即停。
- 同时按住：两台低速运行。
- 每次连续运行最多 3 秒，超时自动停；必须松开两键才能再次测试。
- 测试油门值为 DShot `150`（有效范围 48–2047，约 5.1%）。
- 绿色 LED 只在发送非零油门时点亮。
- 任何错误处理路径会先强制 PA12/PA15 为低电平。

程序只测试正转。若需要反转，优先在 BLHeli 配置中设置该 ESC 通道的方向，或在断电后
调换对应推进器任意两根相线；不要在本次基础测试中尝试 DShot 命令反转。

## 3. 烧录

1. 先不接 22.5 V，只用 Type-C 给 Keysking 板供电。
2. 连接 ST-LINK：`SWDIO→DIO/PA13`、`SWCLK→CLK/PA14`、`GND→GND`。
3. 打开 STM32CubeIDE，选择
   `File > Import > General > Existing Projects into Workspace`。
4. Root directory 选择本工程目录，导入 `Keysking_Mamba_DShot300`。
5. 执行 `Project > Clean` 后 `Project > Build Project`。
6. 在 `Run > Debug Configurations` 创建 STM32 C/C++ Application，接口选 SWD，
   应用选择 `Debug/Keysking_Mamba_DShot300.elf`，然后下载并 Resume。
7. 也可以用 STM32CubeProgrammer 直接写入
   `Debug/Keysking_Mamba_DShot300.hex`，地址由 HEX 文件自带。

若没有 ST-LINK，Type-C 只提供供电和 CH343 串口，不能替代 SWD 下载器。

## 4. 首次实物测试顺序

1. **推进器完全浸没并牢固固定**，涵道内无异物；人员远离。准备随时断开主电源。
2. 22.5 V 电源关闭，先完成三根控制线和电机三相线检查；万用表确认主电源无短路。
3. 只给开发板 Type-C 上电，等待至少 4 秒，两个按键均不得按下。
4. 给电调接通 22.5 V。等待电调完成启动音；不要触碰推进器。
5. 短按/按住 K1，不超过 1 秒，观察一号推进器；松手确认立即停止。
6. 同样测试 K2；两路分别正确后才短暂同时按下。
7. 任一路不对、异常抖动、持续蜂鸣、发热或无法停转，立即断开 22.5 V。

按键无反应时可用板载 Type-C 串口查看状态：115200 baud、8-N-1、无流控。
也可在 CubeIDE Live Expressions 查看 `app_live`，其中包括当前状态、按键、命令值和帧计数。

## 5. 信号检查与限制

在接电调前，可用示波器或逻辑分析仪检查 PA12/PA15：

- 协议 DShot300，bit time 约 3.333 µs；
- 逻辑 0 高电平约 1.25 µs；逻辑 1 高电平约 2.50 µs；
- 每帧 16 bit，非运行状态发送 value=0 停止帧；
- 按键测试时 value=150，代码会自动附加 DShot 校验和。

软件已通过编译和静态时序常量检查，但当前会话无法替你对真实高功率硬件通电，也没有
示波器反馈，所以不能声称已完成实物闭环验证。首次通电必须按上述低风险顺序进行。

## 6. 资料依据

- MAMBA F55_BLS 官方页：
  https://www.diatone.us/products/mamba-f55_bls-dshot600-4in1-esc-40a-6s-2
- Keysking 官方资源/原理图：
  https://docs.keysking.com/docs/stm32/resourcePack/
- ST STM32F103C8 产品页：
  https://www.st.com/en/microcontrollers-microprocessors/stm32f103c8
- ST RM0008 参考手册：
  https://www.st.com/resource/en/reference_manual/rm0008-stm32f101xx-stm32f102xx-stm32f103xx-advanced-arm-based-mcus-stmicroelectronics.pdf

T60-EDU 的公开官方规格未能从可索引网页中可靠取得。因此本工程只根据照片确认它是三相
无刷推进器，并依赖你已将三相线接入电调 1/2 通道；22.5 V 是否符合该**具体 EDU 版本**，
仍应以推进器随附说明书/销售方书面参数为准。相近的 T60 型号不能替代该确认。
