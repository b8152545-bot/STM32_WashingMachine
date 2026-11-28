#ifndef MOTOR_H
#define MOTOR_H

#include "hardware.h"
#include "port_mapping.h"
#include "stm32f103xx_CMSIS.h"
#include "motor_control.h"   // берём TRIAC_DELAY_MIN/MAX оттуда

// Пин управления симистором (PA8, TIM1_CH1)
#define TRIAC_PORT     GPIOA
#define TRIAC_PIN      8

// Значение задержки для режима стирки (мкс) – подобрать экспериментально
#define WASH_DEFAULT_DELAY   8250

// Настройки полярности и скорости (определены в motor.c, но доступны для изменения)
// Полярность: 0 – обычная (HIGH после задержки), 1 – инверсная
#define TRIAC_POLARITY_INVERTED   1
// Скорость выхода PA8: 0x1=10MHz, 0x2=2MHz, 0x3=50MHz (2MHz достаточно)
#define TRIAC_GPIO_SPEED          0x2

// Преобразование мощности (0..100%) в задержку открытия симистора (мкс)
// 100% -> 500 мкс (макс. мощность)
// 0%   -> 9000 мкс (мин. мощность, двигатель еле крутится)
#define POWER_MAX_DELAY_US   500
#define POWER_MIN_DELAY_US   9000
#define POWER_DELAY_RANGE    (POWER_MIN_DELAY_US - POWER_MAX_DELAY_US)  // 8500

uint16_t PowerPercentToDelay(uint8_t percent);
uint8_t  DelayToPowerPercent(uint16_t delay_us);

// Публичные функции
void Motor_Init(void);
void Motor_StartDelay(void);      // вызывать из zero-cross прерывания
void Motor_Stop(void);
void Motor_StartWash(void);
void Motor_StartSpin(uint16_t rpm);
void Relay_Update(void);          // вызывать в главном цикле
void Motor_EnableForward(void);
void Motor_EnableReverse(void);

//void Manual_UpdatePower(void);
//void Manual_ToggleMotor(void);
//void ProcessUserInput(void);


// Глобальные переменные (для доступа из zero-cross)
extern volatile uint16_t triac_delay;
extern volatile uint16_t triac_pulse;

#endif

