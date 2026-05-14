#include "hcsr.h"
#include "Delay.h"

#define HCSR_STATE_IDLE         0
#define HCSR_STATE_WAIT_RISING  1
#define HCSR_STATE_WAIT_FALLING 2

#define HCSR_TIM_PRESCALER      (72 - 1)
#define HCSR_TIM_PERIOD         0xFFFF

static volatile uint8_t hcsr_capture_state = HCSR_STATE_IDLE;
static volatile uint8_t hcsr_data_ready = 0;
static volatile uint8_t hcsr_timeout = 0;
static volatile uint32_t hcsr_echo_time_us = 0;

static void HCSR_SetCapturePolarity(uint16_t polarity)
{
	if (polarity == TIM_ICPolarity_Falling)
	{
		HCSR_ECHO_TIM->CCER |= TIM_CCER_CC1P;
	}
	else
	{
		HCSR_ECHO_TIM->CCER &= (uint16_t)~TIM_CCER_CC1P;
	}
}

void HCSR_GPIO_Config(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_APB2PeriphClockCmd(HCSR_GPIO_CLK, ENABLE);

	GPIO_InitStructure.GPIO_Pin = HCSR_TRIG_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(HCSR_PORT, &GPIO_InitStructure);
	GPIO_ResetBits(HCSR_PORT, HCSR_TRIG_PIN);

	GPIO_InitStructure.GPIO_Pin = HCSR_ECHO_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(HCSR_PORT, &GPIO_InitStructure);
}

void HCSR_TIM2_InputCaptureConfig(void)
{
	TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
	TIM_ICInitTypeDef TIM_ICInitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

	RCC_APB1PeriphClockCmd(HCSR_ECHO_TIM_CLK, ENABLE);

	TIM_TimeBaseStructure.TIM_Period = HCSR_TIM_PERIOD;
	TIM_TimeBaseStructure.TIM_Prescaler = HCSR_TIM_PRESCALER;
	TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit(HCSR_ECHO_TIM, &TIM_TimeBaseStructure);

	TIM_ICInitStructure.TIM_Channel = TIM_Channel_1;
	TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Rising;
	TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;
	TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;
	TIM_ICInitStructure.TIM_ICFilter = 0x03;
	TIM_ICInit(HCSR_ECHO_TIM, &TIM_ICInitStructure);

	TIM_ClearFlag(HCSR_ECHO_TIM, TIM_FLAG_CC1 | TIM_FLAG_Update);
	TIM_ITConfig(HCSR_ECHO_TIM, TIM_IT_CC1 | TIM_IT_Update, ENABLE);

	NVIC_InitStructure.NVIC_IRQChannel = HCSR_ECHO_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	TIM_Cmd(HCSR_ECHO_TIM, ENABLE);
}

void HCSR_Init(void)
{
	HCSR_GPIO_Config();
	HCSR_TIM2_InputCaptureConfig();
	HCSR_ClearDataReady();
}

void HCSR_ClearDataReady(void)
{
	hcsr_capture_state = HCSR_STATE_IDLE;
	hcsr_data_ready = 0;
	hcsr_timeout = 0;
	hcsr_echo_time_us = 0;
	HCSR_SetCapturePolarity(TIM_ICPolarity_Rising);
	TIM_ClearFlag(HCSR_ECHO_TIM, TIM_FLAG_CC1 | TIM_FLAG_Update);
}

void HCSR_Trigger(void)
{
	HCSR_ClearDataReady();
	hcsr_capture_state = HCSR_STATE_WAIT_RISING;
	TIM_SetCounter(HCSR_ECHO_TIM, 0);

	GPIO_ResetBits(HCSR_PORT, HCSR_TRIG_PIN);
	Delay_us(2);
	GPIO_SetBits(HCSR_PORT, HCSR_TRIG_PIN);
	Delay_us(12);
	GPIO_ResetBits(HCSR_PORT, HCSR_TRIG_PIN);
}

uint8_t HCSR_IsDataReady(void)
{
	return hcsr_data_ready;
}

uint8_t HCSR_IsTimeout(void)
{
	return hcsr_timeout;
}

uint32_t HCSR_GetEchoTimeUs(void)
{
	return hcsr_echo_time_us;
}

float HCSR_GetDistanceCm(void)
{
	if (hcsr_data_ready == 0)
	{
		return 0.0f;
	}

	return ((float)hcsr_echo_time_us) * 0.0343f / 2.0f;
}

void Triger(void)
{
	HCSR_Trigger();
}

float Calculation(void)
{
	return HCSR_GetDistanceCm();
}

void TIM2_IRQHandler(void)
{
	if (TIM_GetITStatus(HCSR_ECHO_TIM, TIM_IT_CC1) != RESET)
	{
		uint16_t capture = TIM_GetCapture1(HCSR_ECHO_TIM);
		TIM_ClearITPendingBit(HCSR_ECHO_TIM, TIM_IT_CC1);

		if (hcsr_capture_state == HCSR_STATE_WAIT_RISING)
		{
			TIM_SetCounter(HCSR_ECHO_TIM, 0);
			HCSR_SetCapturePolarity(TIM_ICPolarity_Falling);
			hcsr_capture_state = HCSR_STATE_WAIT_FALLING;
		}
		else if (hcsr_capture_state == HCSR_STATE_WAIT_FALLING)
		{
			hcsr_echo_time_us = capture;
			hcsr_data_ready = 1;
			hcsr_timeout = 0;
			hcsr_capture_state = HCSR_STATE_IDLE;
			HCSR_SetCapturePolarity(TIM_ICPolarity_Rising);
		}
		else
		{
			HCSR_SetCapturePolarity(TIM_ICPolarity_Rising);
		}
	}

	if (TIM_GetITStatus(HCSR_ECHO_TIM, TIM_IT_Update) != RESET)
	{
		TIM_ClearITPendingBit(HCSR_ECHO_TIM, TIM_IT_Update);

		if (hcsr_capture_state != HCSR_STATE_IDLE)
		{
			hcsr_capture_state = HCSR_STATE_IDLE;
			hcsr_data_ready = 0;
			hcsr_timeout = 1;
			HCSR_SetCapturePolarity(TIM_ICPolarity_Rising);
		}
	}
}
