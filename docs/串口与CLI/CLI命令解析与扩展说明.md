# CLI 命令解析与扩展说明

本文档专门说明当前工程中 CLI 模块是如何接收输入、解析命令并分发到具体处理函数的。

如果你更关心串口底层接收链路，而不是命令层，请先看：

- [UART接收调试与CLI运行说明.md](C:/Users/zuolan/Desktop/zuolan_signal_STM32/docs/串口与CLI/UART接收调试与CLI运行说明.md)

## 1. 当前模块位置

CLI 模块位于：

- `App/cli/cli.c`
- `App/cli/cli.h`

UART 应用层位于：

- `App/uart/uart.c`
- `App/uart/uart.h`

当前关系是：

- `App/uart` 负责把 `USART1` 收到的数据喂给 CLI
- `App/cli` 负责做文本解析和命令执行

## 2. 数据进入 CLI 的路径

当前数据路径如下：

```text
USART1 RX
  -> DMA buffer
  -> ring buffer
  -> uart_proc()
  -> CLI_InputBuffer()
  -> 行缓冲
  -> 分词
  -> 命令匹配
  -> handler
```

其中：

- `UART_Init()` 中调用 `CLI_Init(&huart1)`
- `uart_proc()` 中只要发现 `USART1` 有待处理数据，就调用 `CLI_InputBuffer()`
- `USART2` 不进 CLI，而是直接回显

## 3. 当前 CLI 的输入行为

### 3.1 行缓冲

CLI 采用行缓冲模式：

- 输入普通可打印字符时，先写入本地 `line` 缓冲区
- 遇到 `\r` 或 `\n` 时，执行整行命令
- 执行后清空本次行缓冲，再重新显示提示符

当前参数：

- 最大行长度：`128`
- 最大 token 数：`8`

### 3.2 回显

CLI 默认开启回显：

- 输入字符时会直接回显到终端
- 退格支持 `0x08` 和 `0x7F`

### 3.3 非法输入

当前规则：

- 非可打印字符会被忽略
- 输入长度超过 `128` 时，会提示清空
- 未知命令会提示 `Unknown command: <cmd>`

## 4. 命令表结构

CLI 当前使用命令表方式组织：

```c
typedef struct
{
    const char *cmd;
    const char *help;
    cli_handler_t handler;
} cli_command_t;
```

每条命令记录三部分：

- `cmd`
  - 命令关键字
- `help`
  - `help` 命令展示时用到的说明
- `handler`
  - 实际执行函数

当前命令表定义在 `cli.c` 的 `s_cli_commands[]` 中。

## 5. 当前支持命令

### 5.1 help

功能：

- 列出全部已注册命令

示例：

```text
help
```

### 5.2 echo

功能：

- 原样回显参数

示例：

```text
echo hello world
```

### 5.3 led

功能：

- 控制 LED 当前状态和自动闪烁行为

支持形式：

```text
led on
led off
led toggle
led blink
led blink 500
```

行为说明：

- `led on`
  - 关闭自动闪烁
  - 强制 LED 为亮
- `led off`
  - 关闭自动闪烁
  - 强制 LED 为灭
- `led toggle`
  - 关闭自动闪烁
  - 翻转当前 LED 状态
- `led blink`
  - 重新启用自动闪烁
  - 使用当前已保存的翻转间隔
- `led blink 500`
  - 设置翻转间隔为 `500ms`
  - 重新启用自动闪烁

注意：

- `500ms` 表示翻转间隔，不是完整亮灭周期
- 当前板载 LED 为低电平点亮

### 5.4 dac

功能：

- 读取或控制 `DAC1_CH1` 当前参数和输出状态

支持形式：

```text
dac get
dac mode dc
dac mode sine
dac mode ?
dac amp 500
dac amp ?
dac offset 1650
dac offset ?
dac freq 1000
dac freq ?
dac duty 50
dac duty ?
dac start
dac stop
```

行为说明：

- `dac get`
  - 输出当前完整状态和全部参数
- `dac mode <dc|sine|tri|square>`
  - 设置当前输出模式
  - 不会修改其他参数
- `dac mode ?`
  - 查询当前模式
- `dac amp <mv>`
  - 设置当前振幅
- `dac amp ?`
  - 查询当前振幅
- `dac offset <mv>`
  - 设置当前偏置
- `dac offset ?`
  - 查询当前偏置
- `dac freq <hz>`
  - 设置当前频率
- `dac freq ?`
  - 查询当前频率
- `dac duty <0..100>`
  - 设置当前方波占空比百分比
- `dac duty ?`
  - 查询当前方波占空比百分比
- `dac start`
  - 按当前保存参数重新启动 DAC 输出
- `dac stop`
  - 停止当前 DAC 输出，但保留参数

注意：

- 当前 CLI 的电压换算固定按 `3300mV` 参考值计算
- 如果板上实际 `VDDA` 不是严格 `3300mV`，实测输出会有轻微偏差
- `dac amp` 和 `dac offset` 允许范围为 `0..3300`
- `dac duty` 允许范围为 `0..100`
- 当前波形输出固定使用 `128` 点波表
- 建议查询命令统一写成 `dac mode ?` 这种空格分隔形式

## 6. 启动时 CLI 的表现

当前上电后：

1. `CLI_Init()` 只做上下文初始化
2. 启动摘要由 `App/uart` 统一打印
3. 最后调用 `CLI_ShowPrompt()` 显示 `> `

这样做的好处是：

- 启动摘要集中在 UART 层管理
- CLI 层不负责系统状态汇总
- 上电输出顺序更清晰

## 7. 如何新增一个命令

建议按下面顺序做：

1. 在 `cli.c` 中新增一个 `static` 命令处理函数
2. 在 `s_cli_commands[]` 中注册命令
3. 在 handler 里完成参数检查和功能调用
4. 同步更新文档

示例结构：

```c
static void CLI_CmdFoo(UART_HandleTypeDef *huart, uint8_t argc, char *argv[])
{
    (void)argc;
    (void)argv;
    CLI_WriteLine(huart, "foo ok");
}
```

注册方式：

```c
{"foo", "Example command", CLI_CmdFoo},
```

## 8. 当前设计取舍

### 8.1 为什么先只绑定 `USART1`

当前只让 `USART1` 进入 CLI，是为了把职责分清：

- `USART1` 负责上层交互
- `USART2` 负责底层回显联调

在串口基础链路还处于持续完善阶段时，这种分工更稳。

### 8.2 为什么继续使用阻塞输出

当前 CLI 输出使用阻塞式 `HAL_UART_Transmit()` 或 `my_printf()`。

原因是：

- CLI 输出频率不高
- 当前重点是逻辑清晰和稳定
- 暂时不需要为了 CLI 再做一套 TX DMA 日志系统

### 8.3 为什么 LED 行为由 LED 模块管理

这次 LED 控制做了一个关键拆分：

- `ucLed[]` 表示输出状态
- blink 相关状态由 `App/led` 自己管理

这样 CLI 只表达“我要什么行为”，而不是直接抢占底层定时逻辑。

## 9. 后续扩展建议

如果后面 CLI 继续扩展，建议优先考虑：

1. 给命令增加模块级分组
2. 增加更清晰的参数校验工具
3. 如果需要双串口 CLI，再改为多实例上下文
4. 如果输出量增大，再考虑把 CLI 输出迁到 TX DMA
