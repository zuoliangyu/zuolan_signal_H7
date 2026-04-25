#include "dsp_pipeline.h"

#include <string.h>

#include "adc_app.h"
#include "dsp.h"
#include "dsp_filters.h"
#include "uart.h"  // my_printf

// ============================================================================
// 配置常量
// ============================================================================
#define PL_FFT_MAX            1024U
#define PL_BLOCK_SAMPLES      ADC_APP_BLOCK_SAMPLES   // 128

// FIR 最大 tap 数：取所有候选 FIR 的最大值，用作 state buffer 上限
#define PL_FIR_MAX_TAPS \
    ((DSP_FIR_LP_MA5_NUM_TAPS > DSP_FIR_TEMPLATE_NUM_TAPS) \
        ? DSP_FIR_LP_MA5_NUM_TAPS : DSP_FIR_TEMPLATE_NUM_TAPS)

#define PL_BIQUAD_MAX_STAGES \
    ((DSP_BIQUAD_LP_BUTTER2_2KHZ_NUM_STAGES > DSP_BIQUAD_TEMPLATE_NUM_STAGES) \
        ? DSP_BIQUAD_LP_BUTTER2_2KHZ_NUM_STAGES : DSP_BIQUAD_TEMPLATE_NUM_STAGES)

// ============================================================================
// 模块状态
// ============================================================================
static struct {
    pipeline_mode_t   mode;
    pipeline_filter_t filter;
    dsp_fft_size_t    fft_size;
    uint8_t           dc_remove;
    uint16_t          acc_count;
    uint32_t          frame_seq;
    uint16_t          output_rate;     // 每 N 帧打印一次
    UART_HandleTypeDef *out_huart;

    // 算法实例 + 状态缓冲
    dsp_fir_f32_t    fir;
    dsp_biquad_f32_t biq;
} s_pl;

// ============================================================================
// 静态缓冲（.bss）
// ============================================================================
static float32_t s_acc_buf[PL_FFT_MAX];
static float32_t s_fft_scratch[PL_FFT_MAX];
static float32_t s_fft_mag[PL_FFT_MAX / 2U];

// 滤波 state：按各自最大长度分配，供该类滤波器共用
static float32_t s_fir_state[PL_FIR_MAX_TAPS + PL_BLOCK_SAMPLES - 1U];
static float32_t s_biq_state[2U * PL_BIQUAD_MAX_STAGES];

// ============================================================================
// 内部工具
// ============================================================================
static const char *pipeline_filter_name(pipeline_filter_t f)
{
    switch (f) {
    case PIPELINE_FILTER_NONE:             return "none";
    case PIPELINE_FILTER_FIR_MA5:          return "fir_ma5";
    case PIPELINE_FILTER_FIR_TEMPLATE:     return "fir_template";
    case PIPELINE_FILTER_BIQUAD_LP_2K:     return "biquad_butter2_2k";
    case PIPELINE_FILTER_BIQUAD_TEMPLATE:  return "biquad_template";
    default:                               return "?";
    }
}

static const char *pipeline_mode_name(pipeline_mode_t m)
{
    switch (m) {
    case PIPELINE_MODE_IDLE:    return "idle";
    case PIPELINE_MODE_ONESHOT: return "oneshot";
    case PIPELINE_MODE_STREAM:  return "stream";
    default:                    return "?";
    }
}

// 重新初始化滤波器实例（state 清零）
static void pipeline_reload_filter(void)
{
    (void)memset(s_fir_state, 0, sizeof(s_fir_state));
    (void)memset(s_biq_state, 0, sizeof(s_biq_state));

    switch (s_pl.filter) {
    case PIPELINE_FILTER_FIR_MA5:
        (void)DSP_FIR_F32_Init(&s_pl.fir,
                               DSP_FIR_LP_MA5_NUM_TAPS,
                               dsp_fir_lp_ma5_coeffs,
                               s_fir_state, PL_BLOCK_SAMPLES);
        break;
    case PIPELINE_FILTER_FIR_TEMPLATE:
        (void)DSP_FIR_F32_Init(&s_pl.fir,
                               DSP_FIR_TEMPLATE_NUM_TAPS,
                               dsp_fir_template_coeffs,
                               s_fir_state, PL_BLOCK_SAMPLES);
        break;
    case PIPELINE_FILTER_BIQUAD_LP_2K:
        (void)DSP_Biquad_F32_Init(&s_pl.biq,
                                  DSP_BIQUAD_LP_BUTTER2_2KHZ_NUM_STAGES,
                                  dsp_biquad_lp_butter2_2khz_coeffs,
                                  s_biq_state);
        break;
    case PIPELINE_FILTER_BIQUAD_TEMPLATE:
        (void)DSP_Biquad_F32_Init(&s_pl.biq,
                                  DSP_BIQUAD_TEMPLATE_NUM_STAGES,
                                  dsp_biquad_template_coeffs,
                                  s_biq_state);
        break;
    case PIPELINE_FILTER_NONE:
    default:
        break;
    }
}

// ADC 中点（16-bit 单端）。仅作为粗去 DC 的初值，让滤波器拿到"接近零均值"的数据，
// 避免巨大 DC 偏置吃掉浮点动态范围 / 让 IIR 启动瞬态过大。
// 真正的精确去 DC 在 pipeline_remove_frame_mean 里按帧均值二次去除。
#define PL_ADC_MIDSCALE 32768.0f

// 把一块 ADC 样本（uint16）转 float32，可选粗去 DC，写入 dst
// 长度恒等于 PL_BLOCK_SAMPLES
static void pipeline_convert_block(const uint16_t *src, float32_t *dst)
{
    if (s_pl.dc_remove != 0U) {
        for (uint32_t i = 0U; i < PL_BLOCK_SAMPLES; i++) {
            dst[i] = (float32_t)src[i] - PL_ADC_MIDSCALE;
        }
    } else {
        for (uint32_t i = 0U; i < PL_BLOCK_SAMPLES; i++) {
            dst[i] = (float32_t)src[i];
        }
    }
}


// 过滤波器（in 和 out 可指向同一块）
static void pipeline_apply_filter(const float32_t *src, float32_t *dst)
{
    switch (s_pl.filter) {
    case PIPELINE_FILTER_FIR_MA5:
    case PIPELINE_FILTER_FIR_TEMPLATE:
        DSP_FIR_F32_Process(&s_pl.fir, src, dst, PL_BLOCK_SAMPLES);
        break;
    case PIPELINE_FILTER_BIQUAD_LP_2K:
    case PIPELINE_FILTER_BIQUAD_TEMPLATE:
        DSP_Biquad_F32_Process(&s_pl.biq, src, dst, PL_BLOCK_SAMPLES);
        break;
    case PIPELINE_FILTER_NONE:
    default:
        if (src != dst) {
            (void)memcpy(dst, src, PL_BLOCK_SAMPLES * sizeof(float32_t));
        }
        break;
    }
}

// 执行一次 FFT 并输出，结束后清累积器
static void pipeline_emit_frame(void)
{
    float32_t frame_mean = 0.0f;
    if (s_pl.dc_remove != 0U) {
        arm_mean_f32(s_acc_buf, (uint32_t)s_pl.fft_size, &frame_mean);
        for (uint32_t i = 0U; i < (uint32_t)s_pl.fft_size; i++) {
            s_acc_buf[i] -= frame_mean;
        }
    }
    if (DSP_RFFT_Magnitude(s_pl.fft_size,
                           s_acc_buf, s_fft_scratch, s_fft_mag) != 0) {
        (void)my_printf(s_pl.out_huart, "[PIPELINE] rfft failed\r\n");
        s_pl.acc_count = 0U;
        return;
    }

    uint32_t  peak_bin = 0U;
    float32_t peak_val = 0.0f;
    DSP_FindPeak(s_fft_mag, (uint32_t)s_pl.fft_size / 2U, &peak_bin, &peak_val);

    const uint32_t fs       = ADC_APP_GetSampleRateHz();
    const float32_t bin_hz  = (float32_t)fs / (float32_t)s_pl.fft_size;
    const float32_t peak_hz = (float32_t)peak_bin * bin_hz;

    s_pl.frame_seq++;

    // 输出降频：仅每 output_rate 帧打印一次（oneshot 模式总是打）
    uint8_t should_print = (s_pl.mode == PIPELINE_MODE_ONESHOT) ||
                           (s_pl.output_rate <= 1U) ||
                           ((s_pl.frame_seq % (uint32_t)s_pl.output_rate) == 0U);
    if (should_print) {
        (void)my_printf(s_pl.out_huart,
            "[PIPELINE] seq=%lu filter=%s fft=%u fs=%lu Hz frame_mean=%.1f peak_bin=%lu peak_freq=%.2f Hz mag=%.2f\r\n",
            (unsigned long)s_pl.frame_seq,
            pipeline_filter_name(s_pl.filter),
            (unsigned)s_pl.fft_size,
            (unsigned long)fs,
            (double)frame_mean,
            (unsigned long)peak_bin,
            (double)peak_hz,
            (double)peak_val);
    }

    s_pl.acc_count = 0U;
}

// ============================================================================
// 公共 API
// ============================================================================

void DSP_Pipeline_Init(UART_HandleTypeDef *out_huart)
{
    (void)memset(&s_pl, 0, sizeof(s_pl));
    s_pl.mode        = PIPELINE_MODE_IDLE;
    s_pl.filter      = PIPELINE_FILTER_NONE;
    s_pl.fft_size    = DSP_FFT_1024;
    s_pl.dc_remove   = 1U;
    s_pl.output_rate = 1U;
    s_pl.out_huart   = out_huart;
    pipeline_reload_filter();
}

void dsp_pipeline_proc(void)
{
    if (s_pl.mode == PIPELINE_MODE_IDLE) {
        return;
    }

    static float32_t s_blk[PL_BLOCK_SAMPLES];
    adc_app_block_t  adc_block;

    while (ADC_APP_PopBlock(&adc_block) != 0U) {
        pipeline_convert_block(adc_block.samples, s_blk);
        pipeline_apply_filter(s_blk, s_blk);

        // 追加到累积器，不超过 fft_size
        const uint32_t need = (uint32_t)s_pl.fft_size - s_pl.acc_count;
        const uint32_t copy = (PL_BLOCK_SAMPLES < need) ? PL_BLOCK_SAMPLES : need;
        (void)memcpy(&s_acc_buf[s_pl.acc_count], s_blk,
                     copy * sizeof(float32_t));
        s_pl.acc_count += (uint16_t)copy;

        if (s_pl.acc_count >= (uint16_t)s_pl.fft_size) {
            pipeline_emit_frame();
            if (s_pl.mode == PIPELINE_MODE_ONESHOT) {
                s_pl.mode = PIPELINE_MODE_IDLE;
                return;
            }
        }
    }
}

void DSP_Pipeline_PrintStatus(UART_HandleTypeDef *huart)
{
    (void)my_printf(huart,
        "[PIPELINE] mode=%s filter=%s fft=%u dc=%s rate=%u acc=%u/%u seq=%lu fs=%lu Hz\r\n",
        pipeline_mode_name(s_pl.mode),
        pipeline_filter_name(s_pl.filter),
        (unsigned)s_pl.fft_size,
        (s_pl.dc_remove != 0U) ? "on" : "off",
        (unsigned)s_pl.output_rate,
        (unsigned)s_pl.acc_count,
        (unsigned)s_pl.fft_size,
        (unsigned long)s_pl.frame_seq,
        (unsigned long)ADC_APP_GetSampleRateHz());
}

void DSP_Pipeline_PrintFilters(UART_HandleTypeDef *huart)
{
    (void)my_printf(huart, "Available filters:\r\n");
    for (uint32_t i = 0U; i < (uint32_t)PIPELINE_FILTER_COUNT; i++) {
        (void)my_printf(huart, "  %s%s\r\n",
                        pipeline_filter_name((pipeline_filter_t)i),
                        ((pipeline_filter_t)i == s_pl.filter) ? "  (current)" : "");
    }
}

int DSP_Pipeline_SetFilterByName(const char *name)
{
    if (name == NULL) {
        return -1;
    }
    for (uint32_t i = 0U; i < (uint32_t)PIPELINE_FILTER_COUNT; i++) {
        if (strcmp(name, pipeline_filter_name((pipeline_filter_t)i)) == 0) {
            s_pl.filter = (pipeline_filter_t)i;
            s_pl.acc_count = 0U;
            pipeline_reload_filter();
            return 0;
        }
    }
    return -1;
}

int DSP_Pipeline_SetFftLen(uint32_t len)
{
    if ((len != 256U) && (len != 512U) && (len != 1024U)) {
        return -1;
    }
    s_pl.fft_size  = (dsp_fft_size_t)len;
    s_pl.acc_count = 0U;
    return 0;
}

uint32_t DSP_Pipeline_GetFftLen(void)
{
    return (uint32_t)s_pl.fft_size;
}

void DSP_Pipeline_SetDcRemove(uint8_t enabled)
{
    s_pl.dc_remove = (enabled != 0U) ? 1U : 0U;
    s_pl.acc_count = 0U;
}

uint8_t DSP_Pipeline_GetDcRemove(void)
{
    return s_pl.dc_remove;
}

int DSP_Pipeline_RunOneshot(void)
{
    if (ADC_APP_IsStarted() == 0U) {
        return -1;
    }
    s_pl.acc_count = 0U;
    s_pl.mode      = PIPELINE_MODE_ONESHOT;
    return 0;
}

int DSP_Pipeline_SetOutputRate(uint16_t every_n_frames)
{
    if ((every_n_frames == 0U) || (every_n_frames > 1000U)) {
        return -1;
    }
    s_pl.output_rate = every_n_frames;
    return 0;
}

uint16_t DSP_Pipeline_GetOutputRate(void)
{
    return s_pl.output_rate;
}

int DSP_Pipeline_SetStream(uint8_t enabled)
{
    if (enabled != 0U) {
        if (ADC_APP_IsStarted() == 0U) {
            return -1;
        }
        s_pl.acc_count = 0U;
        s_pl.mode      = PIPELINE_MODE_STREAM;
    } else {
        s_pl.mode = PIPELINE_MODE_IDLE;
    }
    return 0;
}
