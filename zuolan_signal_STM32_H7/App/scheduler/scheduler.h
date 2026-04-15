#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#define MAX_SCHEDULER_TASKS 10U
#define MAX_TASK_NAME_LEN   16U

typedef uint8_t task_handle_t;

#define INVALID_TASK_HANDLE ((task_handle_t)0U)

typedef void (*scheduler_task_func_t)(void);

typedef struct
{
    scheduler_task_func_t task_func;
    uint32_t rate_ms;
    uint32_t last_run;
    bool is_active;
    char task_name[MAX_TASK_NAME_LEN];
    task_handle_t id;
} task_t;

void Scheduler_Init(void);
task_handle_t Scheduler_AddTask(scheduler_task_func_t func, uint32_t rate_ms,
                                uint32_t initial_last_run, const char *name);
bool Scheduler_SetTaskStateByID(task_handle_t id, bool is_active);
bool Scheduler_SetTaskRateByID(task_handle_t id, uint32_t rate_ms);
bool Scheduler_ResetTaskTimerByID(task_handle_t id, uint32_t last_run);
void Scheduler_Run(void);
uint8_t Scheduler_GetTaskCount(void);

#ifdef __cplusplus
}
#endif

#endif /* __SCHEDULER_H__ */
