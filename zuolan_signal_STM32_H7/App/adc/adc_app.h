#ifndef ADC_APP_H
#define ADC_APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define ADC_APP_REFERENCE_MV 3300U
#define ADC_APP_MAX_RAW_VALUE 65535U
#define ADC_APP_DMA_SAMPLES 256U
#define ADC_APP_BLOCK_SAMPLES (ADC_APP_DMA_SAMPLES / 2U)
#define ADC_APP_CAPTURE_QUEUE_DEPTH 4U
#define ADC_APP_MIN_STREAM_INTERVAL_MS 2U
#define ADC_APP_DEFAULT_STREAM_INTERVAL_MS 20U
#define ADC_APP_DEFAULT_SAMPLE_RATE_HZ 256000U

typedef enum
{
    ADC_APP_BLOCK_PART_HALF = 0U,
    ADC_APP_BLOCK_PART_FULL,
} adc_app_block_part_t;

typedef struct
{
    uint32_t seq;
    adc_app_block_part_t part;
    uint16_t samples[ADC_APP_BLOCK_SAMPLES];
} adc_app_block_t;

void ADC_APP_Init(void);
uint8_t ADC_APP_IsStarted(void);
uint8_t ADC_APP_SetSampleRateHz(uint32_t sample_rate_hz);
uint32_t ADC_APP_GetSampleRateHz(void);

uint16_t ADC_APP_GetLatestRaw(void);
uint16_t ADC_APP_GetLatestMv(void);
uint16_t ADC_APP_GetAverageRaw(void);
uint16_t ADC_APP_GetAverageMv(void);
uint16_t ADC_APP_GetBufferSamples(void);
uint16_t ADC_APP_GetBlockSamples(void);

void ADC_APP_SetStreamEnabled(uint8_t enabled);
uint8_t ADC_APP_GetStreamEnabled(void);
uint8_t ADC_APP_SetStreamIntervalMs(uint16_t interval_ms);
uint16_t ADC_APP_GetStreamIntervalMs(void);

void ADC_APP_SetBlockStreamEnabled(uint8_t enabled);
uint8_t ADC_APP_GetBlockStreamEnabled(void);
uint32_t ADC_APP_GetDroppedBlockCount(void);
uint32_t ADC_APP_GetHalfEventCount(void);
uint32_t ADC_APP_GetFullEventCount(void);

uint32_t ADC_APP_GetLatestFrameSeq(void);
void ADC_APP_CopyLatestFrame(uint16_t *buffer, uint16_t max_samples);
uint8_t ADC_APP_PopBlock(adc_app_block_t *block);

void adc_proc(void);

#ifdef __cplusplus
}
#endif

#endif /* ADC_APP_H */
