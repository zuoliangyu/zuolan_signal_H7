#ifndef __LED_H__
#define __LED_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define LED_COUNT 1U

extern uint8_t ucLed[LED_COUNT];

void LED_Init(void);
void LED_SetState(uint8_t index, uint8_t state);
void LED_ToggleState(uint8_t index);
void LED_SetBlinkEnabled(uint8_t index, uint8_t enabled);
uint8_t LED_GetBlinkEnabled(uint8_t index);
void LED_SetBlinkIntervalMs(uint8_t index, uint16_t interval_ms);
uint16_t LED_GetBlinkIntervalMs(uint8_t index);
void led_proc(void);

#ifdef __cplusplus
}
#endif

#endif /* __LED_H__ */
