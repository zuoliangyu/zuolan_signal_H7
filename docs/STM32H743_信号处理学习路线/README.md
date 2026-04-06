# STM32H743 信号处理学习路线索引

本目录用于整理从 `STM32F4` 迁移到 `STM32H743` 后，围绕采样、缓存、算法、实时系统和工程化的系统学习路线。

适合你当前这个工程阶段的阅读顺序：

1. [00_总学习计划.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/STM32H743_信号处理学习路线/00_总学习计划.md)
2. [01_阶段一_H743与F4差异.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/STM32H743_信号处理学习路线/01_阶段一_H743与F4差异.md)
3. [02_阶段二_采样链路与基础外设.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/STM32H743_信号处理学习路线/02_阶段二_采样链路与基础外设.md)
4. [03_阶段三_内存_Cache_DMA_MPU.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/STM32H743_信号处理学习路线/03_阶段三_内存_Cache_DMA_MPU.md)
5. [04_阶段四_CMSIS_DSP与常用算法.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/STM32H743_信号处理学习路线/04_阶段四_CMSIS_DSP与常用算法.md)
6. [05_阶段五_实时系统与工程化.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/STM32H743_信号处理学习路线/05_阶段五_实时系统与工程化.md)
7. [06_阶段六_综合项目_实时频谱分析仪.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/STM32H743_信号处理学习路线/06_阶段六_综合项目_实时频谱分析仪.md)
8. [07_资料与例程索引.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/STM32H743_信号处理学习路线/07_资料与例程索引.md)

## 1. 目录定位

这套文档不是单独解释当前仓库某个模块的代码，而是回答：

- `STM32H743` 相比你熟悉的 `F4`，到底难点在哪里
- 为什么做信号处理时，真正的关键不是 GPIO，而是采样链路、缓冲布局和实时性
- 什么时候该先解决 `DMA / Cache / MPU`
- 什么时候再进入 `CMSIS-DSP` 和算法实现

## 2. 适合什么时候看

建议在下面几种场景阅读：

- 想建立系统学习计划，而不是只修当前一个 bug
- 想把当前工程从“功能能跑”逐步推进到“结构正确、可扩展”
- 准备进入更高采样率、更复杂算法、更严格实时性的阶段

## 3. 与当前工程的关系

当前工程里已经落地的内容，和这套路线的对应关系大致是：

- 调度器、串口、CLI：属于基础工程化能力
- `DAC TIM6 + DMA`：属于采样/输出链路基础阶段
- `ADC TIM2 + DMA`：属于稳定采样链路阶段
- `DMA 缓冲区独立 section`：已经进入 `Cache / DMA / 内存布局` 的工程实践

所以这套学习路线已经不只是理论材料，而是和当前工程状态直接对应。
