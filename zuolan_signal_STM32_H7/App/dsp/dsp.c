#include "dsp.h"

#include <string.h>

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
        2.0f * 3.14159265358979323846f * DSP_SELFTEST_TONE_HZ / DSP_SELFTEST_FS_HZ;
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
