#include "dsp_filters.h"

// ---------------------------------------------------------------------------
// FIR
// ---------------------------------------------------------------------------

// 5 点滑动平均：h = ones(1,5)/5
const float32_t dsp_fir_lp_ma5_coeffs[DSP_FIR_LP_MA5_NUM_TAPS] = {
    0.2f, 0.2f, 0.2f, 0.2f, 0.2f
};

// FIR 占位模板：默认初始化为单位冲激响应（pass-through），等待 MATLAB 系数粘贴
const float32_t dsp_fir_template_coeffs[DSP_FIR_TEMPLATE_NUM_TAPS] = {
    /* === 粘贴 MATLAB 输出 === */
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 0.0f
    /* ====================== */
};

// ---------------------------------------------------------------------------
// IIR Biquad
// ---------------------------------------------------------------------------

// 2 阶 Butterworth low-pass，fc=2 kHz，Fs=8 kHz：
//   K = tan(pi * fc/Fs) = tan(pi/4) = 1
//   b0 = K^2/(1+sqrt(2)*K+K^2)        ≈ 0.2928932188
//   b1 = 2*b0                          ≈ 0.5857864376
//   b2 = b0                            ≈ 0.2928932188
//   a1_matlab = 2*(K^2-1)/...          = 0
//   a2_matlab = (1-sqrt(2)*K+K^2)/...  ≈ 0.1715728753
//   CMSIS a1 = -a1_matlab = 0
//   CMSIS a2 = -a2_matlab ≈ -0.1715728753
const float32_t dsp_biquad_lp_butter2_2khz_coeffs[5U * DSP_BIQUAD_LP_BUTTER2_2KHZ_NUM_STAGES] = {
    /* stage 0: {b0, b1, b2, a1, a2} */
    0.2928932188f, 0.5857864376f, 0.2928932188f, 0.0f, -0.1715728753f
};

// Biquad 占位模板：默认初始化为 pass-through（每段都是单位响应）
const float32_t dsp_biquad_template_coeffs[5U * DSP_BIQUAD_TEMPLATE_NUM_STAGES] = {
    /* === 粘贴 MATLAB tf2sos 输出 === */
    /* stage 0 */ 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    /* stage 1 */ 1.0f, 0.0f, 0.0f, 0.0f, 0.0f
    /* ============================== */
};
