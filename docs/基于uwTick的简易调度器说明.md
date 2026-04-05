# 基于 uwTick 的简易调度器说明

本文档对应当前工程新增的两个模块：

- `zuolan_signal_STM32/App/scheduler/scheduler.h`
- `zuolan_signal_STM32/App/scheduler/scheduler.c`
- `zuolan_signal_STM32/App/led/led.h`
- `zuolan_signal_STM32/App/led/led.c`

## 1. 设计目标

这是一套基于 `HAL_GetTick()` / `uwTick` 的协作式调度器。

目标是：

- 不引入 RTOS
- 不在 `SysTick_Handler()` 中直接跑复杂业务任务
- 在主循环中按周期执行多个短任务
- 兼容 `uwTick` 的无符号溢出

## 2. 调度模型

调度器采用静态任务表：

- 最多 `MAX_SCHEDULER_TASKS = 10`
- 每个任务有：
  - 回调函数
  - 周期 `rate_ms`
  - 上次运行时间 `last_run`
  - 使能状态
  - 调试用任务名
  - 任务句柄 `id`

主循环中调用：

```c
Scheduler_Run();
```

调度器内部逻辑：

```c
if ((uint32_t)(now_time - last_run) >= rate_ms)
{
    last_run = now_time;
    task_func();
}
```

这种写法可以直接处理 `uwTick` 回绕。

## 3. 当前接入方式

在 [main.c](C:\Users\zuolan\Desktop\zuolan_signal_STM32\zuolan_signal_STM32\Core\Src\main.c) 中：

1. 初始化 GPIO
2. 调用 `Scheduler_Init()`
3. 调用 `LED_Init()`
4. 以 `1ms` 周期注册 `led_proc`
5. 在 `while(1)` 中不断调用 `Scheduler_Run()`

这意味着当前 LED 模块已经改成：

- `SysTick` 中断只负责维护系统时基
- 调度器只负责每 `1ms` 调用一次 `led_proc`
- `led_proc` 内部自己做毫秒计数，到时再翻转 `ucLed[0]`
- `led_proc` 负责把 `ucLed[]` 中的逻辑状态同步到 GPIO
- `led_proc` 同时负责轻量计时和输出刷新

## 4. LED 模块

`LED` 模块现在是“状态数组 + 底层输出”的最小实现。

当前实现特点：

- 只有一个 LED
- 保留数组写法，当前数组长度仍定义为 `1`
- 默认由 `1ms` 周期的 `led_proc()` 对 `ucLed[0]` 进行软翻转，实现心跳闪烁
- 上层可直接写 `ucLed[0] = 1` 或 `ucLed[0] = 0`
- `led_proc()` 固定每 `1ms` 扫描一次数组
- 仅在状态变化时才真正刷新 GPIO

接口：

- `extern uint8_t ucLed[LED_COUNT]`
- `LED_Init(void)`
- `led_proc(void)`

## 5. 为什么要把 LED 独立成独立模块

这样做的好处是：

- `main.c` 只保留系统初始化和调度入口
- LED 行为变成独立模块
- 现在的 `led` 层只负责“显示”
- 当前默认闪烁在 `led_proc()` 内部维护一个很轻的软计数
- 如果后面你想加更复杂的闪烁、心跳灯、故障码，建议再拆成单独状态机

这样做的前提是：任务本身要足够轻，不在 `1ms` 任务里做重逻辑。

## 6. 如何添加你自己的任务

你只需要：

1. 写一个无参无返回的任务函数
2. 在初始化阶段调用 `Scheduler_AddTask()`

示例：

```c
ucLed[0] = 1U;
```

或者注册你自己的任务：

```c
static void KeyScan_Task(void)
{
    /* scan keys here */
}

task_handle_t key_task;

key_task = Scheduler_AddTask(KeyScan_Task, 10U, HAL_GetTick(), "keyscan");
```

含义：

- 每 `10ms` 执行一次 `KeyScan_Task`
- 初始基准时间是当前 `HAL_GetTick()`

## 7. 任务设计要求

这个调度器是协作式，不是抢占式，所以任务必须满足：

- 尽量短
- 非阻塞
- 不要在任务里调用 `HAL_Delay()`
- 不要长时间等待外设

如果某个任务执行太久，会拖慢后面的所有任务。

## 8. 不建议的用法

下面这些写法不建议在这个调度器里做：

- 在任务里 `HAL_Delay()`
- 在任务里死循环等待标志
- 在 `SysTick_Handler()` 里直接跑复杂业务逻辑
- 把大块 FFT 或长耗时通信事务直接塞进高频任务

## 9. 适合的使用场景

这类简易调度器很适合：

- LED 显示刷新
- 按键扫描
- 软定时器
- 串口轮询服务
- 小型控制逻辑
- 工程起步阶段的任务解耦

## 10. 后续扩展方向

如果你后面继续扩展，可以考虑新增：

- 上层 LED 闪烁任务
- 一次性任务
- 任务删除
- 任务优先级
- 任务运行统计
- 任务超时监测

但当前阶段不建议一开始就做重。

先把“非阻塞 + 模块化 + 基于 uwTick 的周期任务”这条主线跑稳更重要。
