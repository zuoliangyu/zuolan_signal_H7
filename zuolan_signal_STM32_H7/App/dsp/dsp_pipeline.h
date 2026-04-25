#ifndef DSP_PIPELINE_H
#define DSP_PIPELINE_H

#include <stdint.h>
#include "stm32h7xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PIPELINE_MODE_IDLE = 0,
    PIPELINE_MODE_ONESHOT,
    PIPELINE_MODE_STREAM,
} pipeline_mode_t;

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
int  DSP_Pipeline_RunOneshot(void);
int  DSP_Pipeline_SetStream(uint8_t enabled);

#ifdef __cplusplus
}
#endif

#endif /* DSP_PIPELINE_H */
