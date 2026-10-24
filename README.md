# STM32 学习记录

| 项目 | 工程类型 | 外设 / 模块 | 程序功能 |
| --- | --- | --- | --- |
| [depth](CMake/depth/) | CMake | 软件 I²C、MS5837、UART | 采集 MS5837 测量数据并经串口输出。 |
| [hygro_thermometer_](CMake/hygro_thermometer_/) | CMake | I²C、AHT20、OLED | 采集温湿度并在 OLED 上显示。 |
| [JY6P](CMake/JY6P/) | CMake | I²C、DMA、JY61P、UART | 通过 I²C DMA 读取 JY61P 数据并经串口输出。 |
| [line_ide_cmake](CMake/line_ide_cmake/) | CMake | UART、DMA、TIM1/TIM8、PWM | 解析串口命令并调整 TIM1/TIM8 的 PWM 输出。 |
| [oled](CMake/oled/) | CMake | I²C、OLED | 在 OLED 上显示文字和图形。 |
| [stm32_hal_aht20](CMake/stm32_hal_aht20/) | CMake | I²C、AHT20、UART | 读取 AHT20 温湿度并经串口输出。 |
| [TIM](CMake/TIM/) | CMake | TIM、UART | 通过定时器中断计数并经串口观察结果。 |
| [TIM_ExternalInput](CMake/TIM_ExternalInput/) | CMake | TIM、外部脉冲、OLED | 对外部脉冲计数并在 OLED 上显示。 |
| [TIM_InputCapture](CMake/TIM_InputCapture/) | CMake | TIM、超声波模块、OLED | 通过输入捕获测距并在 OLED 上显示。 |
| [TIM_Motor](CMake/TIM_Motor/) | CMake | GPIO、TIM、DRV8833、编码器 | 控制电机方向并读取编码器计数。 |
| [blink](CubeIDE/blink/) | STM32CubeIDE | GPIO、LED | 通过 GPIO 控制 LED 闪烁。 |
| [depthf4](CubeIDE/depthf4/) | STM32CubeIDE | I²C、MS5837、UART | 读取 MS5837 压力，换算水深并经串口输出。 |
| [iic](CubeIDE/iic/) | STM32CubeIDE | I²C、AHT20、UART | 采集 AHT20 温湿度并经串口输出。 |
| [interrupt](CubeIDE/interrupt/) | STM32CubeIDE | GPIO、EXTI、LED | 通过外部中断响应按键并控制 LED。 |
| [JY61P](CubeIDE/JY61P/) | STM32CubeIDE | I²C、JY61P、UART | 读取 JY61P 数据并经串口输出。 |
| [K](CubeIDE/K/) | STM32CubeIDE | GPIO、按键 | 轮询按键并进行软件消抖，切换输出电平。 |
| [pwm_light](CubeIDE/pwm_light/) | STM32CubeIDE | TIM、PWM、按键 | 通过按键调节 PWM 输出。 |
| [serial](CubeIDE/serial/) | STM32CubeIDE | UART、DMA | 练习串口轮询、中断、DMA 和空闲接收。 |
| [stm32_hal_drv8833](CubeIDE/stm32_hal_drv8833/) | STM32CubeIDE | GPIO、TIM、DRV8833、编码器 | 控制电机方向并读取编码器计数。 |
| [STM_Servo](CubeIDE/STM_Servo/) | STM32CubeIDE | UART、TIM、PWM、舵机 | 接收串口角度指令并调节舵机。 |
