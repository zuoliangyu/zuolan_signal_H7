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

#ifdef __cplusplus
}
#endif

#endif /* DSP_H */
