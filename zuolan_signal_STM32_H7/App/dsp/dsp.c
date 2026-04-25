#include "dsp.h"

#include <string.h>

#include "dsp_filters.h"
#include "uart.h"  // my_printf

#define DSP_SELFTEST_LEN     1024U
#define DSP_SELFTEST_FS_HZ   8000.0f
#define DSP_SELFTEST_TONE_HZ 1000.0f
// 期望峰值 bin = 1000 / (8000/1024) = 128

static arm_rfft_fast_instance_f32 s_rfft_256;
static arm_rfft_fast_instance_f32 s_rfft_512;
static arm_rfft_fast_instance_f32 s_rfft_1024;
static uint8_t s_dsp_inited = 0U;

// 自测专用 buffer，仅在 SelfTest 路径使用，正常调用方自带 buffer
static float32_t s_selftest_in[DSP_SELFTEST_LEN];
static float32_t s_selftest_scratch[DSP_SELFTEST_LEN];
static float32_t s_selftest_mag[DSP_SELFTEST_LEN / 2U];

void DSP_Init(void)
{
    (void)arm_rfft_fast_init_f32(&s_rfft_256,  256U);
    (void)arm_rfft_fast_init_f32(&s_rfft_512,  512U);
    (void)arm_rfft_fast_init_f32(&s_rfft_1024, 1024U);
    s_dsp_inited = 1U;
}

uint8_t DSP_IsInited(void)
{
    return s_dsp_inited;
}

static arm_rfft_fast_instance_f32 *DSP_PickInstance(dsp_fft_size_t size)
{
    switch (size) {
    case DSP_FFT_256:  return &s_rfft_256;
    case DSP_FFT_512:  return &s_rfft_512;
    case DSP_FFT_1024: return &s_rfft_1024;
    default:           return NULL;
    }
}

int DSP_RFFT_Magnitude(dsp_fft_size_t size,
                       const float32_t *in,
                       float32_t       *scratch,
                       float32_t       *mag)
{
    if ((s_dsp_inited == 0U) || (in == NULL) || (scratch == NULL) || (mag == NULL)) {
        return -1;
    }

    arm_rfft_fast_instance_f32 *inst = DSP_PickInstance(size);
    if (inst == NULL) {
        return -1;
    }

    arm_rfft_fast_f32(inst, (float32_t *)in, scratch, 0);
    arm_cmplx_mag_f32(scratch, mag, (uint32_t)size / 2U);
    mag[0] = 0.0f;  // bin0 是 DC/Nyquist 打包，作为峰值搜索时忽略
    return 0;
}

void DSP_FindPeak(const float32_t *mag, uint32_t len,
                  uint32_t *peak_bin, float32_t *peak_val)
{
    if ((mag == NULL) || (peak_bin == NULL) || (peak_val == NULL)) {
        return;
    }
    arm_max_f32(mag, len, peak_val, peak_bin);
}

void DSP_SelfTest(UART_HandleTypeDef *huart)
{
    if (huart == NULL) {
        return;
    }

    if (s_dsp_inited == 0U) {
        DSP_Init();
    }

    // 1) 生成测试正弦
    const float32_t two_pi_f0_over_fs =
        2.0f * PI * DSP_SELFTEST_TONE_HZ / DSP_SELFTEST_FS_HZ;
    for (uint32_t i = 0U; i < DSP_SELFTEST_LEN; i++) {
        s_selftest_in[i] = arm_sin_f32(two_pi_f0_over_fs * (float32_t)i);
    }

    // 2) RFFT + 取模
    int rc = DSP_RFFT_Magnitude(DSP_FFT_1024,
                                s_selftest_in,
                                s_selftest_scratch,
                                s_selftest_mag);
    if (rc != 0) {
        (void)my_printf(huart, "[FFT] selftest failed (rc=%d)\r\n", rc);
        return;
    }

    // 3) 找峰
    uint32_t  peak_bin = 0U;
    float32_t peak_val = 0.0f;
    DSP_FindPeak(s_selftest_mag, DSP_SELFTEST_LEN / 2U, &peak_bin, &peak_val);

    // 4) 输出
    const float32_t bin_hz = DSP_SELFTEST_FS_HZ / (float32_t)DSP_SELFTEST_LEN;
    (void)my_printf(huart,
        "[FFT] len=%u Fs=%.1f Hz f0=%.1f Hz -> peak_bin=%lu peak_freq=%.2f Hz mag=%.2f (expect bin=128)\r\n",
        (unsigned)DSP_SELFTEST_LEN,
        (double)DSP_SELFTEST_FS_HZ,
        (double)DSP_SELFTEST_TONE_HZ,
        (unsigned long)peak_bin,
        (double)((float32_t)peak_bin * bin_hz),
        (double)peak_val);
}

// ===========================================================================
// FIR / Biquad 包装层（薄包装，将来可加统计/旁路）
// ===========================================================================

int DSP_FIR_F32_Init(dsp_fir_f32_t  *flt,
                     uint16_t        num_taps,
                     const float32_t *coeffs,
                     float32_t      *state,
                     uint32_t        block_size)
{
    if ((flt == NULL) || (coeffs == NULL) || (state == NULL) ||
        (num_taps == 0U) || (block_size == 0U)) {
        return -1;
    }
    arm_fir_init_f32(&flt->inst, num_taps,
                     (float32_t *)coeffs, state, block_size);
    return 0;
}

void DSP_FIR_F32_Process(dsp_fir_f32_t   *flt,
                         const float32_t *src,
                         float32_t       *dst,
                         uint32_t         block_size)
{
    if ((flt == NULL) || (src == NULL) || (dst == NULL) || (block_size == 0U)) {
        return;
    }
    arm_fir_f32(&flt->inst, (float32_t *)src, dst, block_size);
}

int DSP_Biquad_F32_Init(dsp_biquad_f32_t *flt,
                        uint8_t           num_stages,
                        const float32_t  *coeffs,
                        float32_t        *state)
{
    if ((flt == NULL) || (coeffs == NULL) || (state == NULL) ||
        (num_stages == 0U)) {
        return -1;
    }
    arm_biquad_cascade_df2T_init_f32(&flt->inst, num_stages,
                                     (float32_t *)coeffs, state);
    return 0;
}

void DSP_Biquad_F32_Process(dsp_biquad_f32_t *flt,
                            const float32_t  *src,
                            float32_t        *dst,
                            uint32_t          block_size)
{
    if ((flt == NULL) || (src == NULL) || (dst == NULL) || (block_size == 0U)) {
        return;
    }
    arm_biquad_cascade_df2T_f32(&flt->inst, (float32_t *)src, dst, block_size);
}

// ===========================================================================
// 滤波自测
// ===========================================================================

#define DSP_FLT_SELFTEST_LEN  1024U

// 滤波自测专用 buffer
static float32_t s_flt_in[DSP_FLT_SELFTEST_LEN];
static float32_t s_flt_out[DSP_FLT_SELFTEST_LEN];
static float32_t s_flt_state[DSP_FIR_LP_MA5_NUM_TAPS + DSP_FLT_SELFTEST_LEN - 1U];
static float32_t s_flt_scratch[DSP_FLT_SELFTEST_LEN];      // 复用做 FFT scratch
static float32_t s_flt_mag[DSP_FLT_SELFTEST_LEN / 2U];

static float32_t DSP_RMS(const float32_t *x, uint32_t n)
{
    float32_t rms = 0.0f;
    arm_rms_f32((float32_t *)x, n, &rms);
    return rms;
}

void DSP_Filter_SelfTest(UART_HandleTypeDef *huart)
{
    if (huart == NULL) {
        return;
    }
    if (s_dsp_inited == 0U) {
        DSP_Init();
    }

    // 1) 生成 1 kHz 正弦（与 FFT 自测同源）
    const float32_t two_pi_f0_over_fs =
        2.0f * PI * DSP_SELFTEST_TONE_HZ / DSP_SELFTEST_FS_HZ;
    for (uint32_t i = 0U; i < DSP_FLT_SELFTEST_LEN; i++) {
        s_flt_in[i] = arm_sin_f32(two_pi_f0_over_fs * (float32_t)i);
    }

    // 2) 过 5 点滑动平均 FIR
    dsp_fir_f32_t flt;
    if (DSP_FIR_F32_Init(&flt,
                         DSP_FIR_LP_MA5_NUM_TAPS,
                         dsp_fir_lp_ma5_coeffs,
                         s_flt_state,
                         DSP_FLT_SELFTEST_LEN) != 0) {
        (void)my_printf(huart, "[FILTER] init failed\r\n");
        return;
    }
    DSP_FIR_F32_Process(&flt, s_flt_in, s_flt_out, DSP_FLT_SELFTEST_LEN);

    // 3) 滤波前/后 RMS、峰值
    float32_t rms_in  = DSP_RMS(s_flt_in,  DSP_FLT_SELFTEST_LEN);
    float32_t rms_out = DSP_RMS(s_flt_out, DSP_FLT_SELFTEST_LEN);

    // 4) 滤波后做一次 FFT，看峰值衰减情况
    (void)DSP_RFFT_Magnitude(DSP_FFT_1024,
                             s_flt_out, s_flt_scratch, s_flt_mag);
    uint32_t  peak_bin = 0U;
    float32_t peak_val = 0.0f;
    DSP_FindPeak(s_flt_mag, DSP_FLT_SELFTEST_LEN / 2U, &peak_bin, &peak_val);

    const float32_t gain_lin = (rms_in > 0.0f) ? (rms_out / rms_in) : 0.0f;

    // 5) 输出 FIR 结果
    (void)my_printf(huart,
        "[FILTER] FIR=lp_ma5 N=%u Fs=%.1f Hz f0=%.1f Hz\r\n",
        (unsigned)DSP_FIR_LP_MA5_NUM_TAPS,
        (double)DSP_SELFTEST_FS_HZ,
        (double)DSP_SELFTEST_TONE_HZ);
    (void)my_printf(huart,
        "[FILTER]   FIR rms_in=%.4f rms_out=%.4f gain=%.4f (theory ~0.4828)\r\n",
        (double)rms_in, (double)rms_out, (double)gain_lin);
    (void)my_printf(huart,
        "[FILTER]   FIR post-fft peak_bin=%lu mag=%.2f (1 kHz attenuated)\r\n",
        (unsigned long)peak_bin, (double)peak_val);

    // 6) 同样输入过 Biquad（Butterworth LP fc=2 kHz）
    dsp_biquad_f32_t biq;
    static float32_t s_biq_state[2U * DSP_BIQUAD_LP_BUTTER2_2KHZ_NUM_STAGES];
    if (DSP_Biquad_F32_Init(&biq,
                            DSP_BIQUAD_LP_BUTTER2_2KHZ_NUM_STAGES,
                            dsp_biquad_lp_butter2_2khz_coeffs,
                            s_biq_state) != 0) {
        (void)my_printf(huart, "[FILTER] biquad init failed\r\n");
        return;
    }
    DSP_Biquad_F32_Process(&biq, s_flt_in, s_flt_out, DSP_FLT_SELFTEST_LEN);

    float32_t rms_biq = DSP_RMS(s_flt_out, DSP_FLT_SELFTEST_LEN);
    (void)DSP_RFFT_Magnitude(DSP_FFT_1024, s_flt_out, s_flt_scratch, s_flt_mag);
    DSP_FindPeak(s_flt_mag, DSP_FLT_SELFTEST_LEN / 2U, &peak_bin, &peak_val);
    const float32_t gain_biq = (rms_in > 0.0f) ? (rms_biq / rms_in) : 0.0f;

    (void)my_printf(huart,
        "[FILTER] BIQUAD=lp_butter2 fc=2000 Hz Fs=%.1f Hz stages=%u\r\n",
        (double)DSP_SELFTEST_FS_HZ,
        (unsigned)DSP_BIQUAD_LP_BUTTER2_2KHZ_NUM_STAGES);
    (void)my_printf(huart,
        "[FILTER]   BIQUAD rms_in=%.4f rms_out=%.4f gain=%.4f (passband ~1.0)\r\n",
        (double)rms_in, (double)rms_biq, (double)gain_biq);
    (void)my_printf(huart,
        "[FILTER]   BIQUAD post-fft peak_bin=%lu mag=%.2f (1 kHz preserved)\r\n",
        (unsigned long)peak_bin, (double)peak_val);
}
