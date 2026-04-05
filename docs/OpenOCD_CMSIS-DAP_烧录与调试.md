# OpenOCD + CMSIS-DAP 烧录与调试

本文档对应工程 `zuolan_signal_STM32` 的 VS Code 调试配置。

## 已添加文件

- `zuolan_signal_STM32/.vscode/launch.json`
- `zuolan_signal_STM32/.vscode/tasks.json`

## 适用环境

- 目标芯片：`STM32H743VGTx`
- 调试接口：`SWD`
- 调试探针：`CMSIS-DAP` / `DAPLink`
- GDB：`%LOCALAPPDATA%/stm32cube/bundles/gnu-tools-for-stm32/13.3.1+st.9/bin/arm-none-eabi-gdb.exe`
- OpenOCD 脚本目录：`E:/path/c_c++/openocd/share/openocd/scripts`
- ELF 路径：`zuolan_signal_STM32/build/Debug/zuolan_signal_STM32.elf`

## 使用前提

1. 已安装 VS Code 扩展 `marus25.cortex-debug`
2. `openocd` 已加入系统 `PATH`
3. 已先完成一次 `Debug` 构建，生成 `build/Debug/zuolan_signal_STM32.elf`

## 调试配置

`launch.json` 中提供了两个配置：

- `OpenOCD CMSIS-DAP 调试(Debug, 4MHz)`
- `OpenOCD CMSIS-DAP 调试(Debug, 1MHz 兼容模式)`

默认先使用 `4MHz`。如果连接不稳定、初始化失败或提示无法握手，再切到 `1MHz 兼容模式`。

## 烧录任务

`tasks.json` 中提供了两个任务：

- `OpenOCD: 烧录 Debug ELF (CMSIS-DAP, 4MHz)`
- `OpenOCD: 烧录 Debug ELF (CMSIS-DAP, 1MHz 兼容模式)`

这两个任务都会执行：

- 烧录 `build/Debug/zuolan_signal_STM32.elf`
- `verify`
- `reset`
- `exit`

## 常见调整项

如果后续环境变化，通常只需要改下面几项：

1. 如果 OpenOCD 安装路径变化，修改：
   - `.vscode/launch.json` 中的 `configFiles`
   - `.vscode/tasks.json` 中的 `-f` 参数
2. 如果 STM32Cube 工具链版本变化，修改：
   - `.vscode/launch.json` 中的 `armToolchainPath`
   - `.vscode/launch.json` 中的 `gdbPath`
3. 如果探针或连线不稳定，优先调低：
   - `adapter speed 4000`
   - 可改成 `2000`、`1000`，必要时更低

## 说明

- 当前配置没有自动触发构建，避免调试前重复执行重操作。
- 当前配置使用 `target/stm32h7x.cfg`，对 `STM32H743` 系列是合适的。
- 如果后续你板子的复位链路有特殊要求，可以再补 `connect under reset` 或 `reset_config` 相关命令。
