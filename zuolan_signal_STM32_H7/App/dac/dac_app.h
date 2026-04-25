#ifndef DAC_APP_H
#define DAC_APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define DAC_APP_REFERENCE_MV 3300U
#define DAC_APP_MAX_RAW_VALUE 4095U
#define DAC_APP_WAVE_SAMPLES 256U

#define DAC_APP_DEFAULT_MODE DAC_APP_MODE_DC
#define DAC_APP_DEFAULT_AMP_MV 500U
#define DAC_APP_DEFAULT_OFFSET_MV 1650U
#define DAC_APP_DEFAULT_FREQ_HZ 1000U
#define DAC_APP_DEFAULT_DUTY_PERCENT 50U

typedef enum
{
    DAC_APP_MODE_DC = 0U,
    DAC_APP_MODE_SINE,
    DAC_APP_MODE_TRIANGLE,
    DAC_APP_MODE_SQUARE,
} dac_app_mode_t;

void DAC_APP_Init(void);
void DAC_APP_Start(void);
void DAC_APP_Stop(void);
uint8_t DAC_APP_IsStarted(void);
uint32_t DAC_APP_GetDmaFullCount(void);

void DAC_APP_SetMode(dac_app_mode_t mode);
dac_app_mode_t DAC_APP_GetMode(void);
const char *DAC_APP_GetModeString(void);

uint8_t DAC_APP_SetAmpMv(uint16_t amp_mv);
uint16_t DAC_APP_GetAmpMv(void);
uint16_t DAC_APP_GetAmpMaxMv(void);

uint8_t DAC_APP_SetOffsetMv(uint16_t offset_mv);
uint16_t DAC_APP_GetOffsetMv(void);
uint16_t DAC_APP_GetOffsetMinMv(void);
uint16_t DAC_APP_GetOffsetMaxMv(void);

uint8_t DAC_APP_SetFreqHz(uint32_t frequency_hz);
uint32_t DAC_APP_GetFreqHz(void);

void DAC_APP_SetDutyPercent(uint8_t duty_percent);
uint8_t DAC_APP_GetDutyPercent(void);

uint16_t DAC_APP_GetCurrentRaw(void);

#ifdef __cplusplus
}
#endif

#endif /* DAC_APP_H */
