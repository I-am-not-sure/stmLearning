# 项目筛选记录

扫描范围为本机 `STM32`、`STM32C`、`STM32F407ZGT6` 三个源目录。按独立工程入口去除 EWARM/MDK 子目录后，共检查 72 个工程目录，另外单独排除用户指定不公开的工程。

## 本轮选择

保留并分类原仓库 16 个工程，补充 20 个具有明确外设、通信方式或协议差异的工程。原始三个源目录不作移动和修改。

## 暂不纳入的内容

- 用户指定的 `STM32C/cmake_enemy/pid_versionone` 明确排除，仓库忽略规则保留目录防护。
- `pid_pitchRoll`、`version_two_cmake`、`one_one` 及其失败/旧版本涉及 PID 或综合控制，本轮先不公开。
- `Senior` 中的独立 MDK/Keil 工程保留在原目录，后续可单独规划 Keil 分区或移植，不混写为 CubeIDE。
- 原始嵌套 Git、编译目录、烧录产物、Flash 备份、工具缓存和聊天记录不随新增工程导入。
- 同一应用的不同构建入口按需要保留对照：`line_serial` 两个入口；原有 `TIM_Motor` / `stm32_hal_drv8833`。相同构建方式的相同应用副本不重复添加。

## 逐项结果

“相同”基于主程序用户代码与自定义源文件内容比对；“同主题变体”不表示源码完全相同。

| 原目录中的工程 | 构建类型 | 本轮结果 |
| --- | --- | --- |
| `STM32/abc` | cubeide | 基础模板或同类 GPIO 实验，现有 blink 等已覆盖；暂不增加重复入口。 |
| `STM32/blink` | cubeide | 应用源码相同，保留 CubeIDE/blink；不重复导入此副本。 |
| `STM32/depth` | cmake | 应用源码相同，保留 CMake/depth；不重复导入此副本。 |
| `STM32/depthf1` | cubeide | [纳入 `CubeIDE/depthf1`](../CubeIDE/depthf1/README.md)：读取 MS5837 的 PROM/CRC 和深度测量，串口输出初始化与错误状态。 |
| `STM32/depthf1_novibe` | cubeide | 同主题的早期/调试/引脚变体；本轮优先保留接口、协议或构建方式有明确差异的版本。 |
| `STM32/depthf4` | cubeide | [纳入 `CubeIDE/depthf4`](../CubeIDE/depthf4/README.md)：读取 MS5837，采集参考压力并换算深度，通过串口输出。 |
| `STM32/depthf4_soft` | cubeide | [纳入 `CubeIDE/depthf4_soft`](../CubeIDE/depthf4_soft/README.md)：用软件 I²C 实现 MS5837 参考压力与深度采集流程。 |
| `STM32/Depthsensor` | cubeide | 同主题的早期/调试/引脚变体；本轮优先保留接口、协议或构建方式有明确差异的版本。 |
| `STM32/gyroscope` | cubeide | 基础模板或同类 GPIO 实验，现有 blink 等已覆盖；暂不增加重复入口。 |
| `STM32/hello` | cubeide | 基础模板或同类 GPIO 实验，现有 blink 等已覆盖；暂不增加重复入口。 |
| `STM32/hygro_thermometer_` | cmake | 应用源码相同，保留 CMake/hygro_thermometer_；不重复导入此副本。 |
| `STM32/Hyw` | cubeide | 基础模板或同类 GPIO 实验，现有 blink 等已覆盖；暂不增加重复入口。 |
| `STM32/I2C_OLED_Dino` | cubeide | [纳入 `CubeIDE/I2C_OLED_Dino`](../CubeIDE/I2C_OLED_Dino/README.md)：用 OLED 帧缓冲、字库和按键实现小恐龙游戏交互。 |
| `STM32/iic` | cubeide | 应用源码相同，保留 CubeIDE/iic；不重复导入此副本。 |
| `STM32/interrupt` | cubeide | 应用源码相同，保留 CubeIDE/interrupt；不重复导入此副本。 |
| `STM32/jiangke` | cubeide | 基础模板或同类 GPIO 实验，现有 blink 等已覆盖；暂不增加重复入口。 |
| `STM32/JY61P` | cubeide | 应用源码相同，保留 CubeIDE/JY61P；不重复导入此副本。 |
| `STM32/JY61P_novibe` | cubeide | 同主题的早期/调试/引脚变体；本轮优先保留接口、协议或构建方式有明确差异的版本。 |
| `STM32/K` | cubeide | 应用源码相同，保留 CubeIDE/K；不重复导入此副本。 |
| `STM32/Keysking_Mamba_DShot300` | cubeide | [纳入 `CubeIDE/Keysking_Mamba_DShot300`](../CubeIDE/Keysking_Mamba_DShot300/README.md)：使用 GPIO/DWT 发送两路 DShot300，按键控制并输出串口状态。 |
| `STM32/line_ide` | cubeide | 同主题的早期/调试/引脚变体；本轮优先保留接口、协议或构建方式有明确差异的版本。 |
| `STM32/line_serial` | cubeide | [纳入 `CubeIDE/line_serial`](../CubeIDE/line_serial/README.md)：通过 USART1 的空闲接收和 DMA 发送实现数据回显与接收重启。 |
| `STM32/oled` | cmake | 应用源码相同，保留 CMake/oled；不重复导入此副本。 |
| `STM32/one_one/one_one` | cmake | 包含/关联 PID 或综合控制实现，本轮暂不公开，保留在原始目录。 |
| `STM32/pwm_light` | cubeide | 应用源码相同，保留 CubeIDE/pwm_light；不重复导入此副本。 |
| `STM32/serial` | cubeide | 应用源码相同，保留 CubeIDE/serial；不重复导入此副本。 |
| `STM32/skywalker` | cmake | [纳入 `CMake/skywalker`](../CMake/skywalker/README.md)：使用 TIM3 输出油门 PWM，并通过串口报告启动、短时运行和停止状态。 |
| `STM32/STM32codeDemo` | cmake | 基础模板或同类 GPIO 实验，现有 blink 等已覆盖；暂不增加重复入口。 |
| `STM32/stm32_hal_aht20` | cmake / CubeIDE 双入口 | 应用源码相同，保留 CMake/stm32_hal_aht20；不重复导入此副本。 |
| `STM32/stm32_hal_drv8833` | cubeide | 应用源码相同，保留 CubeIDE/stm32_hal_drv8833；不重复导入此副本。 |
| `STM32/STM_Servo` | cubeide | 应用源码相同，保留 CubeIDE/STM_Servo；不重复导入此副本。 |
| `STM32/t60edu` | cubeide | 同主题的早期/调试/引脚变体；本轮优先保留接口、协议或构建方式有明确差异的版本。 |
| `STM32/TIM` | cmake | 应用源码相同，保留 CMake/TIM；不重复导入此副本。 |
| `STM32/TIM_ExternalInput` | cmake | 应用源码相同，保留 CMake/TIM_ExternalInput；不重复导入此副本。 |
| `STM32/TIM_Motor` | cmake | 应用源码相同，保留 CMake/TIM_Motor；不重复导入此副本。 |
| `STM32/TIM_SlaveMode` | cmake | 定时器从模式配置实验，主循环缺少展示逻辑；补充验证过程和现象后再纳入。 |
| `STM32C/cmake_enemy/all_device` | cmake | [纳入 `CMake/all_device`](../CMake/all_device/README.md)：组合 MS5837 与 JY61P 状态机，进行姿态数学处理和串口 DMA 输出。 |
| `STM32C/cmake_enemy/depthf4` | cmake | [纳入 `CMake/depthf4`](../CMake/depthf4/README.md)：使用 I²C DMA 回调推进 MS5837 采样，并通过串口 DMA 输出数据。 |
| `STM32C/cmake_enemy/JY6P` | cmake | [纳入 `CMake/JY6P`](../CMake/JY6P/README.md)：通过 I²C Mem_Read_DMA 采样 JY61P，结合串口 DMA 与错误恢复。 |
| `STM32C/cmake_enemy/line_ide_cmake` | cmake | [纳入 `CMake/line_ide_cmake`](../CMake/line_ide_cmake/README.md)：接收串口空闲 DMA 数据，解析命令并设置 TIM1/TIM8 的 PWM 比较值。 |
| `STM32C/cmake_enemy/line_serial` | cmake | [纳入 `CMake/line_serial`](../CMake/line_serial/README.md)：通过 USART1 的空闲接收和 DMA 发送实现数据回显与接收重启。 |
| `STM32C/cmake_enemy/ms_ms` | cmake | 同主题的早期/调试/引脚变体；本轮优先保留接口、协议或构建方式有明确差异的版本。 |
| `STM32C/cmake_enemy/ms_soft` | cmake | [纳入 `CMake/ms_soft`](../CMake/ms_soft/README.md)：分离软件 I²C 与 MS5837 驱动，周期采集后经串口报告。 |
| `STM32C/cmake_enemy/ms_soft_i2c2` | cmake | 同主题的早期/调试/引脚变体；本轮优先保留接口、协议或构建方式有明确差异的版本。 |
| `STM32C/cmake_enemy/pid_pitchRoll` | cmake | 包含/关联 PID 或综合控制实现，本轮暂不公开，保留在原始目录。 |
| `STM32C/cmake_enemy/t60edu` | cmake | 同主题的早期/调试/引脚变体；本轮优先保留接口、协议或构建方式有明确差异的版本。 |
| `STM32C/cmake_enemy/version_two_cmake` | cmake | 包含/关联 PID 或综合控制实现，本轮暂不公开，保留在原始目录。 |
| `STM32C/depth` | cmake | 应用源码相同，保留 CMake/depth；不重复导入此副本。 |
| `STM32C/Gyroscope` | cmake | 同主题的早期/调试/引脚变体；本轮优先保留接口、协议或构建方式有明确差异的版本。 |
| `STM32C/Gyroscope_fast` | cmake | [纳入 `CMake/Gyroscope_fast`](../CMake/Gyroscope_fast/README.md)：以软件 I²C 读取 JY61P，保留读取时序和吞吐量对比记录。 |
| `STM32C/Gyroscope_hard` | cmake | [纳入 `CMake/Gyroscope_hard`](../CMake/Gyroscope_hard/README.md)：用 I2C1 读取 JY61P，包含线路诊断、BUSY 恢复和错误输出。 |
| `STM32C/Gyroscope_note` | cmake | [纳入 `CMake/Gyroscope_note`](../CMake/Gyroscope_note/README.md)：把软件 I²C、JY61P 解码和时钟初始化分模块，保留逐步阅读指南。 |
| `STM32C/line` | cmake | 同主题的早期/调试/引脚变体；本轮优先保留接口、协议或构建方式有明确差异的版本。 |
| `STM32C/line_929` | cmake | [纳入 `CMake/line_929`](../CMake/line_929/README.md)：用 GPIO/DWT 生成 DShot300 帧，配合串口命令和协议测试。 |
| `STM32C/line_claude` | cmake | 同主题的早期/调试/引脚变体；本轮优先保留接口、协议或构建方式有明确差异的版本。 |
| `STM32C/line_fly` | cmake | 同主题的早期/调试/引脚变体；本轮优先保留接口、协议或构建方式有明确差异的版本。 |
| `STM32C/line_line_new` | cmake | 同主题的早期/调试/引脚变体；本轮优先保留接口、协议或构建方式有明确差异的版本。 |
| `STM32C/line_mamba` | cmake | [纳入 `CMake/line_mamba`](../CMake/line_mamba/README.md)：用 TIM3 产生 DShot 位脉冲，解析串口命令并控制四路电调。 |
| `STM32C/line_mamba_pwm` | cmake | [纳入 `CMake/line_mamba_pwm`](../CMake/line_mamba_pwm/README.md)：使用 TIM3 输出传统 PWM，以串口命令调整四路电调指令。 |
| `STM32C/line_oneone` | cmake | 同主题的早期/调试/引脚变体；本轮优先保留接口、协议或构建方式有明确差异的版本。 |
| `STM32C/one_one_failversion` | cmake | 包含/关联 PID 或综合控制实现，本轮暂不公开，保留在原始目录。 |
| `STM32C/pwmlinght` | incomplete | 缺少完整 CMake/CubeIDE 工程入口，建议补齐后再纳入。 |
| `STM32C/Senior/only_bmi088` | keil | MDK/Keil 工程，暂不混入 CMake/CubeIDE 分类；可另建 Keil 分区或单独移植。 |
| `STM32C/Senior/only_IST` | keil | MDK/Keil 工程，暂不混入 CMake/CubeIDE 分类；可另建 Keil 分区或单独移植。 |
| `STM32C/Senior/only_ms` | keil | MDK/Keil 工程，暂不混入 CMake/CubeIDE 分类；可另建 Keil 分区或单独移植。 |
| `STM32C/Senior/only_pwm` | keil | MDK/Keil 工程，暂不混入 CMake/CubeIDE 分类；可另建 Keil 分区或单独移植。 |
| `STM32C/Senior/version_one` | keil | MDK/Keil 工程，暂不混入 CMake/CubeIDE 分类；可另建 Keil 分区或单独移植。 |
| `STM32C/Senior/version_two` | keil | MDK/Keil 工程，暂不混入 CMake/CubeIDE 分类；可另建 Keil 分区或单独移植。 |
| `STM32C/teammate_Mamba_DShot300/Keysking_Mamba_DShot300` | cubeide | 应用源码相同，保留 CubeIDE/Keysking_Mamba_DShot300；不重复导入此副本。 |
| `STM32C/TIM_InputCapture` | cmake | [纳入 `CMake/TIM_InputCapture`](../CMake/TIM_InputCapture/README.md)：产生 Trig 脉冲，使用 TIM1 双通道捕获 Echo 边沿，计算距离并显示到 OLED。 |
| `STM32F407ZGT6/one_one` | cmake | 包含/关联 PID 或综合控制实现，本轮暂不公开，保留在原始目录。 |
| `STM32F407ZGT6/origin` | cmake | 工程主体为启动模板；另有 VSCode 无线烧录说明，可后续单独提炼为技术笔记，本轮不整体导入其对话记录。 |
