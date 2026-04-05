#include "led.h"

#include "gpio.h"
#include "main.h"

static task_handle_t s_led_task_handle = INVALID_TASK_HANDLE;
static uint32_t s_led_blink_period_ms = 500U;

void led_proc(void)
{
    HAL_GPIO_TogglePin(LED1_GPIO_Port, LED1_Pin);
}

task_handle_t LED_Init(uint32_t blink_period_ms)
{
    uint32_t now_tick;

    if (blink_period_ms != 0U)
    {
        s_led_blink_period_ms = blink_period_ms;
    }

    if (s_led_task_handle != INVALID_TASK_HANDLE)
    {
        (void)Scheduler_SetTaskStateByID(s_led_task_handle, true);
        return s_led_task_handle;
    }

    now_tick = HAL_GetTick();
    s_led_task_handle = Scheduler_AddTask(led_proc, s_led_blink_period_ms,
                                          now_tick, "led");

    return s_led_task_handle;
}

bool LED_SetEnabled(bool enabled)
{
    if (s_led_task_handle == INVALID_TASK_HANDLE)
    {
        return false;
    }

    return Scheduler_SetTaskStateByID(s_led_task_handle, enabled);
}

bool LED_SetBlinkPeriod(uint32_t blink_period_ms)
{
    if ((blink_period_ms == 0U) || (s_led_task_handle == INVALID_TASK_HANDLE))
    {
        return false;
    }

    s_led_blink_period_ms = blink_period_ms;

    if (!Scheduler_SetTaskRateByID(s_led_task_handle, s_led_blink_period_ms))
    {
        return false;
    }

    return Scheduler_ResetTaskTimerByID(s_led_task_handle, HAL_GetTick());
}

uint32_t LED_GetBlinkPeriod(void)
{
    return s_led_blink_period_ms;
}
