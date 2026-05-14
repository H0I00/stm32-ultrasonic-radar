#include "beep.h"
#include "Delay.h"

#define BEEP_PIN GPIO_Pin_4
#define BEEP_PORT GPIOA

// 蜂鸣器初始化
void BEEP_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    
    GPIO_InitStructure.GPIO_Pin = BEEP_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(BEEP_PORT, &GPIO_InitStructure);
    
    GPIO_SetBits(BEEP_PORT, BEEP_PIN); // 初始状态关闭
}

// 长鸣1s警告
void BEEP_Alert(void)
{
    GPIO_ResetBits(BEEP_PORT, BEEP_PIN);
    Delay_ms(1000);
    GPIO_SetBits(BEEP_PORT, BEEP_PIN);
}

// 成功提示（短鸣两声）
void BEEP_Success(void)
{
    for(uint8_t i=0; i<2; i++){
        GPIO_ResetBits(BEEP_PORT, BEEP_PIN);
        Delay_ms(200);
        GPIO_SetBits(BEEP_PORT, BEEP_PIN);
        Delay_ms(100);
    }
}

// 单次短鸣
void BEEP_ShortBeep(void)
{
    GPIO_ResetBits(BEEP_PORT, BEEP_PIN);
    Delay_ms(200);
    GPIO_SetBits(BEEP_PORT, BEEP_PIN);
}

