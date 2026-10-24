# 代码阅读路线与热复位状态机对比

本次修改只补充中文注释和阅读说明，没有改公式、采样间隔、标定条件、状态跳转或故障恢复行为。
注释前的文件另存于本次工作区 `work/before-reading-comments`；更早的原版三个文件备份仍保留。

## 先读正常流程

```
main 主循环
  DevicesApp_Process
    App_ProcessDepth                 深度驱动推进，成功后保存深度
    App_ProcessMotion                姿态读取应用状态机
      JY61P_Process                  两段 DMA 读取的底层状态机
      JY61P_GetReadResult             消费一份已完成结果
      App_UpdateAttitude             只有读取成功才调用
        AttitudeMath_Process         首10次平均；以后逐次换算
    App_ProcessUart                  每秒显示最新数据
```

数学模块阅读顺序：`AttitudeMath_Init -> AttitudeMath_Process -> FinishCalibration -> Solve`。
先看 Process 的正常分支，再看共享矩阵函数；最后看 INVALID/SINGULAR 和 DiscardPending。

数学模块不是另一个通信状态机：`ready=0` 时收集平均，`ready=1` 时使用固定矩阵。
`calibration_count` 是已累计样本数，`calibration_restarts` 是未完成累计因故障被清掉的次数。
晃动阈值已在上一版删除，本次没有恢复它。

## 三层状态分别负责什么

| 层级 | 正常状态/阶段 | 故障路径 |
|---|---|---|
| device_app 姿态应用 | BOOT_WAIT → START_READ → WAIT_READ → START_READ | 失败进入 RETRY_WAIT，1秒后申请重试 |
| jy61p 底层读取 | IDLE → START_MOTION → WAIT_MOTION → START_ANGLES → WAIT_ANGLES → DATA_READY → IDLE | 启动失败、接收错误或100ms等待超时进入 RECOVER → FAILED → IDLE |
| attitude_math 数学 | Init → 累计10次 → 建立矩阵一次 → 每次 Solve | 输入无效不发布本次结果；通信故障丢弃未完成平均；竖直机头不发布不唯一欧拉角 |

串口另外独立使用 `IDLE → START_TX → WAIT_TX → IDLE`；发送故障才进入 `RECOVER`。
getter 和格式化函数只读已有结果；不会推进采样，也不会增加平均次数。

## 对比了哪些旧代码

- 增加数学模块之前实际保存的 `device_app.c/.h`、`CMakeLists.txt`。
- 工程自带 `Core/Src/Backup/jy61p.c.bak`、`device_app.c.bak`。
- 修改前源码恢复版里的 `jy61p.c`。
- 同级旧单独工程 `JY6P/Core/Src/jy61p.c` 与应用循环。

以下比较结论以本次加注释前的源文件为准；加注释后另检查代码 token 没有变化。

| 项目 | 数学接入前后结果 |
|---|---|
| JY61P 两段 DMA 读取、地址、寄存器、换算 | 未改；当前驱动与工程 .bak/恢复版字节一致 |
| 1500ms启动等待、100ms读取超时、失败后1秒重试 | 未改 |
| I2C2、DMA、中断、main.c、电机 PWM | 未改 |
| 成功取得完整数据后的处理 | 增加数学换算；原始数据继续保留 |
| 失败后的处理 | 增加机体数据失效标志、丢弃未完成平均；未改变I2C恢复步骤 |
| 每秒输出 | 增加 BODY 一行，缓冲区从512扩大到768字节 |

与 JY6P 单独工程比较，回调改为合并工程统一转发，并增加只读错误快照接口。
JY6P 单独应用失败后立即再次申请读取，合并版失败后等1秒；这个差别在数学接入前已经存在。
两者正常状态都连续采样。

因此，不能把“新版本遇到了、以前没遇到”直接解释成底层状态跳转被改坏。
数学换算与更长的输出确实增加了运行时间，但没有实机时序测量，不能认定它们使哪一步失效。

## 已确认的故障位置与恢复缺口

之前的日志 `status=2 err=0x00000020` 对应本地 HAL 的 `HAL_BUSY` 和 `HAL_I2C_ERROR_TIMEOUT`。
`HAL_I2C_Mem_Read_DMA()` 在等 I2C 总线 BUSY 标志清零失败时会返回这组结果。
这发生在获得完整有效数据之前；此时 `App_UpdateAttitude()` 尚未执行。

注意区分两个 BUSY：

- `JY61P_StartReadData()` 返回 BUSY：驱动本来就在处理请求，应用转入等待。
- 已完成读取结果的 `HAL_BUSY + 0x20`：底层等总线空闲超时，应用进入失败重试。

`JY61P_RecoverI2C()` 现有恢复顺序是：

```
关本地相关中断 → 禁止I2C DMA → 确认DMA停止 → DeInit
→ RCC复位I2C2 → 清挂起事件 → Init → 开DMA中断
```

它没有检查 PF0/SDA、PF1/SCL 的电平，没有发送总线清除时钟，也没有生成 GPIO STOP。
本地 `HAL_I2C_Init()` 成功只说明 MCU 外设重新配置成功，不代表外部总线已空闲。

**目前最符合现象的原因是：** 不断电烧录可能在某个字节传输中复位 STM32，仍供电的
传感器没有同时复位，继续保持之前的传输状态。如果它拉住 SDA 等待后续时钟，仅重置
MCU I2C2 不会使该字节结束，因此新程序可能持续 BUSY；一起断电上电后从机状态也重置。

这与用户之前“一起重新上电后恢复”的观察一致；但没有 SDA/SCL 电平或波形，仍是
待验证的具体原因，不是已证明的唯一原因。旧恢复代码也存在这个缺口。

## 下一步如何验证或修复

故障时先记录 I2C2 SR1/SR2、PF0/SDA 与 PF1/SCL 电平，确认是总线线低还是外设状态问题。
这些读取属于诊断建议，本次没有改变运行时日志或新增 GPIO 操作。

若确认 SDA 被拉低，可在独立故障恢复函数中使用开漏 GPIO、至多9个 SCL 时钟和 STOP
释放总线，再恢复 AF/I2C/DMA；每次等待必须有超时，恢复重试也应有间隔。
若 SCL 持续低，或9拍后 SDA 仍不释放，则仍可能需要从机硬件复位/重新上电。
依据：[NXP UM10204，第3.1.16节 Bus clear](https://www.nxp.com/docs/en/user-guide/UM10204.pdf#page=19)。

没有把 STM32F1 的特定模拟滤波器 workaround 套到 F407 上；对照的 F407 文档是
[ST ES0182](https://www.st.com/resource/en/errata_sheet/es0182-stm32f405407xx-and-stm32f415417xx-device-errata-stmicroelectronics.pdf)。

如果后续实施总线恢复，适合模拟的测试包括：SDA在第1/5/9拍释放、SCL持续低、
SDA一直低、DMA还没停止、错误快照保留与恢复后首次采样。真实不断电烧录仍需实机验证。

## 本次注释修改验证

六个 C/H 文件剔除注释后，执行代码 token 与修改前逐项相同；STM32F407 Debug
编译链接通过，编译前后的 `.text` 段哈希相同。因此本次只改变阅读内容，不改变程序行为。
