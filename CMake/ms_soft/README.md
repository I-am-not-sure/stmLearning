# ms_soft：F407 模块化软件 I²C

分离软件 I²C 与 MS5837 驱动，周期采集后经串口报告。

| 项目 | 内容 |
| --- | --- |
| MCU | `STM32F407ZGT6` |
| 构建入口 | CMake |
| 学习内容 | 软件 I²C、MS5837、UART |
| 代码入口 | [`Core/Src/main.c`](Core/Src/main.c) |
| 来源 | `STM32C/cmake_enemy/ms_soft` |

## 建议阅读顺序

1. [`Core/Src/main.c`](Core/Src/main.c)：初始化、主循环和中断回调。
2. [`Core/Inc/ms5837.h`](Core/Inc/ms5837.h)
3. [`Core/Inc/soft_i2c.h`](Core/Inc/soft_i2c.h)
4. [`Core/Src/ms5837.c`](Core/Src/ms5837.c)
5. [`Core/Src/soft_i2c.c`](Core/Src/soft_i2c.c)

外设和引脚配置见 [`ms_soft.ioc`](ms_soft.ioc)；实际启用行为以源码为准。

## CubeMX 配置信号索引

下面列出 `.ioc` 中的配置，包含可能尚未由应用启用的外设；这不是完整硬件接线图。

| MCU 引脚 | 配置信号 |
| --- | --- |
| `PA10` | `USART1_RX` |
| `PA9` | `USART1_TX` |

## 打开与构建

[公共构建与导入指南](../../docs/BUILDING.md) · [全部工程与外设索引](../../README.md)

本次整理的验证范围见[验证记录](../../docs/VALIDATION.md)。

---

## 整理前的项目说明

原有说明保留如下。文中的固件产物、实测记录和本机路径属于原记录；本仓库发布源码，编译输出需自行生成。

# ms_soft — MS5837-02BA 深度传感器（软件模拟 I2C 版）

## 1. 这个工程是干什么的

用 **软件模拟 I2C（bit-banging，GPIO 模拟时序）** 读出 MS5837-02BA 压力传感器，
算出压力（mbar）和水下深度（cm），每 1 秒通过 USART1 打印一行。

**为什么改用软件 I2C：**
硬件 I2C 用开漏输出 `GPIO_MODE_AF_OD`，必须靠外部上拉电阻把总线拉高；
当前板子 SDA/SCL 上没有上拉电阻，硬件 I2C 收不到从机应答，压力读出来恒为 0。
软件 I2C 用推挽输出 `GPIO_MODE_OUTPUT_PP` 主动驱动高低电平，读应答时把 SDA
切成带上拉的输入即可，因此**不需要外接上拉电阻**。

## 2. 功能目标（与 `Depthsensor` 硬件 I2C 版一致）

| 阶段 | 做了什么 |
|---|---|
| 上电初始化 | 复位 → 读 PROM 校准系数 C0..C6 → CRC4 校验 → 采大气压做零位 |
| 主循环 | 读 D1(压力) / D2(温度) → 一阶+二阶补偿 → 算压力(mbar) → 算深度(cm) |
| 输出 | 每 1 秒一行 `P=xxxx mbar  Depth=xx.x cm` |
| 额外 | 启动时打印一行初始化结果，方便排错 |

压力/深度的算法结构、输出格式，与 `D:\codeprofiles\STM32\Depthsensor` 里的 `ms5837.c`
一致；两处不同：(1) 把「HAL 硬件 I2C 调用」换成了「软件 I2C」；(2) **压力公式改用
02BA**（实测传感器是 02BA，套 30BA 公式会恰好偏大 20 倍，见第 9 节）。

## 3. 写了哪些文件

### 3.1 新建的文件（4 个）

| 文件 | 作用 |
|---|---|
| `Core/Inc/soft_i2c.h` | 软件 I2C 接口声明 |
| `Core/Src/soft_i2c.c` | 软件 I2C 实现：PB6=SCL、PB7=SDA，推挽输出，DWT 微秒延时 |
| `Core/Inc/ms5837.h` | MS5837 驱动接口 + 初始化状态码 |
| `Core/Src/ms5837.c` | MS5837 驱动：复位 / 读 PROM / CRC4 / 压力深度计算 |

### 3.2 修改的文件（2 个）

| 文件 | 改了什么 |
|---|---|
| `Core/Src/main.c` | 加 `#include`；`USER CODE 2` 里加 `Soft_I2C_Init()` + `MS5837_init()` 并打印初始化结果；`while(1)` 里加读数 + 1 秒打印 |
| `cmake/stm32cubemx/CMakeLists.txt` | 把 `soft_i2c.c`、`ms5837.c` 加进 `MX_Application_Src`，让 CMake 编译它们 |

### 3.3 没有动的文件

CubeMX 生成的外设文件（`gpio.c`、`usart.c`、`stm32f4xx_it.c`、`stm32f4xx_hal_msp.c`、
时钟配置、`startup_stm32f407xx.s`、`STM32F407xx_FLASH.ld`、链接脚本、工具链 cmake 文件）
**全部未改动**。本项目没有 I2C 外设，PB6/PB7 是空闲引脚，由 `soft_i2c.c` 自己配置。

## 4. 写代码的流程（思路记录）

1. **确定功能目标**：与 `Depthsensor` 一致 —— 输出 `P(mbar)` 和 `Depth(cm)`。
2. **找软件 I2C 参考**：旧工程 `D:\codeprofiles\STM32C\depth` 里有 `soft_i2c.c`
   和 `ms5837_soft.c`（软件 I2C 移植版），拿它做时序模板。
3. **改写 `soft_i2c.c`**：
   - 引脚保持 PB6=SCL、PB7=SDA；
   - 去掉对 `hi2c1` 的依赖（本项目没有 I2C 外设），自己开 `GPIOB` 时钟、配推挽输出；
   - 延时从「NOP 空转循环」换成 **DWT 周期计数器**，按 16MHz 精确计微秒。
4. **写 `ms5837.c`**：
   - 从 `Depthsensor/ms5837.c` 取压力和深度算法（保证结果一致）；
   - 把 `HAL_I2C_*` 调用换成 `Soft_I2C_*`；
   - 中间量改成**有符号 `int64_t`**（修掉原工程 `uint64_t dT` 在负温度时溢出的隐患）；
   - 增加 **CRC4 校验**，能发现 I2C 时序不稳导致的 PROM 读错。
5. **接线进 `main.c`**：`USER CODE 2` 里先 `Soft_I2C_Init()` 再 `MS5837_init()`，
   主循环读数据并按 1 秒打印。
6. **注册到 CMake**：把两个新 `.c` 加进 `cmake/stm32cubemx/CMakeLists.txt`。

## 5. 硬件接线

| 传感器引脚 | 接到 STM32 |
|---|---|
| VCC | 3.3V（MS5837 最高 3.6V，**不能接 5V**） |
| GND | GND |
| SCL | PB6 |
| SDA | PB7 |

> 不需要外接上拉电阻（软件 I2C 用推挽输出）。若之后想切回硬件 I2C，才需要在
> SDA/SCL 上各焊一个 4.7kΩ 上拉电阻到 3.3V。

## 6. 编译 & 烧录

用 CMake（和同目录 `line_serial` 一样）：

```bash
cmake --preset Debug
cmake --build --preset Debug
```

前提：`arm-none-eabi-gcc` 和 `ninja` 在 PATH 里。烧录生成的 `build/Debug/ms_soft.elf`。

## 7. 预期输出 & 排错

上电后串口（USART1，115200）先打印一行初始化结果，再每 1 秒打印一行：

```
MS5837 init=0, P=974 mbar
P=974.2 mbar  Depth=0.0 cm
P=974.6 mbar  Depth=0.4 cm
P=975.1 mbar  Depth=0.9 cm
...
```

**初始化状态码 `init=N` 含义（排错关键）：**

| 值 | 含义 | 说明 |
|---|---|---|
| 0 | 成功 | 传感器已应答、PROM/CRC 正常 |
| 1 | 复位无应答 | SDA/SCL 接反？供电 3.3V？地址不对？ |
| 2 | 读 PROM 失败 | 时序不稳或接线接触不良 |
| 3 | CRC4 校验失败 | 能读到数据但内容错，多为 SDA/SCL 干扰 |
| 4 | 零位校准失败 | 已能读到 PROM，但压力值不合理 |

- 若 `init=1`：先查接线（SDA/SCL 是否接反）、供电、GND 共地。
- 若 `init=0` 但后面 `Depth` 一直 0：说明在陆地上（压力≈大气压），属正常。

## 8. 与硬件 I2C 版（`Depthsensor`）的对应关系

| 项 | Depthsensor（硬件 I2C） | 本工程 ms_soft（软件 I2C） |
|---|---|---|
| I2C 实现 | HAL `hi2c1`，PB6/PB7 开漏 | `soft_i2c.c` 推挽模拟，PB6/PB7 |
| 上拉电阻 | 必须有 | 不需要 |
| 地址 | `0xEC`（7 位 0x76 左移） | `0xEC`（写）/`0xED`（读） |
| 压力/深度算法 | 相同 | 相同 |
| 输出格式 | `P=… mbar Depth=… cm` | 相同（多一行 init 状态） |

## 9. 传感器型号确认：02BA 不是 30BA（20 倍修正）

最初按 `Depthsensor` 的假设写成 **30BA**，烧录后串口读出 `P=19478 mbar`，
而地面大气压应在 ~1013 mbar（海拔高一点会低到 ~970 mbar），明显偏大。

排查结论：**实物是 MS5837-02BA，不是 30BA**。两者 PROM 格式、I2C 协议完全一样，
但压力公式不同：

| 项 | 30BA（误用） | 02BA（正确） |
|---|---|---|
| OFF | `C2·2^16 + C4·dT/2^7` | `C2·2^17 + C4·dT/2^6` |
| SENS | `C1·2^15 + C3·dT/2^8` | `C1·2^16 + C3·dT/2^7` |
| P 求值 | `(D1·SENS/2^21 − OFF) / 2^13 / 10` | `(D1·SENS/2^21 − OFF) / 2^15 / 100` |
| 二阶(低温) | `3 / 3 / 5` 系数 | `11 / 31 / 63` 系数 |

02BA 的 OFF、SENS 恰好是 30BA 的 2 倍，再叠加除数 `2^13→2^15` 与 `/10→/100` 的差异，
套错公式的结果**恰好是正确值的 20 倍**。这也和日志吻合：`19478 / 20 ≈ 974 mbar`，
即当地真实大气压。

进一步确认方法（任选其一）：
- 看芯片封装丝印：上面直接印着 `02BA` 或 `30BA`；
- 打印 PROM 的 `C1`：02BA 的 C1 典型值约 **46372**，30BA 约 32000 上下，差别很大。
