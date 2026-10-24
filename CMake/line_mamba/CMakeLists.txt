cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME line_mamba)
set(MAMBA_DSHOT_RATE_KHZ "300" CACHE STRING "DShot bit rate: 150 or 300 kbit/s")
option(MAMBA_DSHOT_NORMAL_TEST "One-way DShot throttle without 3D command" OFF)
if(NOT MAMBA_DSHOT_RATE_KHZ STREQUAL "150" AND
   NOT MAMBA_DSHOT_RATE_KHZ STREQUAL "300")
    message(FATAL_ERROR "MAMBA_DSHOT_RATE_KHZ must be 150 or 300")
endif()

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    Core/Src/mamba_dshot.c
    Core/Src/mamba_dshot_codec.c
    Core/Src/mamba_app.c
    Core/Src/mamba_command.c
)

# DShot300 has only 560 CPU cycles per bit, including all four CCR updates.
# Keep the waveform code optimized even when the rest of the build is Debug.
set_source_files_properties(Core/Src/mamba_dshot.c PROPERTIES COMPILE_OPTIONS "-O2")

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    MAMBA_DSHOT_RATE_KHZ=${MAMBA_DSHOT_RATE_KHZ}
    MAMBA_DSHOT_NORMAL_TEST=$<BOOL:${MAMBA_DSHOT_NORMAL_TEST}>
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)
