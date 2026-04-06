#ifndef ADC_APP_H
#define ADC_APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define ADC_APP_REFERENCE_MV 3300U
#define ADC_APP_MAX_RAW_VALUE 65535U
#define ADC_APP_DMA_SAMPLES 64U
#define ADC_APP_MIN_STREAM_INTERVAL_MS 2U
#define ADC_APP_DEFAULT_STREAM_INTERVAL_MS 20U

void ADC_APP_Init(void);
uint8_t ADC_APP_IsStarted(void);

uint16_t ADC_APP_GetLatestRaw(void);
uint16_t ADC_APP_GetLatestMv(void);
uint16_t ADC_APP_GetAverageRaw(void);
uint16_t ADC_APP_GetAverageMv(void);
uint16_t ADC_APP_GetBufferSamples(void);

void ADC_APP_SetStreamEnabled(uint8_t enabled);
uint8_t ADC_APP_GetStreamEnabled(void);
uint8_t ADC_APP_SetStreamIntervalMs(uint16_t interval_ms);
uint16_t ADC_APP_GetStreamIntervalMs(void);

void adc_proc(void);

#ifdef __cplusplus
}
#endif

#endif /* ADC_APP_H */
