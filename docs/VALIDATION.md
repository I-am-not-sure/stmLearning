# 本次验证记录

整理日期：2026-10-24。

## 结构与内容检查

- 36 个工程均有独立 README、应用入口和分类目录：21 个 CMake、15 个 CubeIDE。
- 原仓库 3977 个已跟踪文件迁移后字节一致；另保留原 JY61P README 正文并增加索引，更新根忽略规则，修正 AHT20 构建文件清单。
- 新增工程的 95 个自定义源码/入口文件与原始来源逐字节一致。
- 检查 32 份 .project/.cproject XML：15 个 CubeIDE 工程和 AHT20 的双入口元数据均可解析。
- 检查 477 个本地 Markdown 文件链接，目标均存在；编译后才生成的固件路径改为普通代码文本。
- 用户指定的 pid_versionone 目录不存在于发布树；忽略规则可拦截该目录。
- 选入的自定义代码未发现 PID 函数实现，也未匹配待保密工程中控制单元的整文件哈希。
- 原有 MS5837 数据手册笔记另存于 docs；本机助手指引和缓存目录不作为新增发布内容。

## CMake 固件构建

使用本机已有 CMake 4.4.0、Ninja 1.13.2 和 GNU Tools for STM32 14.3.1。对每个工程分别指定 Ninja、自己的 gcc-arm-none-eabi.cmake 和 Debug 配置，从新的构建目录编译。21 个工程均通过。

这验证了独立工程的源码、包含路径、启动文件、链接脚本及链接结果；没有逐个验证原电脑的所有构建预设。AHT20 的预设名为小写 debug；line_929 的原预设仍保留 MinGW 路径，跨电脑构建方法见 BUILDING.md。

| 工程 | 配置与固件构建 |
| --- | --- |
| [CMake/all_device](../CMake/all_device/README.md) | 通过 |
| [CMake/depth](../CMake/depth/README.md) | 通过 |
| [CMake/depthf4](../CMake/depthf4/README.md) | 通过 |
| [CMake/Gyroscope_fast](../CMake/Gyroscope_fast/README.md) | 通过 |
| [CMake/Gyroscope_hard](../CMake/Gyroscope_hard/README.md) | 通过 |
| [CMake/Gyroscope_note](../CMake/Gyroscope_note/README.md) | 通过 |
| [CMake/hygro_thermometer_](../CMake/hygro_thermometer_/README.md) | 通过 |
| [CMake/JY6P](../CMake/JY6P/README.md) | 通过 |
| [CMake/line_929](../CMake/line_929/README.md) | 通过 |
| [CMake/line_ide_cmake](../CMake/line_ide_cmake/README.md) | 通过 |
| [CMake/line_mamba](../CMake/line_mamba/README.md) | 通过 |
| [CMake/line_mamba_pwm](../CMake/line_mamba_pwm/README.md) | 通过 |
| [CMake/line_serial](../CMake/line_serial/README.md) | 通过 |
| [CMake/ms_soft](../CMake/ms_soft/README.md) | 通过 |
| [CMake/oled](../CMake/oled/README.md) | 通过 |
| [CMake/skywalker](../CMake/skywalker/README.md) | 通过 |
| [CMake/stm32_hal_aht20](../CMake/stm32_hal_aht20/README.md) | 通过 |
| [CMake/TIM](../CMake/TIM/README.md) | 通过 |
| [CMake/TIM_ExternalInput](../CMake/TIM_ExternalInput/README.md) | 通过 |
| [CMake/TIM_InputCapture](../CMake/TIM_InputCapture/README.md) | 通过 |
| [CMake/TIM_Motor](../CMake/TIM_Motor/README.md) | 通过 |

### AHT20 原有清单修正

cmake/st-project.cmake 曾引用已不存在的 stm32f1xx_hal_tim.c 和 stm32f1xx_hal_tim_ex.c。该工程 HAL 配置本来就未启用 TIM，因此只删除了这两条失效构建引用。应用源码、HAL 配置及采集逻辑均未改动。

## 现有电脑端测试

使用本机 MinGW GCC，以 -std=c11 -Wall -Wextra -Werror -pedantic 编译并执行工程已有测试，7 个测试程序均通过：

| 工程 | 测试 | 结果 |
| --- | --- | --- |
| line_mamba | command | 通过 |
| line_mamba | dshot | 通过 |
| line_mamba_pwm | command | 通过 |
| line_mamba_pwm | pwm | 通过 |
| line_929 | protocol | 通过 |
| all_device | attitude | 通过 |
| all_device | ms5837_continuous | 通过 |

其中 MS5837 连续采样测试包含 7 组状态机检查；姿态数学测试检查数值变换和状态行为。测试不访问开发板或电调。

## 验证边界

本次没有连接硬件，也没有在 STM32CubeIDE 图形环境逐个执行构建。CubeIDE 部分完成了工程元数据与文件完整性检查；CMake 部分完成了交叉编译。原有接线、实测记录、待验证事项保留在各项目的说明中。
