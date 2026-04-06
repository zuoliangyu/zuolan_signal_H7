#ifndef DAC_APP_H
#define DAC_APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define DAC_APP_REFERENCE_MV 3300U
#define DAC_APP_MAX_RAW_VALUE 4095U
#define DAC_APP_DEFAULT_MV 1650U

void DAC_APP_Init(void);
void DAC_APP_Start(void);
void DAC_APP_Stop(void);
uint8_t DAC_APP_IsStarted(void);
void DAC_APP_SetValueRaw(uint16_t raw_value);
uint16_t DAC_APP_GetValueRaw(void);
void DAC_APP_SetValueMv(uint16_t voltage_mv);
uint16_t DAC_APP_GetValueMv(void);

#ifdef __cplusplus
}
#endif

#endif /* DAC_APP_H */
