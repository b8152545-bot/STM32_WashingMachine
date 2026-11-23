#ifndef PRESSOSTAT_H
#define PRESSOSTAT_H

#include <stdint.h>
#include <stdbool.h>


// Инициализация прессостата (настройка пина, таймера)
void Pressostat_Init(void);
uint8_t Pressostat_GetLevel(void);
bool Pressostat_IsFaulty(void);
void Pressostat_OnCapture(uint16_t capture);


#endif