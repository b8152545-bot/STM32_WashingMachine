/*
**		ipaSoft Electronics (c) 2024
**		https://ipasoft.info
**		Альтернативные RTC для STM32F10x
*/

#ifndef STM32F1_RTC_
#define STM32F1_RTC_

#include "rtc_unit.h"

uint8_t 	rtc_Init(void);
RTC_UNIT rtc_GetTime(void);
void rtc_SetTime(RTC_UNIT dt);
void ResetStructDateTime(RTC_UNIT * unit);

#endif