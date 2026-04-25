#ifndef DSP_PIPELINE_H
#define DSP_PIPELINE_H

#include <stdint.h>
#include "arm_math.h"
#include "stm32h7xx_hal.h"
#include "dsp_window.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PIPELINE_MODE_IDLE = 0,
    PIPELINE_MODE_ONESHOT,
    PIPELINE_MODE_STREAM,
} pipeline_mode_t;

// 单帧结果快照：每次 emit_frame 都会更新 last_result
typedef struct {
    uint8_t           valid;
    uint32_t          frame_seq;
    uint32_t          fft_size;
    uint32_t          fs_hz;
    dsp_window_type_t window;
    float32_t         cgain;
    float32_t         frame_mean;
    uint32_t          peak_bin;
    float32_t         peak_freq_hz;
    float32_t         peak_mag;     // RFFT magnitude（受窗增益影响）
    float32_t         peak_amp;     // = peak_mag / cgain，单频幅度估计
} dsp_pipeline_result_t;

typedef enum {
    PIPELINE_FILTER_NONE = 0,
    PIPELINE_FILTER_FIR_MA5,
    PIPELINE_FILTER_FIR_TEMPLATE,
    PIPELINE_FILTER_BIQUAD_LP_2K,
    PIPELINE_FILTER_BIQUAD_TEMPLATE,
    PIPELINE_FILTER_COUNT,
} pipeline_filter_t;

// 启动初始化（main 在 DSP_Init 之后调用）
void DSP_Pipeline_Init(UART_HandleTypeDef *out_huart);

// scheduler 周期任务（建议 1 ms）
void dsp_pipeline_proc(void);

// CLI 调用入口
void DSP_Pipeline_PrintStatus(UART_HandleTypeDef *huart);
void DSP_Pipeline_PrintFilters(UART_HandleTypeDef *huart);
int  DSP_Pipeline_SetFilterByName(const char *name);
int  DSP_Pipeline_SetFftLen(uint32_t len);
uint32_t DSP_Pipeline_GetFftLen(void);
void DSP_Pipeline_SetDcRemove(uint8_t enabled);
uint8_t DSP_Pipeline_GetDcRemove(void);
void DSP_Pipeline_PrintWindows(UART_HandleTypeDef *huart);
int  DSP_Pipeline_SetWindowByName(const char *name);
dsp_window_type_t DSP_Pipeline_GetWindow(void);
int  DSP_Pipeline_RunOneshot(void);
int  DSP_Pipeline_SetStream(uint8_t enabled);

// 同步执行一次 oneshot：内部反复调用 dsp_pipeline_proc，
//   等满 1 帧（fft_size 个样本）完成 RFFT 后返回。
//   silent != 0 时不产生单帧日志（用于自动测试，避免污染输出）。
//   result != NULL 时把这一帧的结果拷贝出去。
//   返回 0 成功，-1 ADC 未启动，-2 超时
int DSP_Pipeline_RunOneshotBlocking(uint32_t timeout_ms,
                                    uint8_t  silent,
                                    dsp_pipeline_result_t *result);

// 取最近一帧结果快照
void DSP_Pipeline_GetLastResult(dsp_pipeline_result_t *out);

// 取最近一帧 magnitude 数组指针。
//   - 长度 = fft_size / 2，写入 *out_len（可为 NULL）
//   - 仅在下一次 oneshot/stream 帧到来前有效；调用方读完即可
//   - bin 0 已被置 0（DC/Nyquist 打包，无意义）
const float32_t *DSP_Pipeline_GetLastMag(uint32_t *out_len);

// 端到端硬件回环自检：
//   1) 保存当前 DAC 状态
//   2) 强制配置 sine 1 kHz / 1V amp / 1.65V offset 并启动
//   3) 切窗到 hann，warmup 2 帧
//   4) 跑 1 次 oneshot，验证 peak_bin 与 amp 在容差内
//   5) 还原 DAC 状态
//   6) 输出 PASS/FAIL
// 前提：板上把 PA4(DAC1_OUT1) 跳线到 PA0(ADC1_IN16)
// 返回 0 PASS，<0 FAIL（具体原因已通过 huart 打印）
int DSP_Pipeline_SelfTest(UART_HandleTypeDef *huart);
// 输出降频：每 N 帧打印一次（N=1 表示每帧都打），N 范围 [1, 1000]
int  DSP_Pipeline_SetOutputRate(uint16_t every_n_frames);
uint16_t DSP_Pipeline_GetOutputRate(void);

#ifdef __cplusplus
}
#endif

#endif /* DSP_PIPELINE_H */
