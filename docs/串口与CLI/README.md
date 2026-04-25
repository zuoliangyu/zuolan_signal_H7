# 串口与 CLI 文档索引

本目录集中整理当前工程里与串口接收链路、DMA 接收缓冲、CLI 命令解析和启动日志输出相关的正式文档。

## 1. 建议阅读顺序

1. [UART接收调试与CLI运行说明.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/串口与CLI/UART接收调试与CLI运行说明.md)
2. [CLI命令解析与扩展说明.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/串口与CLI/CLI命令解析与扩展说明.md)
3. [UART异步发送与DMA架构.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/串口与CLI/UART异步发送与DMA架构.md)
4. 如需排查历史问题，再回根目录看纠错文档

## 2. 当前工程状态

截至当前版本：

- `USART1` 作为 CLI 口
- `USART2` 作为原样回显口
- 串口接收链路使用 `RX DMA + IDLE + HT + TC + RingBuffer`
- `App/uart` 负责 DMA 接收、环形缓冲和启动摘要
- `App/cli` 负责分词、命令表和命令执行

当前 CLI 命令按模块分为：

- 基础命令
  - `help`
  - `echo`
- LED
  - `led on / off / toggle / blink [interval_ms]`
- DAC
  - `dac get / mode / amp / offset / freq / duty / start / stop / help`
- ADC
  - `adc get / raw / mv / avg / rate / frame / stream / block / next / help`

## 3. 文档分工

### [UART接收调试与CLI运行说明.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/串口与CLI/UART接收调试与CLI运行说明.md)

重点说明：

- `USART1 / USART2` 的分工
- `RX DMA + IDLE + RingBuffer` 的接收流程
- 启动日志输出内容
- `STM32H7` 上 DMA 缓冲区为什么要单独放段

### [CLI命令解析与扩展说明.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/串口与CLI/CLI命令解析与扩展说明.md)

重点说明：

- 输入数据如何进入 CLI
- 行缓冲和分词逻辑
- 命令表结构
- 当前命令如何扩展
- 新增命令时推荐改哪里

### [UART异步发送与DMA架构.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/串口与CLI/UART异步发送与DMA架构.md)

重点说明：

- `my_printf` 从同步阻塞改为 DMA + 4KB 环形缓冲的动机和架构
- 921600 baud + DMA TX 的 CubeMX 配置步骤
- 实时流水线性能数据对比（升级前后 22.5× 帧率提升）
- 相关 CLI 工具（`uarttx` `report` `clearstats`）

## 4. 相关问题记录

以下文档保留为问题追踪记录，不作为正式说明主入口：

- [纠错-STM32H743串口DMA接收无回显.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/纠错-STM32H743串口DMA接收无回显.md)
- [纠错-UART启动摘要被printf缓冲区截断.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/纠错-UART启动摘要被printf缓冲区截断.md)
- [纠错-LED命令状态被自动闪烁覆盖.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/纠错-LED命令状态被自动闪烁覆盖.md)
- [STM32H7_DMA缓冲区与链接脚本说明.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/STM32H7_DMA缓冲区与链接脚本说明.md)
