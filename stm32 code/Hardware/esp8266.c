#include "esp8266.h"
#include "uart.h"
#include "Delay.h"
#include "OLED.h"

#include <stdio.h>
#include <string.h>

#define ESP8266_WIFI_INFO "AT+CWJAP=\"ciallo\",\"0d000721\"\r\n"
#define WINDOWS_IP        "AT+CIPSTART=\"TCP\",\"<windows-ip>\",8089\r\n"
#define ESP8266_STEP_MAX_RETRY 50

unsigned char esp8266_buf[512];
volatile unsigned short esp8266_cnt = 0;
static unsigned short esp8266_cntPre = 0;
volatile uint8_t esp8266_cmd_flag = 0;

static void Usart_SendString(USART_TypeDef *USARTx, unsigned char *str, unsigned short len)
{
	unsigned short count;

	for (count = 0; count < len; count++)
	{
		USART_SendData(USARTx, *str++);
		while (USART_GetFlagStatus(USARTx, USART_FLAG_TC) == RESET)
		{
		}
	}
}

void ESP8266_Clear(void)
{
	memset(esp8266_buf, 0, sizeof(esp8266_buf));
	esp8266_cnt = 0;
	esp8266_cntPre = 0;
}

static _Bool ESP8266_WaitRecive(void)
{
	if (esp8266_cnt == 0)
	{
		return REV_WAIT;
	}

	if (esp8266_cnt == esp8266_cntPre)
	{
		esp8266_cnt = 0;
		esp8266_cntPre = 0;
		return REV_OK;
	}

	esp8266_cntPre = esp8266_cnt;
	return REV_WAIT;
}

_Bool ESP8266_SendCmd(char *cmd, char *res)
{
	unsigned char timeOut = 200;

	Usart_SendString(USART1, (unsigned char *)cmd, strlen(cmd));

	while (timeOut--)
	{
		if (ESP8266_WaitRecive() == REV_OK)
		{
			if (strstr((const char *)esp8266_buf, res) != NULL)
			{
				ESP8266_Clear();
				return 0;
			}
		}

		Delay_ms(10);
	}

	return 1;
}

void ESP8266_SendData(unsigned char *data, unsigned short len)
{
	char cmdBuf[32];

	ESP8266_Clear();
	sprintf(cmdBuf, "AT+CIPSEND=%d\r\n", len);
	if (!ESP8266_SendCmd(cmdBuf, ">"))
	{
		Usart_SendString(USART1, data, len);
	}
}

unsigned char *ESP8266_GetIPD(unsigned short timeOut)
{
	char *ptrIPD = NULL;

	while (1)
	{
		if (ESP8266_WaitRecive() == REV_OK)
		{
			ptrIPD = strstr((char *)esp8266_buf, "IPD,");
			if (ptrIPD != NULL)
			{
				ptrIPD = strchr(ptrIPD, ':');
				if (ptrIPD != NULL)
				{
					return (unsigned char *)(ptrIPD + 1);
				}

				return NULL;
			}
		}

		if (timeOut == 0)
		{
			break;
		}

		Delay_ms(5);
		timeOut--;
	}

	return NULL;
}

void ESP8266_Init(void)
{
	UART1_Init(115200);
	ESP8266_Clear();

	while (ESP8266_SendCmd("AT\r\n", "OK"))
	{
		Delay_ms(1000);
	}

	while (ESP8266_SendCmd("AT+CWMODE=1\r\n", "OK"))
	{
		Delay_ms(500);
	}

	while (ESP8266_SendCmd("AT+CWDHCP=1,1\r\n", "OK"))
	{
		Delay_ms(500);
	}

	while (ESP8266_SendCmd(ESP8266_WIFI_INFO, "GOT IP"))
	{
		Delay_ms(500);
	}

	while (ESP8266_SendCmd("AT+CIPMODE=0\r\n", "OK"))
	{
		Delay_ms(500);
	}

	while (ESP8266_SendCmd(WINDOWS_IP, "CONNECT"))
	{
		Delay_ms(500);
	}
}

static void ESP8266_ShowStep(char *step, uint8_t retry)
{
	OLED_Clear();
	OLED_ShowString(1, 1, "ESP8266 Init");
	OLED_ShowString(2, 1, step);
	OLED_ShowString(3, 1, "Retry:");
	OLED_ShowNum(3, 7, retry, 3);
}

static void ESP8266_ShowStepOK(char *step)
{
	OLED_Clear();
	OLED_ShowString(1, 1, "ESP8266 OK");
	OLED_ShowString(2, 1, step);
	Delay_ms(300);
}

static void ESP8266_WaitCmdWithOLED(char *step, char *cmd, char *res)
{
	uint8_t retry = 0;

	while (ESP8266_SendCmd(cmd, res))
	{
		retry++;
		ESP8266_ShowStep(step, retry);
		Delay_ms(500);

		if (retry >= ESP8266_STEP_MAX_RETRY)
		{
			OLED_Clear();
			OLED_ShowString(1, 1, "ESP8266 FAIL");
			OLED_ShowString(2, 1, step);
			OLED_ShowString(3, 1, "Check wiring/IP");
			retry = 0;
			Delay_ms(2000);
		}
	}

	ESP8266_ShowStepOK(step);
}

void ESP8266_InitWithOLED(void)
{
	UART1_Init(115200);
	ESP8266_Clear();

	ESP8266_WaitCmdWithOLED("1. AT", "AT\r\n", "OK");
	ESP8266_WaitCmdWithOLED("2. CWMODE", "AT+CWMODE=1\r\n", "OK");
	ESP8266_WaitCmdWithOLED("3. DHCP", "AT+CWDHCP=1,1\r\n", "OK");
	ESP8266_WaitCmdWithOLED("4. WIFI", ESP8266_WIFI_INFO, "GOT IP");
	ESP8266_WaitCmdWithOLED("5. CIPMODE", "AT+CIPMODE=0\r\n", "OK");
	ESP8266_WaitCmdWithOLED("6. TCP", WINDOWS_IP, "CONNECT");

	OLED_Clear();
	OLED_ShowString(1, 1, "ESP8266 Ready");
	Delay_ms(500);
}

void USART1_IRQHandler(void)
{
	if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
	{
		if (esp8266_cnt >= sizeof(esp8266_buf))
		{
			esp8266_cnt = 0;
		}

		esp8266_buf[esp8266_cnt++] = USART_ReceiveData(USART1);
		USART_ClearITPendingBit(USART1, USART_IT_RXNE);
	}
}

void ESP8266_CheckCommand(void)
{
	if (strstr((char *)esp8266_buf, "pic") != NULL)
	{
		esp8266_cmd_flag = 1;
		ESP8266_Clear();
	}
}
