#include "dac_app.h"

#include "dac.h"
#include "main.h"
#include "tim.h"

#define DAC_APP_DC_UPDATE_HZ 1000U
#define DAC_APP_WAVE_HALF_SAMPLES (DAC_APP_WAVE_SAMPLES / 2U)
#define DAC_APP_WAVE_CENTER_RAW 2048

#if defined(__GNUC__)
#define DAC_DMA_ALIGN __attribute__((aligned(32)))
#define DAC_DMA_SECTION __attribute__((section(".dma_buffer")))
#else
#define DAC_DMA_ALIGN
#define DAC_DMA_SECTION
#endif

typedef struct
{
    dac_app_mode_t mode;
    uint16_t amp_mv;
    uint16_t offset_mv;
    uint32_t freq_hz;
    uint8_t duty_percent;
} dac_app_config_t;

static const uint16_t s_dac_sine_table[DAC_APP_WAVE_SAMPLES] = {
    2048U, 2098U, 2148U, 2198U, 2248U, 2298U, 2348U, 2398U,
    2447U, 2496U, 2545U, 2594U, 2642U, 2690U, 2737U, 2784U,
    2831U, 2877U, 2923U, 2968U, 3013U, 3057U, 3100U, 3143U,
    3185U, 3226U, 3267U, 3307U, 3346U, 3385U, 3423U, 3459U,
    3495U, 3530U, 3565U, 3598U, 3630U, 3662U, 3692U, 3722U,
    3750U, 3777U, 3804U, 3829U, 3853U, 3876U, 3898U, 3919U,
    3939U, 3958U, 3975U, 3992U, 4007U, 4021U, 4034U, 4045U,
    4056U, 4065U, 4073U, 4080U, 4085U, 4089U, 4093U, 4094U,
    4095U, 4094U, 4093U, 4089U, 4085U, 4080U, 4073U, 4065U,
    4056U, 4045U, 4034U, 4021U, 4007U, 3992U, 3975U, 3958U,
    3939U, 3919U, 3898U, 3876U, 3853U, 3829U, 3804U, 3777U,
    3750U, 3722U, 3692U, 3662U, 3630U, 3598U, 3565U, 3530U,
    3495U, 3459U, 3423U, 3385U, 3346U, 3307U, 3267U, 3226U,
    3185U, 3143U, 3100U, 3057U, 3013U, 2968U, 2923U, 2877U,
    2831U, 2784U, 2737U, 2690U, 2642U, 2594U, 2545U, 2496U,
    2447U, 2398U, 2348U, 2298U, 2248U, 2198U, 2148U, 2098U,
    2048U, 1997U, 1947U, 1897U, 1847U, 1797U, 1747U, 1697U,
    1648U, 1599U, 1550U, 1501U, 1453U, 1405U, 1358U, 1311U,
    1264U, 1218U, 1172U, 1127U, 1082U, 1038U, 995U, 952U,
    910U, 869U, 828U, 788U, 749U, 710U, 672U, 636U,
    600U, 565U, 530U, 497U, 465U, 433U, 403U, 373U,
    345U, 318U, 291U, 266U, 242U, 219U, 197U, 176U,
    156U, 137U, 120U, 103U, 88U, 74U, 61U, 50U,
    39U, 30U, 22U, 15U, 10U, 6U, 2U, 1U,
    0U, 1U, 2U, 6U, 10U, 15U, 22U, 30U,
    39U, 50U, 61U, 74U, 88U, 103U, 120U, 137U,
    156U, 176U, 197U, 219U, 242U, 266U, 291U, 318U,
    345U, 373U, 403U, 433U, 465U, 497U, 530U, 565U,
    600U, 636U, 672U, 710U, 749U, 788U, 828U, 869U,
    910U, 952U, 995U, 1038U, 1082U, 1127U, 1172U, 1218U,
    1264U, 1311U, 1358U, 1405U, 1453U, 1501U, 1550U, 1599U,
    1648U, 1697U, 1747U, 1797U, 1847U, 1897U, 1947U, 1997U,
};

static uint16_t s_dac_wave_buffer[DAC_APP_WAVE_SAMPLES] DAC_DMA_ALIGN DAC_DMA_SECTION;
static dac_app_config_t s_dac_cfg = {
    .mode = DAC_APP_DEFAULT_MODE,
    .amp_mv = DAC_APP_DEFAULT_AMP_MV,
    .offset_mv = DAC_APP_DEFAULT_OFFSET_MV,
    .freq_hz = DAC_APP_DEFAULT_FREQ_HZ,
    .duty_percent = DAC_APP_DEFAULT_DUTY_PERCENT,
};
static uint8_t s_dac_started = 0U;
static uint16_t s_dac_current_raw = 0U;

static uint16_t DAC_APP_ClampVoltageMv(uint16_t voltage_mv);
static uint8_t DAC_APP_ClampDutyPercent(uint8_t duty_percent);
static uint16_t DAC_APP_MvToRaw(uint16_t voltage_mv);
static void DAC_APP_StopHardware(void);
static uint32_t DAC_APP_GetTim6ClockHz(void);
static uint8_t DAC_APP_IsWaveFrequencySupported(uint32_t frequency_hz);
static uint8_t DAC_APP_ConfigureTim6UpdateHz(uint32_t update_hz, uint32_t *actual_update_hz);
static uint16_t DAC_APP_GetAmpMaxMvForOffset(uint16_t offset_mv);
static void DAC_APP_FillSineBuffer(void);
static void DAC_APP_FillTriangleBuffer(void);
static void DAC_APP_FillSquareBuffer(void);
static void DAC_APP_RegenerateWaveBuffer(void);
static uint8_t DAC_APP_StartCurrentMode(void);
static void DAC_APP_ApplyIfStarted(void);

static uint16_t DAC_APP_ClampVoltageMv(uint16_t voltage_mv)
{
    if (voltage_mv > DAC_APP_REFERENCE_MV)
    {
        return DAC_APP_REFERENCE_MV;
    }

    return voltage_mv;
}

static uint8_t DAC_APP_ClampDutyPercent(uint8_t duty_percent)
{
    if (duty_percent > 100U)
    {
        return 100U;
    }

    return duty_percent;
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

static uint16_t DAC_APP_GetAmpMaxMvForOffset(uint16_t offset_mv)
{
    uint16_t clamped_offset;

    clamped_offset = DAC_APP_ClampVoltageMv(offset_mv);
    if (clamped_offset > (DAC_APP_REFERENCE_MV - clamped_offset))
    {
        return (uint16_t)(DAC_APP_REFERENCE_MV - clamped_offset);
    }

    return clamped_offset;
}

static uint8_t DAC_APP_IsWaveFrequencySupported(uint32_t frequency_hz)
{
    uint32_t timer_clk_hz;
    uint32_t prescaler;
    uint64_t requested_update_hz;
    uint64_t ticks_per_update;
    uint64_t reload;

    if (frequency_hz == 0U)
    {
        return 0U;
    }

    timer_clk_hz = DAC_APP_GetTim6ClockHz();
    if (timer_clk_hz == 0U)
    {
        return 0U;
    }

    requested_update_hz = (uint64_t)frequency_hz * (uint64_t)DAC_APP_WAVE_SAMPLES;
    if ((requested_update_hz == 0ULL) || (requested_update_hz > 0xFFFFFFFFULL))
    {
        return 0U;
    }

    ticks_per_update = requested_update_hz * 65536ULL;
    prescaler = (ticks_per_update == 0ULL) ? 0U : (uint32_t)((uint64_t)timer_clk_hz / ticks_per_update);
    if (prescaler > 0xFFFFU)
    {
        return 0U;
    }

    reload = ((uint64_t)timer_clk_hz +
              ((((uint64_t)prescaler + 1ULL) * requested_update_hz) / 2ULL)) /
             (((uint64_t)prescaler + 1ULL) * requested_update_hz);
    if ((reload == 0ULL) || (reload > 65536ULL))
    {
        return 0U;
    }

    return 1U;
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

static void DAC_APP_FillSineBuffer(void)
{
    uint32_t index;

    for (index = 0U; index < DAC_APP_WAVE_SAMPLES; ++index)
    {
        int32_t centered = (int32_t)s_dac_sine_table[index] - DAC_APP_WAVE_CENTER_RAW;
        int32_t sample_mv = (int32_t)s_dac_cfg.offset_mv +
                            ((centered * (int32_t)s_dac_cfg.amp_mv) / DAC_APP_WAVE_CENTER_RAW);
        s_dac_wave_buffer[index] = DAC_APP_MvToRaw((uint16_t)sample_mv);
    }
}

static void DAC_APP_FillTriangleBuffer(void)
{
    uint32_t index;

    for (index = 0U; index < DAC_APP_WAVE_HALF_SAMPLES; ++index)
    {
        int32_t delta_mv = -(int32_t)s_dac_cfg.amp_mv +
                           (int32_t)((2U * (uint32_t)s_dac_cfg.amp_mv * index) /
                                     (DAC_APP_WAVE_HALF_SAMPLES - 1U));
        s_dac_wave_buffer[index] = DAC_APP_MvToRaw((uint16_t)((int32_t)s_dac_cfg.offset_mv + delta_mv));
    }

    for (index = DAC_APP_WAVE_HALF_SAMPLES; index < DAC_APP_WAVE_SAMPLES; ++index)
    {
        uint32_t mirror_index = index - DAC_APP_WAVE_HALF_SAMPLES;
        int32_t delta_mv = (int32_t)s_dac_cfg.amp_mv -
                           (int32_t)((2U * (uint32_t)s_dac_cfg.amp_mv * mirror_index) /
                                     (DAC_APP_WAVE_HALF_SAMPLES - 1U));
        s_dac_wave_buffer[index] = DAC_APP_MvToRaw((uint16_t)((int32_t)s_dac_cfg.offset_mv + delta_mv));
    }
}

static void DAC_APP_FillSquareBuffer(void)
{
    uint32_t index;
    uint32_t high_samples;
    int32_t high_mv;
    int32_t low_mv;

    high_samples = ((uint32_t)s_dac_cfg.duty_percent * DAC_APP_WAVE_SAMPLES + 50U) / 100U;
    if (high_samples > DAC_APP_WAVE_SAMPLES)
    {
        high_samples = DAC_APP_WAVE_SAMPLES;
    }

    high_mv = (int32_t)s_dac_cfg.offset_mv + (int32_t)s_dac_cfg.amp_mv;
    low_mv = (int32_t)s_dac_cfg.offset_mv - (int32_t)s_dac_cfg.amp_mv;

    for (index = 0U; index < DAC_APP_WAVE_SAMPLES; ++index)
    {
        if (index < high_samples)
        {
            s_dac_wave_buffer[index] = DAC_APP_MvToRaw((uint16_t)high_mv);
        }
        else
        {
            s_dac_wave_buffer[index] = DAC_APP_MvToRaw((uint16_t)low_mv);
        }
    }
}

static void DAC_APP_RegenerateWaveBuffer(void)
{
    if (s_dac_cfg.mode == DAC_APP_MODE_SINE)
    {
        DAC_APP_FillSineBuffer();
    }
    else if (s_dac_cfg.mode == DAC_APP_MODE_TRIANGLE)
    {
        DAC_APP_FillTriangleBuffer();
    }
    else
    {
        DAC_APP_FillSquareBuffer();
    }
}

// DMA 完成回调计数（每完成一次 256 样本 = 一个输出周期），用于诊断 DAC 实际输出频率
static volatile uint32_t s_dac_dma_full_count = 0U;

void HAL_DAC_ConvCpltCallbackCh1(DAC_HandleTypeDef *hdac)
{
    if (hdac == &hdac1)
    {
        s_dac_dma_full_count++;
    }
}

uint32_t DAC_APP_GetDmaFullCount(void)
{
    return s_dac_dma_full_count;
}

static uint8_t DAC_APP_StartCurrentMode(void)
{
    uint32_t actual_update_hz = 0U;
    uint64_t requested_update_hz;

    DAC_APP_StopHardware();

    if (s_dac_cfg.mode == DAC_APP_MODE_DC)
    {
        s_dac_current_raw = DAC_APP_MvToRaw(s_dac_cfg.offset_mv);

        if (HAL_DAC_Start(&hdac1, DAC_CHANNEL_1) != HAL_OK)
        {
            Error_Handler();
        }

        if (HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R, s_dac_current_raw) != HAL_OK)
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
        return 1U;
    }

    requested_update_hz = (uint64_t)s_dac_cfg.freq_hz * (uint64_t)DAC_APP_WAVE_SAMPLES;
    if ((requested_update_hz == 0ULL) || (requested_update_hz > 0xFFFFFFFFULL))
    {
        return 0U;
    }

    DAC_APP_RegenerateWaveBuffer();

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

    s_dac_current_raw = DAC_APP_MvToRaw(s_dac_cfg.offset_mv);
    s_dac_cfg.freq_hz = (actual_update_hz + (DAC_APP_WAVE_SAMPLES / 2U)) / DAC_APP_WAVE_SAMPLES;
    s_dac_started = 1U;
    return 1U;
}

static void DAC_APP_ApplyIfStarted(void)
{
    if (s_dac_started != 0U)
    {
        (void)DAC_APP_StartCurrentMode();
    }
}

void DAC_APP_Init(void)
{
    s_dac_cfg.mode = DAC_APP_DEFAULT_MODE;
    s_dac_cfg.amp_mv = DAC_APP_DEFAULT_AMP_MV;
    s_dac_cfg.offset_mv = DAC_APP_DEFAULT_OFFSET_MV;
    s_dac_cfg.freq_hz = DAC_APP_DEFAULT_FREQ_HZ;
    s_dac_cfg.duty_percent = DAC_APP_DEFAULT_DUTY_PERCENT;
    s_dac_started = 0U;
    s_dac_current_raw = 0U;

    DAC_APP_Start();
}

void DAC_APP_Start(void)
{
    (void)DAC_APP_StartCurrentMode();
}

void DAC_APP_Stop(void)
{
    DAC_APP_StopHardware();
}

uint8_t DAC_APP_IsStarted(void)
{
    return s_dac_started;
}

void DAC_APP_SetMode(dac_app_mode_t mode)
{
    if ((mode != DAC_APP_MODE_DC) && (mode != DAC_APP_MODE_SINE) &&
        (mode != DAC_APP_MODE_TRIANGLE) && (mode != DAC_APP_MODE_SQUARE))
    {
        return;
    }

    s_dac_cfg.mode = mode;
    DAC_APP_ApplyIfStarted();
}

dac_app_mode_t DAC_APP_GetMode(void)
{
    return s_dac_cfg.mode;
}

const char *DAC_APP_GetModeString(void)
{
    if (s_dac_cfg.mode == DAC_APP_MODE_SINE)
    {
        return "sine";
    }

    if (s_dac_cfg.mode == DAC_APP_MODE_TRIANGLE)
    {
        return "tri";
    }

    if (s_dac_cfg.mode == DAC_APP_MODE_SQUARE)
    {
        return "square";
    }

    return "dc";
}

uint8_t DAC_APP_SetAmpMv(uint16_t amp_mv)
{
    if (amp_mv > DAC_APP_GetAmpMaxMvForOffset(s_dac_cfg.offset_mv))
    {
        return 0U;
    }

    s_dac_cfg.amp_mv = amp_mv;
    DAC_APP_ApplyIfStarted();
    return 1U;
}

uint16_t DAC_APP_GetAmpMv(void)
{
    return s_dac_cfg.amp_mv;
}

uint16_t DAC_APP_GetAmpMaxMv(void)
{
    return DAC_APP_GetAmpMaxMvForOffset(s_dac_cfg.offset_mv);
}

uint8_t DAC_APP_SetOffsetMv(uint16_t offset_mv)
{
    uint16_t clamped_offset;

    clamped_offset = DAC_APP_ClampVoltageMv(offset_mv);
    if ((clamped_offset < s_dac_cfg.amp_mv) ||
        (clamped_offset > (DAC_APP_REFERENCE_MV - s_dac_cfg.amp_mv)))
    {
        return 0U;
    }

    s_dac_cfg.offset_mv = clamped_offset;
    DAC_APP_ApplyIfStarted();
    return 1U;
}

uint16_t DAC_APP_GetOffsetMv(void)
{
    return s_dac_cfg.offset_mv;
}

uint16_t DAC_APP_GetOffsetMinMv(void)
{
    return s_dac_cfg.amp_mv;
}

uint16_t DAC_APP_GetOffsetMaxMv(void)
{
    return (uint16_t)(DAC_APP_REFERENCE_MV - s_dac_cfg.amp_mv);
}

uint8_t DAC_APP_SetFreqHz(uint32_t frequency_hz)
{
    if (DAC_APP_IsWaveFrequencySupported(frequency_hz) == 0U)
    {
        return 0U;
    }

    s_dac_cfg.freq_hz = frequency_hz;
    DAC_APP_ApplyIfStarted();
    return 1U;
}

uint32_t DAC_APP_GetFreqHz(void)
{
    return s_dac_cfg.freq_hz;
}

void DAC_APP_SetDutyPercent(uint8_t duty_percent)
{
    s_dac_cfg.duty_percent = DAC_APP_ClampDutyPercent(duty_percent);
    DAC_APP_ApplyIfStarted();
}

uint8_t DAC_APP_GetDutyPercent(void)
{
    return s_dac_cfg.duty_percent;
}

uint16_t DAC_APP_GetCurrentRaw(void)
{
    return DAC_APP_MvToRaw(s_dac_cfg.offset_mv);
}
