#ifndef _ENCODER_H
#define _ENCODER_H

#include "stm32f1xx.h"
#include <stdint.h>
#include <stdbool.h>
#include "HAL_GPIO.h"
#include "ILI9341.h"

// 1 — инвертировать направление, 0 — не инвертировать
#define ENCODER_REVERSE_DIRECTION   1   // поменяй на 0, если нужно вернуть обратно

// Для доступа к текущему значению энкодера
#define ENCODER_VALUE()   ((int16_t)(TIM2->CNT >> 1)) // >>2 т.к. 4 импульса на 1 шаг
#define ENCODER_RANGE()   ((int16_t)(TIM2->ARR))

void Encoder_Init(void);
int8_t Encoder_GetDirection(void);
uint8_t Button_GetEvent(void);   // 0-нет,1-короткий,2-двойной,3-длинный

#endif