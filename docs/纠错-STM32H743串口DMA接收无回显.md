# STM32H743 串口 DMA 接收无回显纠错记录

## 1. 问题标题

`STM32H743` 工程中，串口发送 `hello` 后无回显。

## 2. 问题现象

- 板端串口接收方案已经改成 `IDLE + DMA + 环形缓冲区`
- 用户向串口发送 `hello`
- 串口侧没有收到任何回显
- 从现象上看，发送链路可能正常，但接收链路没有真正收到数据

## 3. 影响范围

- `USART1/USART2` 的 DMA 接收
- `App/uart` 的回显测试链路
- `STM32H743` 的内存布局和 DMA 可访问区域

## 4. 复现条件

- 芯片平台：`STM32H743`
- 工程使用 `RX DMA Circular + IDLE`
- 工程链接脚本默认将 `.data/.bss` 放在 `DTCMRAM`
- 串口发送任意数据，例如 `hello`
- 稳定复现为“无回显”

## 5. 根因分析

最终根因：

- 串口 DMA 接收缓冲区被分配在了 `DTCMRAM`
- `STM32H743` 的普通 DMA 不能访问 `DTCM`
- 因此 DMA 无法把 UART 接收到的数据搬运到该缓冲区

分析结论：

- 这不是 `HAL_UARTEx_RxEventCallback()` 逻辑本身的首要问题
- 也不是简单的 `uart_proc()` 调度周期问题
- 真正的底层问题是 DMA 内存区域不可达

## 6. 解决方案

采用的修复方式：

- 在 `uart.c` 中给 DMA 接收缓冲区单独添加 section：`.dma_buffer`
- 在链接脚本中新增 `.dma_buffer` 段
- 将该段映射到 `RAM_D2`
- 注意 `GCC` 不允许把 `section` 属性直接挂在结构体成员上，因此 DMA 缓冲区最终改成了独立静态数组，模块内通过指针引用

这样处理后：

- DMA 接收缓冲区不再落到 `DTCM`
- UART DMA 可以正常访问该内存区域
- 后续 `IDLE / HT / TC` 才有机会把数据推进软件环形缓冲区

## 7. 修改位置

- `zuolan_signal_STM32/App/uart/uart.c`
- `zuolan_signal_STM32/STM32H743XG_FLASH.ld`
- `docs/串口配置与测试说明.md`

## 8. 验证方法

1. 重新编译工程
2. 重新烧录到板子
3. 打开串口助手
4. 上电观察是否打印 `RX DMA + IDLE ready`
5. 向 `USART1` 或 `USART2` 发送 `hello`
6. 观察是否收到原样回显

## 9. 验证结果

- 当前已完成代码修复
- 用户实机反馈：回显已经恢复正常
- 后续二次排查中已确认：上电能够看到 `[USART1] RX DMA + IDLE ready`
- 这说明 `TX` 链路和 `UART_Init()` 启动路径基本正常，后续重点排查 `RX DMA / IDLE / 回调链`
- 为定位问题，曾临时加入启动状态与回调诊断输出
- 在确认串口回显恢复正常后，临时诊断代码已移除，仅保留正式收发逻辑

## 10. 注意事项 / 后续建议

- `STM32H7` 上所有 DMA 缓冲区都要先确认是否位于 DMA 可访问 RAM
- 当前工程普通 `.data/.bss` 在 `DTCMRAM`，后续继续加 DMA 外设时要特别注意
- 如果后续启用 `D-Cache`，除了避开 `DTCM`，还要继续处理 cache 一致性
- 如果以后重新生成或调整链接脚本，要检查 `.dma_buffer` 段是否仍然保留
- 如果使用 `GCC`，`section` 属性要加在独立变量、而不是结构体成员上
