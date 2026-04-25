#ifndef DSP_FILTERS_H
#define DSP_FILTERS_H

// 该文件集中放置工程使用的滤波器系数（FIR/IIR）。
// 所有系数原则上来自 MATLAB 设计脚本，见：
//   docs/DSP/MATLAB滤波器设计与导出.md
//
// 维护规则：
//   - 每个滤波器在头文件里声明：const 系数指针、长度、采样率、设计 spec 注释
//   - 每个滤波器在 .c 里写 const 系数数组 + 一个静态 state 缓冲（按 CMSIS-DSP 要求）
//   - 修改系数请同步更新注释里的 MATLAB 脚本/参数，便于反查
//
// 命名约定：
//   DSP_FIR_<NAME>_*    例如 DSP_FIR_LP_MA5_*
//   DSP_BIQUAD_<NAME>_* 例如 DSP_BIQUAD_LP_BUTTER2_2000HZ_*

#include "arm_math.h"

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------------------------------------------------------
// FIR：5 点滑动平均（low-pass，最简模板）
//
// MATLAB 等价脚本：
//   N  = 5;
//   h  = ones(1,N)/N;        % 系数
//   fvtool(h, 1);            % 可视化频响
//
// 设计目的：作为"系数从 MATLAB 导出 → 工程使用"流程的最小可验证示例。
// 频响：|H(f)| = |sin(pi*f*N/Fs) / (N * sin(pi*f/Fs))|，在 0 处增益 1.0。
// 在 Fs=8 kHz、信号 1 kHz 处：|H| = sin(5pi/8)/(5*sin(pi/8)) ≈ 0.4828
// （即衰减约 -6.32 dB）。
// ---------------------------------------------------------------------------
#define DSP_FIR_LP_MA5_NUM_TAPS  5U

extern const float32_t dsp_fir_lp_ma5_coeffs[DSP_FIR_LP_MA5_NUM_TAPS];

// ---------------------------------------------------------------------------
// FIR：占位模板（粘贴 MATLAB fir1/firpm 输出）
//
// MATLAB 等价脚本：
//   Fs = 8000;
//   N  = 64;                    % 阶数（系数个数 = N+1）
//   fc = 1500;                  % 截止频率
//   h  = fir1(N, fc/(Fs/2));    % Hamming 默认窗
//
// 使用步骤：
//   1) 在 MATLAB 里运行上述脚本，得到向量 h(1..N+1)
//   2) 修改下面 NUM_TAPS 为 N+1
//   3) 用 sprintf('%.10ff,', h) 一次性导出成 C 数组初始化串
//   4) 替换 dsp_filters.c 里 dsp_fir_template_coeffs[] 内容
//   5) 状态缓冲长度 = NUM_TAPS + block_size - 1，由调用方自行声明
// ---------------------------------------------------------------------------
#define DSP_FIR_TEMPLATE_NUM_TAPS  8U  // 改这里同步系数数组长度

extern const float32_t dsp_fir_template_coeffs[DSP_FIR_TEMPLATE_NUM_TAPS];

// ---------------------------------------------------------------------------
// IIR Biquad：2 阶 Butterworth low-pass（fc=2 kHz，Fs=8 kHz，单段）
//
// MATLAB 等价脚本：
//   Fs = 8000;
//   fc = 2000;
//   [b, a]   = butter(2, fc/(Fs/2));
//   [sos, g] = tf2sos(b, a);
//   sos_cmsis = [sos(:,1)*g, sos(:,2)*g, sos(:,3)*g, -sos(:,5), -sos(:,6)]; % 注意 a1/a2 取负
//   sprintf('%.10ff,', sos_cmsis')   % 按行导出
//
// CMSIS 系数布局：每段 5 个 {b0, b1, b2, a1, a2}，a1/a2 已取负。
// 期望：1 kHz 信号几乎无衰减（处于通带内）。
// ---------------------------------------------------------------------------
#define DSP_BIQUAD_LP_BUTTER2_2KHZ_NUM_STAGES  1U

extern const float32_t dsp_biquad_lp_butter2_2khz_coeffs[5U * DSP_BIQUAD_LP_BUTTER2_2KHZ_NUM_STAGES];

// ---------------------------------------------------------------------------
// IIR Biquad：占位模板（粘贴 MATLAB butter+tf2sos 输出）
//
// 使用步骤：
//   1) MATLAB 中运行 butter/cheby1/ellip 得到 [b,a]
//   2) [sos,g] = tf2sos(b,a) 拆 SOS
//   3) 对每段：CMSIS 顺序 = [b0*g_i, b1*g_i, b2*g_i, -a1, -a2]（增益放进第一段 b0/b1/b2）
//   4) 修改 NUM_STAGES，更新 dsp_filters.c 里 dsp_biquad_template_coeffs[]
//   5) 状态缓冲长度 = 2 * NUM_STAGES，由调用方声明
// ---------------------------------------------------------------------------
#define DSP_BIQUAD_TEMPLATE_NUM_STAGES  2U

extern const float32_t dsp_biquad_template_coeffs[5U * DSP_BIQUAD_TEMPLATE_NUM_STAGES];

#ifdef __cplusplus
}
#endif

#endif /* DSP_FILTERS_H */
