# ADC 设计与实现说明

本文档记录当前工程中 `ADC` 部分的第一版实现、配置依据、CLI 命令和后续扩展建议。当前平台为 `STM32H743`。

## 1. 当前实现结论

当前 `ADC` 已按“先稳定跑通单通道链路”的思路落地完成：

1. `ADC1`
2. 单通道
3. `PA0 -> ADC1_INP16`
4. `TIM2 TRGO` 定时触发
5. 非连续转换
6. `DMA Circular`

应用层已新增：

- `App/adc/adc_app.c`
- `App/adc/adc_app.h`

当前已经具备：

- 连续采样
- DMA 环形缓冲
- 半缓冲 / 全缓冲回调
- 块队列缓存
- 整帧快照
- 最新值读取
- 平均值读取
- 原始值转毫伏
- 串口 CLI 查询
- 串口连续 CSV 输出

## 1.1 当前默认对齐关系

当前工程默认将 `DAC` 和 `ADC` 的时基按“一周期 256 点”对齐：

- `DAC` 默认波形频率：`1kHz`
- `DAC` 默认波表点数：`256`
- `DAC` 默认更新频率：`256kHz`
- `ADC` 默认采样点数：`256`
- `ADC` 默认采样率：`256kHz`

因此默认状态下：

- `DAC` 输出一个完整周期
- `ADC` 恰好采满一帧 `256` 点

这更适合直接观察输入输出波形对应关系。

## 2. CubeMX 关键配置

当前第一版配置如下：

- `ADC1`
- `Channel 16`
- `PA0`
- `Resolution = 16-bit`
- `Continuous Conversion Mode = Disabled`
- `External Trigger = TIM2 TRGO`
- `External Trigger Edge = Rising`
- `Conversion Data Management Mode = DMA Circular`
- `Overrun = Data Overwritten`
- `Sampling Time = 387.5 cycles`
- `Clock Prescaler = Asynchronous /4`

DMA 配置如下：

- `DMA1_Stream3`
- `Peripheral To Memory`
- `Mode = Circular`
- `Peripheral Data Width = Half Word`
- `Memory Data Width = Half Word`
- `Priority = High`

时钟方面：

- 当前 `ADC` 内核时钟使用 `PER_CK`
- 这样做是为了绕开 `PLL2` 非法参数组合带来的 `CubeMX` 生成警告

对应问题记录见：

- `docs/纠错-ADC时钟配置导致CubeMX生成警告.md`

## 3. 软件结构

当前职责划分如下：

- `Core/Inc/adc.h`
- `Core/Src/adc.c`

职责：

- 保留 `CubeMX` 自动生成的 ADC 外设初始化

- `App/adc/adc_app.c`
- `App/adc/adc_app.h`

职责：

- 启动 ADC 校准
- 启动 ADC DMA 连续采样
- 维护 ADC DMA 缓冲区
- 提供最新值、平均值、毫伏换算接口
- 提供连续串口输出控制

## 4. 当前实现细节

### 4.1 DMA 缓冲区

当前 ADC 应用层使用一个固定长度的 DMA 缓冲区：

- `ADC_APP_DMA_SAMPLES = 256`

该缓冲区放在 `.dma_buffer` 段中，而不是默认 `.bss`。

原因：

- `STM32H743` 的 `DMA` 不能访问 `DTCM`
- 当前工程默认普通全局变量大多落在 `DTCM`
- 如果 ADC DMA 缓冲区放错内存，轻则数据不更新，重则直接异常

### 4.2 采样时基

当前 ADC 的采样时基由 `TIM2` 提供。

关系如下：

- `TIM2 update event`
- `TIM2 TRGO`
- `ADC1 external trigger`
- `ADC1 sample once`
- `DMA store result`

这和最早的“软件启动 + 连续转换”有本质区别：

- 现在每个采样点之间的时间间隔是固定的
- 更适合做周期信号采集
- 后续调整采样率时，只需要重配 `TIM2`

### 4.3 校准与启动

当前 `ADC_APP_Init()` 的流程是：

1. 清空 DMA 缓冲区
2. 执行 `ADC` 校准
3. 配置 `TIM2` 采样率
4. 启动 `HAL_ADC_Start_DMA()`
5. 启动 `TIM2`

当前默认采样率：

- `ADC_APP_DEFAULT_SAMPLE_RATE_HZ = 256000`

当前使用：

- `ADC_CALIB_OFFSET_LINEARITY`
- `ADC_SINGLE_ENDED`

这符合当前单端输入场景。

### 4.4 半缓冲 / 全缓冲处理

当前应用层已经实现：

- `HAL_ADC_ConvHalfCpltCallback()`
- `HAL_ADC_ConvCpltCallback()`

处理原则：

- 半缓冲完成时，把前半块复制进软件队列
- 全缓冲完成时，把后半块复制进软件队列
- 全缓冲完成时，再额外复制一份完整帧快照

这样做的目的：

- 半块数据可以尽快被主循环处理
- 完整一帧数据也能稳定导出
- 即使后面 DMA 继续覆盖原始缓冲区，软件层仍然保留了一个稳定副本

当前块大小：

- `ADC_APP_BLOCK_SAMPLES = 128`

当前块队列深度：

- `ADC_APP_CAPTURE_QUEUE_DEPTH = 4`

如果主循环或串口输出跟不上，后续新块会被丢弃，并累计到：

- `dropped_blocks`

### 4.5 数据接口

当前对外提供：

- 最新原始值
- 最新毫伏值
- 平均原始值
- 平均毫伏值
- 采样率查询/设置
- 最新完整帧复制
- 块队列读取

其中：

- 最新值用于快速观测
- 平均值用于看更稳定的趋势

### 4.6 连续串口输出

当前 `adc_proc` 周期任务同时承担两类输出：

1. 低速监视输出
2. 块流输出

它的职责不是采样本身，而是：

- 判断是否开启了 ADC 低速监视流
- 判断是否有半块/整块待导出的采样块
- 通过 `USART1` 输出监视值或块数据

当前输出格式：

```text
raw,mv
```

示例：

```text
32450,1634
32518,1638
32390,1631
```

这样做的原因：

- 简单
- 易被上位机工具或串口绘图工具识别
- 后续如果你要导入 `VOFA+`、串口助手、脚本采集，都比较方便

对于块流输出，当前格式更偏向“批量导出”：

```text
12345
12340
12338
...
```

也就是：

- 一行一个 `raw`
- 由半缓冲 / 全缓冲回调驱动
- 更适合后续保存后离线画图

## 5. CLI 命令集

当前 ADC 命令如下：

- `adc get`
- `adc raw`
- `adc mv`
- `adc avg`
- `adc rate <hz|?>`
- `adc frame`
- `adc frame ?`
- `adc stream on [ms]`
- `adc stream off`
- `adc stream ?`
- `adc block on`
- `adc block off`
- `adc block ?`
- `adc next`
- `adc help`

说明：

- `adc get`
  - 显示完整 ADC 状态
- `adc raw`
  - 显示最新 ADC 原始值
- `adc mv`
  - 显示最新 ADC 毫伏值
- `adc avg`
  - 显示当前缓冲区平均原始值和平均毫伏值
- `adc rate <hz>`
  - 设置 ADC 实际采样率
  - 本质上是重配 `TIM2`
- `adc rate ?`
  - 查询当前采样率
- `adc frame`
  - 导出最近一整帧完整数据
  - 输出格式为 `index,raw,mv`
- `adc frame ?`
  - 查看整帧大小与最近帧序号
- `adc stream on [ms]`
  - 开启低速监视输出
  - 可选参数 `ms` 为输出周期
  - 当前允许范围为 `2..65535ms`
  - 省略时沿用当前周期
- `adc stream off`
  - 关闭低速监视输出
- `adc stream ?`
  - 查看低速监视输出状态和周期
- `adc block on`
  - 开启块流输出
  - 每当半缓冲或全缓冲就导出一块 `raw`
- `adc block off`
  - 关闭块流输出
- `adc block ?`
  - 查看块流状态、事件计数、丢块统计
- `adc next`
  - 手动取出一个已完成的采样块并打印
- `adc help`
  - 显示帮助

默认流输出周期：

- `20ms`

建议：

- 连续串口输出不要设得过快
- 当前工程将最小周期限制为 `2ms`
- 串口已经从 `115200` 升到 `921600` 并改 TX DMA 异步，`adc stream` 已基本不会把串口打满；该 2ms 下限主要为防止 CLI 解析占满主循环
- 同理，块流输出（`adc block on`）会按 ADC 实际节奏推 128 点块，在 256 kHz 默认采样率下每秒 ~2000 块，串口仍可能跟不上 → 出现 `dropped_blocks`，需要时配合 `pipeline rate` 思路降频或改走 `fftdump` 一次性导出

## 5.1 DAC 与 ADC 的默认关系

当前默认 `DAC` 侧的设计关系为：

- 波形频率：`1kHz`
- 波表点数：`256`
- `TIM6` 更新频率：`256kHz`

对应 `ADC` 默认关系为：

- `TIM2` 采样率：`256kHz`
- DMA 一帧点数：`256`

所以当前默认状态适合理解为：

- `ADC frame` 默认就是 `DAC` 一个完整周期的采样结果

## 6. 启动摘要

当前系统上电后，`USART1` 启动摘要中会新增 ADC 状态行，包含：

- `state`
- `pin`
- `channel`
- `rate_hz`
- `dma_samples`
- `block_samples`
- `latest_raw`
- `latest_mv`
- `monitor`
- `interval_ms`
- `block_stream`

这样上电后可以第一时间确认：

- ADC 是否已成功启动
- 当前采样链路是否已接通
- 默认串口流输出是否关闭

## 7. 风险与注意事项

当前主要注意点如下：

- 如果后续启用 `D-Cache`，ADC DMA 缓冲区还要补缓存一致性处理
- 当前毫伏换算默认按 `3300mV` 参考电压计算，并不等价于高精度绝对电压测量
- 当前采样时间是 `8.5 cycles`（`adc.c:80` 的 `ADC_SAMPLETIME_8CYCLES_5`），是为了配合 256 kHz 采样率压缩单次转换耗时；如果模拟源阻抗较高，建议实测看 raw 是否稳定，必要时上调到 `16.5 / 32.5 cycles`，但要同步降低采样率避免触发被吞（详见 `docs/纠错/ADC采样时间过长导致实际采样率被吞.md` —— 这是反向警告，但同样适用于"过短"情况下的 SNR 评估）
- 当前连续输出与 CLI 共用 `USART1`，在监视流或块流开启时，命令行内容会和数据共存，这是正常现象
- 采样率越高，越不适合直接开串口块流，必要时优先使用 `adc frame`

## 8. 当前验收标准

当前版本建议按以下方式验收：

1. 上电后启动摘要中看到 `ADC1: state=running`
2. 输入 `adc rate ?`，能看到当前采样率
3. 输入 `adc raw`，能读到非固定死值
4. 改变 `PA0` 输入电压后，`adc mv` 随之变化
5. 输入 `adc frame`，能导出完整一帧数据
6. 输入 `adc next`，能取出半缓冲/全缓冲采样块
7. 输入 `adc block ?`，事件计数持续增长

## 9. 后续扩展建议

当前第一版已经够做基础观测和串口绘图，后续如要继续增强，建议顺序如下：

1. 增加滑动平均或中值滤波
2. 增加多通道扫描
3. 改为定时器触发固定采样率
4. 增加 DMA 半满 / 满回调处理
5. 增加 `VREFINT` 标定与更准确的电压换算
