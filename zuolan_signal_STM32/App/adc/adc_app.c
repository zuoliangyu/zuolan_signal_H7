#include "adc_app.h"

#include <string.h>

#include "adc.h"
#include "uart.h"
#include "usart.h"

#if defined(__GNUC__)
#define ADC_DMA_ALIGN __attribute__((aligned(32)))
#define ADC_DMA_SECTION __attribute__((section(".dma_buffer")))
#else
#define ADC_DMA_ALIGN
#define ADC_DMA_SECTION
#endif

typedef struct
{
    uint8_t started;
    uint8_t stream_enabled;
    uint16_t stream_interval_ms;
    uint32_t last_stream_tick;
} adc_app_state_t;

/* H7 上 ADC DMA 缓冲区同样必须放在 DMA 可达 RAM。 */
static uint16_t s_adc_dma_buffer[ADC_APP_DMA_SAMPLES] ADC_DMA_ALIGN ADC_DMA_SECTION;
static adc_app_state_t s_adc_state = {
    .started = 0U,
    .stream_enabled = 0U,
    .stream_interval_ms = ADC_APP_DEFAULT_STREAM_INTERVAL_MS,
    .last_stream_tick = 0U,
};

static uint16_t ADC_APP_RawToMv(uint16_t raw_value)
{
    uint32_t numerator = ((uint32_t)raw_value * (uint32_t)ADC_APP_REFERENCE_MV) +
                         ((uint32_t)ADC_APP_MAX_RAW_VALUE / 2U);

    return (uint16_t)(numerator / (uint32_t)ADC_APP_MAX_RAW_VALUE);
}

static uint16_t ADC_APP_GetLatestIndex(void)
{
    uint32_t remaining;
    uint32_t current_pos;

    if ((hadc1.DMA_Handle == NULL) || (ADC_APP_DMA_SAMPLES == 0U))
    {
        return 0U;
    }

    remaining = __HAL_DMA_GET_COUNTER(hadc1.DMA_Handle);
    if ((remaining == 0U) || (remaining > ADC_APP_DMA_SAMPLES))
    {
        current_pos = 0U;
    }
    else
    {
        current_pos = (uint32_t)ADC_APP_DMA_SAMPLES - remaining;
    }

    if (current_pos == 0U)
    {
        return (uint16_t)(ADC_APP_DMA_SAMPLES - 1U);
    }

    return (uint16_t)(current_pos - 1U);
}

void ADC_APP_Init(void)
{
    (void)memset(s_adc_dma_buffer, 0, sizeof(s_adc_dma_buffer));
    s_adc_state.started = 0U;
    s_adc_state.stream_enabled = 0U;
    s_adc_state.stream_interval_ms = ADC_APP_DEFAULT_STREAM_INTERVAL_MS;
    s_adc_state.last_stream_tick = HAL_GetTick();

    (void)HAL_ADC_Stop_DMA(&hadc1);
    (void)HAL_ADC_Stop(&hadc1);

    if (HAL_ADCEx_Calibration_Start(&hadc1, ADC_CALIB_OFFSET_LINEARITY,
                                    ADC_SINGLE_ENDED) != HAL_OK)
    {
        return;
    }

    if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)s_adc_dma_buffer,
                          ADC_APP_DMA_SAMPLES) != HAL_OK)
    {
        return;
    }

    s_adc_state.started = 1U;
}

uint8_t ADC_APP_IsStarted(void)
{
    return s_adc_state.started;
}

uint16_t ADC_APP_GetLatestRaw(void)
{
    return s_adc_dma_buffer[ADC_APP_GetLatestIndex()];
}

uint16_t ADC_APP_GetLatestMv(void)
{
    return ADC_APP_RawToMv(ADC_APP_GetLatestRaw());
}

uint16_t ADC_APP_GetAverageRaw(void)
{
    uint32_t sum = 0U;

    for (uint32_t i = 0U; i < ADC_APP_DMA_SAMPLES; ++i)
    {
        sum += s_adc_dma_buffer[i];
    }

    return (uint16_t)(sum / ADC_APP_DMA_SAMPLES);
}

uint16_t ADC_APP_GetAverageMv(void)
{
    return ADC_APP_RawToMv(ADC_APP_GetAverageRaw());
}

uint16_t ADC_APP_GetBufferSamples(void)
{
    return ADC_APP_DMA_SAMPLES;
}

void ADC_APP_SetStreamEnabled(uint8_t enabled)
{
    s_adc_state.stream_enabled = (enabled != 0U) ? 1U : 0U;
    s_adc_state.last_stream_tick = HAL_GetTick();
}

uint8_t ADC_APP_GetStreamEnabled(void)
{
    return s_adc_state.stream_enabled;
}

uint8_t ADC_APP_SetStreamIntervalMs(uint16_t interval_ms)
{
    if (interval_ms < ADC_APP_MIN_STREAM_INTERVAL_MS)
    {
        return 0U;
    }

    s_adc_state.stream_interval_ms = interval_ms;
    return 1U;
}

uint16_t ADC_APP_GetStreamIntervalMs(void)
{
    return s_adc_state.stream_interval_ms;
}

void adc_proc(void)
{
    uint32_t now_tick;

    if ((s_adc_state.started == 0U) || (s_adc_state.stream_enabled == 0U))
    {
        return;
    }

    now_tick = HAL_GetTick();
    if ((uint32_t)(now_tick - s_adc_state.last_stream_tick) <
        (uint32_t)s_adc_state.stream_interval_ms)
    {
        return;
    }

    s_adc_state.last_stream_tick = now_tick;
    (void)my_printf(&huart1, "%u,%u\r\n", (unsigned int)ADC_APP_GetLatestRaw(),
                    (unsigned int)ADC_APP_GetLatestMv());
}
