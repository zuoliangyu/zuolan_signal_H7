#include "led.h"

#include "gpio.h"
#include "main.h"

#define LED_DEFAULT_BLINK_HALF_PERIOD_MS 500U

uint8_t ucLed[LED_COUNT] = {0U};
static uint8_t s_led_cached_state[LED_COUNT] = {0xFFU};
static uint8_t s_led_blink_enabled[LED_COUNT] = {1U};
static uint16_t s_led_blink_interval_ms[LED_COUNT] = {LED_DEFAULT_BLINK_HALF_PERIOD_MS};
static uint16_t s_led_blink_elapsed_ms[LED_COUNT] = {0U};

static void LED_WriteHardware(uint8_t index, uint8_t state)
{
    GPIO_PinState pin_state;

    if (index != 0U)
    {
        return;
    }

    pin_state = (state != 0U) ? GPIO_PIN_RESET : GPIO_PIN_SET;
    HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, pin_state);
}

void LED_SetState(uint8_t index, uint8_t state)
{
    if (index >= LED_COUNT)
    {
        return;
    }

    ucLed[index] = (state != 0U) ? 1U : 0U;
}

void LED_ToggleState(uint8_t index)
{
    if (index >= LED_COUNT)
    {
        return;
    }

    ucLed[index] ^= 1U;
}

void LED_SetBlinkEnabled(uint8_t index, uint8_t enabled)
{
    if (index >= LED_COUNT)
    {
        return;
    }

    s_led_blink_enabled[index] = (enabled != 0U) ? 1U : 0U;
}

uint8_t LED_GetBlinkEnabled(uint8_t index)
{
    if (index >= LED_COUNT)
    {
        return 0U;
    }

    return s_led_blink_enabled[index];
}

void LED_SetBlinkIntervalMs(uint8_t index, uint16_t interval_ms)
{
    if ((index >= LED_COUNT) || (interval_ms == 0U))
    {
        return;
    }

    s_led_blink_interval_ms[index] = interval_ms;
    s_led_blink_elapsed_ms[index] = 0U;
}

uint16_t LED_GetBlinkIntervalMs(uint8_t index)
{
    if (index >= LED_COUNT)
    {
        return 0U;
    }

    return s_led_blink_interval_ms[index];
}

void led_proc(void)
{
    uint8_t index;

    for (index = 0U; index < LED_COUNT; ++index)
    {
        if (s_led_blink_enabled[index] != 0U)
        {
            ++s_led_blink_elapsed_ms[index];

            if (s_led_blink_elapsed_ms[index] >= s_led_blink_interval_ms[index])
            {
                s_led_blink_elapsed_ms[index] = 0U;
                LED_ToggleState(index);
            }
        }
        else
        {
            s_led_blink_elapsed_ms[index] = 0U;
        }
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
        s_led_blink_enabled[index] = 1U;
        s_led_blink_interval_ms[index] = LED_DEFAULT_BLINK_HALF_PERIOD_MS;
        s_led_blink_elapsed_ms[index] = 0U;
    }

    led_proc();
}
