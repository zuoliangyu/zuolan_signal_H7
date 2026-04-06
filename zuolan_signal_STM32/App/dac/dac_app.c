#include "dac_app.h"

#include "dac.h"
#include "main.h"
#include "tim.h"

#define DAC_APP_DC_UPDATE_HZ 1000U
#define DAC_APP_WAVE_HALF_SAMPLES (DAC_APP_WAVE_SAMPLES / 2U)

#if defined(__GNUC__)
#define DAC_DMA_ALIGN __attribute__((aligned(32)))
#define DAC_DMA_SECTION __attribute__((section(".dma_buffer")))
#else
#define DAC_DMA_ALIGN
#define DAC_DMA_SECTION
#endif

static const uint16_t s_dac_sine_table[DAC_APP_WAVE_SAMPLES] = {
    2048U, 2148U, 2248U, 2348U, 2447U, 2545U, 2642U, 2737U,
    2831U, 2923U, 3013U, 3100U, 3185U, 3267U, 3346U, 3423U,
    3495U, 3565U, 3630U, 3692U, 3750U, 3804U, 3853U, 3898U,
    3939U, 3975U, 4007U, 4034U, 4056U, 4073U, 4085U, 4093U,
    4095U, 4093U, 4085U, 4073U, 4056U, 4034U, 4007U, 3975U,
    3939U, 3898U, 3853U, 3804U, 3750U, 3692U, 3630U, 3565U,
    3495U, 3423U, 3346U, 3267U, 3185U, 3100U, 3013U, 2923U,
    2831U, 2737U, 2642U, 2545U, 2447U, 2348U, 2248U, 2148U,
    2048U, 1947U, 1847U, 1747U, 1648U, 1550U, 1453U, 1358U,
    1264U, 1172U, 1082U, 995U, 910U, 828U, 749U, 672U,
    600U, 530U, 465U, 403U, 345U, 291U, 242U, 197U,
    156U, 120U, 88U, 61U, 39U, 22U, 10U, 2U,
    0U, 2U, 10U, 22U, 39U, 61U, 88U, 120U,
    156U, 197U, 242U, 291U, 345U, 403U, 465U, 530U,
    600U, 672U, 749U, 828U, 910U, 995U, 1082U, 1172U,
    1264U, 1358U, 1453U, 1550U, 1648U, 1747U, 1847U, 1947U,
};

static uint16_t s_dac_wave_buffer[DAC_APP_WAVE_SAMPLES] DAC_DMA_ALIGN DAC_DMA_SECTION;

static uint16_t s_dac_raw_value = 0U;
static uint8_t s_dac_started = 0U;
static uint16_t s_dac_voltage_mv = 0U;
static dac_app_waveform_t s_dac_waveform = DAC_APP_WAVE_NONE;
static uint32_t s_dac_wave_frequency_hz = 0U;

static uint16_t DAC_APP_ClampRawValue(uint16_t raw_value);
static uint16_t DAC_APP_ClampVoltageMv(uint16_t voltage_mv);
static uint16_t DAC_APP_RawToMv(uint16_t raw_value);
static uint16_t DAC_APP_MvToRaw(uint16_t voltage_mv);
static void DAC_APP_StopHardware(void);
static uint32_t DAC_APP_GetTim6ClockHz(void);
static uint8_t DAC_APP_ConfigureTim6UpdateHz(uint32_t update_hz, uint32_t *actual_update_hz);
static void DAC_APP_FillTriangleBuffer(void);
static void DAC_APP_FillSquareBuffer(void);
static void DAC_APP_LoadWaveformBuffer(dac_app_waveform_t waveform);
static void DAC_APP_StartDcOutput(void);
static uint8_t DAC_APP_StartWaveOutput(void);

static uint16_t DAC_APP_ClampRawValue(uint16_t raw_value)
{
    if (raw_value > DAC_APP_MAX_RAW_VALUE)
    {
        return DAC_APP_MAX_RAW_VALUE;
    }

    return raw_value;
}

static uint16_t DAC_APP_ClampVoltageMv(uint16_t voltage_mv)
{
    if (voltage_mv > DAC_APP_REFERENCE_MV)
    {
        return DAC_APP_REFERENCE_MV;
    }

    return voltage_mv;
}

static uint16_t DAC_APP_RawToMv(uint16_t raw_value)
{
    uint32_t numerator;

    raw_value = DAC_APP_ClampRawValue(raw_value);
    numerator = ((uint32_t)raw_value * (uint32_t)DAC_APP_REFERENCE_MV) +
                ((uint32_t)DAC_APP_MAX_RAW_VALUE / 2U);
    return (uint16_t)(numerator / (uint32_t)DAC_APP_MAX_RAW_VALUE);
}

static uint16_t DAC_APP_MvToRaw(uint16_t voltage_mv)
{
    uint32_t numerator;

    voltage_mv = DAC_APP_ClampVoltageMv(voltage_mv);
    numerator = ((uint32_t)voltage_mv * (uint32_t)DAC_APP_MAX_RAW_VALUE) +
                ((uint32_t)DAC_APP_REFERENCE_MV / 2U);
    return (uint16_t)(numerator / (uint32_t)DAC_APP_REFERENCE_MV);
}

static void DAC_APP_StopHardware(void)
{
    (void)HAL_TIM_Base_Stop(&htim6);
    (void)HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_1);
    (void)HAL_DAC_Stop(&hdac1, DAC_CHANNEL_1);
    s_dac_started = 0U;
}

static uint32_t DAC_APP_GetTim6ClockHz(void)
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

static uint8_t DAC_APP_ConfigureTim6UpdateHz(uint32_t update_hz, uint32_t *actual_update_hz)
{
    uint32_t timer_clk_hz;
    uint32_t prescaler;
    uint32_t period;
    uint64_t ticks_per_update;
    uint64_t reload;

    if (update_hz == 0U)
    {
        return 0U;
    }

    timer_clk_hz = DAC_APP_GetTim6ClockHz();
    if (timer_clk_hz == 0U)
    {
        return 0U;
    }

    ticks_per_update = (uint64_t)update_hz * 65536ULL;
    prescaler = (ticks_per_update == 0ULL) ? 0U : (uint32_t)((uint64_t)timer_clk_hz / ticks_per_update);
    if (prescaler > 0xFFFFU)
    {
        return 0U;
    }

    reload = ((uint64_t)timer_clk_hz +
              ((((uint64_t)prescaler + 1ULL) * (uint64_t)update_hz) / 2ULL)) /
             (((uint64_t)prescaler + 1ULL) * (uint64_t)update_hz);
    if ((reload == 0ULL) || (reload > 65536ULL))
    {
        return 0U;
    }

    period = (uint32_t)(reload - 1ULL);

    __HAL_TIM_DISABLE(&htim6);
    __HAL_TIM_SET_PRESCALER(&htim6, prescaler);
    __HAL_TIM_SET_AUTORELOAD(&htim6, period);
    __HAL_TIM_SET_COUNTER(&htim6, 0U);
    htim6.Instance->EGR = TIM_EGR_UG;

    htim6.Init.Prescaler = prescaler;
    htim6.Init.Period = period;

    if (actual_update_hz != NULL)
    {
        *actual_update_hz = (uint32_t)((uint64_t)timer_clk_hz /
                                       (((uint64_t)prescaler + 1ULL) * ((uint64_t)period + 1ULL)));
    }

    return 1U;
}

static void DAC_APP_FillTriangleBuffer(void)
{
    uint32_t index;

    for (index = 0U; index < DAC_APP_WAVE_HALF_SAMPLES; ++index)
    {
        s_dac_wave_buffer[index] =
            (uint16_t)((index * (uint32_t)DAC_APP_MAX_RAW_VALUE) / (DAC_APP_WAVE_HALF_SAMPLES - 1U));
    }

    for (index = DAC_APP_WAVE_HALF_SAMPLES; index < DAC_APP_WAVE_SAMPLES; ++index)
    {
        uint32_t mirror_index = index - DAC_APP_WAVE_HALF_SAMPLES;
        s_dac_wave_buffer[index] =
            (uint16_t)(DAC_APP_MAX_RAW_VALUE -
                       ((mirror_index * (uint32_t)DAC_APP_MAX_RAW_VALUE) /
                        (DAC_APP_WAVE_HALF_SAMPLES - 1U)));
    }
}

static void DAC_APP_FillSquareBuffer(void)
{
    uint32_t index;

    for (index = 0U; index < DAC_APP_WAVE_HALF_SAMPLES; ++index)
    {
        s_dac_wave_buffer[index] = DAC_APP_MAX_RAW_VALUE;
    }

    for (index = DAC_APP_WAVE_HALF_SAMPLES; index < DAC_APP_WAVE_SAMPLES; ++index)
    {
        s_dac_wave_buffer[index] = 0U;
    }
}

static void DAC_APP_LoadWaveformBuffer(dac_app_waveform_t waveform)
{
    uint32_t index;

    if (waveform == DAC_APP_WAVE_SINE)
    {
        for (index = 0U; index < DAC_APP_WAVE_SAMPLES; ++index)
        {
            s_dac_wave_buffer[index] = s_dac_sine_table[index];
        }
    }
    else if (waveform == DAC_APP_WAVE_TRIANGLE)
    {
        DAC_APP_FillTriangleBuffer();
    }
    else
    {
        DAC_APP_FillSquareBuffer();
    }
}

static void DAC_APP_StartDcOutput(void)
{
    uint32_t actual_update_hz = 0U;

    DAC_APP_StopHardware();

    if (HAL_DAC_Start(&hdac1, DAC_CHANNEL_1) != HAL_OK)
    {
        Error_Handler();
    }

    if (HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R, s_dac_raw_value) != HAL_OK)
    {
        Error_Handler();
    }

    if (DAC_APP_ConfigureTim6UpdateHz(DAC_APP_DC_UPDATE_HZ, &actual_update_hz) == 0U)
    {
        Error_Handler();
    }

    if (HAL_TIM_Base_Start(&htim6) != HAL_OK)
    {
        Error_Handler();
    }

    s_dac_started = 1U;
}

static uint8_t DAC_APP_StartWaveOutput(void)
{
    uint32_t actual_update_hz = 0U;
    uint64_t requested_update_hz;

    if ((s_dac_waveform == DAC_APP_WAVE_NONE) || (s_dac_wave_frequency_hz == 0U))
    {
        return 0U;
    }

    requested_update_hz = (uint64_t)s_dac_wave_frequency_hz * (uint64_t)DAC_APP_WAVE_SAMPLES;
    if ((requested_update_hz == 0ULL) || (requested_update_hz > 0xFFFFFFFFULL))
    {
        return 0U;
    }

    DAC_APP_LoadWaveformBuffer(s_dac_waveform);
    DAC_APP_StopHardware();

    if (DAC_APP_ConfigureTim6UpdateHz((uint32_t)requested_update_hz, &actual_update_hz) == 0U)
    {
        return 0U;
    }

    if (HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1, (uint32_t *)s_dac_wave_buffer,
                          DAC_APP_WAVE_SAMPLES, DAC_ALIGN_12B_R) != HAL_OK)
    {
        Error_Handler();
    }

    if (HAL_TIM_Base_Start(&htim6) != HAL_OK)
    {
        Error_Handler();
    }

    s_dac_wave_frequency_hz = (actual_update_hz + (DAC_APP_WAVE_SAMPLES / 2U)) / DAC_APP_WAVE_SAMPLES;
    s_dac_started = 1U;
    return 1U;
}

void DAC_APP_Start(void)
{
    if (s_dac_started != 0U)
    {
        return;
    }

    if (s_dac_waveform != DAC_APP_WAVE_NONE)
    {
        (void)DAC_APP_StartWaveOutput();
    }
    else
    {
        DAC_APP_StartDcOutput();
    }
}

void DAC_APP_Stop(void)
{
    DAC_APP_StopHardware();
}

uint8_t DAC_APP_IsStarted(void)
{
    return s_dac_started;
}

void DAC_APP_SetValueRaw(uint16_t raw_value)
{
    s_dac_raw_value = DAC_APP_ClampRawValue(raw_value);
    s_dac_voltage_mv = DAC_APP_RawToMv(s_dac_raw_value);
    s_dac_waveform = DAC_APP_WAVE_NONE;
    s_dac_wave_frequency_hz = 0U;
    DAC_APP_StartDcOutput();
}

uint16_t DAC_APP_GetValueRaw(void)
{
    return s_dac_raw_value;
}

void DAC_APP_SetValueMv(uint16_t voltage_mv)
{
    voltage_mv = DAC_APP_ClampVoltageMv(voltage_mv);
    DAC_APP_SetValueRaw(DAC_APP_MvToRaw(voltage_mv));
}

uint16_t DAC_APP_GetValueMv(void)
{
    return s_dac_voltage_mv;
}

uint8_t DAC_APP_StartWave(dac_app_waveform_t waveform, uint32_t frequency_hz)
{
    if ((waveform != DAC_APP_WAVE_SINE) && (waveform != DAC_APP_WAVE_TRIANGLE) &&
        (waveform != DAC_APP_WAVE_SQUARE))
    {
        return 0U;
    }

    s_dac_waveform = waveform;
    s_dac_wave_frequency_hz = frequency_hz;

    return DAC_APP_StartWaveOutput();
}

dac_app_waveform_t DAC_APP_GetWaveform(void)
{
    return s_dac_waveform;
}

uint32_t DAC_APP_GetWaveFrequencyHz(void)
{
    return s_dac_wave_frequency_hz;
}

const char *DAC_APP_GetModeString(void)
{
    if (s_dac_waveform == DAC_APP_WAVE_SINE)
    {
        return "sine";
    }

    if (s_dac_waveform == DAC_APP_WAVE_TRIANGLE)
    {
        return "triangle";
    }

    if (s_dac_waveform == DAC_APP_WAVE_SQUARE)
    {
        return "square";
    }

    return "dc";
}

void DAC_APP_Init(void)
{
    s_dac_raw_value = 0U;
    s_dac_started = 0U;
    s_dac_voltage_mv = 0U;
    s_dac_waveform = DAC_APP_WAVE_NONE;
    s_dac_wave_frequency_hz = 0U;

    DAC_APP_Start();
    DAC_APP_SetValueMv(DAC_APP_DEFAULT_MV);
}
