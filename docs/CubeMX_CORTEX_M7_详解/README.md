# CubeMX CORTEX_M7 详解

这是一套专门针对 `STM32H743` 和 `STM32CubeMX -> CORTEX_M7` 配置页的专题文档。

适合你当前这个阶段的阅读顺序：

1. [01_先建立正确理解.md](C:\Users\zuolan\Desktop\zuolan_signal_STM32\docs\CubeMX_CORTEX_M7_详解\01_先建立正确理解.md)
2. [02_CubeMX页面逐项解释.md](C:\Users\zuolan\Desktop\zuolan_signal_STM32\docs\CubeMX_CORTEX_M7_详解\02_CubeMX页面逐项解释.md)
3. [03_结合当前工程看生成代码.md](C:\Users\zuolan\Desktop\zuolan_signal_STM32\docs\CubeMX_CORTEX_M7_详解\03_结合当前工程看生成代码.md)
4. [04_推荐启用顺序与工程策略.md](C:\Users\zuolan\Desktop\zuolan_signal_STM32\docs\CubeMX_CORTEX_M7_详解\04_推荐启用顺序与工程策略.md)
5. [05_常见误区与排查.md](C:\Users\zuolan\Desktop\zuolan_signal_STM32\docs\CubeMX_CORTEX_M7_详解\05_常见误区与排查.md)

这套文档重点回答五个问题：

- `Speculation default mode` 到底是什么
- `ICache / DCache` 到底该不该开
- `MPU` 到底是在保护什么
- 为什么很多 H7 工程一开 `DCache` 就出怪问题
- 你这个工程现在应该怎么一步一步配，而不是一下子全开

配套阅读：

- [CubeMX_CORTEX_M7_配置说明.md](C:\Users\zuolan\Desktop\zuolan_signal_STM32\docs\CubeMX_CORTEX_M7_配置说明.md)
- [第三阶段：内存、Cache、DMA、MPU](C:\Users\zuolan\Desktop\zuolan_signal_STM32\docs\STM32H743_信号处理学习路线\03_阶段三_内存_Cache_DMA_MPU.md)

官方资料入口：

- [STM32H743/753 产品资料页](https://www.st.com/en/microcontrollers-microprocessors/stm32h743-753/documentation.html)
- [PM0253: STM32F7/STM32H7 Cortex-M7 processor programming manual](https://www.st.com/resource/en/programming_manual/pm0253-stm32f7-series-and-stm32h7-series-cortexm7-processor-programming-manual-stmicroelectronics.pdf)
- [AN4839: Level 1 cache on STM32F7 Series and STM32H7 Series](https://www.st.com/resource/en/application_note/an4839-level-1-cache-on-stm32f7-series-and-stm32h7-series-stmicroelectronics.pdf)
- [RM0433 文档入口](https://www.st.com/en/microcontrollers-microprocessors/stm32h743-753/documentation.html)
