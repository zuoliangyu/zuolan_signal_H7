# ADC 与 DAC 文档索引

本目录集中整理工程里与 `ADC`、`DAC`、定时器触发、DMA 缓冲和波形联调相关的正式说明。

## 1. 建议阅读顺序

如果你是第一次接触当前工程，建议按下面顺序阅读：

1. [ADC设计与实现说明.md](ADC设计与实现说明.md)
2. [DAC设计与实现说明.md](DAC设计与实现说明.md)
3. 再按需要看 [../纠错/README.md](../纠错/README.md) 里的相关历史踩坑

## 2. 当前工程状态

`ADC / DAC` 子系统已进入"周期采集 + 规则输出"稳定阶段：

- `DAC1_CH1 → PA4`，`TIM6 + DAC + DMA` 波形输出
- `ADC1 → PA0(INP16)`，16-bit 分辨率，`SamplingTime = 8.5 cycles`
- `TIM2 + ADC + DMA` 周期采样
- 半缓冲 / 全缓冲回调、块队列、整帧快照
- DAC 支持 `dc / sine / tri / square`

默认对齐关系：

| 参数 | 数值 |
|---|---|
| DAC 波形频率 | 1 kHz |
| DAC 波表点数 | 256 |
| DAC 更新频率 | 256 kHz |
| ADC 采样率 | 256 kHz |
| ADC 缓冲点数 | 256 |

→ ADC 一帧对应 DAC 一个完整周期。

## 3. 文档分工

### [ADC设计与实现说明.md](ADC设计与实现说明.md)

- `ADC1` 的 CubeMX 配置
- `TIM2 TRGO` 触发链路
- `DMA Circular` + 半/全缓冲处理
- `adc` CLI 命令与使用方式
- 默认 256 kHz / 256 点的采样组织方式

### [DAC设计与实现说明.md](DAC设计与实现说明.md)

- `DAC1_CH1` 输出链路
- `TIM6 TRGO` + `DMA` 波形更新
- `dac` CLI 命令与参数语义
- 波表组织方式与输出频率关系

## 4. 相关问题记录

ADC / DAC 直接相关的历史踩坑：

- [../纠错/ADC时钟配置导致CubeMX生成警告.md](../纠错/ADC时钟配置导致CubeMX生成警告.md)
- [../纠错/ADC改为TIM2触发后未启动定时器导致无采样.md](../纠错/ADC改为TIM2触发后未启动定时器导致无采样.md)
- [../纠错/ADC采样时间过长导致实际采样率被吞.md](../纠错/ADC采样时间过长导致实际采样率被吞.md)
- [../STM32H7_DMA缓冲区与链接脚本说明.md](../STM32H7_DMA缓冲区与链接脚本说明.md)

## 5. 当前代码落点

```
Core/{Inc,Src}/  ← CubeMX 生成的初始化和中断入口
  ├── adc.{h,c}
  ├── dac.{h,c}
  └── tim.{h,c}

App/adc/adc_app.{h,c}  ← 采样率、DMA 缓冲、块队列、整帧快照、CLI 导出
App/dac/dac_app.{h,c}  ← 默认参数、波表、输出模式、运行时重配置
```
