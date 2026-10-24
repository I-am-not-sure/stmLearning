# MS5837 连续采样状态机回归

本测试在电脑上模拟 HAL 和 DMA 完成通知，直接编译项目当前的 `ms5837.c` 和 `ms5837.h`，不访问真实硬件，不参与固件编译。每组测试均从完整的复位、PROM/CRC 和基准流程进入正常测量。

从 `all_device` 项目根目录运行（需要主机 GCC，不使用 ARM 交叉编译器）：

```powershell
gcc -std=c11 -Wall -Wextra -Werror -pedantic -I Core/Src -I Core/Inc tests/ms5837_continuous/test_ms5837_continuous.c -o "$env:TEMP/ms5837_continuous_test.exe"
if ($LASTEXITCODE -ne 0) { throw 'Host test compilation failed' }
& "$env:TEMP/ms5837_continuous_test.exe"
if ($LASTEXITCODE -ne 0) { throw 'Host test failed' }
```

预期输出：`ms5837_continuous: 7 test groups passed`。

覆盖零间隔启动、D1/D2 各20ms转换等待、未取走样本时禁止覆盖、取走后连续开始下一轮、正数采样间隔兼容，以及 I2C 错误、零 ADC、DMA 超时后的故障冻结。`hal_stub.h` 仅供这份主机测试使用，提前定义 CubeMX `main.h` 的保护宏，使生产头文件在电脑上也可编译。

真实采样速度仍取决于硬件通信和主循环耗时，主机测试不能代替上板验证。
