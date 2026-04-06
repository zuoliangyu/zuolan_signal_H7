# 1. 问题标题

LED CLI command state was overwritten by the auto blink task.

## 2. 问题现象

- `led on` or `led off` returned a normal CLI response
- The physical LED did not stay in the commanded state
- LED state changed again after a short delay

## 3. 影响范围

- `App/cli/cli.c`
- `App/led/led.c`
- UART CLI LED control behavior

## 4. 复现条件

- Hardware platform: current STM32H743 board
- Scheduler enabled and `led_proc` running at `1ms`
- Send `led on` or `led off` from `USART1` CLI
- Wait up to one blink half-period

## 5. 根因分析

Confirmed root cause:

- `led_proc()` toggled `ucLed[0]` every `500ms`
- CLI also wrote directly to `ucLed[0]`
- Manual command state and auto blink shared the same variable without arbitration

This means the CLI command did take effect for a moment, but the periodic blink task overwrote it on the next blink tick.

## 6. 解决方案

Applied fix:

- Keep `ucLed[]` as the actual output state
- Add a separate blink enable state inside the LED module
- Make `led on/off/toggle` disable auto blink before forcing state
- Add `led blink` to re-enable default blink mode

## 7. 修改位置

- `zuolan_signal_STM32/App/led/led.h`
- `zuolan_signal_STM32/App/led/led.c`
- `zuolan_signal_STM32/App/cli/cli.c`
- `docs/串口CLI说明.md`

## 8. 验证方法

1. Power on the board
2. Open `USART1` terminal
3. Send `led on` and observe LED stays on
4. Send `led off` and observe LED stays off
5. Send `led blink` and observe default blinking resumes

## 9. 验证结果

尚未验证，仅完成代码修复。

Reason:

- No on-board runtime verification was executed in this turn

## 10. 注意事项 / 后续建议

- If more LEDs are added later, keep blink policy and output state separated
- Avoid direct cross-module writes to LED state when behavior modes exist
- If CLI control grows further, consider adding a formal LED mode enum
- Current board LED is active-low on `PC13`, so `state=1` must drive the pin low
