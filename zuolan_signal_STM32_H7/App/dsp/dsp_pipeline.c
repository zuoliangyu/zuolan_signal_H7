#include "dsp_pipeline.h"

#include <string.h>

#include "adc_app.h"
#include "dac_app.h"
#include "dsp.h"
#include "dsp_filters.h"
#include "dsp_window.h"
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

    // 加窗
    dsp_window_type_t window;
    uint32_t          window_len;     // 当前窗系数对应的长度
    float32_t         window_cgain;   // coherent gain = sum(w)/N

    // 自动测试用：silent=1 时 emit_frame 不打印单帧日志
    uint8_t           quiet;

    // 最近一帧结果（emit_frame 末尾填）
    dsp_pipeline_result_t last_result;
} s_pl;

// ============================================================================
// 静态缓冲（.bss）
// ============================================================================
static float32_t s_acc_buf[PL_FFT_MAX];
static float32_t s_fft_scratch[PL_FFT_MAX];
static float32_t s_fft_mag[PL_FFT_MAX / 2U];
static float32_t s_window_buf[PL_FFT_MAX];

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

// 重新生成当前窗系数（fft 改变 / window 改变时调用）
// 失败（如 KAISER_TEMPLATE 长度不匹配）时自动回退到 NONE，并把 cgain 置 1
static void pipeline_reload_window(void)
{
    const uint32_t N = (uint32_t)s_pl.fft_size;
    if (DSP_Window_Generate(s_pl.window, s_window_buf, N) != 0) {
        // 长度不匹配（最常见：选了 kaiser 但 fft != 1024），退回 NONE
        if (s_pl.out_huart != NULL) {
            (void)my_printf(s_pl.out_huart,
                "[PIPELINE] window=%s requires fft_len=%u, fall back to none\r\n",
                DSP_Window_Name(s_pl.window),
                (unsigned)DSP_WINDOW_KAISER_TEMPLATE_LEN);
        }
        s_pl.window = DSP_WINDOW_NONE;
        (void)DSP_Window_Generate(DSP_WINDOW_NONE, s_window_buf, N);
    }
    s_pl.window_len   = N;
    s_pl.window_cgain = DSP_Window_CoherentGain(s_window_buf, N);
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
    const uint32_t N = (uint32_t)s_pl.fft_size;

    float32_t frame_mean = 0.0f;
    if (s_pl.dc_remove != 0U) {
        arm_mean_f32(s_acc_buf, N, &frame_mean);
        for (uint32_t i = 0U; i < N; i++) {
            s_acc_buf[i] -= frame_mean;
        }
    }

    // 加窗（in-place）：仅在 window != NONE 时实际改变数据
    // NONE 时窗系数也是全 1，arm_mult_f32 等同于不变 —— 为了少一次 N 次乘，
    // 这里仍跳过避免浪费 CPU
    if (s_pl.window != DSP_WINDOW_NONE) {
        arm_mult_f32(s_acc_buf, s_window_buf, s_acc_buf, N);
    }

    if (DSP_RFFT_Magnitude(s_pl.fft_size,
                           s_acc_buf, s_fft_scratch, s_fft_mag) != 0) {
        (void)my_printf(s_pl.out_huart, "[PIPELINE] rfft failed\r\n");
        s_pl.acc_count = 0U;
        return;
    }

    uint32_t  peak_bin = 0U;
    float32_t peak_val = 0.0f;
    DSP_FindPeak(s_fft_mag, N / 2U, &peak_bin, &peak_val);

    const uint32_t fs       = ADC_APP_GetSampleRateHz();
    const float32_t bin_hz  = (float32_t)fs / (float32_t)N;
    const float32_t peak_hz = (float32_t)peak_bin * bin_hz;

    // 单频幅度补偿：peak_amp ≈ peak_val / coherent_gain
    // NONE 下 cgain=1，等同于不补偿
    const float32_t cg = (s_pl.window_cgain > 1e-6f) ? s_pl.window_cgain : 1.0f;
    const float32_t peak_amp = peak_val / cg;

    s_pl.frame_seq++;

    // 写 last_result 快照
    s_pl.last_result.valid        = 1U;
    s_pl.last_result.frame_seq    = s_pl.frame_seq;
    s_pl.last_result.fft_size     = N;
    s_pl.last_result.fs_hz        = fs;
    s_pl.last_result.window       = s_pl.window;
    s_pl.last_result.cgain        = cg;
    s_pl.last_result.frame_mean   = frame_mean;
    s_pl.last_result.peak_bin     = peak_bin;
    s_pl.last_result.peak_freq_hz = peak_hz;
    s_pl.last_result.peak_mag     = peak_val;
    s_pl.last_result.peak_amp     = peak_amp;

    // 输出降频：仅每 output_rate 帧打印一次（oneshot 模式总是打）
    // quiet=1 时强制不打印（自动测试用）
    uint8_t should_print = (s_pl.quiet == 0U) &&
                           ((s_pl.mode == PIPELINE_MODE_ONESHOT) ||
                            (s_pl.output_rate <= 1U) ||
                            ((s_pl.frame_seq % (uint32_t)s_pl.output_rate) == 0U));
    if (should_print) {
        (void)my_printf(s_pl.out_huart,
            "[PIPELINE] seq=%lu filter=%s window=%s fft=%u fs=%lu Hz frame_mean=%.1f peak_bin=%lu peak_freq=%.2f Hz mag=%.2f amp=%.2f\r\n",
            (unsigned long)s_pl.frame_seq,
            pipeline_filter_name(s_pl.filter),
            DSP_Window_Name(s_pl.window),
            (unsigned)s_pl.fft_size,
            (unsigned long)fs,
            (double)frame_mean,
            (unsigned long)peak_bin,
            (double)peak_hz,
            (double)peak_val,
            (double)peak_amp);
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
    s_pl.window      = DSP_WINDOW_NONE;
    pipeline_reload_filter();
    pipeline_reload_window();
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
        "[PIPELINE] mode=%s filter=%s window=%s cgain=%.3f fft=%u dc=%s rate=%u acc=%u/%u seq=%lu fs=%lu Hz\r\n",
        pipeline_mode_name(s_pl.mode),
        pipeline_filter_name(s_pl.filter),
        DSP_Window_Name(s_pl.window),
        (double)s_pl.window_cgain,
        (unsigned)s_pl.fft_size,
        (s_pl.dc_remove != 0U) ? "on" : "off",
        (unsigned)s_pl.output_rate,
        (unsigned)s_pl.acc_count,
        (unsigned)s_pl.fft_size,
        (unsigned long)s_pl.frame_seq,
        (unsigned long)ADC_APP_GetSampleRateHz());
}

void DSP_Pipeline_PrintWindows(UART_HandleTypeDef *huart)
{
    (void)my_printf(huart, "Available windows:\r\n");
    for (uint32_t i = 0U; i < (uint32_t)DSP_WINDOW_COUNT; i++) {
        (void)my_printf(huart, "  %s%s\r\n",
                        DSP_Window_Name((dsp_window_type_t)i),
                        ((dsp_window_type_t)i == s_pl.window) ? "  (current)" : "");
    }
}

int DSP_Pipeline_SetWindowByName(const char *name)
{
    dsp_window_type_t t;
    if (DSP_Window_FromName(name, &t) != 0) {
        return -1;
    }
    s_pl.window    = t;
    s_pl.acc_count = 0U;
    pipeline_reload_window();
    return 0;
}

dsp_window_type_t DSP_Pipeline_GetWindow(void)
{
    return s_pl.window;
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
    pipeline_reload_window();   // 窗长度必须跟 fft 同步
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

int DSP_Pipeline_RunOneshotBlocking(uint32_t timeout_ms,
                                    uint8_t  silent,
                                    dsp_pipeline_result_t *result)
{
    if (ADC_APP_IsStarted() == 0U) {
        return -1;
    }
    s_pl.acc_count            = 0U;
    s_pl.last_result.valid    = 0U;
    s_pl.mode                 = PIPELINE_MODE_ONESHOT;
    const uint8_t prev_quiet  = s_pl.quiet;
    s_pl.quiet                = (silent != 0U) ? 1U : 0U;

    const uint32_t t0 = HAL_GetTick();
    while (s_pl.mode == PIPELINE_MODE_ONESHOT) {
        // 主动驱动 pipeline_proc：消费 ADC 队列里的块
        // ADC 是 DMA + 中断填队列，所以这里阻塞不会丢数据
        dsp_pipeline_proc();
        if ((uint32_t)(HAL_GetTick() - t0) > timeout_ms) {
            s_pl.mode  = PIPELINE_MODE_IDLE;
            s_pl.quiet = prev_quiet;
            return -2;
        }
    }

    s_pl.quiet = prev_quiet;
    if (result != NULL) {
        *result = s_pl.last_result;
    }
    return 0;
}

void DSP_Pipeline_GetLastResult(dsp_pipeline_result_t *out)
{
    if (out != NULL) {
        *out = s_pl.last_result;
    }
}

const float32_t *DSP_Pipeline_GetLastMag(uint32_t *out_len)
{
    if (out_len != NULL) {
        *out_len = (uint32_t)s_pl.fft_size / 2U;
    }
    return s_fft_mag;
}

// ============================================================================
// 端到端硬件回环自检：DAC sine -> PA4 -> 跳线 -> PA0 -> ADC -> pipeline
// 跟 window test 同款 4 窗对比表风格，但额外自己驱动 DAC 并还原现场。
// ============================================================================
int DSP_Pipeline_SelfTest(UART_HandleTypeDef *huart)
{
    static const char *const k_windows[] = {"none", "hann", "hamming", "blackman"};
    static const uint32_t     k_count    = (uint32_t)(sizeof(k_windows) / sizeof(k_windows[0]));

    if (huart == NULL) { return -1; }
    if (ADC_APP_IsStarted() == 0U) {
        (void)my_printf(huart, "[PL-SELFTEST] FAIL: ADC not running\r\n");
        return -1;
    }

    // 1) 保存 DAC + window 现场
    const dac_app_mode_t    prev_mode    = DAC_APP_GetMode();
    const uint16_t          prev_amp     = DAC_APP_GetAmpMv();
    const uint16_t          prev_offset  = DAC_APP_GetOffsetMv();
    const uint32_t          prev_freq    = DAC_APP_GetFreqHz();
    const uint8_t           prev_started = DAC_APP_IsStarted();
    const dsp_window_type_t prev_window  = s_pl.window;

    // 2) 强制 1 kHz / 1V amp / 1.65V offset sine
    DAC_APP_Stop();
    (void)DAC_APP_SetAmpMv(1000U);
    (void)DAC_APP_SetOffsetMv(1650U);
    (void)DAC_APP_SetFreqHz(1000U);
    DAC_APP_SetMode(DAC_APP_MODE_SINE);
    DAC_APP_Start();

    (void)my_printf(huart,
        "[PL-SELFTEST] DAC sine 1 kHz / 1000 mV / offset 1650 mV; fft=%u; warmup 2 frames\r\n",
        (unsigned)s_pl.fft_size);

    // 3) warmup 2 帧
    (void)DSP_Pipeline_SetWindowByName("none");
    for (uint32_t w = 0U; w < 2U; w++) {
        dsp_pipeline_result_t discard;
        (void)DSP_Pipeline_RunOneshotBlocking(2000U, 1U, &discard);
    }

    // 4) 4 窗对比测量
    dsp_pipeline_result_t results[4];
    (void)memset(results, 0, sizeof(results));

    for (uint32_t i = 0U; i < k_count; i++) {
        if (DSP_Pipeline_SetWindowByName(k_windows[i]) != 0) { continue; }
        (void)DSP_Pipeline_RunOneshotBlocking(2000U, 1U, &results[i]);
    }

    // 5) 还原 DAC + window 现场（不论后面 PASS/FAIL）
    DAC_APP_Stop();
    (void)DAC_APP_SetAmpMv(prev_amp);
    (void)DAC_APP_SetOffsetMv(prev_offset);
    (void)DAC_APP_SetFreqHz(prev_freq);
    DAC_APP_SetMode(prev_mode);
    if (prev_started != 0U) { DAC_APP_Start(); }
    (void)DSP_Pipeline_SetWindowByName(DSP_Window_Name(prev_window));

    // 6) 期望值
    if (results[0].valid == 0U) {
        (void)my_printf(huart, "[PL-SELFTEST] FAIL: baseline (none) frame missing\r\n");
        return -2;
    }
    const uint32_t  fs            = results[0].fs_hz;
    const float32_t bin_hz        = (float32_t)fs / (float32_t)results[0].fft_size;
    const uint32_t  expected_bin  = (uint32_t)(1000.0f / bin_hz + 0.5f);
    // 1000 mV peak / 3300 mV ref * 65535 → A_raw → amp = A_raw * N / 2
    const float32_t expected_a    = (1000.0f / 3300.0f) * 65535.0f;
    const float32_t expected_amp  = expected_a * (float32_t)results[0].fft_size / 2.0f;
    const float32_t amp_none      = (results[0].peak_amp > 1.0f) ? results[0].peak_amp : 1.0f;

    // 7) 打印对比表
    (void)my_printf(huart,
        "[PL-SELFTEST] expected: peak_bin=%lu (1000 Hz / %.3f Hz/bin) amp=%.0f\r\n",
        (unsigned long)expected_bin, (double)bin_hz, (double)expected_amp);
    (void)my_printf(huart, "\r\n");
    (void)my_printf(huart,
        "  window     cgain  peak_bin  peak_freq      mag           amp           amp/none\r\n");
    (void)my_printf(huart,
        "  ---------  -----  --------  -------------  ------------  ------------  --------\r\n");

    for (uint32_t i = 0U; i < k_count; i++) {
        if (results[i].valid == 0U) {
            (void)my_printf(huart, "  %-9s  (no result)\r\n", k_windows[i]);
            continue;
        }
        (void)my_printf(huart,
            "  %-9s  %.3f  %8lu  %9.2f Hz  %12.0f  %12.0f  %.3f\r\n",
            k_windows[i],
            (double)results[i].cgain,
            (unsigned long)results[i].peak_bin,
            (double)results[i].peak_freq_hz,
            (double)results[i].peak_mag,
            (double)results[i].peak_amp,
            (double)(results[i].peak_amp / amp_none));
    }

    // 8) verdict
    //   - 每个窗 peak_bin 在 expected ±2
    //   - 每个窗 amp 在 expected_amp 的 0.7~1.3
    //   - 加窗 amp 与 none 偏差 < 15%
    uint8_t pass = 1U;
    const char *fail_reason = "";
    for (uint32_t i = 0U; i < k_count; i++) {
        if (results[i].valid == 0U) { continue; }
        int32_t db = (int32_t)results[i].peak_bin - (int32_t)expected_bin;
        if ((db < -2) || (db > 2)) {
            pass = 0U; fail_reason = "peak_bin shifted from expected"; break;
        }
        float32_t ar = results[i].peak_amp / expected_amp;
        if ((ar < 0.7f) || (ar > 1.3f)) {
            pass = 0U; fail_reason = "amp deviates from expected (>30%)"; break;
        }
        if (i > 0U) {
            float32_t cr = results[i].peak_amp / amp_none;
            if ((cr < 0.85f) || (cr > 1.15f)) {
                pass = 0U; fail_reason = "amp inconsistent across windows (>15%)"; break;
            }
        }
    }

    if (pass != 0U) {
        (void)my_printf(huart,
            "  verdict: PASS  (peak_bin within +/-2 of %lu, amp within +/-30%% of expected, cross-window within +/-15%%)\r\n",
            (unsigned long)expected_bin);
        return 0;
    }
    (void)my_printf(huart, "  verdict: FAIL  (%s)\r\n", fail_reason);
    return -3;
}
