# Gyroscope_fast：JY61P 软件 I²C 时序对比

以软件 I²C 读取 JY61P，保留读取时序和吞吐量对比记录。

| 项目 | 内容 |
| --- | --- |
| MCU | `STM32F103C8T6` |
| 构建入口 | CMake |
| 学习内容 | 软件 I²C、JY61P、时序测量、UART |
| 代码入口 | [`Core/Src/main.c`](Core/Src/main.c) |
| 来源 | `STM32C/Gyroscope_fast` |

## 建议阅读顺序

1. [`Core/Src/main.c`](Core/Src/main.c)：初始化、主循环和中断回调。
2. [`Core/Inc/soft_i2c.h`](Core/Inc/soft_i2c.h)
3. [`Core/Src/soft_i2c.c`](Core/Src/soft_i2c.c)

本目录没有 `.ioc`；引脚、时钟和外设初始化直接维护在源码中。

## 本工程说明

原说明记录使用 HSI 8 MHz；fast 指软件 I²C 时序实验，不把它误写成 72 MHz 时钟版本。该版本没有 .ioc。

## 打开与构建

[公共构建与导入指南](../../docs/BUILDING.md) · [全部工程与外设索引](../../README.md)

本次整理的验证范围见[验证记录](../../docs/VALIDATION.md)。

---

## 整理前的项目说明

原有说明保留如下。文中的固件产物、实测记录和本机路径属于原记录；本仓库发布源码，编译输出需自行生成。

# JY61P 软件 IIC：当前连线的实测速度

原 `Gyroscope` / `Gyroscope_note` 每 **1000 ms 输出一次**，相当于 1 帧/秒。软件 IIC 驱动里的 `TRANSFER_TIMEOUT_MS=100` 是一次事务的**故障超时上限**，并非采样周期。本目录是独立实验工程，未改动原目录；保留同样的 PB6/SCL、PB7/SDA、七位地址 `0x50`、两段寄存器读取、九项换算和 USART1 文本格式。

## 实测结论

在这块已连接的 STM32F103C8T6、JY61P、原接线、HSI 8 MHz、COM10 115200 下，连续读取测试每档统计两段各约 10 秒：

| 编译 | 每次边沿等待 | 完整读取并换算一次 | 连续读数率 | 两段错误数 |
| --- | ---: | ---: | ---: | ---: |
| Debug | 50 µs，原设置 | 约 51.37 ms | 19.5 次/秒 | 0 |
| Debug | 20 µs | 约 26.63 ms | 37.5 次/秒 | 0 |
| Debug | 10 µs | 约 19.72 ms | 50.7 次/秒 | 0 |
| Debug | 5 µs | 约 15.60 ms | 64.1 次/秒 | 0 |
| Debug | 2 µs | 约 12.85 ms | 77.8 次/秒 | 0 |
| Debug | 1 µs | 约 12.85 ms | 77.8 次/秒 | 0 |
| Release | 5 µs | 约 8.87 ms | 112.7 次/秒 | 0 |
| Release | 2 µs | 约 6.51 ms | 153.6 次/秒 | 0 |
| Release | 1 µs | 约 5.92 ms | **168.9 次/秒** | 0 |

这是主循环**不逐帧发串口**时，连续完成两段 IIC 读取和数值换算的速度。表中最快的 168.9 次/秒是本次**已测最高值**，不是该芯片、传感器或线路的绝对上限，也没有跨不同线长和供电条件验证。1 µs 时序裕量很小；交付固件选用经过连续读取验证的 **2 µs** 档。

完整输出 `ACC(g)=... GYRO(deg/s)=... ANGLE(deg)=...` 时，还需要浮点格式化与串口发送。115200、8N1 每个字符占 10 位，约 90 字符的一行在线路上约需 7.8 ms。现场连续采集 1000 帧的结果：

| Release 设置 | 实际 1000 帧用时 | 平均帧率 | 错误行 | 电脑端接收间隔 |
| --- | ---: | ---: | ---: | --- |
| 5 µs，目标 20 ms | 21.67 秒 | 46.10 帧/秒 | 0 | 平均 21.69 ms |
| **2 µs，目标 20 ms** | **19.91 秒** | **50.18 帧/秒** | **0** | 平均 19.93 ms，最小 16.4 / 最大 23.1 ms |

所以在现有接线上，**20 ms 一帧、约 50 帧/秒的完整九项文本输出已经实测跑通**，比 100 ms 一帧快 5 倍，比原程序 1000 ms 一帧快约 50 倍。电脑端的单帧到达间隔也含 USB 串口缓冲和 Windows 调度抖动，不能直接当作 STM32 采样时刻；平均帧率更有参考价值。

上面的“读数/秒”和“帧/秒”是 STM32 发起读取的次数。它们**不证明 JY61P 每次都生成了全新的内部测量**。传感器的刷新设置、滤波和两段读取是否跨更新点，本次未做独立验证。WIT 的[寄存器协议](https://wit-motion.gitbook.io/witmotion-sdk/wit-standard-protocol/wit-standard-communication-protocol)列有 `RRATE` 输出率配置，默认值为 10 Hz，但它不能仅凭主机轮询次数推断当前 IIC 寄存器数据的实际刷新率。本程序没有修改模块配置。若应用必须获得 50 Hz 独立新样本，应再检查传感器设置和时间戳/数据就绪机制。

## 使用与复现

默认配置为 **Release、2 µs 边沿等待、20 ms 输出周期**；板上当前运行的就是这个软件 IIC 版本。交付固件位于 `build/Debug/Gyroscope_fast.elf`（本机编译后生成）；同目录也提供 HEX 和 BIN（BIN 烧录起点 `0x08000000`）。在装有 CMake、Ninja、GNU Arm GCC 的终端中：

```powershell
cmake --preset Release
cmake --build --preset Release
```

修改边沿等待或输出周期时：

```powershell
cmake --preset Release -D EDGE_US=5 -D REPORT_MS=25
cmake --build --preset Release
```

`EDGE_US` 是每处额外等待，不是一个 IIC 位的总时长；实际总线频率没有在本次测试中用逻辑分析仪测量。若换线、加负载或改主频，需要重新验证时序。`REPORT_MS` 只规定主循环**期望**的输出间隔；一轮读取＋格式化＋串口发送比它长时，会自动降到实际可完成的速率。

要复现“只读取、不逐帧串口输出”的十秒统计：

```powershell
cmake --preset Release -D GYRO_BENCHMARK=ON -D EDGE_US=2
cmake --build --preset Release
```

此模式每十秒发送一行 `BENCH`，其中 `ok/err` 是成功和失败次数，`min/avg/max` 是成功读数的微秒耗时。切回正常输出时执行 `cmake --preset Release -D GYRO_BENCHMARK=OFF -D EDGE_US=2 -D REPORT_MS=20`，随后重新构建和烧录。编译成功只证明生成了固件；上表数字来自本次实际板上运行和 COM10 连续采集。长线、其他电源和长期运行的错误率仍需重新测试。
