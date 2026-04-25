# STM32H743 信号处理学习路线索引

围绕 H743 的采样、缓存、算法、实时系统、工程化的系统学习路径。

## 阅读顺序

1. [00_总学习计划.md](00_总学习计划.md)
2. [01_阶段一_H743与F4差异.md](01_阶段一_H743与F4差异.md)
3. [02_阶段二_采样链路与基础外设.md](02_阶段二_采样链路与基础外设.md)
4. [03_阶段三_内存_Cache_DMA_MPU.md](03_阶段三_内存_Cache_DMA_MPU.md)
5. [04_阶段四_CMSIS_DSP与常用算法.md](04_阶段四_CMSIS_DSP与常用算法.md)
6. [05_阶段五_实时系统与工程化.md](05_阶段五_实时系统与工程化.md)
7. [06_阶段六_综合项目_实时频谱分析仪.md](06_阶段六_综合项目_实时频谱分析仪.md)
8. [07_资料与例程索引.md](07_资料与例程索引.md)

## 1. 这套文档解决什么

不是逐行讲当前仓库的代码，而是回答：

- 从 F4 迁移到 H743，难点究竟在哪
- 信号处理为什么真正的关键是采样链路、缓冲布局、实时性，而不是 GPIO
- 什么时候该先解决 DMA / Cache / MPU
- 什么时候再进入 CMSIS-DSP 和算法实现

## 2. 适合什么时候看

- 想建立系统学习计划，而不是只修一个 bug
- 想把工程从"功能能跑"推进到"结构正确、可扩展"
- 准备进入更高采样率、更复杂算法、更严格实时性的阶段

## 3. 与当前工程的对应关系

| 路线阶段 | 当前工程已落地 |
|---|---|
| 调度器 / 串口 / CLI | ✅ `App/scheduler` `App/uart` `App/cli` |
| DAC TIM6 + DMA | ✅ `App/dac` |
| ADC TIM2 + DMA | ✅ `App/adc`，256 kHz / 16-bit |
| DMA 缓冲区独立 section | ✅ `.dma_buffer` → `RAM_D2` |
| CMSIS-DSP 集成 | ✅ V1.10，FFT + FIR + Biquad |
| 实时流水线 | ✅ ADC → 滤波 → FFT (`App/dsp/dsp_pipeline`) |
| UART DMA TX 异步 | ✅ 921600 baud + 4KB ring + DMA |
| Cache 优化 | ⏳ 仅 ICache 启用 |
| 综合项目（频谱仪等） | ⏳ |

## 4. 配套阅读

- [../DSP/README.md](../DSP/README.md) —— DSP 模块当前实现细节
- [../CubeMX_CORTEX_M7_详解/README.md](../CubeMX_CORTEX_M7_详解/README.md) —— Cortex-M7 配置页系统讲解
- [../STM32H7_DMA缓冲区与链接脚本说明.md](../STM32H7_DMA缓冲区与链接脚本说明.md) —— DMA 内存布局
