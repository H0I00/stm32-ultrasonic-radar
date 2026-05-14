#ifndef __BEEP_H
#define __BEEP_H

#include "stm32f10x.h"

void BEEP_Init(void);
void BEEP_Alert(void);       // 长鸣1s警告
void BEEP_Success(void);     // 短鸣两声成功提示
void BEEP_ShortBeep(void);   // 单次短鸣

#endif

