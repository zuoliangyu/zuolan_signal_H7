# UART 异步发送与 DMA TX 架构

工程的 UART 输出从 **115200 baud + 阻塞 polled** 升级到 **921600 baud + DMA TX + 环形缓冲**。本文档说明动机、架构、实现细节与实测性能。

## 1. 升级背景：实时流水线被 UART 拖死

实时 ADC → 滤波 → FFT 流水线在 256 kHz 采样、1024 点 FFT 配置下，理论帧率 250 fps。实测发现 pipeline 只跑到 ~10 fps，96% 的 ADC 块被 ADC 队列丢掉。

排查路径见 `docs/DSP/实时流水线设计.md` 第 9 节。最终定位到瓶颈是 `my_printf` 用 `HAL_UART_Transmit` 同步阻塞发送：

| 项 | 数值 |
|---|---|
| 每行长度 | ~130 字节 |
| 115200 baud | 87 µs/字节 |
| 一次 my_printf 阻塞时间 | **11.3 ms** |
| 流水线每帧产出周期 | 4 ms |
| 结论 | UART 慢于流水线 ~3× |

每次 `my_printf` 让 scheduler 任务卡死 11 ms，期间 ADC ISR 持续把块塞进队列直到溢出。

## 2. 升级目标

- **不阻塞调用方**：`my_printf` 调用立即返回（< 50 µs），让 scheduler 任务保持节奏
- **承载实时流量**：能稳定输出 250 fps × 130 字节 ≈ 32 KB/秒
- **保持 API 兼容**：`my_printf` 签名不变，所有调用方零改动

## 3. 架构

```
   App / CLI / pipeline  ──my_printf("...")──┐
                                              │
                                              ▼
                          ┌──────────────────────────┐
                          │  vsnprintf to stack buf  │
                          │  push bytes to TX ring   │ ← 临界区，最多 ~10 µs
                          │  if !busy: kick DMA      │
                          └──────────────┬───────────┘
                                         │
                  ┌──────────────────────┘
                  ▼
          ┌────────────────┐
          │  TX ring 4 KB  │  in .dma_buffer (RAM_D2)
          └───────┬────────┘
                  │
                  ▼
       HAL_UART_Transmit_DMA(&huart1, &ring->buf[tail], chunk)
                  │
                  ▼
          DMA1_Stream4 搬运 → USART1 TDR → 线上
                  │
              (传输完成)
                  │
                  ▼
       DMA1_Stream4_IRQHandler
              │
              ▼
       HAL_UART_TxCpltCallback (我们 override)
              │
              ▼ 推进 tail，busy=0，kick 下一段
```

环形缓冲采用经典生产者/消费者：

- **生产者**：CPU（任意线程）通过 `my_printf` / `UART_WriteAsync` 写入 head
- **消费者**：DMA 控制器从 tail 读出，直接送 USART
- **触发回填**：DMA 完成中断里推进 tail 并 kick 下一段

## 4. 关键设计决策

### 4.1 环形缓冲大小 4 KB / 端口

每端口 4 KB（USART1 + USART2 共 8 KB），存在 `.dma_buffer` 段（→ RAM_D2）。

- 4 KB ÷ 130 字节/帧 = 30 帧 buffer 容量
- 高峰 250 fps × 130 字节 = 32 KB/秒。921600 baud 实际线速 ~92 KB/秒，所以 buffer 不会持续累积，4 KB 足够吸收瞬时抖动

### 4.2 NORMAL 模式而非 CIRCULAR

CubeMX 配置 DMA `Mode = Normal`：每次传输完一段就停，由完成回调触发下一段。

- CIRCULAR 模式适合周期性固定数据（如音频回放），不适合任意长度文本
- NORMAL + 回调链能精确控制每段长度，正确处理 head/tail wrap

### 4.3 wrap-around 分两次传输

如果 head 在 tail 之前（环形已绕回），`uart_tx_kick_locked` 一次只传到 buffer 末尾：

```c
chunk = (head > tail) ? (head - tail) : (UART_TX_BUF_SIZE - tail);
```

下一次 TxCpltCallback 触发时再传剩余部分。这样 DMA 始终是连续物理地址，不需要 scatter-gather。

### 4.4 临界区最小化

`my_printf` 的临界区只覆盖：
1. 把 N 字节 push 进环形缓冲
2. 调用 `uart_tx_kick_locked`

`vsnprintf` 在临界区**外**（最耗时的部分），所以临界区典型 ~10 µs。

### 4.5 缓冲满时丢弃，不阻塞

```c
if (next_head == ring->tail)
{
    ring->dropped += (uint32_t)(len - i);
    break;
}
```

设计权衡：流水线/CLI 永远不阻塞；高输出密度下宁可丢字节也不卡 scheduler。`uarttx` CLI 命令查丢字节累计。

实测压测中 dropped 始终 = 0，说明 4 KB buffer + 921600 baud 对当前流水线裕量充分。

## 5. CubeMX 必做的配置

新工程要复用这套架构，CubeMX 配置：

1. **USART1 → DMA Settings → Add**
   - DMA Request: `USART1_TX`
   - Stream: 自动（一般是 `DMA1_Stream4`）
   - Direction: `Memory To Peripheral`
   - **Mode: `Normal`（不要 Circular）**
   - Memory Increment: 勾选；Peripheral Increment: 不勾
   - Data Width: Byte / Byte
   - FIFO: 不开
   - Priority: High（CLI 是主出口）

2. **USART1 → Parameter Settings**
   - BaudRate: 921600（或你想要的高速）

3. 生成代码，确认 `Core/Src/usart.c` 出现：
   - `DMA_HandleTypeDef hdma_usart1_tx`
   - `__HAL_LINKDMA(uartHandle, hdmatx, hdma_usart1_tx)`

4. 应用层提供 `HAL_UART_TxCpltCallback` override 即可（已在 `App/uart/uart.c`）。

## 6. API

```c
// 公共接口（uart.h）
int my_printf(UART_HandleTypeDef *huart, const char *format, ...);
int UART_WriteAsync(UART_HandleTypeDef *huart, const uint8_t *data, uint16_t length);
uint32_t UART_GetTxDroppedBytes(uart_port_id_t port);
uint32_t UART_GetTxRingFreeBytes(uart_port_id_t port);
```

- `my_printf`：vsnprintf + 异步推送，签名跟旧版一样，无破坏性变更
- `UART_WriteAsync`：直接推送字节流，CLI 单字符回显和短提示走这条
- 监控接口：`uarttx` CLI 命令封装

## 7. 性能数据（对比升级前后）

测试条件：DAC 输出 1 kHz 正弦 → ADC 256 kHz 16-bit → 1024 点 RFFT + 找峰，stream 模式 30 秒。

| 指标 | 升级前 | 升级后 (rate=5) | 改善倍数 |
|---|---|---|---|
| Pipeline 实际帧率 | 9.93 fps | **223 fps** | 22.5× |
| ADC 块丢率 | 96% | **10.7%** | 9× |
| `my_printf` 调用阻塞 | ~11 ms | **< 50 µs** | 220× |
| UART 数据可靠性 | 不可测 | **0 字节丢弃** | ∞ |

实时性达到理论上限的 89%。剩余 10.7% 主要受限于 DSP 计算 + scheduler 节奏，非 UART。

## 8. 相关 CLI 工具

| 命令 | 用途 |
|---|---|
| `uarttx` | 显示 USART1/USART2 TX 环形缓冲 dropped 计数与剩余空间 |
| `clearstats` | 清空 ADC 事件计数（用于干净基线压测）|
| `report` | 一次性停 pipeline + dump 全部健康指标，避开 stream 输出和命令响应抢通道 |
| `dac test` | 一行配置 1 kHz / 1V / 1.65V offset 正弦预设 |

## 9. 未来可优化方向

- **整数化输出**：把 4 个 `%f` 换定点整数，`vsnprintf` 时间从 ~500 µs 降到 ~100 µs
- **启用 DCache**：FFT/滤波加速 2-3×，但 ADC DMA 缓冲区需配 MPU 非 cacheable
- **更小 FFT (256 / 512)**：每帧 1-4 个 ADC 块，计算量降 4-16×

实时性需求继续推高时按这个顺序逐步上。但当前 89% 实时 + 0 UART 丢字节已可支撑大多数信号处理用例。

## 10. 相关文档

- `docs/DSP/实时流水线设计.md` —— 流水线整体架构与状态机
- `docs/纠错-ADC采样时间过长导致实际采样率被吞.md` —— 上一轮排查中暴露的另一个采样链路问题
- `docs/STM32H7_DMA缓冲区与链接脚本说明.md` —— DMA 缓冲区放在 RAM_D2 的设计依据
