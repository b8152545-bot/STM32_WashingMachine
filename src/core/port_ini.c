#include "main.h"
#include "port_mapping.h"



void port_init (void) {
	//**************************************************************** */
	SET_BIT(RCC->APB2ENR, RCC_APB2ENR_IOPAEN); //Включим тактирование порта А
	SET_BIT(RCC->APB2ENR, RCC_APB2ENR_AFIOEN); //Включим тактирование альтернативных функций
	AFIO->MAPR = 0b010 << AFIO_MAPR_SWJ_CFG_Pos; //Serial wire

	/*Заблокируем доступ для редактирования PA13 PA14*/
	GPIOA->LCKR = GPIO_LCKR_LCKK | GPIO_LCKR_LCK13 | GPIO_LCKR_LCK14;
	GPIOA->LCKR = GPIO_LCKR_LCK13 | GPIO_LCKR_LCK14;
	GPIOA->LCKR = GPIO_LCKR_LCKK | GPIO_LCKR_LCK13 | GPIO_LCKR_LCK14;
	GPIOA->LCKR;

	/*****************************************************************
	*	типа запретили JTAG и освободили порт для использования
	**************************************************************** */
	SET_BIT(RCC->APB2ENR, RCC_APB2ENR_AFIOEN);
	CLEAR_BIT(AFIO->MAPR, AFIO_MAPR_SWJ_CFG);
	SET_BIT(AFIO->MAPR, AFIO_MAPR_SWJ_CFG_JTAGDISABLE);
	/*****************************************************************/


	GPIO_TYPE myGPIO;						//	Создаём структуру
	myGPIO.port = PORTC;					//	заопоняем пораметры порта
	myGPIO.pin = 13;						//	заопоняем пораметры порта
	myGPIO.mode = OUTPUT_MODE;				//	заопоняем пораметры порта
	myGPIO.mode_type = OUTPUT_GEN_PURPOSE;	//	заопоняем пораметры порта
	myGPIO.speed = SPEED_10MHZ;				//	заопоняем пораметры порта
	gpio_init(myGPIO);						//	передаем параметры на выполнение

	
	// Настройка всех выходов
	 // Структура для описания выходного пина с начальным состоянием
    typedef struct {
        GPIO_TypeDef *port;
        uint32_t pin;
        uint8_t initial_state; // 1 = высокий, 0 = низкий
    } PinInit;

    const PinInit outputs[] = {
        {DOOR_PORT, DOOR_PIN, 0},
        {VALVE_1_PORT, VALVE_1_PIN, 1},
        {VALVE_2_PORT, VALVE_2_PIN, 1},
        {PUMP_CIRC_PORT, PUMP_CIRC_PIN, 1},
        {PUMP_DRAIN_PORT, PUMP_DRAIN_PIN, 1},
		{POWER_MANAGEMENT_PORT,POWER_MANAGEMENT_PIN,0},
        {TEN_PORT, TEN_PIN, 0},          // ТЭН – низкий уровень выключен (если управление через реле с инверсией)
        {DIR_FWD_PORT, DIR_FWD_PIN, 0},
        {DIR_REV_PORT, DIR_REV_PIN, 0},
        {RELAY_3_PORT, RELAY_3_PIN, 0},
    };

    for (int i = 0; i < sizeof(outputs)/sizeof(outputs[0]); i++) {
        // 1. Устанавливаем желаемый уровень в регистре ODR (до того, как пин станет выходом)
        if (outputs[i].initial_state) {
            GPIO_SET_PIN(outputs[i].port, outputs[i].pin);
        } else {
            GPIO_RESET_PIN(outputs[i].port, outputs[i].pin);
        }

        // 2. Настраиваем пин как выход push-pull 2 MHz
        GPIO_TYPE gpio;
        gpio.port = outputs[i].port;
        gpio.pin = outputs[i].pin;
        gpio.mode = OUTPUT_MODE;
        gpio.mode_type = OUTPUT_GEN_PURPOSE;
        gpio.speed = SPEED_2MHZ;
        gpio_init(gpio);
    }

	GPIO_SET_PIN(DOOR_PORT, DOOR_PIN);
	GPIO_SET_PIN(VALVE_1_PORT,VALVE_1_PIN);
	GPIO_SET_PIN(VALVE_2_PORT,VALVE_2_PIN);
	GPIO_SET_PIN(PUMP_CIRC_PORT, PUMP_CIRC_PIN);
	GPIO_SET_PIN(PUMP_DRAIN_PORT, PUMP_DRAIN_PIN);
	GPIO_RESET_PIN(DIR_FWD_PORT,DIR_FWD_PIN);
	GPIO_RESET_PIN(DIR_REV_PORT,DIR_REV_PIN);
	GPIO_RESET_PIN(RELAY_3_PORT,RELAY_3_PIN);
	GPIO_RESET_PIN(TEN_PORT,TEN_PIN);

	
}
