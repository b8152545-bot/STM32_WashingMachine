#ifndef _HAL_GPIO
#define _HAL_GPIO


#include "stm32f1xx.h"


/*	=================================================
 *  https://youtu.be/n6dF6W6Qh4E?list=PLPEoa7U3speM3W_Umbgz4cj62G1mHEoGo
 *
 * 	Пример настройки прерывания на PORTB 11
 *
 * 	configure_gpio_interrupt(PORTB, 11, RISING_EDGE);
 * 	//	передаём в функцию порт, ногу, и фронт: восходящий, падающий или тот и тот.
 *	enable_gpio_interrupt(11,EXTI15_10_IRQn);
 *	//	передаёмв функцыю ногу и (EXTI15_10_IRQn) эта хрень берётся из startup_stm32f103c8tx.с
 *
 *	Hу и хендлер
 *
 *	void EXTI15_10_IRQHandler()
 *	{
 *		clear_gpio_interrupt(11);	//	эта хрень здесь обязательна и перед кодом
 *		gpio_toggle(PORTC, 13);		//	код, который хотим выполнить
 *	}
 *	=================================================
 *
 *	Пример настройки прерывания на PORTA 1
 *
 *	configure_gpio_interrupt(PORTA, 1, RISING_EDGE);
 *	enable_gpio_interrupt(1,EXTI1_IRQn);
 *
 *	Hу и хендлер
 *
 *	void EXTI1_IRQHandler()
 *	{
 *		clear_gpio_interrupt(1);	//	эта хрень здесь обязательна и перед кодом
 *		gpio_toggle(PORTC, 13);		//	код, который хотим выполнить
 *	}
 *	==================================================
 *
 *			Пример настройки PORTC 13 на выход
 *
 *	GPIO_TYPE myGPIO;						//	Создаём структуру
 *	myGPIO.port = PORTC;					//	заопоняем пораметры порта
 *	myGPIO.pin = 13;						//	заопоняем пораметры порта
 *	myGPIO.mode = OUTPUT_MODE;				//	заопоняем пораметры порта
 *	myGPIO.mode_type = OUTPUT_GEN_PURPOSE;	//	заопоняем пораметры порта
 *	myGPIO.speed = SPEED_2MHZ;				//	заопоняем пораметры порта
 *
 *	gpio_init(myGPIO);						//	передаем параметры на выполнение
 *	==================================================
 */


#define LOW		0
#define HIGH	1
//================port=======================================//
//	Выбор порта
#define PORTA		GPIOA
//	Выбор порта
#define PORTB		GPIOB
//	Выбор порта
#define PORTC		GPIOC
//	Выбор порта
#define PORTD		GPIOD
//	Выбор порта
#define PORTE		GPIOE
//================mode=======================================//
//	: выход
#define OUTPUT_MODE			((uint32_t) 0X01)
//	: вход
#define INPUT_MODE			((uint32_t) 0X02)
//=======================================================//
//	: аналоговый вход
#define INPUT_ANALOG		((uint32_t) 0X00)
//	: плавыющий вход
#define INPUT_FLOATING		((uint32_t) 0X01)
//	: вход с подътяжкой
#define INPUT_PU_DP			((uint32_t) 0X02)
//===================mode_type====================================//
//	: Выход общего назначения, (Push-pull) двухтактный
#define OUTPUT_GEN_PURPOSE		((uint32_t) 0X00)
//	: Выход общего назначения с открытым стоком
#define OUTPUT_OD				((uint32_t) 0X01)
//	: Альтернативный выход функции Push-pull
#define OUTPUT_ALT_FUNCTION		((uint32_t) 0X02)
//	: Альтернативный выход функции Открытый сток
#define OUTPUT_ALT_FUNCTION_OD	((uint32_t) 0X03)
//=======================================================//
//	: скорость нарастания фронта
#define SPEED_2MHZ			((uint32_t) 0X02)
//	: скорость нарастания фронта
#define SPEED_10MHZ			((uint32_t) 0X01)
//	: скорость нарастания фронта
#define SPEED_50MHZ			((uint32_t) 0X03)
//=======================================================//
//	Включение тактирования (Альторнотивных Функцый) прерывания
#define GPIO_CLOCK_ENABLE_ALT_FUNC	(RCC->APB2ENR |= (1<<0))
//	Включение тактирования порта А
#define GPIO_CLOCK_ENABLE_PORTA		(RCC->APB2ENR |= (1<<2))
//	Включение тактирования порта В
#define GPIO_CLOCK_ENABLE_PORTB		(RCC->APB2ENR |= (1<<3))
//	Включение тактирования порта С
#define GPIO_CLOCK_ENABLE_PORTC		(RCC->APB2ENR |= (1<<4))
//	Включение тактирования порта D
#define GPIO_CLOCK_ENABLE_PORTD		(RCC->APB2ENR |= (1<<5))
//	Включение тактирования порта E
#define GPIO_CLOCK_ENABLE_PORTE		(RCC->APB2ENR |= (1<<6))
//=======================================================//
//	ПОЛОЖЕНИЕ ВЕРХНЕГО БИТА ДЛЯ РЕГИСТРА CRH CNFYG И РЕЖИМА
#define CNF_POS_BIT1			(PINPOS[pinNumber] + 2)
#define CNF_POS_BIT2			(PINPOS[pinNumber] + 3)

// Структура для хранения всех параметров GPIO
typedef struct
{
    GPIO_TypeDef *port;    // Указатель на порт (PORTA, PORTB и т.д.)
    uint32_t    pin;       // Номер пина (0-15)
    uint32_t    mode;      // Режим: INPUT_MODE или OUTPUT_MODE
    uint32_t    mode_type; // Тип входа/выхода 
    uint32_t    pull;      // Подтяжка 
    uint32_t    speed;     // Скорость (для выходов)
    uint32_t    alt_func;  // Альтернативная функция 
} GPIO_TYPE;

typedef enum
{
	RISING_EDGE,
	FALLING_EDGE,
	RISING_FALLING_EDGE
}edge_select;

//=================================================================================
//			КОНФИГУРАЦИЯ GPIO
void gpio_init (GPIO_TYPE gpio_type);
void config_pin (GPIO_TypeDef *port, uint32_t pinNumber, uint32_t mode_type);
void config_pin_speed (GPIO_TypeDef *port, uint32_t pinNumber, uint32_t pinSped, uint32_t mode);
//=================================================================================
//			ФУНКЦИИ КОНТАКТОВ GPIO
void gpio_write (GPIO_TypeDef *port, uint32_t pinNumber, uint8_t state);
void gpio_toggle (GPIO_TypeDef *port, uint32_t pinNumber);

//=================================================================================
//			ФУНКЦИЯ ПРЕРЫВАНИЯ
//	фунцыя принимпет      ( 	порт	,         номер пина,             фронт);
void configure_gpio_interrupt (GPIO_TypeDef *port, uint32_t pinNumber, edge_select edge);

void enable_gpio_interrupt (uint32_t pinNumber, IRQn_Type irqNumber);
void clear_gpio_interrupt (uint32_t pinNumber);


#endif
