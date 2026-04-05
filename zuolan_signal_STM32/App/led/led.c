#include "led.h"

#include "gpio.h"
#include "main.h"

#define LED_DEFAULT_BLINK_HALF_PERIOD_MS 500U

uint8_t ucLed[LED_COUNT] = {0U};
static uint8_t s_led_cached_state[LED_COUNT] = {0xFFU};
static uint16_t s_led_blink_tick_ms = 0U;

static void LED_WriteHardware(uint8_t index, uint8_t state)
{
    GPIO_PinState pin_state;

    if (index != 0U)
    {
        return;
    }

    pin_state = (state != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET;
    HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, pin_state);
}

void led_proc(void)
{
    uint8_t index;

    ++s_led_blink_tick_ms;

    if (s_led_blink_tick_ms >= LED_DEFAULT_BLINK_HALF_PERIOD_MS)
    {
        s_led_blink_tick_ms = 0U;
        ucLed[0] ^= 1U;
    }

    for (index = 0U; index < LED_COUNT; ++index)
    {
        if (ucLed[index] != s_led_cached_state[index])
        {
            LED_WriteHardware(index, ucLed[index]);
            s_led_cached_state[index] = ucLed[index];
        }
    }
}

void LED_Init(void)
{
    uint8_t index;

    for (index = 0U; index < LED_COUNT; ++index)
    {
        ucLed[index] = 0U;
        s_led_cached_state[index] = 0xFFU;
    }

    s_led_blink_tick_ms = 0U;
    led_proc();
}
