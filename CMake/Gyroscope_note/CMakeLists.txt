cmake_minimum_required(VERSION 3.22)

# 独立的 GCC 学习工程：不依赖旁边的 Gyroscope，也不需要 CubeMX 重新生成。
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
project(Gyroscope_note LANGUAGES C ASM)

add_executable(${PROJECT_NAME}
    Core/Src/main.c
    Core/Src/clock.c
    Core/Src/jy61p.c
    Core/Src/soft_i2c.c
    Core/Src/gpio.c
    Core/Src/usart.c
    Core/Src/stm32f1xx_it.c
    Core/Src/stm32f1xx_hal_msp.c
    Core/Src/system_stm32f1xx.c
    Core/Src/sysmem.c
    Core/Src/syscalls.c
    startup_stm32f103xb.s
)

# 仅编译实际参与当前固件的五个 HAL 模块；其它接口由链接器按函数裁剪。
set(HAL_DIR Drivers/STM32F1xx_HAL_Driver)
foreach(module IN ITEMS hal hal_gpio hal_uart hal_rcc hal_cortex)
    target_sources(${PROJECT_NAME} PRIVATE ${HAL_DIR}/Src/stm32f1xx_${module}.c)
endforeach()
target_include_directories(${PROJECT_NAME} PRIVATE
    Core/Inc
    ${HAL_DIR}/Inc
    ${HAL_DIR}/Inc/Legacy
    Drivers/CMSIS/Device/ST/STM32F1xx/Include
    Drivers/CMSIS/Include
)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    USE_HAL_DRIVER STM32F103xB $<$<CONFIG:Debug>:DEBUG>
)
# newlib-nano 默认不链接浮点格式化；串口的 %.3f 需要这个选项。
target_link_options(${PROJECT_NAME} PRIVATE -u _printf_float)
target_link_libraries(${PROJECT_NAME} PRIVATE ${TOOLCHAIN_LINK_LIBRARIES})

# ELF 用于下载/调试；HEX 自带地址，BIN 烧录时需指定 0x08000000。
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${PROJECT_NAME}> ${PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}> ${PROJECT_NAME}.bin
    VERBATIM
)
set_target_properties(${PROJECT_NAME} PROPERTIES
    ADDITIONAL_CLEAN_FILES "${PROJECT_NAME}.map;${PROJECT_NAME}.hex;${PROJECT_NAME}.bin"
)
