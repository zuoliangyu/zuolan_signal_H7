#include "dsp_window.h"

#include <string.h>

// ---------------------------------------------------------------------------
// Kaiser 占位模板：长度固定 1024
//
// MATLAB 等价脚本：
//   N    = 1024;
//   beta = 8.6;          % beta 越大主瓣越宽、旁瓣越低（≈ -84 dB at 8.6）
//   w    = kaiser(N, beta);
//   sprintf('%.10ff,', w)   % 一次性导出 C 数组初始化串
//
// 使用步骤：
//   1) 在 MATLAB 里运行上述脚本，得到 1024 长向量 w
//   2) 把 sprintf 输出粘贴替换下面 dsp_window_kaiser_template[] 的内容
//   3) 默认占位是全 1（等同于矩形窗），用于在没替换前不破坏现有行为
// ---------------------------------------------------------------------------
const float32_t dsp_window_kaiser_template[DSP_WINDOW_KAISER_TEMPLATE_LEN] = {
    /* === 粘贴 MATLAB 输出（共 1024 个 float） === */
    [0 ... (DSP_WINDOW_KAISER_TEMPLATE_LEN - 1U)] = 1.0f
    /* ============================================ */
};

// ---------------------------------------------------------------------------
// 名称映射
// ---------------------------------------------------------------------------
static const char *const s_window_names[DSP_WINDOW_COUNT] = {
    [DSP_WINDOW_NONE]            = "none",
    [DSP_WINDOW_HANN]            = "hann",
    [DSP_WINDOW_HAMMING]         = "hamming",
    [DSP_WINDOW_BLACKMAN]        = "blackman",
    [DSP_WINDOW_KAISER_TEMPLATE] = "kaiser",
};

const char *DSP_Window_Name(dsp_window_type_t type)
{
    if ((unsigned)type >= (unsigned)DSP_WINDOW_COUNT) {
        return "?";
    }
    return s_window_names[type];
}

int DSP_Window_FromName(const char *name, dsp_window_type_t *out)
{
    if ((name == NULL) || (out == NULL)) {
        return -1;
    }
    for (uint32_t i = 0U; i < (uint32_t)DSP_WINDOW_COUNT; i++) {
        if (strcmp(name, s_window_names[i]) == 0) {
            *out = (dsp_window_type_t)i;
            return 0;
        }
    }
    return -1;
}

// ---------------------------------------------------------------------------
// 生成窗系数
//
// 公式（n = 0..N-1，分母用 N-1 是对称窗的常用约定）：
//   Hann       : 0.5 - 0.5*cos(2π n/(N-1))
//   Hamming    : 0.54 - 0.46*cos(2π n/(N-1))
//   Blackman   : 0.42 - 0.5*cos(2π n/(N-1)) + 0.08*cos(4π n/(N-1))
// ---------------------------------------------------------------------------
int DSP_Window_Generate(dsp_window_type_t type, float32_t *dst, uint32_t N)
{
    if ((dst == NULL) || (N < 2U)) {
        return -1;
    }

    switch (type) {
    case DSP_WINDOW_NONE: {
        for (uint32_t n = 0U; n < N; n++) {
            dst[n] = 1.0f;
        }
        return 0;
    }

    case DSP_WINDOW_HANN: {
        const float32_t k = 2.0f * PI / (float32_t)(N - 1U);
        for (uint32_t n = 0U; n < N; n++) {
            dst[n] = 0.5f - 0.5f * cosf(k * (float32_t)n);
        }
        return 0;
    }

    case DSP_WINDOW_HAMMING: {
        const float32_t k = 2.0f * PI / (float32_t)(N - 1U);
        for (uint32_t n = 0U; n < N; n++) {
            dst[n] = 0.54f - 0.46f * cosf(k * (float32_t)n);
        }
        return 0;
    }

    case DSP_WINDOW_BLACKMAN: {
        const float32_t k1 = 2.0f * PI / (float32_t)(N - 1U);
        const float32_t k2 = 4.0f * PI / (float32_t)(N - 1U);
        for (uint32_t n = 0U; n < N; n++) {
            const float32_t x = (float32_t)n;
            dst[n] = 0.42f - 0.5f * cosf(k1 * x) + 0.08f * cosf(k2 * x);
        }
        return 0;
    }

    case DSP_WINDOW_KAISER_TEMPLATE: {
        if (N != DSP_WINDOW_KAISER_TEMPLATE_LEN) {
            return -1;  // 模板长度固定 1024，调用方需保证 fft_size 一致
        }
        (void)memcpy(dst, dsp_window_kaiser_template,
                     N * sizeof(float32_t));
        return 0;
    }

    default:
        return -1;
    }
}

float32_t DSP_Window_CoherentGain(const float32_t *w, uint32_t N)
{
    if ((w == NULL) || (N == 0U)) {
        return 1.0f;
    }
    float32_t sum = 0.0f;
    for (uint32_t n = 0U; n < N; n++) {
        sum += w[n];
    }
    return sum / (float32_t)N;
}
