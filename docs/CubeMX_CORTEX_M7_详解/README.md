# CubeMX CORTEX_M7 详解

这是一套专门针对 `STM32H743` 和 `STM32CubeMX -> CORTEX_M7` 配置页的专题文档。

适合你当前这个阶段的阅读顺序：

1. [00_配置页快速结论与当前工程建议.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/CubeMX_CORTEX_M7_详解/00_配置页快速结论与当前工程建议.md)
2. [01_先建立正确理解.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/CubeMX_CORTEX_M7_详解/01_先建立正确理解.md)
3. [02_CubeMX页面逐项解释.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/CubeMX_CORTEX_M7_详解/02_CubeMX页面逐项解释.md)
4. [03_结合当前工程看生成代码.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/CubeMX_CORTEX_M7_详解/03_结合当前工程看生成代码.md)
5. [04_推荐启用顺序与工程策略.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/CubeMX_CORTEX_M7_详解/04_推荐启用顺序与工程策略.md)
6. [05_常见误区与排查.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/CubeMX_CORTEX_M7_详解/05_常见误区与排查.md)

这套文档重点回答五个问题：

- `Speculation default mode` 到底是什么
- `ICache / DCache` 到底该不该开
- `MPU` 到底是在保护什么
- 为什么很多 H7 工程一开 `DCache` 就出怪问题
- 你这个工程现在应该怎么一步一步配，而不是一下子全开

配套阅读：

- [第三阶段：内存、Cache、DMA、MPU](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/STM32H743_信号处理学习路线/03_阶段三_内存_Cache_DMA_MPU.md)

## 1. 当前建议

如果你只想先拿到当前工程可执行的结论，而不想一上来读完整套专题，先看：

- [00_配置页快速结论与当前工程建议.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/CubeMX_CORTEX_M7_详解/00_配置页快速结论与当前工程建议.md)

当前工程推荐方向仍然是：

- `I-Cache` 可以优先启用
- `D-Cache` 要和 `DMA buffer`、`MPU`、链接脚本一起规划
- 不要只在 `CubeMX` 页面点选，不去看生成代码和内存布局

官方资料入口：

- [STM32H743/753 产品资料页](https://www.st.com/en/microcontrollers-microprocessors/stm32h743-753/documentation.html)
- [PM0253: STM32F7/STM32H7 Cortex-M7 processor programming manual](https://www.st.com/resource/en/programming_manual/pm0253-stm32f7-series-and-stm32h7-series-cortexm7-processor-programming-manual-stmicroelectronics.pdf)
- [AN4839: Level 1 cache on STM32F7 Series and STM32H7 Series](https://www.st.com/resource/en/application_note/an4839-level-1-cache-on-stm32f7-series-and-stm32h7-series-stmicroelectronics.pdf)
- [RM0433 文档入口](https://www.st.com/en/microcontrollers-microprocessors/stm32h743-753/documentation.html)
