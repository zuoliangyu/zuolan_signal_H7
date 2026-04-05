#ifndef __LED_H__
#define __LED_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "scheduler.h"

task_handle_t LED_Init(uint32_t blink_period_ms);
bool LED_SetEnabled(bool enabled);
bool LED_SetBlinkPeriod(uint32_t blink_period_ms);
uint32_t LED_GetBlinkPeriod(void);
void led_proc(void);

#ifdef __cplusplus
}
#endif

#endif /* __LED_H__ */
