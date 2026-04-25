# zuolan_signal_H7

基于 `STM32H743VGTx` 的信号处理学习与工程实验仓库。

## 仓库结构

```
.
├── README.md                       — 本文件
├── AGENTS.md                       — AI 工具协作元数据
├── .gitignore
├── docs/                           — 文档（学习路线、模块说明、纠错记录）
└── zuolan_signal_STM32_H7/         — 嵌入式工程主体
    ├── App/                        — 应用层（adc / dac / dsp / cli / uart / led / scheduler）
    ├── Core/                       — CubeMX 生成的初始化与中断入口
    ├── Drivers/                    — STM32 HAL + CMSIS-Core + CMSIS-DSP V1.10
    ├── Middlewares/                — 由 CubeMX 维护
    ├── cmake/ + CMakeLists.txt     — CMake + ARM GCC 构建链
    ├── STM32H743XG_FLASH.ld
    ├── startup_stm32h743xx.s
    └── zuolan_signal_STM32.ioc     — CubeMX 配置源文件
```

## 当前能力（commit `107b525`）

- **MCU**：STM32H743VGTx，HCLK 240 MHz，I-Cache 启用
- **采样链路**：ADC1/PA0 16-bit @ 256 kHz 由 TIM2 触发，DMA 双半缓冲
- **波形输出**：DAC1/PA4，TIM6 触发，DMA 循环波表，支持 dc / sine / triangle / square
- **信号处理**：CMSIS-DSP V1.10，FFT (256/512/1024) + FIR + IIR Biquad DF2T
- **实时流水线**：ADC → 可选滤波 → FFT 帧（1024 点 4 ms 周期），CLI 控制
- **串口**：USART1 @ 921600 baud，DMA RX + DMA TX 异步发送（4 KB 环形缓冲）
- **CLI**：`help / echo / led / dac / adc / fft / filter / pipeline / dacrate / adcrate / adcdump / uarttx / report / clearstats`
- **调试**：VS Code + OpenOCD + CMSIS-DAP

## 主要入口

| 类别 | 路径 |
|---|---|
| 文档总索引 | [docs/README.md](docs/README.md) |
| DSP 与实时流水线 | [docs/DSP/README.md](docs/DSP/README.md) |
| 串口/CLI/UART DMA | [docs/串口与CLI/README.md](docs/串口与CLI/README.md) |
| ADC 与 DAC | [docs/ADC与DAC/README.md](docs/ADC与DAC/README.md) |
| OpenOCD 烧录调试 | [docs/OpenOCD_CMSIS-DAP_烧录与调试.md](docs/OpenOCD_CMSIS-DAP_烧录与调试.md) |
| 历史踩坑 | [docs/纠错/README.md](docs/纠错/README.md) |
| 工程主源码 | [zuolan_signal_STM32_H7/App/](zuolan_signal_STM32_H7/App/) |
| CubeMX 配置 | [zuolan_signal_STM32_H7/zuolan_signal_STM32.ioc](zuolan_signal_STM32_H7/zuolan_signal_STM32.ioc) |

## 构建

工程使用 CMake，预设见 `zuolan_signal_STM32_H7/CMakePresets.json`：

```bash
cd zuolan_signal_STM32_H7
cmake --preset Debug          # configure
cmake --build build/Debug     # build
```

或在 VS Code 中用 STM32 扩展自带的 build 任务一键构建。

## 命名说明

- 仓库名：`zuolan_signal_H7`
- 工程目录：`zuolan_signal_STM32_H7/`
- CMake 项目名 / 编译产物：`zuolan_signal_STM32`（保留早期命名，未跟随仓库重命名以避免改动 `.vscode/` 等多处引用）

## 维护约定

- `build/` 构建产物不纳入版本管理（见 `.gitignore`）
- `.claude/` 是 Claude Code 本地状态目录，已忽略
- 文档优先放在 `docs/` 对应专题目录，避免根目录散文档过多
- 历史问题记录写到 `docs/纠错/`，统一使用 [docs/纠错/模板.md](docs/纠错/模板.md)

## License

工程代码继承自 STMicroelectronics 提供的 STM32Cube HAL 与 CMSIS 库，遵循各原始组件的许可证（见 `Drivers/` 各子目录的 `LICENSE.txt`）。应用层代码 (`App/`) 与文档 (`docs/`) 由作者维护，未单独声明许可证时按仓库默认。
