# 纠错：ADC 采样时间过长导致实际采样率比配置触发率慢 13 倍

## 1. 问题标题

`STM32H7 ADC 配置 256 kHz 触发但实际只采样 19.7 kHz，频谱分析得到错误频率`

## 2. 问题现象

- DAC 输出 1 kHz 正弦（已用 `dacrate` 命令验证 DMA 完成回调=1000 Hz/秒，DAC 真在 1 kHz）
- DAC 输出经 PA4 → PA0 接入 ADC
- `pipeline run` 报 `peak_freq=12993 Hz`、`peak_bin=52`，与 1 kHz 期望差出**精确 13 倍**
- ADC 启动摘要 / `pipeline status` 显示采样率 = 255,863 Hz（接近名义 256 kHz）
- 用 `dacrate` 验证 DAC 没问题，用 `adcrate` 验证发现 `actual_sps=19712 Hz`、`ratio=0.077`

## 3. 影响范围

- `App/adc/adc_app.c`：ADC 周期采集功能
- `App/dsp/dsp_pipeline.c`：实时流水线频谱分析
- 任何依赖 ADC `DEFAULT_SAMPLE_RATE_HZ=256000` 的下游模块（FFT bin→Hz 换算、滤波带宽等）
- CubeMX `.ioc` 配置文件
- CubeMX 生成的 `Core/Src/adc.c`

## 4. 复现条件

- 硬件：STM32H743，HCLK 240 MHz，CKPER = HSI = 64 MHz
- ADC1 配置（CubeMX 默认）：
  - `ClockPrescaler = ADC_CLOCK_ASYNC_DIV4` → ADCCLK = 16 MHz
  - `Resolution = ADC_RESOLUTION_16B`
  - `ExternalTrigConv = ADC_EXTERNALTRIG_T2_TRGO`
  - **`SamplingTime = ADC_SAMPLETIME_387CYCLES_5`**（CubeMX 默认值）
- TIM2 配置（应用层 `ADC_APP_ConfigureTim2Rate`）：256 kHz 触发率
- 复现稳定。任何 PA0 上的输入信号，FFT 峰值频率都被错算 ×13。

## 5. 根因分析

**结论：CubeMX 默认的 387.5 周期采样时间，让 ADC 单次转换需要约 396 cycles，而 16 MHz 的 ADCCLK 下这相当于每秒最多约 40 kHz 的转换速度，远低于 256 kHz 的触发速度，导致 ADC 大部分触发被丢弃。**

数学：

| 项 | 值 |
|---|---|
| ADCCLK | 64 MHz / 4 = 16 MHz |
| 每次转换周期 | 387.5（采样）+ 8.5（16-bit conv）≈ 396 cycles |
| 实际每次转换耗时 | 396 / 16M ≈ 24.75 µs |
| 每秒最大转换数 | 16M / 396 ≈ 40,404 Hz |
| 实测 | 19,712 Hz（实际 H7 ADC 还有额外触发判断与数据处理开销）|
| 256 kHz 触发实际命中率 | 19712 / 256000 ≈ 7.7% = **1/13** |

下游 pipeline 用"声称的"采样率 255,863 Hz 把 bin 索引反算成 Hz，于是**真实 1 kHz** 在真实采样率 19712 Hz 下落在 bin 52；pipeline 错把它当作 `52 × 255863 / 1024 = 12993 Hz`——这就是 13× 的精确出处。

排查过程中曾经怀疑但被排除的项：

- DAC TIM6 配错（用 `[DAC-DBG]` 寄存器读出来 PSC=0/ARR=937 完全正确）
- DAC trigger 错（CR 寄存器 TSEL=5 = TIM6_TRGO 正确）
- DAC HFSEL 没设（H743 设备根本没这个寄存器位）
- DMA 触发错（`dacrate` 命令测出 DAC 真出 1 kHz）
- TIM2 配错（`adcdump` 命令读出 PSC=0/ARR=937 完全正确）
- ADC EXTSEL 错（CFGR 寄存器 EXTSEL=11 = TIM2_TRGO 正确）

最终通过 `adcrate` 命令直接测 ADC 半/全 DMA 回调速率，定位到**ADC 真在丢触发**，然后才往采样时间方向查到根因。

## 6. 解决方案

把 ADC 通道 16（PA0）的采样时间从 `ADC_SAMPLETIME_387CYCLES_5` 改成 `ADC_SAMPLETIME_8CYCLES_5`。

修改后：

| 项 | 值 |
|---|---|
| 每次转换周期 | 8.5 + 8.5 = 17 cycles |
| 每秒最大转换数 | 16M / 17 ≈ **940 kHz** |
| 256 kHz 命中率 | 100% ✅ |
| 实测采样率 | 255,872 Hz（与名义 256 kHz 误差 0.05%）|

8.5 cycle 采样窗口 = 531 ns，对 DAC buffer 模式输出（输出阻抗 ~10-15 Ω）和典型运放/低阻抗信号源完全够用。如果以后接入 >50 kΩ 的高阻抗源（热敏电阻、压电传感器等），需要重新评估是否改用更长采样时间。

**没用的替代方案**：

- 在 `ADC_APP_Init` 里 runtime 调 `HAL_ADC_ConfigChannel` 覆盖采样时间——可行但 CubeMX 重生时仍会写错值，每次都要 runtime 修复。直接改 `.ioc` 一劳永逸。
- 改预分频器从 DIV4 改 DIV1 → ADCCLK 提到 64 MHz——不解决根因（仍需 396 cycles），且超过 H743 ADC 最大 ADCCLK 50 MHz 限值。

## 7. 修改位置

- `zuolan_signal_STM32.ioc`
  - `ADC1.SamplingTime-0\#ChannelRegularConversion=ADC_SAMPLETIME_8CYCLES_5`
- 重新用 CubeMX 生成 `Core/Src/adc.c`，里面 `sConfig.SamplingTime` 会跟着改

## 8. 验证方法

1. CubeMX 打开 `.ioc`，**Code generator → Generate Code**（不需要改任何其它配置）
2. `rm -rf build`，重新 configure + build + 烧录
3. 通过 USART1 CLI 测试：

   ```
   > adcrate
   预期：actual_sps ≈ 256000，ratio ≈ 1.000
   ```

4. 再做闭环验证：

   ```
   > dac stop
   > dac freq 1000
   > dac mode sine
   > dac start
   > pipeline run
   预期：peak_bin=4 peak_freq≈999.5 Hz mag≈数百万级
   ```

## 9. 验证结果

✅ 已验证通过。`adcrate` 输出：

```
[ADC-RATE] events/sec=1999 actual_sps=255872 claimed_sps=255863 ratio=1.000
```

`pipeline run` 输出：

```
[PIPELINE] seq=1 filter=none fft=1024 fs=255863 Hz frame_mean=-41.7 peak_bin=4 peak_freq=999.46 Hz mag=2503881.50
```

## 10. 注意事项 / 后续建议

- **CubeMX 默认采样时间是为高阻抗源准备的**。任何对采样率敏感的 DSP 应用都应该显式核对 `SamplingTime`，不能默认相信 CubeMX 给的值。
- **采样时间 + 预分频 + 分辨率三者共同决定 ADC 最大转换率**。换公式：`max_sps = ADCCLK / (SamplingTime + ResolutionCycles)`。其中 `ResolutionCycles` 见下表：

  | Resolution | Cycles |
  |---|---|
  | 16-bit | 8.5 |
  | 14-bit | 7.5 |
  | 12-bit | 6.5 |
  | 10-bit | 5.5 |
  | 8-bit  | 4.5 |

- **触发率 ≤ 最大转换率** 是硬性要求。一旦突破，ADC 静默丢触发，不会报错也不会拉 OVR 标志（因为是触发被吞，不是数据被覆盖），表现为采样率诡异变慢，需要靠 `adcrate` 这类计数器才能发现。
- 工程当前提供的诊断工具（保留）：
  - `adcrate` / `dacrate`：实测真实采样/输出速率
  - `adcdump`：dump TIM2 + ADC1 关键寄存器
- 如果以后改 ADC 分辨率（比如降到 12-bit）、或换其它 ADC 通道，必须**重新评估采样时间**。低分辨率允许更短采样时间，可拉满 1+ MHz 转换率。
- 如果以后 HCLK 变化（比如做低功耗）、或 CKPER 切换源（HSE/PLL），ADCCLK 跟着变，最大转换率也会变，要重新核算。
