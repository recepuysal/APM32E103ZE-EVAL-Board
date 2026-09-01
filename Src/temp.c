/**
 ******************************************************************************
 * @file    temp.c
 * @brief   APM32E103ZE internal die temperature sensor body.
 *
 * ADC1 channel 16 is the internal temperature sensor (ADC_CHANNEL_TEMP_SENSOR
 * / ADC_EnableTempSensorVrefint), same as STM32F103 (APM32E103 is
 * pin/register compatible). This sensor type has no factory calibration
 * register (unlike newer STM32 lines with TS_CAL1/TS_CAL2) - the datasheet's
 * "typical" V25/Avg_Slope constants can be tens of degC off per chip due to
 * manufacturing spread (confirmed on this board: read ~8 degC at room
 * temperature with the raw typical constants).
 *
 * Fix: single-point runtime calibration in Temp_Init() - assume the board is
 * at TEMP_CAL_REF_C (~25 degC, typical indoor ambient) at power-up and use
 * the very first averaged reading as this specific chip's V25 baseline.
 * Avg_Slope is left at the typical constant (process-family slope varies far
 * less than the absolute offset), so temperature deltas from that baseline
 * still track correctly.
 ******************************************************************************
 */

#include "temp.h"
#include "apm32e10x.h"
#include "apm32e10x_adc.h"
#include "apm32e10x_rcm.h"

#define TEMP_V25_MV      1430.0f
#define TEMP_SLOPE_MVC   4.3f
#define TEMP_VREF_MV     3300.0f
#define TEMP_CAL_REF_C   25.0f
#define TEMP_CAL_SAMPLES 8U

static float s_v25CalibratedMv = TEMP_V25_MV;

static uint16_t ReadRawSample(void)
{
	ADC_ConfigRegularChannel(ADC1, ADC_CHANNEL_TEMP_SENSOR, 1, ADC_SAMPLETIME_239CYCLES5);
	ADC_EnableSoftwareStartConv(ADC1);

	while (ADC_ReadStatusFlag(ADC1, ADC_FLAG_EOC) == RESET)
	{
	}

	return ADC_ReadConversionValue(ADC1);
}

void Temp_Init(void)
{
	ADC_Config_T adcConfig;
	uint32_t sum = 0U;

	RCM_ConfigADCCLK(RCM_PCLK2_DIV_6); /* 72MHz / 6 = 12MHz, under the 14MHz ADC max */
	RCM_EnableAPB2PeriphClock(RCM_APB2_PERIPH_ADC1);

	ADC_ConfigStructInit(&adcConfig);
	adcConfig.mode = ADC_MODE_INDEPENDENT;
	adcConfig.scanConvMode = DISABLE;
	adcConfig.continuosConvMode = DISABLE;
	adcConfig.externalTrigConv = ADC_EXT_TRIG_CONV_None;
	adcConfig.dataAlign = ADC_DATA_ALIGN_RIGHT;
	adcConfig.nbrOfChannel = 1;
	ADC_Config(ADC1, &adcConfig);

	ADC_EnableTempSensorVrefint(ADC1);
	ADC_Enable(ADC1);

	ADC_ResetCalibration(ADC1);
	while (ADC_ReadResetCalibrationStatus(ADC1))
	{
	}

	ADC_StartCalibration(ADC1);
	while (ADC_ReadCalibrationStartFlag(ADC1))
	{
	}

	/* Let the temp sensor's internal buffer settle after being enabled. */
	for (uint8_t i = 0; i < 4U; i++)
	{
		(void)ReadRawSample();
	}

	for (uint8_t i = 0; i < TEMP_CAL_SAMPLES; i++)
	{
		sum += ReadRawSample();
	}

	s_v25CalibratedMv = (float)(sum / TEMP_CAL_SAMPLES) * TEMP_VREF_MV / 4095.0f;
}

float Temp_ReadCelsius(void)
{
	uint16_t raw = ReadRawSample();
	float millivolts = (float)raw * TEMP_VREF_MV / 4095.0f;

	return (s_v25CalibratedMv - millivolts) / TEMP_SLOPE_MVC + TEMP_CAL_REF_C;
}
