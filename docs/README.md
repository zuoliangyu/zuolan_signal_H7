# 工程文档总索引

本文档用于统一导航当前工程 `docs/` 下的正式说明、专题文档、学习路线和纠错记录。

如果你是第一次看这个仓库，建议先按下面顺序阅读：

1. 工程搭建与调试
2. 串口与 CLI
3. ADC 与 DAC
4. `STM32H7` 的 `DMA / Cache / MPU`
5. 进阶学习路线

## 1. 当前文档结构

### 专题目录

- [串口与CLI/README.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/串口与CLI/README.md)
  - 当前 `USART1/USART2`、`DMA + IDLE + RingBuffer`、CLI 解析与命令扩展
- [ADC与DAC/README.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/ADC与DAC/README.md)
  - 当前 `ADC1/TIM2/DMA` 周期采集、`DAC1/TIM6/DMA` 波形输出、默认对齐关系
- [CubeMX_CORTEX_M7_详解/README.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/CubeMX_CORTEX_M7_详解/README.md)
  - `CubeMX -> CORTEX_M7` 配置页的系统理解、启用顺序和常见误区
- [STM32H743_信号处理学习路线/README.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/STM32H743_信号处理学习路线/README.md)
  - 围绕 `STM32H743` 信号采集、算法、实时处理的阶段化学习路径

### 根目录通用文档

- [OpenOCD_CMSIS-DAP_烧录与调试.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/OpenOCD_CMSIS-DAP_烧录与调试.md)
  - VS Code 下 `OpenOCD + CMSIS-DAP` 烧录与调试配置说明
- [STM32H7_DMA缓冲区与链接脚本说明.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/STM32H7_DMA缓冲区与链接脚本说明.md)
  - `STM32H7` 上 DMA 缓冲区、链接脚本、自定义 section 的核心说明
- [基于uwTick的简易调度器说明.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/基于uwTick的简易调度器说明.md)
  - 当前轻量调度器的设计与使用方式
- [纠错文档模板.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/纠错文档模板.md)
  - 后续新增问题记录时统一使用的模板
- [纠错索引.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/纠错索引.md)
  - 当前所有纠错文档的集中入口

## 2. 按任务阅读

### 如果你要做串口、CLI、调试输出

建议阅读：

1. [串口与CLI/README.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/串口与CLI/README.md)
2. [串口与CLI/UART接收调试与CLI运行说明.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/串口与CLI/UART接收调试与CLI运行说明.md)
3. [串口与CLI/CLI命令解析与扩展说明.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/串口与CLI/CLI命令解析与扩展说明.md)

### 如果你要做 ADC / DAC / 周期信号采集

建议阅读：

1. [ADC与DAC/README.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/ADC与DAC/README.md)
2. [ADC与DAC/ADC设计与实现说明.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/ADC与DAC/ADC设计与实现说明.md)
3. [ADC与DAC/DAC设计与实现说明.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/ADC与DAC/DAC设计与实现说明.md)
4. [STM32H7_DMA缓冲区与链接脚本说明.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/STM32H7_DMA缓冲区与链接脚本说明.md)

### 如果你要理解 H7 上的 Cache / MPU / DMA

建议阅读：

1. [CubeMX_CORTEX_M7_详解/README.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/CubeMX_CORTEX_M7_详解/README.md)
2. [CubeMX_CORTEX_M7_详解/00_配置页快速结论与当前工程建议.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/CubeMX_CORTEX_M7_详解/00_配置页快速结论与当前工程建议.md)
3. [STM32H743_信号处理学习路线/03_阶段三_内存_Cache_DMA_MPU.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/STM32H743_信号处理学习路线/03_阶段三_内存_Cache_DMA_MPU.md)

### 如果你要看历史问题和踩坑记录

建议阅读：

1. [纠错索引.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/纠错索引.md)
2. 再按问题主题跳转到具体纠错文档

## 3. 当前工程重点状态

截至当前版本，文档反映的工程状态如下：

- `USART1` 为 CLI，`USART2` 为 echo
- CLI 已支持 `led / dac / adc` 三类主要命令
- `DAC1_CH1 -> PA4` 已支持 `TIM6 + DMA` 规则波形输出
- `ADC1 -> PA0` 已支持 `TIM2` 触发的周期采样
- 默认按 `DAC 1kHz + 256点` 对齐 `ADC 256kHz + 256点`
- `STM32H7` 上 DMA 缓冲区统一按 `.dma_buffer` + `RAM_D2` 思路管理

## 4. 维护约定

后续维护 `docs/` 时，默认遵循：

- 专题说明优先收敛到专题目录
- 纠错记录统一保留在根目录 `纠错-*.md`
- 如果主题已经有目录，优先更新目录内 `README.md`
- 避免同主题在根目录和专题目录各写一份长期重复内容
