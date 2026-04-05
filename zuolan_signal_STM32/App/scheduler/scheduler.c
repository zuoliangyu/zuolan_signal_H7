#include "scheduler.h"

#include "stm32h7xx_hal.h"

#include <string.h>

static task_t scheduler_tasks[MAX_SCHEDULER_TASKS];
static uint8_t task_count = 0U;
static task_handle_t next_task_id = 1U;

static task_t *Scheduler_FindTaskByID(task_handle_t id)
{
    for (uint8_t i = 0U; i < task_count; i++)
    {
        if (scheduler_tasks[i].id == id)
        {
            return &scheduler_tasks[i];
        }
    }

    return NULL;
}

void Scheduler_Init(void)
{
    (void)memset(scheduler_tasks, 0, sizeof(scheduler_tasks));
    task_count = 0U;
    next_task_id = 1U;
}

task_handle_t Scheduler_AddTask(scheduler_task_func_t func, uint32_t rate_ms,
                                uint32_t initial_last_run, const char *name)
{
    task_t *new_task;

    if ((task_count >= MAX_SCHEDULER_TASKS) || (func == NULL) || (rate_ms == 0U) ||
        (next_task_id == INVALID_TASK_HANDLE))
    {
        return INVALID_TASK_HANDLE;
    }

    new_task = &scheduler_tasks[task_count];
    new_task->id = next_task_id;
    new_task->task_func = func;
    new_task->rate_ms = rate_ms;
    new_task->last_run = initial_last_run;
    new_task->is_active = true;

    if (name != NULL)
    {
        (void)strncpy(new_task->task_name, name, MAX_TASK_NAME_LEN - 1U);
        new_task->task_name[MAX_TASK_NAME_LEN - 1U] = '\0';
    }
    else
    {
        new_task->task_name[0] = '\0';
    }

    task_count++;
    return next_task_id++;
}

bool Scheduler_SetTaskStateByID(task_handle_t id, bool is_active)
{
    task_t *task = Scheduler_FindTaskByID(id);

    if (task == NULL)
    {
        return false;
    }

    task->is_active = is_active;
    return true;
}

bool Scheduler_SetTaskRateByID(task_handle_t id, uint32_t rate_ms)
{
    task_t *task = Scheduler_FindTaskByID(id);

    if ((task == NULL) || (rate_ms == 0U))
    {
        return false;
    }

    task->rate_ms = rate_ms;
    return true;
}

bool Scheduler_ResetTaskTimerByID(task_handle_t id, uint32_t last_run)
{
    task_t *task = Scheduler_FindTaskByID(id);

    if (task == NULL)
    {
        return false;
    }

    task->last_run = last_run;
    return true;
}

void Scheduler_Run(void)
{
    uint32_t now_time = HAL_GetTick();

    for (uint8_t i = 0U; i < task_count; i++)
    {
        if (!scheduler_tasks[i].is_active)
        {
            continue;
        }

        if ((uint32_t)(now_time - scheduler_tasks[i].last_run) >=
            scheduler_tasks[i].rate_ms)
        {
            scheduler_tasks[i].last_run = now_time;

            if (scheduler_tasks[i].task_func != NULL)
            {
                scheduler_tasks[i].task_func();
            }
        }
    }
}

uint8_t Scheduler_GetTaskCount(void)
{
    return task_count;
}
