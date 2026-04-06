# 串口与 CLI 文档索引

本目录用于集中整理当前工程中与串口调试、DMA 接收链路、CLI 命令解析相关的正式文档。

建议阅读顺序：

1. `UART接收调试与CLI运行说明.md`
2. `CLI命令解析与扩展说明.md`
3. 如需排查历史问题，再看根目录下的纠错文档

## 1. 文档分工

### `UART接收调试与CLI运行说明.md`

说明当前工程中串口底层接收链路和运行状态，重点包括：

- `USART1 / USART2` 的硬件配置
- `RX DMA + IDLE + RingBuffer` 的接收流程
- 启动状态打印内容
- 当前运行行为和验证方式
- 与 `STM32H7` 内存布局、`D-Cache` 相关的注意事项

适合在你需要回答这些问题时阅读：

- 当前串口为什么这样配置
- 为什么 `USART1` 走 CLI，`USART2` 只做回显
- 为什么 DMA 缓冲区必须单独放到 `RAM_D2`
- 现在应该如何做串口联调

### `CLI命令解析与扩展说明.md`

说明当前 CLI 层的实现与扩展方式，重点包括：

- 输入数据如何从 UART 进入 CLI
- 行缓冲与分词逻辑
- 命令表结构
- 当前支持的命令与行为
- 后续如何新增命令

适合在你需要回答这些问题时阅读：

- `help / echo / led` 是怎么实现的
- `dac get / dac mode ? / dac amp 500` 是怎么实现的
- `led blink 500` 的参数语义是什么
- 要增加一个新命令应该改哪里

## 2. 相关问题记录

以下文档保留为问题追踪记录，不作为正式功能说明的主入口：

- [纠错-STM32H743串口DMA接收无回显.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/纠错-STM32H743串口DMA接收无回显.md)
- [纠错-LED命令状态被自动闪烁覆盖.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/纠错-LED命令状态被自动闪烁覆盖.md)
- [STM32H7_DMA缓冲区与链接脚本说明.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/STM32H7_DMA缓冲区与链接脚本说明.md)

## 3. 当前实现结论

截至当前版本：

- `USART1` 作为 CLI 口
- `USART2` 作为原样回显口
- 串口接收使用 `RX DMA + IDLE + HT + TC + RingBuffer`
- CLI 在 `App/cli`，UART 应用层在 `App/uart`
- LED 命令支持 `on / off / toggle / blink [interval_ms]`
- DAC 命令支持 `get / mode / amp / offset / freq / duty(%) / help / start / stop`
