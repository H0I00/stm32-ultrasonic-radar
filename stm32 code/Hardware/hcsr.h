#ifndef HCSR_H
#define HCSR_H

#include "stm32f10x.h"

#define HCSR_GPIO_CLK      RCC_APB2Periph_GPIOA
#define HCSR_PORT          GPIOA
#define HCSR_TRIG_PIN      GPIO_Pin_1
#define HCSR_ECHO_PIN      GPIO_Pin_0
#define HCSR_ECHO_TIM      TIM2
#define HCSR_ECHO_TIM_CLK  RCC_APB1Periph_TIM2
#define HCSR_ECHO_IRQn     TIM2_IRQn

void HCSR_Init(void);
void HCSR_GPIO_Config(void);
void HCSR_TIM2_InputCaptureConfig(void);
void HCSR_Trigger(void);
uint8_t HCSR_IsDataReady(void);
uint8_t HCSR_IsTimeout(void);
uint32_t HCSR_GetEchoTimeUs(void);
float HCSR_GetDistanceCm(void);
void HCSR_ClearDataReady(void);

/* Legacy names kept for existing test code. */
void Triger(void);
float Calculation(void);


#endif
