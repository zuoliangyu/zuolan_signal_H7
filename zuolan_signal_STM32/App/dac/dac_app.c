#include "dac_app.h"

#include "dac.h"
#include "main.h"

static uint16_t s_dac_raw_value = 0U;
static uint8_t s_dac_started = 0U;
static uint16_t s_dac_voltage_mv = 0U;

static uint16_t DAC_APP_ClampRawValue(uint16_t raw_value)
{
    if (raw_value > DAC_APP_MAX_RAW_VALUE)
    {
        return DAC_APP_MAX_RAW_VALUE;
    }

    return raw_value;
}

static uint16_t DAC_APP_ClampVoltageMv(uint16_t voltage_mv)
{
    if (voltage_mv > DAC_APP_REFERENCE_MV)
    {
        return DAC_APP_REFERENCE_MV;
    }

    return voltage_mv;
}

static uint16_t DAC_APP_RawToMv(uint16_t raw_value)
{
    uint32_t numerator;

    raw_value = DAC_APP_ClampRawValue(raw_value);
    numerator = ((uint32_t)raw_value * (uint32_t)DAC_APP_REFERENCE_MV) +
                ((uint32_t)DAC_APP_MAX_RAW_VALUE / 2U);
    return (uint16_t)(numerator / (uint32_t)DAC_APP_MAX_RAW_VALUE);
}

static uint16_t DAC_APP_MvToRaw(uint16_t voltage_mv)
{
    uint32_t numerator;

    voltage_mv = DAC_APP_ClampVoltageMv(voltage_mv);
    numerator = ((uint32_t)voltage_mv * (uint32_t)DAC_APP_MAX_RAW_VALUE) +
                ((uint32_t)DAC_APP_REFERENCE_MV / 2U);
    return (uint16_t)(numerator / (uint32_t)DAC_APP_REFERENCE_MV);
}

void DAC_APP_Start(void)
{
    if (s_dac_started != 0U)
    {
        return;
    }

    if (HAL_DAC_Start(&hdac1, DAC_CHANNEL_1) != HAL_OK)
    {
        Error_Handler();
    }

    s_dac_started = 1U;
}

void DAC_APP_Stop(void)
{
    if (s_dac_started == 0U)
    {
        return;
    }

    if (HAL_DAC_Stop(&hdac1, DAC_CHANNEL_1) != HAL_OK)
    {
        Error_Handler();
    }

    s_dac_started = 0U;
}

uint8_t DAC_APP_IsStarted(void)
{
    return s_dac_started;
}

void DAC_APP_SetValueRaw(uint16_t raw_value)
{
    s_dac_raw_value = DAC_APP_ClampRawValue(raw_value);
    s_dac_voltage_mv = DAC_APP_RawToMv(s_dac_raw_value);

    if (HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R, s_dac_raw_value) != HAL_OK)
    {
        Error_Handler();
    }
}

uint16_t DAC_APP_GetValueRaw(void)
{
    return s_dac_raw_value;
}

void DAC_APP_SetValueMv(uint16_t voltage_mv)
{
    voltage_mv = DAC_APP_ClampVoltageMv(voltage_mv);
    DAC_APP_SetValueRaw(DAC_APP_MvToRaw(voltage_mv));
}

uint16_t DAC_APP_GetValueMv(void)
{
    return s_dac_voltage_mv;
}

void DAC_APP_Init(void)
{
    s_dac_raw_value = 0U;
    s_dac_started = 0U;
    s_dac_voltage_mv = 0U;

    DAC_APP_Start();
    DAC_APP_SetValueMv(DAC_APP_DEFAULT_MV);
}
