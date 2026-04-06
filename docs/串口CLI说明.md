# UART CLI Guide

This document describes how to use and extend the UART command line interface (CLI).

## 1. Overview

The current CLI runs on `USART1` and provides:

- Line-based input buffer
- Basic tokenization
- Command dispatch table
- Echo on by default
- Prompt `> `

The CLI parses commands and prints responses, while UART low-level RX stays in `App/uart`.

## 2. Supported Commands

| Command | Description | Example |
| --- | --- | --- |
| `help` | List all commands | `help` |
| `echo` | Echo parameters | `echo hello` |
| `led` | Control LED | `led on` / `led off` / `led toggle` / `led blink` / `led blink 500` |

### 2.1 help

Print all registered commands.

### 2.2 echo

Echo the arguments back.

Example:

```
echo hello world
```

### 2.3 led

Control application LED state and blink mode.

Example:

```
led on
led off
led toggle
led blink
led blink 500
```

Behavior:

- `led on`: disable auto blink and force LED on
- `led off`: disable auto blink and force LED off
- `led toggle`: disable auto blink and invert current state
- `led blink`: enable default auto blink again
- `led blink 500`: set blink toggle interval to `500ms` and enable blink

Note:

- `interval_ms` is the LED toggle interval, not the full on-off cycle

## 3. Usage

1. On boot, `USART1` prints a boot summary with UART and LED status
2. Prompt is: `> `
3. Type a line and press Enter to execute

Notes:

- Echo is enabled by default
- Backspace supported (`0x08` or `0x7F`)
- Lines longer than `128` bytes will be cleared with a warning

## 4. Code Layout

- `App/cli/cli.c`
- `App/cli/cli.h`

Key APIs:

- `CLI_Init(UART_HandleTypeDef *huart)`
- `CLI_InputBuffer(UART_HandleTypeDef *huart, const uint8_t *data, uint16_t length)`

## 5. Extension Guide

1. Add a new handler function in `cli.c`
2. Register the command in `s_cli_commands`
3. Print responses to `huart`

Example (pseudo code):

```
static void CLI_CmdFoo(UART_HandleTypeDef *huart, uint8_t argc, char *argv[])
{
    (void)argc;
    (void)argv;
    CLI_WriteLine(huart, "foo ok");
}
```

## 6. Notes

- The CLI is currently bound to `USART1` only. For multi-port CLI, extend to multiple instances.
- CLI uses blocking `my_printf`; heavy logs may consume time.
- If `D-Cache` is enabled later, ensure UART DMA buffer cache coherence.
