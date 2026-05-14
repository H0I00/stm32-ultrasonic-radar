#include "stm32f10x.h"
#include "Delay.h"
#include "OLED.h"
#include "sg90.h"
#include "hcsr.h"
#include "esp8266.h"

#include <stdio.h>
#include <string.h>

#define RADAR_SCAN_START_DEG       0
#define RADAR_SCAN_END_DEG         180
#define RADAR_SCAN_STEP_DEG        5
#define RADAR_SERVO_SETTLE_MS      90
#define RADAR_ECHO_WAIT_MS         70
#define RADAR_ECHO_QUIET_MS        60
#define RADAR_SERVO_RETURN_MS      700
#define RADAR_MAX_VALID_ECHO_US    30000
#define RADAR_POLL_STEP_MS         5

static uint16_t radar_windows_nearest_cm = 0;
static uint16_t radar_obstacle_count = 0;

static uint8_t Radar_ParseUint16(const char **text, uint16_t *value)
{
	uint32_t result = 0;
	uint8_t hasDigit = 0;

	while (**text >= '0' && **text <= '9')
	{
		hasDigit = 1;
		result = result * 10 + (uint32_t)(**text - '0');
		if (result > 65535)
		{
			result = 65535;
		}
		(*text)++;
	}

	if (!hasDigit)
	{
		return 0;
	}

	*value = (uint16_t)result;
	return 1;
}

static uint8_t Radar_ParseWindowsFeedback(const char *data, uint16_t *nearest_cm, uint16_t *obstacle_count)
{
	const char *p = data;

	while (*p != '\0')
	{
		if ((*p == 'R' || *p == 'W') && *(p + 1) == ',')
		{
			p += 2;
			if (!Radar_ParseUint16(&p, nearest_cm))
			{
				return 0;
			}

			if (*p != ',')
			{
				return 0;
			}
			p++;

			return Radar_ParseUint16(&p, obstacle_count);
		}

		p++;
	}

	return 0;
}

static void Radar_UpdateWindowsSummary(uint16_t nearest_cm, uint16_t obstacle_count)
{
	if (nearest_cm > 0 && (radar_windows_nearest_cm == 0 || nearest_cm < radar_windows_nearest_cm))
	{
		radar_windows_nearest_cm = nearest_cm;
	}

	if ((uint32_t)radar_obstacle_count + obstacle_count > 9999)
	{
		radar_obstacle_count = 9999;
	}
	else
	{
		radar_obstacle_count += obstacle_count;
	}
}

static void Radar_PollWindowsFeedback(void)
{
	unsigned char *payload = ESP8266_GetIPD(0);
	uint16_t nearest_cm = 0;
	uint16_t obstacle_count = 0;

	if (payload == NULL)
	{
		return;
	}

	if (Radar_ParseWindowsFeedback((const char *)payload, &nearest_cm, &obstacle_count))
	{
		Radar_UpdateWindowsSummary(nearest_cm, obstacle_count);
	}

	ESP8266_Clear();
}

static void Radar_DelayAndPoll(uint16_t ms)
{
	while (ms >= RADAR_POLL_STEP_MS)
	{
		Delay_ms(RADAR_POLL_STEP_MS);
		Radar_PollWindowsFeedback();
		ms -= RADAR_POLL_STEP_MS;
	}

	if (ms > 0)
	{
		Delay_ms(ms);
		Radar_PollWindowsFeedback();
	}
}

static uint16_t Radar_MeasureDistanceCm(void)
{
	uint16_t elapsed_ms = 0;
	uint32_t echo_us;

	HCSR_Trigger();

	while (!HCSR_IsDataReady() && !HCSR_IsTimeout() && elapsed_ms < RADAR_ECHO_WAIT_MS)
	{
		Radar_DelayAndPoll(RADAR_POLL_STEP_MS);
		elapsed_ms += RADAR_POLL_STEP_MS;
	}

	if (!HCSR_IsDataReady())
	{
		return 0;
	}

	echo_us = HCSR_GetEchoTimeUs();
	if (echo_us == 0 || echo_us > RADAR_MAX_VALID_ECHO_US)
	{
		return 0;
	}

	return (uint16_t)((echo_us + 29) / 58);
}

static void Radar_SendPolarData(uint8_t angle, uint16_t distance_cm)
{
	char message[24];

	sprintf(message, "P,%u,%u\r\n", (unsigned int)angle, (unsigned int)distance_cm);
	ESP8266_SendData((unsigned char *)message, (unsigned short)strlen(message));
}

static void Radar_ShowStatus(uint8_t angle, uint16_t distance_cm, uint8_t timeout)
{
	char line[17];
	uint16_t nearest = radar_windows_nearest_cm;
	uint16_t count = radar_obstacle_count;

	if (distance_cm > 999) distance_cm = 999;
	if (nearest > 999) nearest = 999;
	if (count > 9999) count = 9999;

	sprintf(line, "A:%3u D:%3u   ", (unsigned int)angle, (unsigned int)distance_cm);
	OLED_ShowString(0, 0, line, OLED_8X16);

	sprintf(line, "Min:%3u Obs:%4u", (unsigned int)nearest, (unsigned int)count);
	OLED_ShowString(0, 16, line, OLED_8X16);

	if (timeout)
	{
		OLED_ShowString(0, 32, "HCSR:Timeout   ", OLED_8X16);
	}
	else
	{
		sprintf(line, "Echo:%5lu us  ", (unsigned long)HCSR_GetEchoTimeUs());
		OLED_ShowString(0, 32, line, OLED_8X16);
	}

	OLED_ShowString(0, 48, "Mode:Scanning  ", OLED_8X16);
	OLED_Update();
}

static void Radar_ResetSweepSummary(void)
{
	radar_windows_nearest_cm = 0;
	radar_obstacle_count = 0;
}

int main(void)
{
	uint8_t angle;
	uint16_t distance_cm;
	uint8_t timeout;

	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

	Delay_ms(300);
	OLED_Init();
	OLED_Clear();
	OLED_ShowString(0, 0, "BOOT OK", OLED_8X16);
	OLED_ShowString(0, 16, "OLED OK", OLED_8X16);
	OLED_Update();
	Delay_ms(500);

	OLED_Clear();
	OLED_ShowString(0, 0, "Init SG90...", OLED_8X16);
	OLED_Update();
	SG90_Init();
	OLED_ShowString(0, 16, "SG90 OK", OLED_8X16);
	OLED_Update();
	Delay_ms(500);

	OLED_Clear();
	OLED_ShowString(0, 0, "Init HCSR...", OLED_8X16);
	OLED_Update();
	HCSR_Init();
	OLED_ShowString(0, 16, "HCSR OK", OLED_8X16);
	OLED_Update();
	Delay_ms(500);

	OLED_Clear();
	OLED_ShowString(0, 0, "Init ESP8266...", OLED_8X16);
	OLED_Update();
	ESP8266_InitWithOLED();

	OLED_Clear();
	OLED_ShowString(0, 0, "UltrasonicRadar", OLED_8X16);
	OLED_ShowString(0, 16, "ESP8266 Ready  ", OLED_8X16);
	OLED_Update();
	Radar_DelayAndPoll(500);

	while (1)
	{
		Radar_ResetSweepSummary();

		for (angle = RADAR_SCAN_START_DEG; angle <= RADAR_SCAN_END_DEG; angle += RADAR_SCAN_STEP_DEG)
		{
			SG90_SetAngle(angle);
			Radar_DelayAndPoll(RADAR_SERVO_SETTLE_MS);

			distance_cm = Radar_MeasureDistanceCm();
			timeout = HCSR_IsTimeout() || !HCSR_IsDataReady();

			Radar_SendPolarData(angle, distance_cm);
			Radar_PollWindowsFeedback();
			Radar_ShowStatus(angle, distance_cm, timeout);

			Radar_DelayAndPoll(RADAR_ECHO_QUIET_MS);
		}

		SG90_SetAngle(RADAR_SCAN_START_DEG);
		Radar_DelayAndPoll(RADAR_SERVO_RETURN_MS);
	}
}
