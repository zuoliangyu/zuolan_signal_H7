# ADC 与 DAC 文档索引

本目录集中整理当前工程里与 `ADC`、`DAC`、定时器触发、DMA 缓冲和波形联调相关的正式说明文档。

## 1. 建议阅读顺序

如果你是第一次接触当前工程，建议按下面顺序阅读：

1. [ADC设计与实现说明.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/ADC与DAC/ADC设计与实现说明.md)
2. [DAC设计与实现说明.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/ADC与DAC/DAC设计与实现说明.md)
3. 再按需要跳到根目录下的纠错文档

## 2. 当前工程状态

截至当前版本，`ADC / DAC` 子系统已经进入“周期采集 + 规则输出”阶段，主要状态如下：

- `DAC1_CH1 -> PA4`
- `TIM6 + DAC + DMA` 用于波形输出
- `ADC1 -> PA0(INP16)`
- `TIM2 + ADC + DMA` 用于周期采样
- `ADC` 已支持半缓冲 / 全缓冲回调、块队列、整帧快照
- `DAC` 已支持 `dc / sine / tri / square`

当前默认对齐关系：

- `DAC` 默认波形频率：`1kHz`
- `DAC` 默认波表点数：`256`
- `DAC` 默认更新频率：`256kHz`
- `ADC` 默认采样点数：`256`
- `ADC` 默认采样率：`256kHz`

也就是：

- 默认情况下，`ADC frame` 对应 `DAC` 的一个完整周期

## 3. 文档分工

### [ADC设计与实现说明.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/ADC与DAC/ADC设计与实现说明.md)

重点说明：

- `ADC1` 的 `CubeMX` 配置
- `TIM2 TRGO` 触发链路
- `DMA Circular`
- 半缓冲 / 全缓冲处理
- `adc` CLI 命令与使用方式
- 默认 `256kHz / 256点` 的采样组织方式

### [DAC设计与实现说明.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/ADC与DAC/DAC设计与实现说明.md)

重点说明：

- `DAC1_CH1` 的输出链路
- `TIM6 TRGO` 与 `DMA` 波形更新
- `dac` CLI 命令与参数语义
- 波表组织方式与输出频率关系

## 4. 相关问题记录

当前和 `ADC / DAC` 直接相关的历史问题，建议从下面几篇开始看：

- [纠错-ADC时钟配置导致CubeMX生成警告.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/纠错-ADC时钟配置导致CubeMX生成警告.md)
- [纠错-ADC改为TIM2触发后未启动定时器导致无采样.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/纠错-ADC改为TIM2触发后未启动定时器导致无采样.md)
- [STM32H7_DMA缓冲区与链接脚本说明.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/STM32H7_DMA缓冲区与链接脚本说明.md)

## 5. 当前代码落点

当前工程中，`ADC / DAC` 相关代码主要分布在：

- `Core/Inc/adc.h`
- `Core/Src/adc.c`
- `Core/Inc/dac.h`
- `Core/Src/dac.c`
- `Core/Inc/tim.h`
- `Core/Src/tim.c`
- `App/adc/adc_app.c`
- `App/adc/adc_app.h`
- `App/dac/dac_app.c`
- `App/dac/dac_app.h`

职责划分：

- `Core`
  - 只保留 `CubeMX` 生成初始化和中断入口
- `App/adc`
  - 管理采样率、DMA 缓冲、块队列、整帧快照、CLI 导出
- `App/dac`
  - 管理默认参数、波表、输出模式、运行时重配置
