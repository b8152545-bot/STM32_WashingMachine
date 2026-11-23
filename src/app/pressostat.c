#include "pressostat.h"
#include "stm32f103xx_CMSIS.h"
#include <stdint.h>
#include <stdbool.h>

// ---------- Конфигурация ----------
#define TIMER_FREQ_HZ            1000000UL   // 1 MHz (PSC=71)
#define SCALE_MHZ                1000UL

// Частота датчика при пустом / полном баке (mHz)
#define IMPULSES_EMPTY_MHZ       44304UL     // 44.304 Hz
#define IMPULSES_FULL_MHZ        34050UL     // 34.050 Hz

#define MIN_VALID_PERIOD_US      1000UL      // 1 мс (макс 1 кГц)
#define MAX_VALID_PERIOD_US      60000UL     // 60 мс (мин ~16.7 Гц)

// EMA сглаживание (alpha = 1/4)
#define EMA_NUM                  1U
#define EMA_DEN                  4U

extern volatile uint32_t SysTimer_ms; 

// ---------- Глобальные переменные ----------
static volatile uint32_t period_us = 0;      // последний валидный период (мкс)
static volatile uint8_t  ic_data_ready = 0;  // флаг новых данных
static uint32_t last_freq_mhz = 0;           // сглаженная частота (mHz)

// ---------- Инициализация TIM3_CH4 на PB1 ----------
void Pressostat_Init(void)
{
    // Тактирование уже включено в ZeroCross_Init (TIM3, GPIOB)
    // Дополнительно убедимся, что порт B тактирован
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;

    // PB1 как вход с подтяжкой (CNF=10, MODE=00)
    GPIOB->CRL &= ~(0xF << 4);          // очистить биты 4-7 (pin 1)
    GPIOB->CRL |= (0x8 << 4);           // CNF=10 (input pull-up/down), MODE=00
    GPIOB->ODR |= (1 << 1);             // pull-up

    // Настройка канала 4 таймера TIM3 (захват по нарастающему фронту)
    // Предполагаем, что TIM3 уже инициализирован (PSC=71, ARR=0xFFFF, CR1_CEN)
    TIM3->CCMR2 &= ~TIM_CCMR2_CC4S;
    TIM3->CCMR2 |= TIM_CCMR2_CC4S_0;     // CC4S = 01 (захват на TI4)

    // Фильтр IC4F = 0110 (N=8, fDTS/8) – подавление дребезга
    TIM3->CCMR2 &= ~(0xF << 12);         // очистить IC4F
    TIM3->CCMR2 |= (0x6 << 12);          // IC4F = 0110

    TIM3->CCER &= ~TIM_CCER_CC4P;        // полярность: нарастающий фронт
    TIM3->CCER |= TIM_CCER_CC4E;         // включить захват

    // Прерывание по CC4 (добавляем к уже настроенным CC1 и CC3)
    TIM3->DIER |= TIM_DIER_CC4IE;

    // Приоритет уже установлен в ZeroCross_Init (0 – наивысший)
    // Можно оставить как есть или понизить для прессостата
    // NVIC_SetPriority(TIM3_IRQn, 2); // опционально
    NVIC_EnableIRQ(TIM3_IRQn);
}

// Обработчик TIM3_IRQHandler будет дополнен в файле zero_cross.c.
// Чтобы не дублировать код, мы поместим логику прессостата в отдельную функцию,
// которую вызовем из общего обработчика. Но проще добавить прямо в zero_cross.c.
// Поэтому здесь функция обработки не пишется – она будет вставлена в zero_cross.c.
// См. пояснения ниже.

// ---------- Функция для вызова из TIM3_IRQHandler (добавить в zero_cross.c) ----------
void Pressostat_OnCapture(uint16_t capture)
{
    static uint16_t last_capture = 0;
    uint16_t diff = capture - last_capture;   // работает с переполнением (uint16_t)
    last_capture = capture;

    if (diff >= MIN_VALID_PERIOD_US && diff <= MAX_VALID_PERIOD_US)
    {
        period_us = diff;
        ic_data_ready = 1;
    }
}

// ---------- Публичная функция: получить уровень воды (0-100%) ----------
uint8_t Pressostat_GetLevel(void)
{
    if (ic_data_ready)
    {
        __disable_irq();
        uint32_t local_period = period_us;
        ic_data_ready = 0;
        __enable_irq();

        if (local_period != 0)
        {
            uint32_t freq_mhz_new = (uint32_t)((uint64_t)TIMER_FREQ_HZ * SCALE_MHZ / local_period);
            if (last_freq_mhz == 0)
                last_freq_mhz = freq_mhz_new;
            else
                last_freq_mhz = (EMA_NUM * freq_mhz_new + (EMA_DEN - EMA_NUM) * last_freq_mhz) / EMA_DEN;
        }
    }

    uint32_t freq = last_freq_mhz;
    if (freq == 0) return 0;

    if (freq >= IMPULSES_EMPTY_MHZ) return 0;
    if (freq <= IMPULSES_FULL_MHZ)  return 100;

    uint32_t numerator   = (IMPULSES_EMPTY_MHZ - freq) * 100UL;
    uint32_t denominator = IMPULSES_EMPTY_MHZ - IMPULSES_FULL_MHZ;
    uint32_t level = numerator / denominator;
    if (level > 100) level = 100;
    return (uint8_t)level;
}

// ---------- Проверка на ошибку (обрыв датчика) ----------
bool Pressostat_IsFaulty(void)
{
    static uint32_t last_time = 0;
    uint32_t now = SysTimer_ms;   // глобальная переменная из main
    if (now - last_time > 1000)
    {
        last_time = now;
        if (period_us == 0) return true;
    }
    return false;
}