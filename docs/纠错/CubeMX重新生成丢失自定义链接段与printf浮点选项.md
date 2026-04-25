# CubeMX 重新生成丢失自定义链接段与 printf 浮点选项

## 1. 问题标题

`CubeMX` 重新生成代码 / 切换 MCU 后，自定义链接器段 `.dma_buffer` 与链接选项 `-u _printf_float` 被默认模板覆盖，导致串口无输出且 `printf("%f")` 不出值。

## 2. 问题现象

- `STM32H743VGT6` → `STM32H743ZIT6` 迁移后编译通过
- 烧录后板载 LED 不亮 + USART1 完全没有任何输出
- 修复 LED 引脚（PC13 → PG7）后 LED 可亮，但串口仍然没有输出
- 修复 `.dma_buffer` 段后串口能输出大部分内容，但浮点格式串只显示前半段，例如：

  ```
  [ADC-RATE] events/sec=2001 actual_sps=256128 claimed_sps=255863 ratio=
  ```

  `ratio=` 后面应该显示浮点比值，但完全空白（不是 0、不是乱码，是空字符串）

## 3. 影响范围

- `STM32H743XX_FLASH.ld`（CubeMX 生成的链接脚本）
- `cmake/gcc-arm-none-eabi.cmake`、`cmake/starm-clang.cmake`（toolchain 配置）
- 所有用 `__attribute__((section(".dma_buffer")))` 标记的 DMA 缓冲区：
  - `App/adc/adc_app.c` 的 `s_adc_dma_buffer`
  - `App/uart/uart.c` 的 UART RX/TX DMA 缓冲
  - `App/dac/dac_app.c` 的 DAC DMA 缓冲
- 所有使用 `%f / %g / %e` 格式的 `printf / my_printf` 调用：
  - DSP 自检输出
  - Pipeline 帧统计
  - ADC 实时速率统计

## 4. 复现条件

- 工程链接脚本使用了非默认的 `.dma_buffer` 段
- 工程链接器选项依赖 `-u _printf_float` 启用 newlib-nano 的浮点 printf
- 在 CubeMX 中执行以下任一操作：
  - `File → New Project` 新建工程到同一目录
  - `Pinout 下拉 → List Pinout Compatible MCUs` 切换到不同型号
  - `Project → Generate Code` 在 `.ld` 名称发生变化的情况下
- 必现，不需要特定时序条件

## 5. 根因分析

**结论**：CubeMX 不维护这两类内容，每次重新生成都会用模板默认值覆盖：

1. **链接脚本中的自定义段**：CubeMX 生成的 `.ld` 模板只包含标准段（`.isr_vector / .text / .rodata / .data / .bss / ._user_heap_stack`）。任何工程自加的 section 定义（如 `.dma_buffer`）都不在模板里，重新生成会被丢弃。
2. **toolchain cmake 中的链接选项**：CubeMX 生成的 `cmake/gcc-arm-none-eabi.cmake` 模板里链接器命令是固定的几条（`--specs=nano.specs / -Wl,--gc-sections / -Wl,-Map=...`），不带 `-u _printf_float`。任何工程额外加的链接选项都会被覆盖。

具体后果：

**`.dma_buffer` 段缺失的链路**：

1. App 代码中 `static uint16_t s_adc_dma_buffer[...] __attribute__((section(".dma_buffer")))`
2. 链接器找不到 `.dma_buffer` 段定义，按 orphan section 规则把它放到内存图末尾
3. CubeMX 模板把 `.bss / .data` 都放在 `>DTCMRAM`，所以末尾也在 DTCM (0x20000000)
4. **`GPDMA1` / `GPDMA2` 不能访问 DTCM**（只有 CPU 和 MDMA 能访问）
5. ADC DMA、UART DMA 启动后实际不传输任何数据，HAL 不报错，外设和 CPU 都正常运行
6. 表现为串口 TX 完全静默 / ADC 取不到样本

**`-u _printf_float` 缺失的链路**：

1. `--specs=nano.specs` 让链接器使用 newlib-nano 的精简版 `_vfprintf_r`
2. 精简版 `_vfprintf_r` 不实现 `%f / %g / %e` 等浮点格式
3. 遇到 `%f` 直接消费栈上的 `double` 参数但**不输出任何字符**（不是崩溃，是静默跳过）
4. `%f` 之后的格式串和后续输出全部丢失
5. 表现为 `printf("xxx=%f\r\n", val)` 输出 `xxx=`（无值，无换行，无后续）
6. `-u _printf_float` 强制链接 `_printf_float` 符号，引入完整版 `_vfprintf_r`，浮点格式才工作

## 6. 解决方案

### 6.1 链接脚本补 `.dma_buffer` 段

在 `STM32H743XX_FLASH.ld`（或当前生成的对应文件名）的 `.bss` 段之后、`._user_heap_stack` 段之前插入：

```ld
  /* DMA buffers must live in DMA-accessible RAM (D2 SRAM at 0x30000000),
     not DTCM, since GPDMA1 cannot reach DTCM. App code marks buffers with
     __attribute__((section(".dma_buffer"))). 32-byte aligned for cache lines. */
  .dma_buffer (NOLOAD) :
  {
    . = ALIGN(32);
    *(.dma_buffer)
    *(.dma_buffer*)
    . = ALIGN(32);
  } >RAM_D2
```

### 6.2 toolchain 补 `-u _printf_float`

`cmake/gcc-arm-none-eabi.cmake` 和 `cmake/starm-clang.cmake` 里链接器主行末尾追加：

```cmake
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-Map=${CMAKE_PROJECT_NAME}.map -Wl,--gc-sections -u _printf_float")
```

### 6.3 替代方案

**针对 `-u _printf_float`**：可以改用 picolibc（`--specs=picolibc.specs`），它默认带浮点 printf 不用此 flag。但需要额外集成工作，详见 [STM32H7_MCU迁移与CubeMX重生成避坑指南.md](../STM32H7_MCU迁移与CubeMX重生成避坑指南.md) 第 3.3 节讨论。

**针对 `.dma_buffer`**：可以让整个 `.bss` 都映射到 RAM_D2，但会丧失 DTCM 的高速 CPU 访问优势。本工程选择"DMA 缓冲单独分段、其它数据保留在 DTCM"的方式。

## 7. 修改位置

- `zuolan_signal_STM32_H7/STM32H743XX_FLASH.ld`：补 `.dma_buffer` 段
- `zuolan_signal_STM32_H7/cmake/gcc-arm-none-eabi.cmake`：链接器行追加 `-u _printf_float`
- `zuolan_signal_STM32_H7/cmake/starm-clang.cmake`：同上

## 8. 验证方法

1. 重新编译并烧录
2. 上电观察启动日志，确认 `[STARTUP]`、`[FFT]`、`[FILTER]` 等行完整
3. CLI 发送 `adcrate` 命令观察实时速率统计
4. 确认输出带有完整的浮点数值，例如：

   ```
   [ADC-RATE] events/sec=2001 actual_sps=256128 claimed_sps=255863 ratio=1.001
   ```

5. CLI 发送 `dac sine 1000` 让 DAC 输出 1 kHz 正弦
6. 确认外接示波器或 ADC 采样反馈观察到信号 → 间接证明 DAC DMA 链路工作

## 9. 验证结果

已验证通过。

- `.dma_buffer` 段补回后 USART1 输出恢复正常
- `-u _printf_float` 补回后所有 `%f` 输出正常
- 提交在 `commit 2b4e143`

## 10. 注意事项 / 后续建议

- **每次跑 CubeMX `Generate Code` 后必检**：链接脚本里 `.dma_buffer` 是否还在、toolchain 里 `-u _printf_float` 是否还在
- 这两条容易在迁移、升级 CubeMX 版本、改外设配置后无声丢失，且测试现象（串口完全静默）和"CubeMX 配置错"几乎相同，难一眼区分
- 如果有 CI，可以加一个 grep 检查保证这两个内容存在
- 长远方案是用 `--specs=picolibc.specs` 替代 newlib-nano 解决浮点 printf 问题，但代价是迁移成本（不推荐为这一项专门换 libc）
- 引入 D-Cache 优化时，`.dma_buffer` 段在 RAM_D2 仍然安全，但要注意 cache 一致性处理（手动 `SCB_CleanInvalidateDCache_by_Addr` 或把段标为 Non-Cacheable MPU region）
