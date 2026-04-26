# UART 接收调试与 CLI 运行说明

本文档用于说明当前工程中串口接收链路、调试方式和运行行为。它关注的是“串口整条链路是如何工作的”，而不是单独某一条命令如何解析。

## 1. 当前硬件配置

当前工程启用了两路异步串口：

- `USART1`（CLI / 数据口）
  - `TX = PA9`
  - `RX = PA10`
  - `921600 8N1`（已从 115200 提速以容纳 pipeline / fftdump 的高频输出）
  - `RX DMA = DMA1_Stream0`
  - `TX DMA = DMA1_Stream2`（异步发送，详见 `UART异步发送与DMA架构.md`）
  - `DMA Request = DMA_REQUEST_USART1_RX / DMA_REQUEST_USART1_TX`
- `USART2`（保底回显口）
  - `TX = PA2`
  - `RX = PA3`
  - `115200 8N1`（仍保持低速，仅作底层回显联调用）
  - `RX DMA = DMA1_Stream1`
  - `DMA Request = DMA_REQUEST_USART2_RX`

当前 `.ioc` 已同步以下关键配置：

- `USART1_RX -> DMA1_Stream0`
- `USART2_RX -> DMA1_Stream1`
- `USART1 global interrupt`
- `USART2 global interrupt`
- `DMA1_Stream0_IRQn`
- `DMA1_Stream1_IRQn`

## 2. 当前分层结构

当前工程按职责分层如下：

- `Core/dma.*`
  - DMA 时钟与 DMA NVIC 初始化
- `Core/usart.*`
  - `USART1 / USART2` 底层初始化
  - RX DMA 句柄绑定
  - USART NVIC 初始化
- `Core/stm32h7xx_it.*`
  - DMA 和 USART 中断入口
- `App/uart/uart.*`
  - DMA 接收启动
  - `IDLE / HT / TC` 回调处理
  - 软件环形缓冲区
  - `USART1` 的 CLI 数据投递
  - `USART2` 的原样回显
- `App/cli/cli.*`
  - 命令行解析
  - 命令表分发
  - 提示符与响应输出

这套分层的核心原则是：

- `Core` 负责硬件和中断入口
- `App/uart` 负责串口应用收发链路
- `App/cli` 负责文本命令解析

## 3. 接收链路总览

当前串口接收不是阻塞式轮询，而是：

- `RX DMA + Circular`
- `HAL_UARTEx_ReceiveToIdle_DMA()`
- `HAL_UARTEx_RxEventCallback()`
- 软件环形缓冲区

接收链路如下：

```text
USART RX
  -> DMA buffer
  -> HAL_UARTEx_RxEventCallback()
  -> delta copy to ring buffer
  -> uart_proc()
  -> USART1: CLI_InputBuffer()
  -> USART2: raw echo
```

### 3.1 回调事件

当前会处理三类接收事件：

- `HAL_UART_RXEVENT_HT`
- `HAL_UART_RXEVENT_TC`
- `HAL_UART_RXEVENT_IDLE`

含义分别是：

- 半缓冲触发
- 满缓冲触发
- 串口空闲触发

### 3.2 环形缓冲区设计

每个串口各自维护一套缓冲区：

- DMA 接收缓冲区：`256B`
- 软件环形缓冲区：`1024B`

设计目的：

- DMA 缓冲区负责连续接收
- 环形缓冲区负责把中断侧和主循环侧解耦

## 4. 为什么 DMA 缓冲区单独分段

当前工程中 DMA 接收缓冲区放在 `.dma_buffer` 段，并映射到 `RAM_D2`。

这样做的原因是：

- 当前工程普通 `.data/.bss` 默认在 `DTCMRAM`
- `STM32H743` 的普通 DMA 不能访问 `DTCM`
- 如果 UART DMA 缓冲区继续落在 `DTCM`，就会出现“发送正常、接收无回显”的问题

如果你想专门理解这部分，可继续看：

- [STM32H7_DMA缓冲区与链接脚本说明.md](../STM32H7_DMA缓冲区与链接脚本说明.md)
- [STM32H743串口DMA接收无回显.md](../纠错/STM32H743串口DMA接收无回显.md)

## 5. 当前运行行为

`main.c` 当前接入顺序为：

1. `MX_GPIO_Init()`
2. `MX_DMA_Init()`
3. `MX_USART1_UART_Init()`
4. `MX_USART2_UART_Init()`
5. `Scheduler_Init()`
6. `LED_Init()`
7. `UART_Init()`
8. 注册 `led_proc`
9. 注册 `uart_proc`

当前行为如下：

- `USART1` 作为 CLI 口
- `USART2` 保留为原样回显口
- 上电后 `USART1` 会先打印启动状态摘要，再显示 `> ` 提示符

### 5.1 启动状态打印

当前启动时 `USART1` 会输出类似下面的内容（精确字段以代码为准，这里只示意结构）：

```text
System boot summary
USART1: mode=cli, baud=921600, rx_dma=ready, tx_dma=ready, ring_buf=4096
USART2: mode=echo, baud=115200, rx_dma=ready, dma_buf=256, ring_buf=1024
LED0:   pin=PG7, state=0, blink=1, interval_ms=500, active_level=low
DAC1_CH1: state=running, mode=dc, amp_mv=500, offset_mv=1650, freq_hz=1000, duty=50, raw=2048
ADC1:   pin=PA0, channel=16, rate_hz=256000, dma_samples=256, block_samples=128
Commands: help / echo / led / adc / dac / fft / fftdump / filter / pipeline /
          dacrate / adcrate / adcdump / uarttx / report / clearstats

>
```

其中：

- `rx_dma=ready` 表示 `HAL_UARTEx_ReceiveToIdle_DMA()` 启动成功
- `state` 表示 LED 当前逻辑状态
- `blink` 表示是否允许自动闪烁
- `interval_ms` 表示 LED 翻转间隔，不是完整亮灭周期
- 启动摘要按多次逐行发送，避免单次 `printf` 缓冲区截断后把提示符 `> ` 挤到半截状态行后面

### 5.2 两路串口的职责

当前两路串口分工明确：

- `USART1`
  - 进入 CLI 命令行
  - 接收文本命令
  - 输出启动摘要、命令回显和命令响应
- `USART2`
  - 不进入 CLI
  - 收到什么就回显什么
  - 用作底层链路保底测试口

这样做的好处是：

- 一路用于上层交互
- 一路用于底层联调
- CLI 解析异常时，仍有一条简单的回显口可用于排查

## 6. 联调方法

### 6.1 联调 `USART2`

用于确认底层 UART 接收链路是否正常。

步骤：

1. 打开 `USART2`
2. 发送 `hello`
3. 观察是否收到同样的 `hello`

如果 `USART2` 回显正常，说明以下链路基本是通的：

- UART 硬件接收
- DMA 接收
- 回调进入
- 环形缓冲区搬运
- `uart_proc()` 运行

### 6.2 联调 `USART1`

用于确认 CLI 是否正常。

步骤：

1. 上电观察启动摘要是否打印
2. 输入 `help`
3. 观察是否输出命令列表
4. 输入 `echo hello`
5. 观察是否回显 `hello`

如果 `USART2` 正常但 `USART1` 不正常，优先排查：

- CLI 初始化是否完成
- CLI 是否接到了 `uart_proc()` 投递的数据
- CLI 输出是否被其它逻辑干扰

## 7. 与 LED 逻辑的关系

当前 CLI 提供 `led` 命令用于验证“命令解析 -> 业务处理”的最小闭环。

LED 的逻辑重点如下：

- 板载 LED 为低电平点亮
- `led on / off / toggle` 会先关闭自动闪烁，再强制写入状态
- `led blink [interval_ms]` 会启用自动闪烁，并可设置翻转间隔

与该部分相关的问题记录可见：

- [LED命令状态被自动闪烁覆盖.md](../纠错/LED命令状态被自动闪烁覆盖.md)

## 8. 当前限制

当前实现适合：

- 基础 UART 收发验证
- CLI 文本命令交互
- DMA 接收链路联调

当前实现不适合：

- 高速大吞吐日志
- 多串口多实例 CLI
- 启用 `D-Cache` 但未做一致性处理的场景

## 9. 注意事项

### 9.1 UART FIFO

当前 FIFO 阈值已配置，但 FIFO 模式关闭。

原因是：

- 当前目标是优先稳定 `IDLE + DMA + RingBuffer` 链路
- 过早叠加 FIFO 会增加调试复杂度

### 9.2 D-Cache

当前工程：

- 开启了 `I-Cache`
- 没开启 `D-Cache`

如果后续开启 `D-Cache`，就必须处理 DMA 缓冲区的一致性问题。常见办法：

1. 把 DMA 缓冲区放到 non-cacheable 区域
2. 在读 DMA 缓冲区前做 cache invalidate

### 9.3 my_printf 现已走 TX DMA 异步

`my_printf()` 已不是阻塞实现，而是把字节推入 USART1 TX 4 KB 环形缓冲后立即返回，由 `HAL_UART_Transmit_DMA` 在 TC 中断里链式发出。

实际影响：

- pipeline / fftdump 这类高频或大块输出不会再卡主循环
- 输出量超过 ring 容量时会丢字节并累加 `dropped`，可用 `uarttx` CLI 命令查看
- 启动摘要分多行发送的习惯仍然推荐 —— 单次 `my_printf` 缓冲超 4 KB 仍会被截

详细实现见 `UART异步发送与DMA架构.md`。

## 10. 后续建议

建议后续扩展顺序如下：

1. 在 `App/uart` 之上补充更清晰的读接口（目前直接喂 CLI）
2. 在 CLI 之外继续拆协议层（例如二进制 frame，方便 fftdump 之外的批量数据上传）
3. 如启用 `D-Cache`，同步处理缓存一致性
