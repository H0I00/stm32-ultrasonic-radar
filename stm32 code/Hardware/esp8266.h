#ifndef _ESP8266_H_
#define _ESP8266_H_

#include "stm32f10x.h"





#define REV_OK		0
#define REV_WAIT	1

void ESP8266_Init(void);
void ESP8266_InitWithOLED(void);

void ESP8266_Clear(void);

_Bool ESP8266_SendCmd(char *cmd, char *res);

void ESP8266_SendData(unsigned char *data, unsigned short len);

unsigned char *ESP8266_GetIPD(unsigned short timeOut);

extern volatile uint8_t esp8266_cmd_flag;

void ESP8266_CheckCommand(void);


#endif
