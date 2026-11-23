#ifndef ZERO_CROSS_H
#define ZERO_CROSS_H

#include "hardware.h"
#include "port_mapping.h"
#include "stm32f103xx_CMSIS.h"

#define ZERO_CROSS_TIMEOUT_MS   30
#define MIN_ZC_INTERVAL_US   5000    // 5 мс (фильтр дребезга)

// Раскомментировать для отладки
// #define DEBUG_ZC

void ZeroCross_Init(void);
void CheckPowerFailure(void);
uint16_t HW_GetRPM(void);

extern volatile uint32_t last_zero_cross_time;
extern volatile uint8_t  power_failure_saved;

#ifdef DEBUG_ZC
uint16_t ZeroCross_GetFrequency(void);
#endif

#endif