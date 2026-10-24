# 打开与构建

## CMake 工程

需要 CMake、GNU Arm Embedded 工具链（`arm-none-eabi-gcc` 等）和所选构建后端。多数工程使用 Ninja；`line_929` 的原预设使用 MinGW Makefiles，且含原电脑的 make 路径。

从仓库根目录进入具体工程再执行预设，例如：

```powershell
Set-Location CMake/TIM_InputCapture
cmake --preset Debug
cmake --build --preset Debug
```

预设名称区分大小写：`stm32_hal_aht20` 使用小写 `debug` / `release`，应执行 `cmake --preset debug` 和 `cmake --build --preset debug`；它的输出目录是 `build/debug/build/`。其他工程通常使用 `Debug`。

编译结果通常在该工程的 `build/Debug/`。仓库不上传 ELF/HEX/BIN、CMake 缓存或实物 Flash 备份。

### 换目录后重新配置，或改用 Ninja

原有 `build` 缓存会记录整理前的绝对目录。首次整理后，可以使用一个新的构建目录；下面的命令也可绕过 `line_929` 原预设中的本机 make 路径：

```powershell
cmake -S . -B build/portable -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake -DCMAKE_BUILD_TYPE=Debug
cmake --build build/portable
```

需要使用原 Debug 预设并清理配置缓存时，CMake 3.24 及以上可以执行 `cmake --fresh --preset Debug`，再执行构建命令。先确认自己的 CMake 版本，不要复用其他目录复制来的缓存。

`Gyroscope_note`、`Gyroscope_fast`、`Gyroscope_hard` 没有 `.ioc`，它们的时钟、GPIO 和外设初始化直接维护在源码中。`stm32_hal_aht20` 同时保留 CubeIDE 元数据，既可按 CMake 构建，也可按下节导入。

## STM32CubeIDE 工程

1. 打开 STM32CubeIDE。
2. 选择 `File > Import > General > Existing Projects into Workspace`。
3. 选择 `CubeIDE` 下的某个工程目录，或选择 `CubeIDE` 并搜索子目录。
4. 以工程自带 `.project` / `.cproject` 导入，再执行 Clean / Build。

不要直接把整个仓库作为一个固件工程导入。只有 `.ioc` 的文件夹不自动成为 CubeIDE 工程；本轮没有把原来的 Keil/MDK 工程伪装成 CubeIDE 工程。

## 阅读源码与配置

- `Core/Src/main.c`：初始化、主循环与应用回调。
- `Core/Inc` / `Core/Src`：自定义驱动及应用模块，具体入口在各项目 README 中列出。
- `.ioc`：CubeMX 配置；与源码同时查看，区分“配置了外设”和“应用实际使用了外设”。
- `cmake/`、启动文件和链接脚本：CMake 工程构建依赖，保留在各子目录内。
- `Drivers/`：随工程保留的 HAL/CMSIS，不能当作编译缓存删除。

本轮没有更改 GPIO、传感器算法、电调参数或应用运行逻辑。原有接线和调试说明保留在各工程 README 的原文部分或链接中。
