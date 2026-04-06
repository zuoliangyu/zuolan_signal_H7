# ADC 设计与实现说明

本文档记录当前工程中 `ADC` 部分的第一版实现、配置依据、CLI 命令和后续扩展建议。当前平台为 `STM32H743`。

## 1. 当前实现结论

当前 `ADC` 已按“先稳定跑通单通道链路”的思路落地完成：

1. `ADC1`
2. 单通道
3. `PA0 -> ADC1_INP16`
4. 软件启动
5. 连续转换
6. `DMA Circular`

应用层已新增：

- `App/adc/adc_app.c`
- `App/adc/adc_app.h`

当前已经具备：

- 连续采样
- DMA 环形缓冲
- 最新值读取
- 平均值读取
- 原始值转毫伏
- 串口 CLI 查询
- 串口连续 CSV 输出

## 2. CubeMX 关键配置

当前第一版配置如下：

- `ADC1`
- `Channel 16`
- `PA0`
- `Resolution = 16-bit`
- `Continuous Conversion Mode = Enabled`
- `External Trigger = Software Start`
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

- `ADC_APP_DMA_SAMPLES = 64`

该缓冲区放在 `.dma_buffer` 段中，而不是默认 `.bss`。

原因：

- `STM32H743` 的 `DMA` 不能访问 `DTCM`
- 当前工程默认普通全局变量大多落在 `DTCM`
- 如果 ADC DMA 缓冲区放错内存，轻则数据不更新，重则直接异常

### 4.2 校准与启动

当前 `ADC_APP_Init()` 的流程是：

1. 清空 DMA 缓冲区
2. 执行 `ADC` 校准
3. 启动 `HAL_ADC_Start_DMA()`

当前使用：

- `ADC_CALIB_OFFSET_LINEARITY`
- `ADC_SINGLE_ENDED`

这符合当前单端输入场景。

### 4.3 数据接口

当前对外提供：

- 最新原始值
- 最新毫伏值
- 平均原始值
- 平均毫伏值

其中：

- 最新值用于快速观测
- 平均值用于看更稳定的趋势

### 4.4 连续串口输出

当前增加了 `adc_proc` 周期任务。

它的职责不是采样本身，而是：

- 判断是否开启了 ADC 串口流输出
- 到达设定周期后，从当前 DMA 缓冲区读最新值
- 通过 `USART1` 输出一行 CSV

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

## 5. CLI 命令集

当前 ADC 命令如下：

- `adc get`
- `adc raw`
- `adc mv`
- `adc avg`
- `adc stream on [ms]`
- `adc stream off`
- `adc stream ?`
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
- `adc stream on [ms]`
  - 开启连续输出
  - 可选参数 `ms` 为输出周期
  - 当前允许范围为 `2..65535ms`
  - 省略时沿用当前周期
- `adc stream off`
  - 关闭连续输出
- `adc stream ?`
  - 查看流输出状态和周期
- `adc help`
  - 显示帮助

默认流输出周期：

- `20ms`

建议：

- 连续串口输出不要设得过快
- 当前工程将最小周期限制为 `2ms`
- 原因是 `115200` 波特率下，`1ms` 输出一次 `raw,mv` 很容易把串口打满

## 6. 启动摘要

当前系统上电后，`USART1` 启动摘要中会新增 ADC 状态行，包含：

- `state`
- `pin`
- `channel`
- `dma_samples`
- `latest_raw`
- `latest_mv`
- `stream`
- `interval_ms`

这样上电后可以第一时间确认：

- ADC 是否已成功启动
- 当前采样链路是否已接通
- 默认串口流输出是否关闭

## 7. 风险与注意事项

当前主要注意点如下：

- 如果后续启用 `D-Cache`，ADC DMA 缓冲区还要补缓存一致性处理
- 当前毫伏换算默认按 `3300mV` 参考电压计算，并不等价于高精度绝对电压测量
- 如果模拟源阻抗较高，虽然当前 `387.5 cycles` 已较保守，但仍应实测稳定性
- 当前连续输出与 CLI 共用 `USART1`，在流输出开启时，命令行内容会和流数据共存，这是正常现象

## 8. 当前验收标准

当前版本建议按以下方式验收：

1. 上电后启动摘要中看到 `ADC1: state=running`
2. 输入 `adc raw`，能读到非固定死值
3. 改变 `PA0` 输入电压后，`adc mv` 随之变化
4. 输入 `adc stream on 20`
5. 串口持续输出 `raw,mv`
6. 输入 `adc stream off`
7. 流输出停止

## 9. 后续扩展建议

当前第一版已经够做基础观测和串口绘图，后续如要继续增强，建议顺序如下：

1. 增加滑动平均或中值滤波
2. 增加多通道扫描
3. 改为定时器触发固定采样率
4. 增加 DMA 半满 / 满回调处理
5. 增加 `VREFINT` 标定与更准确的电压换算
