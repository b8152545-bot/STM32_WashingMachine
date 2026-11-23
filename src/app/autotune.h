#ifndef AUTOTUNE_H
#define AUTOTUNE_H

#include <stdint.h>
#include <stdbool.h>

#define NORMAL  0
#define REVERSE 1

// Логирование UART
#define AUTOTUNE_LOG_ENABLE 1
#define AUTOTUNE_LOG_PERIOD 100   // мс

// Инициализация тюнера
void AutoTune_Init(uint8_t direction, int steady, int step, uint32_t wait_ms,
                   float window, uint32_t pulse_ms, uint32_t period_ms);

// Передать текущее значение с датчика (RPM)
void AutoTune_SetInput(float input);

// Вызывать с периодом period_ms
void AutoTune_Compute(void);

// Получить текущий управляющий сигнал (задержка в мкс)
int AutoTune_GetOutput(void);

// Получить точность (0..100)
uint8_t AutoTune_GetAccuracy(void);

// Закончил ли тюнинг?
bool AutoTune_IsFinished(void);

// Получить коэффициенты (PI и PID)
void AutoTune_GetPI(float *kp, float *ki);
void AutoTune_GetPID(float *kp, float *ki, float *kd);

// Сбросить состояние
void AutoTune_Reset(void);

// Получить текущее состояние (0-3)
uint8_t AutoTune_GetState(void);

#endif