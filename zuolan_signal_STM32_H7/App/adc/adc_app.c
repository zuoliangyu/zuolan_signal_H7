#include "adc_app.h"

#include <string.h>

#include "adc.h"
#include "tim.h"
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
    uint8_t timer_started;
    uint8_t monitor_stream_enabled;
    uint8_t block_stream_enabled;
    uint16_t monitor_interval_ms;
    uint32_t last_monitor_tick;
    uint32_t sample_rate_hz;
    volatile uint32_t half_event_count;
    volatile uint32_t full_event_count;
    volatile uint32_t dropped_block_count;
    volatile uint32_t latest_block_seq;
    volatile uint32_t latest_frame_seq;
    volatile uint8_t block_head;
    volatile uint8_t block_tail;
    volatile uint8_t block_count;
} adc_app_state_t;

/* H7 上 ADC DMA 缓冲区同样必须放在 DMA 可达 RAM。 */
static uint16_t s_adc_dma_buffer[ADC_APP_DMA_SAMPLES] ADC_DMA_ALIGN ADC_DMA_SECTION;
static uint16_t s_adc_latest_frame[ADC_APP_DMA_SAMPLES] ADC_DMA_ALIGN ADC_DMA_SECTION;
static adc_app_block_t s_adc_block_queue[ADC_APP_CAPTURE_QUEUE_DEPTH] ADC_DMA_ALIGN
    ADC_DMA_SECTION;

static adc_app_state_t s_adc_state = {
    .started = 0U,
    .timer_started = 0U,
    .monitor_stream_enabled = 0U,
    .block_stream_enabled = 0U,
    .monitor_interval_ms = ADC_APP_DEFAULT_STREAM_INTERVAL_MS,
    .last_monitor_tick = 0U,
    .sample_rate_hz = ADC_APP_DEFAULT_SAMPLE_RATE_HZ,
    .half_event_count = 0U,
    .full_event_count = 0U,
    .dropped_block_count = 0U,
    .latest_block_seq = 0U,
    .latest_frame_seq = 0U,
    .block_head = 0U,
    .block_tail = 0U,
    .block_count = 0U,
};

static uint32_t ADC_APP_EnterCritical(void)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    return primask;
}

static void ADC_APP_ExitCritical(uint32_t primask)
{
    __set_PRIMASK(primask);
}

static uint16_t ADC_APP_RawToMv(uint16_t raw_value)
{
    uint32_t numerator = ((uint32_t)raw_value * (uint32_t)ADC_APP_REFERENCE_MV) +
                         ((uint32_t)ADC_APP_MAX_RAW_VALUE / 2U);

    return (uint16_t)(numerator / (uint32_t)ADC_APP_MAX_RAW_VALUE);
}

static uint32_t ADC_APP_GetTim2ClockHz(void)
{
    RCC_ClkInitTypeDef clk_init = {0};
    uint32_t flash_latency = 0U;
    uint32_t pclk1_hz;

    HAL_RCC_GetClockConfig(&clk_init, &flash_latency);
    pclk1_hz = HAL_RCC_GetPCLK1Freq();

    if (clk_init.APB1CLKDivider == RCC_APB1_DIV1)
    {
        return pclk1_hz;
    }

    return pclk1_hz * 2U;
}

static uint8_t ADC_APP_ConfigureTim2Rate(uint32_t sample_rate_hz,
                                         uint32_t *actual_sample_rate_hz)
{
    uint32_t timer_clk_hz;
    uint32_t prescaler;
    uint32_t period;
    uint64_t ticks_per_update;
    uint64_t reload;

    if (sample_rate_hz == 0U)
    {
        return 0U;
    }

    timer_clk_hz = ADC_APP_GetTim2ClockHz();
    if (timer_clk_hz == 0U)
    {
        return 0U;
    }

    ticks_per_update = (uint64_t)sample_rate_hz * 65536ULL;
    prescaler =
        (ticks_per_update == 0ULL)
            ? 0U
            : (uint32_t)((uint64_t)timer_clk_hz / ticks_per_update);
    if (prescaler > 0xFFFFU)
    {
        return 0U;
    }

    reload = ((uint64_t)timer_clk_hz +
              ((((uint64_t)prescaler + 1ULL) * (uint64_t)sample_rate_hz) /
               2ULL)) /
             (((uint64_t)prescaler + 1ULL) * (uint64_t)sample_rate_hz);
    if ((reload == 0ULL) || (reload > 65536ULL))
    {
        return 0U;
    }

    period = (uint32_t)(reload - 1ULL);

    __HAL_TIM_DISABLE(&htim2);
    __HAL_TIM_SET_PRESCALER(&htim2, prescaler);
    __HAL_TIM_SET_AUTORELOAD(&htim2, period);
    __HAL_TIM_SET_COUNTER(&htim2, 0U);
    htim2.Instance->EGR = TIM_EGR_UG;

    htim2.Init.Prescaler = prescaler;
    htim2.Init.Period = period;

    if (actual_sample_rate_hz != NULL)
    {
        *actual_sample_rate_hz =
            (uint32_t)((uint64_t)timer_clk_hz /
                       (((uint64_t)prescaler + 1ULL) *
                        ((uint64_t)period + 1ULL)));
    }

    return 1U;
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

static void ADC_APP_ResetRuntimeState(void)
{
    uint32_t primask = ADC_APP_EnterCritical();

    s_adc_state.timer_started = 0U;
    s_adc_state.monitor_stream_enabled = 0U;
    s_adc_state.block_stream_enabled = 0U;
    s_adc_state.monitor_interval_ms = ADC_APP_DEFAULT_STREAM_INTERVAL_MS;
    s_adc_state.last_monitor_tick = HAL_GetTick();
    s_adc_state.half_event_count = 0U;
    s_adc_state.full_event_count = 0U;
    s_adc_state.dropped_block_count = 0U;
    s_adc_state.latest_block_seq = 0U;
    s_adc_state.latest_frame_seq = 0U;
    s_adc_state.block_head = 0U;
    s_adc_state.block_tail = 0U;
    s_adc_state.block_count = 0U;

    ADC_APP_ExitCritical(primask);
}

void ADC_APP_ClearEventStats(void)
{
    uint32_t primask = ADC_APP_EnterCritical();
    s_adc_state.half_event_count = 0U;
    s_adc_state.full_event_count = 0U;
    s_adc_state.dropped_block_count = 0U;
    ADC_APP_ExitCritical(primask);
}

static void ADC_APP_QueueBlock(const uint16_t *source, adc_app_block_part_t part)
{
    adc_app_block_t *block;

    if (s_adc_state.block_count >= ADC_APP_CAPTURE_QUEUE_DEPTH)
    {
        s_adc_state.dropped_block_count++;
        return;
    }

    block = &s_adc_block_queue[s_adc_state.block_head];
    block->seq = ++s_adc_state.latest_block_seq;
    block->part = part;
    (void)memcpy(block->samples, source, sizeof(block->samples));

    s_adc_state.block_head =
        (uint8_t)((s_adc_state.block_head + 1U) % ADC_APP_CAPTURE_QUEUE_DEPTH);
    s_adc_state.block_count++;
}

void ADC_APP_Init(void)
{
    uint32_t actual_sample_rate_hz = 0U;

    (void)memset(s_adc_dma_buffer, 0, sizeof(s_adc_dma_buffer));
    (void)memset(s_adc_latest_frame, 0, sizeof(s_adc_latest_frame));
    (void)memset(s_adc_block_queue, 0, sizeof(s_adc_block_queue));

    ADC_APP_ResetRuntimeState();
    s_adc_state.started = 0U;
    s_adc_state.sample_rate_hz = ADC_APP_DEFAULT_SAMPLE_RATE_HZ;

    (void)HAL_TIM_Base_Stop(&htim2);
    (void)HAL_ADC_Stop_DMA(&hadc1);
    (void)HAL_ADC_Stop(&hadc1);

    if (HAL_ADCEx_Calibration_Start(&hadc1, ADC_CALIB_OFFSET_LINEARITY,
                                    ADC_SINGLE_ENDED) != HAL_OK)
    {
        return;
    }

    if (ADC_APP_ConfigureTim2Rate(s_adc_state.sample_rate_hz,
                                  &actual_sample_rate_hz) == 0U)
    {
        return;
    }
    s_adc_state.sample_rate_hz = actual_sample_rate_hz;

    if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)s_adc_dma_buffer,
                          ADC_APP_DMA_SAMPLES) != HAL_OK)
    {
        return;
    }

    if (HAL_TIM_Base_Start(&htim2) != HAL_OK)
    {
        (void)HAL_ADC_Stop_DMA(&hadc1);
        return;
    }

    s_adc_state.timer_started = 1U;
    s_adc_state.started = 1U;
}

uint8_t ADC_APP_IsStarted(void)
{
    return s_adc_state.started;
}

uint8_t ADC_APP_SetSampleRateHz(uint32_t sample_rate_hz)
{
    uint32_t actual_sample_rate_hz = 0U;
    uint8_t was_timer_started;

    if (sample_rate_hz == 0U)
    {
        return 0U;
    }

    was_timer_started = s_adc_state.timer_started;
    if (was_timer_started != 0U)
    {
        (void)HAL_TIM_Base_Stop(&htim2);
        s_adc_state.timer_started = 0U;
    }

    if (ADC_APP_ConfigureTim2Rate(sample_rate_hz, &actual_sample_rate_hz) == 0U)
    {
        if (was_timer_started != 0U)
        {
            (void)HAL_TIM_Base_Start(&htim2);
            s_adc_state.timer_started = 1U;
        }
        return 0U;
    }

    s_adc_state.sample_rate_hz = actual_sample_rate_hz;

    if ((was_timer_started != 0U) && (HAL_TIM_Base_Start(&htim2) == HAL_OK))
    {
        s_adc_state.timer_started = 1U;
    }

    return 1U;
}

uint32_t ADC_APP_GetSampleRateHz(void)
{
    return s_adc_state.sample_rate_hz;
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

uint16_t ADC_APP_GetBlockSamples(void)
{
    return ADC_APP_BLOCK_SAMPLES;
}

void ADC_APP_SetStreamEnabled(uint8_t enabled)
{
    s_adc_state.monitor_stream_enabled = (enabled != 0U) ? 1U : 0U;
    s_adc_state.last_monitor_tick = HAL_GetTick();
}

uint8_t ADC_APP_GetStreamEnabled(void)
{
    return s_adc_state.monitor_stream_enabled;
}

uint8_t ADC_APP_SetStreamIntervalMs(uint16_t interval_ms)
{
    if (interval_ms < ADC_APP_MIN_STREAM_INTERVAL_MS)
    {
        return 0U;
    }

    s_adc_state.monitor_interval_ms = interval_ms;
    return 1U;
}

uint16_t ADC_APP_GetStreamIntervalMs(void)
{
    return s_adc_state.monitor_interval_ms;
}

void ADC_APP_SetBlockStreamEnabled(uint8_t enabled)
{
    s_adc_state.block_stream_enabled = (enabled != 0U) ? 1U : 0U;
}

uint8_t ADC_APP_GetBlockStreamEnabled(void)
{
    return s_adc_state.block_stream_enabled;
}

uint32_t ADC_APP_GetDroppedBlockCount(void)
{
    return s_adc_state.dropped_block_count;
}

uint32_t ADC_APP_GetHalfEventCount(void)
{
    return s_adc_state.half_event_count;
}

uint32_t ADC_APP_GetFullEventCount(void)
{
    return s_adc_state.full_event_count;
}

uint32_t ADC_APP_GetLatestFrameSeq(void)
{
    return s_adc_state.latest_frame_seq;
}

void ADC_APP_CopyLatestFrame(uint16_t *buffer, uint16_t max_samples)
{
    uint32_t primask;
    uint32_t copy_samples;

    if ((buffer == NULL) || (max_samples == 0U))
    {
        return;
    }

    copy_samples =
        (max_samples < ADC_APP_DMA_SAMPLES) ? max_samples : ADC_APP_DMA_SAMPLES;

    primask = ADC_APP_EnterCritical();
    (void)memcpy(buffer, s_adc_latest_frame, copy_samples * sizeof(uint16_t));
    ADC_APP_ExitCritical(primask);
}

uint8_t ADC_APP_PopBlock(adc_app_block_t *block)
{
    uint32_t primask;

    if (block == NULL)
    {
        return 0U;
    }

    primask = ADC_APP_EnterCritical();
    if (s_adc_state.block_count == 0U)
    {
        ADC_APP_ExitCritical(primask);
        return 0U;
    }

    *block = s_adc_block_queue[s_adc_state.block_tail];
    s_adc_state.block_tail =
        (uint8_t)((s_adc_state.block_tail + 1U) % ADC_APP_CAPTURE_QUEUE_DEPTH);
    s_adc_state.block_count--;
    ADC_APP_ExitCritical(primask);

    return 1U;
}

void adc_proc(void)
{
    uint32_t now_tick;
    adc_app_block_t block;

    if (s_adc_state.started == 0U)
    {
        return;
    }

    if (s_adc_state.block_stream_enabled != 0U)
    {
        if (ADC_APP_PopBlock(&block) != 0U)
        {
            for (uint32_t i = 0U; i < ADC_APP_BLOCK_SAMPLES; ++i)
            {
                (void)my_printf(&huart1, "%u\r\n",
                                (unsigned int)block.samples[i]);
            }
            return;
        }
    }

    if (s_adc_state.monitor_stream_enabled == 0U)
    {
        return;
    }

    now_tick = HAL_GetTick();
    if ((uint32_t)(now_tick - s_adc_state.last_monitor_tick) <
        (uint32_t)s_adc_state.monitor_interval_ms)
    {
        return;
    }

    s_adc_state.last_monitor_tick = now_tick;
    (void)my_printf(&huart1, "%u,%u\r\n", (unsigned int)ADC_APP_GetLatestRaw(),
                    (unsigned int)ADC_APP_GetLatestMv());
}

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc != &hadc1)
    {
        return;
    }

    s_adc_state.half_event_count++;
    ADC_APP_QueueBlock(&s_adc_dma_buffer[0], ADC_APP_BLOCK_PART_HALF);
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc != &hadc1)
    {
        return;
    }

    s_adc_state.full_event_count++;
    ADC_APP_QueueBlock(&s_adc_dma_buffer[ADC_APP_BLOCK_SAMPLES],
                       ADC_APP_BLOCK_PART_FULL);
    (void)memcpy(s_adc_latest_frame, s_adc_dma_buffer, sizeof(s_adc_latest_frame));
    s_adc_state.latest_frame_seq++;
}
