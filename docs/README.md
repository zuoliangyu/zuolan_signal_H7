# 工程文档总索引

本目录用于统一导航工程的设计说明、专题文档、学习路线和纠错记录。

> 所有链接均为相对路径，可在 GitHub / 本地 / VS Code 中正常跳转。

## 1. 文档结构

```
docs/
├── ADC与DAC/                       — ADC1/TIM2 周期采集、DAC1/TIM6 波形输出
├── CubeMX_CORTEX_M7_详解/          — Cortex-M7 配置页系统理解与 CubeMX 操作
├── DSP/                            — CMSIS-DSP 集成、FFT/滤波、实时流水线
├── STM32H743_信号处理学习路线/      — 阶段化学习路径
├── 串口与CLI/                       — USART/CLI/DMA 收发架构
├── 纠错/                           — 历史踩坑记录
├── OpenOCD_CMSIS-DAP_烧录与调试.md
├── STM32H7_DMA缓冲区与链接脚本说明.md
└── 基于uwTick的简易调度器说明.md
```

## 2. 各专题入口

- [ADC与DAC/README.md](ADC与DAC/README.md) —— ADC1/TIM2/DMA 周期采集，DAC1/TIM6/DMA 波形输出
- [DSP/README.md](DSP/README.md) —— `App/dsp` 模块、CMSIS-DSP V1.10、FFT、FIR/IIR、ADC→滤波→FFT 实时流水线
- [串口与CLI/README.md](串口与CLI/README.md) —— USART 接收 DMA 链路、CLI 解析与扩展、UART 异步发送（DMA TX 环形缓冲）
- [CubeMX_CORTEX_M7_详解/README.md](CubeMX_CORTEX_M7_详解/README.md) —— Cortex-M7 配置页系统讲解与 CubeMX 操作建议
- [STM32H743_信号处理学习路线/README.md](STM32H743_信号处理学习路线/README.md) —— 围绕 H743 的信号采集、算法、实时处理阶段化学习路径
- [纠错/README.md](纠错/README.md) —— 集中存放工程历史踩坑记录

## 3. 根目录通用文档

- [OpenOCD_CMSIS-DAP_烧录与调试.md](OpenOCD_CMSIS-DAP_烧录与调试.md) —— VS Code + OpenOCD + CMSIS-DAP 烧录调试配置
- [STM32H7_DMA缓冲区与链接脚本说明.md](STM32H7_DMA缓冲区与链接脚本说明.md) —— H7 上 DMA 缓冲区放置、`.dma_buffer` 段、链接脚本设计
- [基于uwTick的简易调度器说明.md](基于uwTick的简易调度器说明.md) —— 当前轻量调度器的设计与使用

## 4. 按任务找文档

### 串口、CLI、调试输出

1. [串口与CLI/UART接收调试与CLI运行说明.md](串口与CLI/UART接收调试与CLI运行说明.md)
2. [串口与CLI/CLI命令解析与扩展说明.md](串口与CLI/CLI命令解析与扩展说明.md)
3. [串口与CLI/UART异步发送与DMA架构.md](串口与CLI/UART异步发送与DMA架构.md)

### ADC / DAC / 周期信号采集

1. [ADC与DAC/ADC设计与实现说明.md](ADC与DAC/ADC设计与实现说明.md)
2. [ADC与DAC/DAC设计与实现说明.md](ADC与DAC/DAC设计与实现说明.md)
3. [STM32H7_DMA缓冲区与链接脚本说明.md](STM32H7_DMA缓冲区与链接脚本说明.md)

### DSP / FFT / 滤波

1. [DSP/MATLAB滤波器设计与导出.md](DSP/MATLAB滤波器设计与导出.md)
2. [DSP/实时流水线设计.md](DSP/实时流水线设计.md)
3. [STM32H743_信号处理学习路线/04_阶段四_CMSIS_DSP与常用算法.md](STM32H743_信号处理学习路线/04_阶段四_CMSIS_DSP与常用算法.md)

### Cache / MPU / DMA / 内存布局

1. [CubeMX_CORTEX_M7_详解/00_配置页快速结论与当前工程建议.md](CubeMX_CORTEX_M7_详解/00_配置页快速结论与当前工程建议.md)
2. [STM32H743_信号处理学习路线/03_阶段三_内存_Cache_DMA_MPU.md](STM32H743_信号处理学习路线/03_阶段三_内存_Cache_DMA_MPU.md)
3. [STM32H7_DMA缓冲区与链接脚本说明.md](STM32H7_DMA缓冲区与链接脚本说明.md)

### 历史踩坑

进入 [纠错/README.md](纠错/README.md) 看分类索引。

## 5. 当前工程状态（截至 commit `bd3f573`）

- 通讯：`USART1` 921600 baud（CLI，DMA TX 异步发送 + DMA RX）；`USART2` 原样回显
- CLI 命令：`help / echo / led / dac / adc / fft / filter / pipeline / dacrate / adcrate / adcdump / uarttx / report / clearstats`
- DAC：`DAC1_CH1 → PA4`，`TIM6 + DMA` 波形输出（dc / sine / triangle / square）
- ADC：`ADC1 → PA0`，16-bit，`TIM2` 触发周期采样 256 kHz，`SamplingTime = 8.5 cycles`
- DSP：`CMSIS-DSP V1.10` 集成完成，FFT (256/512/1024) + FIR + Biquad DF2T
- 流水线：`ADC → optional filter → FFT` 实时管线，单帧 / 流式触发，可降频输出
- DMA 缓冲区：统一通过 `.dma_buffer` 段放到 `RAM_D2`

## 6. 维护约定

- 专题说明优先收敛到对应专题目录，不要在根目录散写
- 历史问题记录写到 `docs/纠错/` 下，文件名仅保留主题（不再加 `纠错-` 前缀，因为目录名已表明分类）
- 新增专题主题且文件 ≥ 2 个时，再单独建子目录
- 同主题已有目录的，优先更新 README，避免根目录与子目录重复
- README 索引中的链接**统一用相对路径**，绝对路径会跨机器失效
