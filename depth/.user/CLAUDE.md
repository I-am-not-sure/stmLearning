硬件： STM32F103C8T6，HAL 库
深度传感器MS5837连接I2C1，串口通信模块连接USART2，已连接ST-Link
已开启：Generate peripheral initialization as a pair of '.c/.h' files per peripheral

项目实现：每隔一秒获取深度传感器数据，并通过串口发送给电脑

I2c开发流程:定义一对MS5837的.c/.h文件，并在里面编写代码，最后在主程序中使用相关函数。如果对流程不确定，可以参考"D:\codeprofiles\STM32\iic"（我的aht20传感器项目）

可参考的文件：
- "C:\Users\Iamno\Desktop\stm资料\深度传感器数据表\ENG_DS_MS5837-02BA01_A8.pdf"（pdf如果打不开就调用skills阅读）
- "C:\Users\Iamno\Desktop\stm资料\例程\STM32\B02 深度传感器开发套件测试例程 V1.2"
- "C:\Users\Iamno\Desktop\stm资料\dk\02电控程序"（这是去年学长机器人的全套代码，目前需要参考的是ms5837.c等文件，因为目前仅需要让深度传感器工作起来即可）