#ifndef DAC_APP_H
#define DAC_APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define DAC_APP_REFERENCE_MV 3300U
#define DAC_APP_MAX_RAW_VALUE 4095U
#define DAC_APP_DEFAULT_MV 1650U
#define DAC_APP_WAVE_SAMPLES 128U

typedef enum
{
    DAC_APP_WAVE_NONE = 0U,
    DAC_APP_WAVE_SINE,
    DAC_APP_WAVE_TRIANGLE,
    DAC_APP_WAVE_SQUARE,
} dac_app_waveform_t;

void DAC_APP_Init(void);
void DAC_APP_Start(void);
void DAC_APP_Stop(void);
uint8_t DAC_APP_IsStarted(void);
void DAC_APP_SetValueRaw(uint16_t raw_value);
uint16_t DAC_APP_GetValueRaw(void);
void DAC_APP_SetValueMv(uint16_t voltage_mv);
uint16_t DAC_APP_GetValueMv(void);
uint8_t DAC_APP_StartWave(dac_app_waveform_t waveform, uint32_t frequency_hz);
dac_app_waveform_t DAC_APP_GetWaveform(void);
uint32_t DAC_APP_GetWaveFrequencyHz(void);
const char *DAC_APP_GetModeString(void);

#ifdef __cplusplus
}
#endif

#endif /* DAC_APP_H */
