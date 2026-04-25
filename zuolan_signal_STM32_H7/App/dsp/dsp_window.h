#ifndef DSP_WINDOW_H
#define DSP_WINDOW_H

// 该文件集中放置 FFT 前置加窗的相关定义。
// 设计思路（与 dsp_filters 平行）：
//   1) 经典窗（Hann/Hamming/Blackman）公式简单，且 FFT 长度可变
//      → 运行时按公式生成，不占 Flash
//   2) 特殊窗（Kaiser/Chebyshev/Flat-top/自定义）参数复杂
//      → MATLAB 出系数 + 静态长度模板，按 dsp_filters 同款套路
//
// CMSIS-DSP 配套用法：
//   arm_mult_f32(buf, w, buf, N);   // 时域加窗（in-place 也可）
//
// 加窗后的幅度补偿（让 peak_mag 仍能反映原始正弦幅度）：
//   coherent_gain = sum(w[n]) / N
//   amp_corrected = peak_mag / coherent_gain
//   - rectangular(NONE) : CG=1.0
//   - Hann               : CG≈0.5
//   - Hamming            : CG≈0.54
//   - Blackman           : CG≈0.42
//   - Kaiser(beta=8.6)   : CG≈0.40（具体看系数）

#include "arm_math.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DSP_WINDOW_NONE = 0,             // 矩形窗（不加窗）
    DSP_WINDOW_HANN,                 // 公式生成
    DSP_WINDOW_HAMMING,              // 公式生成
    DSP_WINDOW_BLACKMAN,             // 公式生成
    DSP_WINDOW_KAISER_TEMPLATE,      // MATLAB 模板，固定 1024 长度
    DSP_WINDOW_COUNT,
} dsp_window_type_t;

// Kaiser 模板：长度固定 1024，由 MATLAB 设计后粘贴系数到 dsp_window.c
// 默认初始化为全 1（pass-through，等同于 NONE），等待替换
#define DSP_WINDOW_KAISER_TEMPLATE_LEN  1024U
extern const float32_t dsp_window_kaiser_template[DSP_WINDOW_KAISER_TEMPLATE_LEN];

// 名称映射（用于 CLI / 状态打印）
const char *DSP_Window_Name(dsp_window_type_t type);
int  DSP_Window_FromName(const char *name, dsp_window_type_t *out);

// 生成窗系数到 dst[0..N-1]
//   - NONE      : 全 1
//   - HANN/HAM/BLK : 按公式现算
//   - KAISER_TEMPLATE : 仅 N==1024 时有效，会从模板拷贝；否则返回 -1
// 成功返回 0，失败返回 -1（参数非法或长度不匹配）
int DSP_Window_Generate(dsp_window_type_t type, float32_t *dst, uint32_t N);

// 计算 coherent gain = sum(w)/N，用于单频幅度补偿
float32_t DSP_Window_CoherentGain(const float32_t *w, uint32_t N);

#ifdef __cplusplus
}
#endif

#endif /* DSP_WINDOW_H */
