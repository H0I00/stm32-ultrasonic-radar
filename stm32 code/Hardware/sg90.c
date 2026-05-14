/* sg90.c */
#include "sg90.h"
#include "stm32f10x_tim.h"

#define PWM_PERIOD 20000  // 20ms���ڣ�50Hz��
#define MIN_PULSE 500     // 0�ȶ�Ӧ0.5ms����
#define MAX_PULSE 2500    // 180�ȶ�Ӧ2.5ms����

void SG90_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStruct;
    TIM_OCInitTypeDef TIM_OCInitStruct;
    
    // 1. Enable GPIOB and TIM3 clocks
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
    
    // 2. Configure PB0 as TIM3_CH3 alternate function output
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStruct);
    
    // 3. Configure TIM3 as 50Hz PWM timebase
    TIM_TimeBaseStruct.TIM_Prescaler = 72 - 1;  // 72MHz/72 = 1MHz
    TIM_TimeBaseStruct.TIM_Period = PWM_PERIOD - 1;  // 20ms����
    TIM_TimeBaseStruct.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseStruct.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStruct);
    
    // 4. ����PWMģʽ
    TIM_OCInitStruct.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStruct.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStruct.TIM_Pulse = MIN_PULSE;  // ��ʼ�Ƕ�0��
    TIM_OCInitStruct.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC3Init(TIM3, &TIM_OCInitStruct);
    
    // 5. ʹ��Ԥװ�غͶ�ʱ��
    TIM_OC3PreloadConfig(TIM3, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(TIM3, ENABLE);
    TIM_Cmd(TIM3, ENABLE);
}

void SG90_SetAngle(uint8_t angle) {
    if (angle > 180) angle = 180;
    uint16_t pulse = MIN_PULSE + (angle * (MAX_PULSE - MIN_PULSE)) / 180;
    TIM_SetCompare3(TIM3, pulse);
}
