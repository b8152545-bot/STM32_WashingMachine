/*
 * Тахометр на TIM3_CH3 (PB0) с 32-битным расширением через счётчик переполнений.
 * Стабильно измеряет обороты от 150 об/мин и выше.
 */

#include "zero_cross.h"
#include "motor.h"
#include "pressostat.h"

extern volatile uint32_t SysTimer_ms;

volatile uint32_t last_zero_cross_time = 0;
volatile uint8_t  power_failure_saved = 0;

// ===== Тахометр =====
static volatile uint32_t tim3_overflows = 0;          // счётчик переполнений таймера
static volatile uint32_t tacho_last_capture = 0;      // 32-битное время последнего захвата
static volatile uint32_t tacho_last_edge_time = 0;    // время последнего фронта (мс)
static volatile uint16_t tacho_rpm = 0;               // отфильтрованные обороты

#define TACHO_PULSES_PER_REV  8
#define TACHO_FILTER_WINDOW   4

static uint16_t tacho_buffer[TACHO_FILTER_WINDOW];
static uint8_t  tacho_idx = 0;
static uint32_t tacho_sum = 0;
static uint8_t  tacho_valid_samples = 0;   // сколько реально накоплено отсчётов

// Инициализация TIM3 (PA6 – Zero Cross, PB0 – тахометр)
void ZeroCross_Init(void) {
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_AFIOEN;

    // PA6 – вход с подтяжкой (Zero Cross)
    GPIOA->CRL &= ~(0xF << 24);
    GPIOA->CRL |= (0x8 << 24);
    GPIOA->ODR |= (1 << 6);

    // TIM3: 1 МГц, 16-bit
    TIM3->PSC = 72 - 1;
    TIM3->ARR = 0xFFFF;
    TIM3->CNT = 0;
    TIM3->CR1 = TIM_CR1_CEN;

    // Канал 1 (Zero Cross) – захват спадающего фронта
    TIM3->CCMR1 &= ~TIM_CCMR1_CC1S;
    TIM3->CCMR1 |= TIM_CCMR1_CC1S_0;
    TIM3->CCMR1 &= ~(0xF << 4);
    TIM3->CCMR1 |= (0x6 << 4);
    TIM3->CCMR1 &= ~TIM_CCMR1_IC1PSC;
    TIM3->CCER &= ~TIM_CCER_CC1P;
    TIM3->CCER |= TIM_CCER_CC1P;
    TIM3->CCER |= TIM_CCER_CC1E;
    TIM3->DIER |= TIM_DIER_CC1IE;

    // Канал 3 (тахометр) – захват нарастающего фронта
    GPIOB->CRL &= ~(0xF << 0);
    GPIOB->CRL |= (0x8 << 0);
    GPIOB->ODR |= (1 << 0);

    TIM3->CCMR2 &= ~TIM_CCMR2_CC3S;
    TIM3->CCMR2 |= TIM_CCMR2_CC3S_0;
    TIM3->CCMR2 &= ~(0xF << 12);
    TIM3->CCMR2 |= (0x6 << 12);      // цифровой фильтр IC3F = 0110
    TIM3->CCER &= ~TIM_CCER_CC3P;
    TIM3->CCER |= TIM_CCER_CC3E;
    TIM3->DIER |= TIM_DIER_CC3IE;

    // Прерывание по переполнению таймера
    TIM3->DIER |= TIM_DIER_UIE;

    // Прерывание по CC4 (прессостат), если используется
    TIM3->DIER |= TIM_DIER_CC4IE;

    TIM3->SR = 0;
    NVIC_SetPriority(TIM3_IRQn, 0);
    NVIC_EnableIRQ(TIM3_IRQn);
}

// Обработчик прерываний TIM3
void TIM3_IRQHandler(void) {
    uint32_t sr = TIM3->SR;
    TIM3->SR &= ~sr;   // сброс всех установленных флагов

    // Счётчик переполнений (расширяем таймер до 32 бит)
    if (sr & TIM_SR_UIF) {
        tim3_overflows++;
    }

    // --- Zero Cross (канал 1) ---
    if (sr & TIM_SR_CC1IF) {
        uint16_t capture = TIM3->CCR1;
        static uint16_t last_zc_capture = 0;
        uint16_t interval = capture - last_zc_capture;
        last_zc_capture = capture;

        if (interval >= MIN_ZC_INTERVAL_US) {
            last_zero_cross_time = SysTimer_ms;
            power_failure_saved = 0;
#ifdef DEBUG_ZC
            gpio_toggle(GPIOC, 13);
#endif
            Motor_StartDelay();
        }
    }

    // --- Тахометр (канал 3) ---
    if (sr & TIM_SR_CC3IF) {
        uint16_t capture = TIM3->CCR3;
        tacho_last_edge_time = SysTimer_ms;

        // 32-битное время захвата
        uint32_t now = (tim3_overflows << 16) | capture;
        uint32_t diff = now - tacho_last_capture;
        tacho_last_capture = now;

        // Отсеиваем слишком короткие импульсы (помехи) и слишком длинные (ошибки)
        if (diff > 200 && diff < 600000) {
            // Константа: 60 * 1 000 000 / TACHO_PULSES_PER_REV = 7 500 000
            uint32_t rpm = 7500000UL / diff;
            if (rpm > 15000) rpm = 15000;

            // Обновляем скользящее среднее
            tacho_sum -= tacho_buffer[tacho_idx];
            tacho_buffer[tacho_idx] = (uint16_t)rpm;
            tacho_sum += rpm;
            tacho_idx = (tacho_idx + 1) % TACHO_FILTER_WINDOW;

            if (tacho_valid_samples < TACHO_FILTER_WINDOW)
                tacho_valid_samples++;

            tacho_rpm = (uint16_t)(tacho_sum / tacho_valid_samples);
        }
    }

    // --- Прессостат (канал 4) ---
    if (sr & TIM_SR_CC4IF) {
        uint16_t capture = TIM3->CCR4;
        Pressostat_OnCapture(capture);
    }
}

// Проверка пропадания Zero Cross
void CheckPowerFailure(void) {
    if (power_failure_saved) return;
    uint32_t now = SysTimer_ms;
    if ((now - last_zero_cross_time) > ZERO_CROSS_TIMEOUT_MS) {
        GPIO_RESET_PIN(POWER_MANAGEMENT_PORT, POWER_MANAGEMENT_PIN);
        SaveWashState();
        ILI9341_DrawRect_DMA(0, 0, 320, 240, 1, COLOR_RED);
        power_failure_saved = 1;
    }
}

// Получение оборотов (вызывается из основного цикла)
uint16_t HW_GetRPM(void) {
    uint32_t now = SysTimer_ms;
    uint32_t last = tacho_last_edge_time;

    // Если с последнего импульса прошло > 200 мс – двигатель остановлен
    if ((now - last) > 200) {
        // Сбрасываем фильтр
        tacho_sum = 0;
        for (int i = 0; i < TACHO_FILTER_WINDOW; i++) tacho_buffer[i] = 0;
        tacho_idx = 0;
        tacho_valid_samples = 0;
        tacho_rpm = 0;
        return 0;
    }
    return tacho_rpm;
}