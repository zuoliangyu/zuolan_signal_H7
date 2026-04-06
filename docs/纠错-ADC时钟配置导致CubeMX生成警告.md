# ADC 时钟配置导致 CubeMX 生成警告

## 1. 问题标题

ADC 时钟配置导致 `CubeMX` 在生成代码前弹出 `[Clock]` 警告。

## 2. 问题现象

在启用 `ADC1` 并准备重新生成代码时，`CubeMX` 弹出如下警告：

- `Main Config: These peripherals still have some not configured or wrong parameter values: [Clock]`

同时在 `Clock Configuration` 页面中，`PLL2` 区域出现红色参数项。

## 3. 影响范围

- `ADC1` 新增后的时钟配置
- `CubeMX` 代码生成阶段
- 可能影响后续 `adc.c` 初始化的正确性

## 4. 复现条件

1. 在 `STM32H743` 工程中启用 `ADC1`
2. 使用默认或自动生成的 ADC 时钟链路
3. 打开 `Clock Configuration`
4. 会看到 `PLL2` 参数存在非法组合，生成代码时触发警告

## 5. 根因分析

本次问题的根因不是 ADC 通道配置错误，而是：

- 启用 `ADC1` 后，`CubeMX` 尝试为 ADC 选择一个新的内核时钟源
- 当时 ADC 时钟链路落到了 `PLL2`
- `PLL2` 当前参数组合不合法，截图中表现为 `DIVN2` 红色
- 因此 `CubeMX` 在生成代码前报告 `[Clock]` 警告

也就是说，问题在“ADC 时钟源配置”，不在“ADC 通道/DMA 配置”。

## 6. 解决方案

将 ADC 时钟源改为：

- `PER_CK`

处理方式：

1. 打开 `CubeMX`
2. 进入 `RCC` 或 `Clock Configuration`
3. 找到 `ADC Clock Source / ADC peripheral clock / ADC kernel clock source`
4. 将其从 `PLL2` 改为 `PER_CK`

调整后：

- `PLL2` 红色非法参数消失
- 生成代码前的 `[Clock]` 警告消失

## 7. 修改位置

修改发生在：

- `.ioc` 的 RCC / ADC 时钟源配置
- 重新生成后的 [main.c](C:/Users/zuolan/Desktop/zuolan_signal_STM32/zuolan_signal_STM32/Core/Src/main.c)
- 重新生成后的 [adc.c](C:/Users/zuolan/Desktop/zuolan_signal_STM32/zuolan_signal_STM32/Core/Src/adc.c)

其中最终生成结果体现为：

- `PeriphClkInitStruct.AdcClockSelection = RCC_ADCCLKSOURCE_CLKP;`

## 8. 验证方法

1. 保存 `.ioc`
2. 重新打开 `Clock Configuration`
3. 确认没有 ADC 相关红色参数
4. 再次执行 `Generate Code`
5. 确认不再弹出 `[Clock]` 警告

## 9. 验证结果

本次调整后：

- `CubeMX` 可以正常生成 ADC 代码
- 已生成 `Core/Src/adc.c`
- 已生成 `Core/Inc/adc.h`
- 当前 `ADC1 + PA0(INP16) + DMA Circular` 配置可以进入后续应用层实现

## 10. 注意事项 / 后续建议

- 第一版 ADC 如果不追求极限采样率，优先使用更稳的时钟方案，不要为了“看起来更快”强行挂到 `PLL2/PLL3`
- 只要 `CubeMX` 出现 `[Clock]` 级别警告，优先先把时钟树修干净，再继续生成代码
- 后续如果要上更高采样率，再单独评估 ADC 内核时钟来源和分频参数
