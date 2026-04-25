#ifndef DSP_H
#define DSP_H

#include <stdint.h>

#include "arm_math.h"
#include "stm32h7xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DSP_FFT_256  = 256,
    DSP_FFT_512  = 512,
    DSP_FFT_1024 = 1024,
} dsp_fft_size_t;

void DSP_Init(void);

// 是否已完成初始化（启动摘要、健康检查使用）
uint8_t DSP_IsInited(void);

// 实数 FFT 取模值。in[size]、scratch[size]、mag[size/2] 全部由调用方提供。
// 返回 0 成功，-1 size 不支持或参数非法。bin0 已置 0。
int DSP_RFFT_Magnitude(dsp_fft_size_t size,
                       const float32_t *in,
                       float32_t       *scratch,
                       float32_t       *mag);

// 找峰值 bin（包装 arm_max_f32）
void DSP_FindPeak(const float32_t *mag, uint32_t len,
                  uint32_t *peak_bin, float32_t *peak_val);

// 自测：内部生成 1 kHz/1024 点正弦做 RFFT，期望峰值 bin=128。
// 通过传入的 UART 输出结果，可被 CLI 命令触发。
void DSP_SelfTest(UART_HandleTypeDef *huart);

// ---------------------------------------------------------------------------
// FIR (real, f32)
//
// state 缓冲长度必须 >= num_taps + block_size - 1，由调用方提供。
// coeffs 的内存生命周期需覆盖到滤波器使用结束（CMSIS-DSP 内部只保存指针）。
// ---------------------------------------------------------------------------
typedef struct {
    arm_fir_instance_f32 inst;
} dsp_fir_f32_t;

int  DSP_FIR_F32_Init(dsp_fir_f32_t  *flt,
                      uint16_t        num_taps,
                      const float32_t *coeffs,
                      float32_t      *state,
                      uint32_t        block_size);

void DSP_FIR_F32_Process(dsp_fir_f32_t   *flt,
                         const float32_t *src,
                         float32_t       *dst,
                         uint32_t         block_size);

// ---------------------------------------------------------------------------
// Biquad (DF2 Transposed, real, f32)
//
// coeffs[5*N]：每段 5 个系数 {b0, b1, b2, a1, a2}（注意 a1/a2 已取负，
//   即 CMSIS 约定 y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] + a1*y[n-1] + a2*y[n-2]）。
// state[2*N]：每段 2 个状态。
// ---------------------------------------------------------------------------
typedef struct {
    arm_biquad_cascade_df2T_instance_f32 inst;
} dsp_biquad_f32_t;

int  DSP_Biquad_F32_Init(dsp_biquad_f32_t *flt,
                         uint8_t           num_stages,
                         const float32_t  *coeffs,
                         float32_t        *state);

void DSP_Biquad_F32_Process(dsp_biquad_f32_t *flt,
                            const float32_t  *src,
                            float32_t        *dst,
                            uint32_t          block_size);

// 滤波自测：生成 1 kHz 正弦，过 5 点滑动平均 FIR，
// 输出滤波前/后 RMS、峰值幅度、FFT 峰值 bin。
void DSP_Filter_SelfTest(UART_HandleTypeDef *huart);

#ifdef __cplusplus
}
#endif

#endif /* DSP_H */
