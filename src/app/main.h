/*
Архитектура проекта
src/
├── app/          ← Логика приложения
│   ├── main.c          — точка входа
│   ├── selection_menu  — меню выбора программ
│   ├── motor.c / motor_control.c — управление двигателем
│   ├── pid.c           — ПИД-регулятор
│   ├── encoder.c       — энкодер (PA0/PA1, TIM2)
│   ├── zero_cross.c    — детектор нуля (PA6, TIM3_CH1)
│   ├── pressostat.c    — прессостат (PB1, TIM3_CH4)
│   ├── autotune.c      — автонастройка
│   ├── hardware.c      — инициализация оборудования
│   └── eeprom_images.c — работа с EEPROM (изображения/шрифты)
│
├── core/         ← Ядро системы
│   ├── stm32f103xx_CMSIS — CMSIS-инициализация (clock, SysTick)
│   ├── stm32f1_rtc       — RTC (часы реального времени)
│   ├── timestamp         — работа с временем
│   ├── port_ini.c        — настройка GPIO портов
│   └── port_mapping.h    — карта пинов
│
├── drivers/      ← Драйверы периферии
│   ├── ADC.c       — АЦП (температура PC0)
│   ├── SPI2.c      — SPI для дисплея ILI9341
│   ├── UART.c      — USART1 (PA9/PA10)
│   ├── i2c_drive.c — I2C для EEPROM
│   ├── HAL_GPIO.c  — GPIO
│   └── gp_drive.c  — общие драйверы
│
├── gfx/          — Графика (ILI9341 + DMA)
├── fonts/        — Шрифты
├── images/       — Изображения (иконки программ)
└── utils/        — Утилиты
*/



/*  Конфигурация портов STM32F103RCT6 - 64 pins

Назначение          Порт/пин	Периферия / Альт. функция       Примечания
Кварцы_______________________________			
HSE (8 МГц)         PD0, PD1	OSC_IN, OSC_OUT                 обязательно
LSE (32.768 кГц)	PC14, PC15	OSC32_IN, OSC32_OUT             для RTC
SWD                 PA13, PA14	SWDIO, SWCLK                    для отладки
BOOT0               60
BOOT1               PB2
Дисплей (SPI2)_______________________			
MOSI	            PB15	SPI2_MOSI	
MISO	            PB14	SPI2_MISO	
SCK	                PB13	SPI2_SCK	
NSS (CS)	        PB12	SPI2_NSS	
DS (Data/Command)	PB11	(GPIO)	
RESET	            PB10	(GPIO)	
UART (связь)_________________________			
TX	                PA9	    USART1_TX	
RX	                PA10	USART1_RX	
I2C (EEPROM)_________________________			
SCL	                PB6	    I2C1_SCL	
SDA	                PB7	    I2C1_SDA	
ADC__________________________________			
Температура         PC0	    ADC1_IN10
резервный канал   	PC1	    ADC1_IN11
Энкодер______________________________			
CLK (A)	            PA0	    TIM2_CH1	
DT (B)	            PA1	    TIM2_CH2	
SW (кнопка)	        PA2	    (GPIO)	
_____________________________________
Zero-cross	        PA6	    TIM3_CH1 – вход захват
Управление дверью	PB0	    (GPIO)	
Ос. Двери           PA7     EXTI*   прерывание
Клапаны______________________________			
Valve 1             PA11	(GPIO)	
Valve 2             PA12	(GPIO)	
Помпы________________________________			
Pump Circul.	    PC3	    (GPIO)	
Pump Drain	        PB5	    (GPIO)	
Двигатель____________________________			
Triac управление	PA8	    TIM1_CH1 (выход OPM)    фазовое регулирование
_____________________________________
ТЭН	                PA5	    (GPIO)	управление реле
Реле направления_____________________			
DIR_FWD_Relay	    PB	    (GPIO)	управление реле
DIR_REV_Relay	    PA15	(GPIO)	управление реле
Relay_3           	PB9	    (GPIO)	управление реле 3
_____________________________________
DiGiTAL_PRESS	    PB1     TIM3 CH4 	вход от прессостата
PowerManagement	    PB3	    (GPIO)	
Зуммер	            PB8	    TIM4_CH3	
Тахогенератор	    PB0	    TIM3_CH3 (вход захват)	для обратной связи по оборотам
Свободные / резерв			
PC3, PC4, PC5, PC6, PC7, PC8, PC9, PC10, PC11, PC12, PC13	GPIO (некоторые – аналоговые)	
 TIM2 CH4 (PA3) , TIM4 CH4 (PB9
*/




#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "HAL_GPIO.h"
#include "ILI9341.h"
#include "i2c_drive.h"
#include "gp_drive.h"
#include "encoder.h"
#include "eeprom_images.h"
#include "zero_cross.h"
#include "ADC.h"
#include "UART.h"



void port_init (void); //Настройка портов



#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
