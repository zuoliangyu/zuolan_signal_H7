# 串口与 CLI 文档索引

本目录集中整理工程里与串口收发链路、DMA 缓冲、CLI 命令解析和启动日志相关的正式文档。

## 1. 建议阅读顺序

1. [UART接收调试与CLI运行说明.md](UART接收调试与CLI运行说明.md)
2. [CLI命令解析与扩展说明.md](CLI命令解析与扩展说明.md)
3. [UART异步发送与DMA架构.md](UART异步发送与DMA架构.md)
4. 排查历史问题：[../纠错/README.md](../纠错/README.md)

## 2. 当前工程状态

- `USART1` @ 921600 baud：CLI 口，**RX 与 TX 都走 DMA**，TX 配 4KB 环形缓冲（异步发送）
- `USART2` @ 115200 baud：原样回显口
- `App/uart` 负责 RX DMA + IDLE + HT + TC + RingBuffer 接收，TX DMA 异步发送，启动摘要
- `App/cli` 负责分词、命令表与执行

当前 CLI 命令：

| 类别 | 命令 |
|---|---|
| 基础 | `help` `echo` |
| LED | `led on / off / toggle / blink [ms]` |
| DAC | `dac get/mode/amp/offset/freq/duty/start/stop/test/help` |
| ADC | `adc get/raw/mv/avg/rate/frame/stream/block/next/help` |
| FFT 自测 | `fft selftest` |
| 滤波器自测 | `filter selftest` |
| 实时流水线 | `pipeline status/filter/fft/dc/rate/run/stream` |
| 诊断工具 | `dacrate` `adcrate` `adcdump` `uarttx` `report` `clearstats` |

## 3. 文档分工

### [UART接收调试与CLI运行说明.md](UART接收调试与CLI运行说明.md)

- `USART1 / USART2` 分工
- `RX DMA + IDLE + RingBuffer` 接收流程
- 启动日志输出内容
- H7 上 DMA 缓冲区为什么要单独放段

### [CLI命令解析与扩展说明.md](CLI命令解析与扩展说明.md)

- 输入数据如何进入 CLI
- 行缓冲与分词逻辑
- 命令表结构与扩展
- 新增命令推荐改哪里

### [UART异步发送与DMA架构.md](UART异步发送与DMA架构.md)

- `my_printf` 从同步阻塞改为 DMA + 4 KB 环形缓冲
- 921600 baud + DMA TX 的 CubeMX 配置步骤
- 实时流水线性能数据对比（22.5× 帧率提升）
- 配套 CLI 工具：`uarttx` `report` `clearstats`

## 4. 相关问题记录

- [../纠错/STM32H743串口DMA接收无回显.md](../纠错/STM32H743串口DMA接收无回显.md)
- [../纠错/UART启动摘要被printf缓冲区截断.md](../纠错/UART启动摘要被printf缓冲区截断.md)
- [../纠错/LED命令状态被自动闪烁覆盖.md](../纠错/LED命令状态被自动闪烁覆盖.md)
- [../STM32H7_DMA缓冲区与链接脚本说明.md](../STM32H7_DMA缓冲区与链接脚本说明.md)
