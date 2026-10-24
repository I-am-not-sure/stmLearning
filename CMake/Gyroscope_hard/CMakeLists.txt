cmake_minimum_required(VERSION 3.22)

# 硬件 I2C1 独立工程；PB6/PB7 由外设驱动，USART1 输出与软件版一致。
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
project(Gyroscope_hard LANGUAGES C ASM)

add_executable(${PROJECT_NAME}
    Core/Src/main.c
    Core/Src/i2c.c
    Core/Src/gpio.c
    Core/Src/usart.c
    Core/Src/stm32f1xx_it.c
    Core/Src/stm32f1xx_hal_msp.c
    Core/Src/system_stm32f1xx.c
    Core/Src/sysmem.c
    Core/Src/syscalls.c
    startup_stm32f103xb.s
)

# HAL I2C 与基础驱动；当前工程不编译软件 IIC。
set(HAL_DIR Drivers/STM32F1xx_HAL_Driver)
foreach(module IN ITEMS hal hal_gpio hal_uart hal_rcc hal_cortex hal_i2c)
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
