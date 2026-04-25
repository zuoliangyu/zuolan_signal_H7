# ADC 改为 TIM2 触发后未启动定时器导致无采样

## 1. 问题标题

ADC 从“软件连续采样”改为“`TIM2 TRGO` 触发采样”后，如果应用层没有启动 `TIM2`，ADC DMA 不会持续更新。

## 2. 问题现象

`CubeMX` 配置已经显示：

- `ADC1` 外部触发源为 `TIM2 TRGO`
- `Continuous Conversion Mode = Disabled`

但应用层仍沿用旧逻辑时，会出现：

- `adc raw` 数值不变化或几乎不变化
- DMA 缓冲区不按预期持续刷新
- 周期信号采集看起来像“没采到”

## 3. 影响范围

- `App/adc/adc_app.c`
- ADC 周期信号采集链路
- 基于半缓冲/全缓冲的后续数据处理

## 4. 复现条件

1. `ADC1` 从软件连续采样改成 `TIM2 TRGO` 触发
2. `Continuous Conversion` 改为 `Disabled`
3. 代码中只执行了 `HAL_ADC_Start_DMA()`
4. 没有执行 `HAL_TIM_Base_Start(&htim2)`

## 5. 根因分析

外部触发模式下，ADC 自己不会连续启动转换。

这时真正的采样链路是：

- `TIM2 update`
- `TIM2 TRGO`
- `ADC1 trigger`
- `ADC sample`
- `DMA write`

如果 `TIM2` 没有启动：

- 不会有触发脉冲
- ADC 就不会真正按采样率持续转换

也就是说，问题不在 ADC DMA 本身，而在“外部触发源定时器未启动”。

## 6. 解决方案

在 ADC 应用层初始化中，除了：

- `HAL_ADCEx_Calibration_Start()`
- `HAL_ADC_Start_DMA()`

还必须执行：

- `HAL_TIM_Base_Start(&htim2)`

并且建议初始化顺序为：

1. ADC 校准
2. 配置 `TIM2` 采样率
3. 启动 `HAL_ADC_Start_DMA()`
4. 启动 `TIM2`

## 7. 修改位置

主要修改位置：

- [adc_app.c](C:/Users/zuolan/Desktop/zuolan_signal_STM32/zuolan_signal_STM32/App/adc/adc_app.c)

关键点：

- 应用层负责启动 `TIM2`
- 采样率也统一由应用层重配 `TIM2`

## 8. 验证方法

1. 上电后输入 `adc rate ?`
2. 输入 `adc raw`
3. 输入 `adc block ?`
4. 观察 `half_events` / `full_events` 是否持续增长
5. 输入 `adc frame` 或 `adc next`
6. 确认能导出有效采样数据

## 9. 验证结果

本次修正后：

- `TIM2` 已在 ADC 应用层中启动
- ADC 周期采样链路恢复正常
- 半缓冲 / 全缓冲回调可以持续触发
- 可以继续实现块流和整帧导出

## 10. 注意事项 / 后续建议

- 以后只要 ADC 使用外部触发，就必须同时检查“触发源是否真的启动”
- 不要只看 `.ioc` 配置正确就认为采样链路已经完整
- 如果后续切换到其他 `TIMx TRGO`，同样要同步修改应用层启动逻辑
