# 纠错记录索引

集中存放工程历史踩坑记录，按主题归类便于回溯。

新增问题统一使用 [模板.md](模板.md)。

## 1. ADC / DAC / 采样链路

- [ADC时钟配置导致CubeMX生成警告.md](ADC时钟配置导致CubeMX生成警告.md)
  - 启用 `ADC` 后 CubeMX 因时钟树未完整配置产生警告
- [ADC改为TIM2触发后未启动定时器导致无采样.md](ADC改为TIM2触发后未启动定时器导致无采样.md)
  - `ADC` 切换到 `TIM2 TRGO` 后应用层未 `HAL_TIM_Base_Start` 导致采样链中断
- [ADC采样时间过长导致实际采样率被吞.md](ADC采样时间过长导致实际采样率被吞.md)
  - CubeMX 默认 `SamplingTime = 387.5 cycles` 在 256 kHz 触发率下让 ADC 丢掉 12/13 触发，下游 FFT 频率算错 13×

## 2. 串口 / CLI / 启动日志

- [STM32H743串口DMA接收无回显.md](STM32H743串口DMA接收无回显.md)
  - H7 上 UART DMA 缓冲区放错内存导致接收链路异常
- [UART启动摘要被printf缓冲区截断.md](UART启动摘要被printf缓冲区截断.md)
  - 启动摘要单次输出过长被 `printf` 缓冲长度截断

## 3. LED / 命令行交互

- [LED命令状态被自动闪烁覆盖.md](LED命令状态被自动闪烁覆盖.md)
  - LED 手动控制命令与自动闪烁逻辑冲突

## 4. CubeMX / 工具链 / 迁移

- [CubeMX重新生成丢失自定义链接段与printf浮点选项.md](CubeMX重新生成丢失自定义链接段与printf浮点选项.md)
  - 切换 MCU 或重新生成代码后 `.dma_buffer` 段与 `-u _printf_float` 被默认模板覆盖，导致串口无输出 + `printf("%f")` 不出值

## 5. 维护约定

- 文件名格式：`<主题>.md`（不需要 `纠错-` 前缀，目录名已标明分类）
- 新增前先看是否已有相同主题文档可更新，避免重复建条目
- 模板见 [模板.md](模板.md)
