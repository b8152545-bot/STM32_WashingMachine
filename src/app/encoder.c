#include "encoder.h"
#include <stdlib.h>
#include "stm32f103xx_CMSIS.h" // если нужно
extern volatile uint32_t SysTimer_ms; // объявление глобального таймера из stm32f103xx_CMSIS.c


/**
 *  Encoder GPIO	PA0 и PA1	Задейсвован таймер TIN2
 */
/*void Encoder_Init(void) {

	// Защита от повторной инициализации
	static int initialized = 0;
    if (initialized) return;
    initialized = 1;

	// Настройка PA0 (канал TIM2_CH1)
    GPIO_TYPE gpio_a;
    gpio_a.port = PORTA;
    gpio_a.pin = 0;
    gpio_a.mode = INPUT_MODE;               // вход
    gpio_a.mode_type = OUTPUT_ALT_FUNCTION; // альтернативная функция (вход)
    gpio_a.speed = SPEED_2MHZ;
    gpio_init(gpio_a);

    // Настройка PA1 (канал TIM2_CH2)
    GPIO_TYPE gpio_b;
    gpio_b.port = PORTA;
    gpio_b.pin = 1;
    gpio_b.mode = INPUT_MODE;
    gpio_b.mode_type = OUTPUT_ALT_FUNCTION;
    gpio_b.speed = SPEED_2MHZ;
    gpio_init(gpio_b);

    // Кнопка на PA2
    GPIO_TYPE gpio_btn;
    gpio_btn.port = PORTA;
    gpio_btn.pin = 2;
    gpio_btn.mode = INPUT_MODE;
    gpio_btn.mode_type = INPUT_FLOATING;   // или INPUT_PU_DP с подтяжкой
    gpio_init(gpio_btn);

	
	RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;						// Включаем тактирование таймера TIN2
	TIM2->CCMR1 |= (TIM_CCMR1_CC1S_0 | TIM_CCMR1_CC2S_0);	// типо захват событий по двум каналам TI1 и TI2
    TIM2->CCER &= ~(TIM_CCER_CC1P | TIM_CCER_CC2P);         // ПРАВИЛЬНО
	//TIM2->CCMR1 &= ~(TIM_CCER_CC1P | TIM_CCER_CC2P);		// нарастающий фронт
	//TIM2->SMCR |= TIM_SMCR_SMS_0;							// 2 Импульса
	TIM2->SMCR |= (TIM_SMCR_SMS_0 | TIM_SMCR_SMS_1);		// 4 Импульса
	//TIM2->CCMR1 |= TIM_CCMR1_IC1PSC_1;					// Регистр предварительной загрузки на TIMx_CCR1 включен.
	//TIM2->DIER|=TIM_DIER_UIE;								// Запрос обновления DMA включен.
	//NVIC_EnableIRQ(TIM2_IRQn);
	//TIM2->CR1 |= TIM_CR1_ARPE;							// включение предварительной загрузки с автоматической перезагрузкой, TIMx_ARR буферизирован
	// 1111: fSAMPLING = fDTS / 32, N = 8 
	TIM2->CCMR1 |= (TIM_CCMR1_IC1F_0 | TIM_CCMR1_IC1F_1 | TIM_CCMR1_IC1F_2 | TIM_CCMR1_IC1F_3);
	TIM2->CCMR1 |= (TIM_CCMR1_IC2F_0 | TIM_CCMR1_IC2F_1 | TIM_CCMR1_IC2F_2 | TIM_CCMR1_IC2F_3);
    TIM2->ARR = 0xFFFF;
	//TIM2->ARR = 39;											// Значение предделителя, регистр автоперезагрузки.
	TIM2->CR1 |= TIM_CR1_CEN;								// Counter enabled
	
}*/
void Encoder_Init(void)
{
    // Тактирование
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_AFIOEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

    // PA0, PA1 – альтернативная функция (вход для TIM2)
    GPIOA->CRL &= ~((GPIO_CRL_MODE0 | GPIO_CRL_CNF0) | (GPIO_CRL_MODE1 | GPIO_CRL_CNF1));
    GPIOA->CRL |= (GPIO_CRL_CNF0_1 | GPIO_CRL_CNF1_1);   // CNF = 10
    GPIOA->ODR |= (1 << 0) | (1 << 1);                  // подтяжка вверх

    // PA2 – кнопка, вход с подтяжкой
    GPIOA->CRL &= ~(GPIO_CRL_MODE2 | GPIO_CRL_CNF2);
    GPIOA->CRL |= GPIO_CRL_CNF2_0;                      // CNF = 01
    GPIOA->ODR |= (1 << 2);                             // подтяжка вверх

    // Настройка TIM2 в режим энкодера (как у вас уже есть)
    TIM2->CCMR1 |= (TIM_CCMR1_CC1S_0 | TIM_CCMR1_CC2S_0);
    TIM2->CCER &= ~(TIM_CCER_CC1P | TIM_CCER_CC2P);
    TIM2->SMCR |= (TIM_SMCR_SMS_0 | TIM_SMCR_SMS_1);    // режим 4 импульса
    TIM2->CCMR1 |= (TIM_CCMR1_IC1F_0 | TIM_CCMR1_IC1F_1 | TIM_CCMR1_IC1F_2 | TIM_CCMR1_IC1F_3);
    TIM2->CCMR1 |= (TIM_CCMR1_IC2F_0 | TIM_CCMR1_IC2F_1 | TIM_CCMR1_IC2F_2 | TIM_CCMR1_IC2F_3);
    TIM2->ARR = 0xFFFF;
    TIM2->CR1 |= TIM_CR1_CEN;
}

/** Возвращает направление изменения состояния энкодера:
*  1  -> вправо (увеличение счётчика)
* -1  -> влево (уменьшение счётчика)
*  0  -> без изменений
*/
int8_t Encoder_GetDirection(void) {
    static int16_t last_cnt = 0;
    static int32_t accumulator = 0;
    static const int16_t STEP = 4;               // полный шаг энкодера (4 импульса)
    static const int16_t THRESHOLD = STEP;

    int16_t curr_cnt = TIM2->CNT;
    int16_t delta = curr_cnt - last_cnt;         // знаковое приращение
    last_cnt = curr_cnt;

    // Если знак delta противоположен знаку accumulator → сменили направление
    if ((accumulator > 0 && delta < 0) || (accumulator < 0 && delta > 0)) {
        accumulator = 0;                          // начинаем копить заново
    }

    accumulator += delta;

    int8_t result = 0;
    if (accumulator >= THRESHOLD) {
        result = 1;
        accumulator = 0;
    } else if (accumulator <= -THRESHOLD) {
        result = -1;
        accumulator = 0;
    }

    #if ENCODER_REVERSE_DIRECTION
        return -result;
    #else
        return result;
    #endif
}


#define CLICK_TIME_MIN     30     // мс, можно сделать маленьким, т.к. дребезг уже железом
#define CLICK_TIME_MAX     400    // мс, максимум для короткого нажатия
#define LONGCLICK_TIME     1500   // мс, длинное нажатие
#define DOUBLECLICK_GAP    400    // мс, пауза между кликами

typedef enum {
    IDLE,
    PRESS,
    WAIT_SECOND,
    SECOND_PRESS,
    LONG_REPORTED   // новое состояние: long уже отчитан, ждём отпускания
} State;

uint8_t Button_GetEvent(void) {
    static State state = IDLE;
    static uint32_t pressStart = 0;
    static uint32_t waitStart  = 0;

    uint8_t pin = (GPIOA->IDR & GPIO_IDR_IDR2) ? 1 : 0; // 0 = нажато
    uint32_t now = SysTimer_ms;

    switch (state) {

        case IDLE:
            if (pin == 0) {
                pressStart = now;
                state = PRESS;
            }
            break;

        case PRESS:
            // Если удержание превысило порог — немедленно вернуть long и перейти в LONG_REPORTED
            if ((now - pressStart) >= LONGCLICK_TIME) {
                state = LONG_REPORTED;
                return 3; // long click, возвращаем до отпускания
            }

            if (pin == 1) { // отпустили до порога long
                uint32_t dt = now - pressStart;

                if (dt >= CLICK_TIME_MIN && dt <= CLICK_TIME_MAX) {
                    waitStart = now;
                    state = WAIT_SECOND;
                } else {
                    state = IDLE; // слишком короткое или некорректное — игнор
                }
            }
            break;

        case LONG_REPORTED:
            // Ждём отпускания, но не возвращаем повторно long
            if (pin == 1) {
                state = IDLE;
            }
            break;

        case WAIT_SECOND:
            if (pin == 0) { // второе нажатие началось
                pressStart = now;
                state = SECOND_PRESS;
            } else if ((now - waitStart) >= DOUBLECLICK_GAP) {
                state = IDLE;
                return 1; // single click
            }
            break;

        case SECOND_PRESS:
            if (pin == 1) { // отпустили второй раз
                uint32_t dt = now - pressStart;

                if (dt >= CLICK_TIME_MIN && dt <= CLICK_TIME_MAX) {
                    state = IDLE;
                    return 2; // double click
                } else {
                    state = IDLE;
                    return 1; // считаем одиночным
                }
            }
            break;
    }
    return 0;
}
