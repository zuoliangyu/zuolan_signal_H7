# zuolan_signal_STM32

这是一个基于 `STM32H743` 的信号处理学习与工程实验仓库。

## 仓库结构

- `zuolan_signal_STM32/`
  - 主工程目录
  - 使用 `CMake + STM32CubeMX` 生成与维护
  - 当前包含 `VS Code` 调试与烧录配置
- `docs/`
  - 学习路线、阶段文档和工程配套说明

## 当前工程

主工程路径：

- `zuolan_signal_STM32/`

当前目标芯片：

- `STM32H743VGTx`

当前已补充的开发配置：

- `OpenOCD + CMSIS-DAP` 烧录与调试配置
- `VS Code + STM32 clangd` 工作区配置

## 常用入口

- 工程说明文档：`docs/`
- OpenOCD 调试说明：`docs/OpenOCD_CMSIS-DAP_烧录与调试.md`
- 主工程源码：`zuolan_signal_STM32/Core/`
- CubeMX 工程文件：`zuolan_signal_STM32/zuolan_signal_STM32.ioc`

## 说明

- `build/` 构建产物默认不纳入版本管理
- 根目录用于管理整个学习仓库，实际嵌入式工程位于 `zuolan_signal_STM32/`
