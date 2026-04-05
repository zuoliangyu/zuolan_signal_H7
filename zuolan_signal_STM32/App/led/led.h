#ifndef __LED_H__
#define __LED_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define LED_COUNT 1U

extern uint8_t ucLed[LED_COUNT];

void LED_Init(void);
void led_proc(void);

#ifdef __cplusplus
}
#endif

#endif /* __LED_H__ */
