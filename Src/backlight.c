/**
 ******************************************************************************
 * @file    backlight.c
 * @brief   LCD backlight PWM + potentiometer body.
 *
 * Confirmed from the official schematic (APM32E103ZE_EVALBOARD_V1.0,
 * "Display&Button" sheet): RV1 (10K pot) wiper -> PC0 (ADC12_IN10), and the
 * LCD backlight pin is PA8 -> TIM1_CH1 (fixed STM32F1-style channel, no AF
 * remap needed). Shares ADC1 with temp.c (single-conversion-per-call
 * pattern) - Temp_Init() must run first to configure/enable/calibrate ADC1.
 ******************************************************************************
 */

#include "backlight.h"
#include "apm32e10x.h"
#include "apm32e10x_gpio.h"
#include "apm32e10x_rcm.h"
#include "apm32e10x_tmr.h"
#include "apm32e10x_adc.h"

#define BL_TMR       TMR1
#define BL_PORT      GPIOA
#define BL_PIN       GPIO_PIN_8

#define POT_PORT     GPIOC
#define POT_PIN      GPIO_PIN_0
#define POT_CHANNEL  ADC_CHANNEL_10

#define PWM_PERIOD   999U   /* 72MHz / 72 / 1000 = 1kHz PWM, 1000 duty steps */
#define PWM_MIN_DUTY 20U    /* never fully off - a 0% reading still shows something */

#define POT_POLL_MS  50U

extern volatile uint32_t g_tickMs;

void Backlight_Init(void)
{
	GPIO_Config_T gpioConfig;
	TMR_BaseConfig_T baseConfig;
	TMR_OCConfig_T ocConfig;

	RCM_EnableAPB2PeriphClock(RCM_APB2_PERIPH_GPIOA | RCM_APB2_PERIPH_GPIOC | RCM_APB2_PERIPH_TMR1);

	gpioConfig.pin = BL_PIN;
	gpioConfig.mode = GPIO_MODE_AF_PP;
	gpioConfig.speed = GPIO_SPEED_50MHz;
	GPIO_Config(BL_PORT, &gpioConfig);

	gpioConfig.pin = POT_PIN;
	gpioConfig.mode = GPIO_MODE_ANALOG;
	GPIO_Config(POT_PORT, &gpioConfig);

	baseConfig.countMode = TMR_COUNTER_MODE_UP;
	baseConfig.clockDivision = TMR_CLOCK_DIV_1;
	baseConfig.period = PWM_PERIOD;
	baseConfig.division = 71U; /* 72MHz / (71+1) = 1MHz timer clock */
	baseConfig.repetitionCounter = 0U;
	TMR_ConfigTimeBase(BL_TMR, &baseConfig);

	ocConfig.mode = TMR_OC_MODE_PWM1;
	ocConfig.outputState = TMR_OC_STATE_ENABLE;
	ocConfig.outputNState = TMR_OC_NSTATE_DISABLE;
	ocConfig.polarity = TMR_OC_POLARITY_HIGH;
	ocConfig.nPolarity = TMR_OC_NPOLARITY_HIGH;
	ocConfig.idleState = TMR_OC_IDLE_STATE_RESET;
	ocConfig.nIdleState = TMR_OC_NIDLE_STATE_RESET;
	ocConfig.pulse = PWM_PERIOD; /* full brightness until the first pot read */
	TMR_ConfigOC1(BL_TMR, &ocConfig);

	TMR_Enable(BL_TMR);
	TMR_EnablePWMOutputs(BL_TMR); /* MOE - required for TMR1/TMR8 advanced-timer outputs */
}

void Backlight_Poll(void)
{
	static uint32_t lastPoll = 0U;
	uint16_t raw;
	uint32_t duty;

	if ((g_tickMs - lastPoll) < POT_POLL_MS)
	{
		return;
	}
	lastPoll = g_tickMs;

	ADC_ConfigRegularChannel(ADC1, POT_CHANNEL, 1, ADC_SAMPLETIME_239CYCLES5);
	ADC_EnableSoftwareStartConv(ADC1);

	while (ADC_ReadStatusFlag(ADC1, ADC_FLAG_EOC) == RESET)
	{
	}

	raw = ADC_ReadConversionValue(ADC1);
	duty = PWM_MIN_DUTY + ((uint32_t)raw * (PWM_PERIOD - PWM_MIN_DUTY)) / 4095U;

	TMR_ConfigCompare1(BL_TMR, (uint16_t)duty);
}
