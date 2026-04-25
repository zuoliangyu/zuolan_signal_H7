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
set(CMAKE_PROJECT_NAME zuolan_signal_STM32)

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

# ====== CMSIS-DSP (V1.10, unity build) ======
set(DSP_ROOT ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/DSP)
add_library(cmsis_dsp STATIC
    ${DSP_ROOT}/Source/BasicMathFunctions/BasicMathFunctions.c
    ${DSP_ROOT}/Source/CommonTables/CommonTables.c
    ${DSP_ROOT}/Source/ComplexMathFunctions/ComplexMathFunctions.c
    ${DSP_ROOT}/Source/ControllerFunctions/ControllerFunctions.c
    ${DSP_ROOT}/Source/FastMathFunctions/FastMathFunctions.c
    ${DSP_ROOT}/Source/FilteringFunctions/FilteringFunctions.c
    ${DSP_ROOT}/Source/InterpolationFunctions/InterpolationFunctions.c
    ${DSP_ROOT}/Source/MatrixFunctions/MatrixFunctions.c
    ${DSP_ROOT}/Source/StatisticsFunctions/StatisticsFunctions.c
    ${DSP_ROOT}/Source/SupportFunctions/SupportFunctions.c
    ${DSP_ROOT}/Source/TransformFunctions/TransformFunctions.c
)
target_include_directories(cmsis_dsp PUBLIC
    ${DSP_ROOT}/Include
)
target_include_directories(cmsis_dsp PRIVATE
    ${DSP_ROOT}/PrivateInclude
)
target_compile_definitions(cmsis_dsp PUBLIC
    ARM_MATH_CM7
)
target_link_libraries(cmsis_dsp PUBLIC stm32cubemx)
target_compile_options(cmsis_dsp PRIVATE
    -O2
    -Wno-unused-parameter
    -Wno-sign-compare
    -Wno-unused-variable
)
# ====== /CMSIS-DSP ======

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    App/scheduler/scheduler.c
    App/led/led.c
    App/uart/uart.c
    App/cli/cli.c
    App/adc/adc_app.c
    App/dac/dac_app.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    App/scheduler
    App/led
    App/uart
    App/cli
    App/adc
    App/dac
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
    cmsis_dsp

    # Add user defined libraries
)
