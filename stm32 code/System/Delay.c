#include "Delay.h"

/**
  * @brief  微秒级延时
  * @param  us 延时时长，范围：0~233015
  * @retval 无
  */
void Delay_us(uint32_t us)
{
	if (us == 0) return;
	if (us > 233015) us = 233015;		//防止超过SysTick 24位上限
	SysTick->LOAD = 72 * us;				//设置定时器重装值
	SysTick->VAL = 0x00;					//清空当前计数值
	SysTick->CTRL = 0x00000005;				//设置时钟源为HCLK，启动定时器
	while(!(SysTick->CTRL & 0x00010000));	//等待计数到0
	SysTick->CTRL = 0x00000004;				//关闭定时器
}

/**
  * @brief  毫秒级延时
  * @param  ms 延时时长，范围：0~4294967295
  * @retval 无
  */
void Delay_ms(uint32_t ms)
{
	while(ms--)
	{
		Delay_us(1000);
	}
}
 
/**
  * @brief  秒级延时
  * @param  s 延时时长，范围：0~4294967295
  * @retval 无
  */
void Delay_s(uint32_t s)
{
	while(s--)
	{
		Delay_ms(1000);
	}
} 
