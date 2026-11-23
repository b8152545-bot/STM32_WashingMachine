#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include <stdint.h>
#include "pid.h"   

#define ENABLE_UART_LOG   1

// Ограничения задержки (мкс) – доступны для всех модулей
#define TRIAC_DELAY_MIN  500
#define TRIAC_DELAY_MAX  9500

void MotorControl_Init(void);
void MotorControl_SetRPM(int32_t target_rpm, uint32_t ramp_rate);
void MotorControl_Task(void);   // вызывать периодически
void MotorControl_ApplyPercent(int32_t percent);
// НОВАЯ ФУНКЦИЯ: запуск двигателя с начальной мощностью и задержкой
void MotorControl_StartMotor(int32_t target_rpm, uint32_t ramp_rate, uint8_t startup_power_percent, uint32_t startup_delay_ms);

// Для отладки – доступ к структуре PID
extern PID_Handle motor_pid;

#endif