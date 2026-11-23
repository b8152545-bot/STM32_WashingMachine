/*
TODO
PID
*/


#include "motor.h"

extern volatile uint32_t SysTimer_ms;

// Ручной режим: 0 – выключен (работает программа), 1 – ручное управление
volatile uint8_t manual_mode = 0;
volatile uint8_t motor_running = 0;   // 0 – стоп, 1 – вращение
volatile uint8_t manual_power = 50;   // 50% мощности (значение по умолчанию)

/*  Управление симистором
*   Минимальная задержка – максимум мощности. Обычно 500–1000 мкс (нельзя 0, так как при нулевой задержке могут быть помехи).
*   Максимальная задержка – минимум мощности. Ограничена 10000 - triac_pulse.
*   Для triac_pulse=750 макс. задержка = 10000-750 = 9250 мкс.
*   Давать больше 9000 обычно не имеет смысла – двигатель уже еле крутится.*/
volatile uint16_t triac_delay = 0;      // задержка включения симистора (в микросекундах от 10 - 9800)
volatile uint16_t triac_pulse = 500;   // длительность управляющего импульса (в микросекундах)

// Состояние направления и реле
static volatile uint8_t motor_direction = 0; // 0 – стоп, 1 – вперёд, 2 – назад
static uint32_t relay_switch_time = 0;
static uint8_t  relay_pending_dir = 0;
static uint8_t  relay_phase = 0;         // 0 – idle, 1 – ждём выключения (100 мс)

// Вспомогательная функция map (защищённая)
static inline uint16_t map_u16(uint16_t x,
                               uint16_t in_min, uint16_t in_max,
                               uint16_t out_min, uint16_t out_max)
{
    if (in_max == in_min) return out_min;
    int32_t num = ((int32_t)x - (int32_t)in_min) *
                  ((int32_t)out_max - (int32_t)out_min);
    int32_t den = (int32_t)in_max - (int32_t)in_min;
    int32_t res = num / den + out_min;
    if (res < 0) res = 0;
    if (res > 65535) res = 65535;
    return (uint16_t)res;
}

// ------------------ Управление реле направления ------------------
static void set_direction(uint8_t dir)
{
    // Если уже переключаемся на это направление – ничего не делаем
    if (relay_phase != 0 && dir == relay_pending_dir) return;
    // Если уже в этом направлении – тоже выходим
    if (relay_phase == 0 && dir == motor_direction) return;

    relay_pending_dir = dir;
    relay_phase = 1;
    relay_switch_time = SysTimer_ms;

    // Сразу выключаем оба реле
    gpio_write(DIR_FWD_PORT, DIR_FWD_PIN, 0);
    gpio_write(DIR_REV_PORT, DIR_REV_PIN, 0);
}

void Relay_Update(void)
{
    if (relay_phase == 0) return;

    uint32_t now = SysTimer_ms;
    if ((uint32_t)(now - relay_switch_time) < 100) return; // ждём 100 мс

    // По истечении паузы включаем нужное реле
    if (relay_pending_dir == 1) {
        gpio_write(DIR_FWD_PORT, DIR_FWD_PIN, 1);
    } else if (relay_pending_dir == 2) {
        gpio_write(DIR_REV_PORT, DIR_REV_PIN, 1);
    }

    motor_direction = relay_pending_dir;
    relay_phase = 0;   // переключение завершено
}

// ------------------ Настройка таймера TIM1 и GPIO PA8 ------------------
static void TIM1_GPIO_AF_Init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_AFIOEN;
    // PA8 в CRH, биты 0-3
    GPIOA->CRH &= ~(0xF << 0);
    // CNF=10 (AF push-pull), MODE = TRIAC_GPIO_SPEED
    GPIOA->CRH |= ((0x2 << 2) | (TRIAC_GPIO_SPEED << 0));
}

void Motor_Init(void)
{
    // Включить тактирование TIM1
    RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;

    TIM1_GPIO_AF_Init();

    // Базовая настройка TIM1
    TIM1->PSC = 72 - 1;          // 1 МГц
    TIM1->ARR = 0xFFFF;          // на случай ошибок
    TIM1->CR1 = 0;               // выключен, up‑counter
    TIM1->CNT = 0;
    TIM1->SR = 0;
    TIM1->DIER = 0;              // прерывания не используем
    TIM1->CCMR1 = 0;
    TIM1->CCMR2 = 0;
    TIM1->CCER = 0;

    // Настройка канала 1 (один раз, статическая часть)
    TIM1->CCMR1 &= ~TIM_CCMR1_OC1M;
    TIM1->CCMR1 |= (6 << 4);          // PWM mode 1
    TIM1->CCMR1 &= ~TIM_CCMR1_OC1PE;  // без предзагрузки

#if TRIAC_POLARITY_INVERTED
    TIM1->CCER |= TIM_CCER_CC1P;      // инверсия
#else
    TIM1->CCER &= ~TIM_CCER_CC1P;     // обычная полярность
#endif
    // CC1E пока не включаем – включим в момент запуска

    // Разрешить главный выход (обязательно для advanced timer)
    TIM1->BDTR |= TIM_BDTR_MOE;
    
}

// ------------------ Запуск импульса (вызывается из zero-cross) ------------------
void Motor_StartDelay(void)
{
    uint16_t delay = triac_delay;
    uint16_t pulse = triac_pulse;
    uint8_t dir = motor_direction;
    uint8_t phase = relay_phase;

    if (delay == 0) return;
    if (phase != 0 || dir == 0) return;

    uint32_t c1 = delay;
    uint32_t c2 = c1 + pulse;
    const uint32_t HALF_PERIOD_US = 10000;
    if (c2 > HALF_PERIOD_US) {
        if (pulse > HALF_PERIOD_US) return;
        c1 = HALF_PERIOD_US - pulse;
        c2 = HALF_PERIOD_US;
    }

    // Остановить таймер и сбросить флаги
    TIM1->CR1 &= ~TIM_CR1_CEN;
    TIM1->CNT = 0;
    TIM1->SR = 0;

    // --- Настройка канала 1: PWM mode 1, предзагрузка ---
    TIM1->CCMR1 &= ~TIM_CCMR1_OC1M;                         // очистить биты режима
    TIM1->CCMR1 |= (TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2);   // PWM mode 1 (добавить!)
    TIM1->CCMR1 |= TIM_CCMR1_OC1PE;                         // включить предзагрузку CCR1

    // Включить выход канала
    TIM1->CCER |= TIM_CCER_CC1E;

    // Записать новые значения в регистры
    TIM1->ARR  = (uint16_t)c2;
    TIM1->CCR1 = (uint16_t)c1;

    // Сгенерировать событие обновления, чтобы загрузить ARR и CCR1 в теневые регистры
    TIM1->EGR = TIM_EGR_UG;
    TIM1->SR &= ~TIM_SR_UIF;   // очистить флаг обновления

    // One Pulse Mode
    TIM1->CR1 |= TIM_CR1_OPM;
    // Запуск счётчика
    TIM1->CR1 |= TIM_CR1_CEN;
}

// ------------------ Публичные функции управления двигателем ------------------
void Motor_StartWash(void)
{
    set_direction(1);                 // вперёд
    //triac_delay = WASH_DEFAULT_DELAY; // фиксированная мощность
}

void Motor_StartSpin(uint16_t rpm)
{
    // Ограничение входного диапазона
    if (rpm < 500) rpm = 500;
    if (rpm > 10000) rpm = 10000;

    set_direction(1);

    uint16_t delay = map_u16(rpm, 500, 10000, 9000, 500);
    if (delay < 500) delay = 500;
    if (delay > 9000) delay = 9000;

    triac_delay = delay;
}

void Motor_Stop(void)
{
    triac_delay = 0;                  // запретить новые импульсы

    // Остановить таймер и отключить канал
    TIM1->CR1 &= ~TIM_CR1_CEN;
    TIM1->CCER &= ~TIM_CCER_CC1E;

    // Принудительно установить неактивный уровень на PA8 через таймер
    TIM1->CCMR1 &= ~TIM_CCMR1_OC1M;
    TIM1->CCMR1 |= (4 << 4);          // OC1M = 100 (force inactive)
    TIM1->CCER |= TIM_CCER_CC1E;      // включить выход (форсирован)
    TIM1->EGR = TIM_EGR_UG;           // обновить
    TIM1->CCER &= ~TIM_CCER_CC1E;     // снова отключить

    // Сбросить всё состояние реле
    relay_phase = 0;
    relay_pending_dir = 0;
    motor_direction = 0;
    gpio_write(DIR_FWD_PORT, DIR_FWD_PIN, 0);
    gpio_write(DIR_REV_PORT, DIR_REV_PIN, 0);
}



//Функция обновления мощности по энкодеру (вызывать в главном цикле)
/*void Manual_UpdatePower(void) {
    if (!manual_mode) return;

    int8_t dir = Encoder_GetDirection();
    if (dir != 0) {
        int16_t new_power = manual_power + dir * 5;   // шаг 5%
        if (new_power < 0) new_power = 0;
        if (new_power > 100) new_power = 100;
        manual_power = (uint8_t)new_power;

        uint16_t delay = PowerPercentToDelay(manual_power);

        // Если мотор включён – применяем новую задержку
        if (motor_running) {
            triac_delay = delay;
        }

        // Для отладки можно выводить процент на дисплей
        // char buf[32];
        // sprintf(buf, "Power: %3d%%", manual_power);
        // ILI9341_WriteString_DMA(10, 30, buf, &Font_12x16, COLOR_WHITE, COLOR_BLACK);
    }
}*/

//Функция запуска/остановки в ручном режиме
/*void Manual_ToggleMotor(void) {
    if (!manual_mode) return;

    if (motor_running) {
        Motor_Stop();
        motor_running = 0;
    } else {
        set_direction(1);
        triac_delay = PowerPercentToDelay(manual_power);
        motor_running = 1;
    }
}*/

/*
* Обработка кнопки для входа/выхода из ручного режима и пуска/стопа
* Используем Button_GetEvent():
* Длинное нажатие (3) – переключение ручного/автоматического режима.
* Короткое нажатие (1) – в ручном режиме пуск/стоп мотора.
*/
/*void ProcessUserInput(void) {
    uint8_t btn = Button_GetEvent();

    if (btn == 3) {  // длинное нажатие – переключить ручной/автомат
        if (manual_mode) {
            // Выход из ручного: останавливаем мотор, уходим в авто
            Manual_ToggleMotor();   // выключить если был включён
            manual_mode = 0;
            // Здесь можно переключить дисплей на автоматический режим
        } else {
            // Вход в ручной режим
            manual_mode = 1;
            motor_running = 0;
            triac_delay = 0;        // гарантированно выключить
            // Можно сбросить позицию энкодера (TIM2->CNT = 0)
        }
    }
    else if (btn == 1 && manual_mode) {  // короткое нажатие в ручном режиме
        Manual_ToggleMotor();
    }
}*/

// ------------------------------------------------------------------
// Преобразование процентов мощности (0-100) в задержку (500-9000 мкс)
// ------------------------------------------------------------------
uint16_t PowerPercentToDelay(uint8_t percent) {
    if (percent >= 100) return POWER_MAX_DELAY_US;
    if (percent == 0)   return POWER_MIN_DELAY_US;
    // Линейная интерполяция:
    // delay = max_delay - (percent * range / 100)
    // т.к. чем больше процент, тем меньше задержка
    uint16_t delay = POWER_MIN_DELAY_US - (percent * POWER_DELAY_RANGE / 100);
    return delay;
}

// ------------------------------------------------------------------
// Преобразование задержки (500-9000 мкс) в проценты мощности (0-100)
// ------------------------------------------------------------------
uint8_t DelayToPowerPercent(uint16_t delay_us) {
    if (delay_us <= POWER_MAX_DELAY_US) return 100;
    if (delay_us >= POWER_MIN_DELAY_US) return 0;
    // percent = (max_delay - delay) * 100 / range
    uint8_t percent = (POWER_MIN_DELAY_US - delay_us) * 100 / POWER_DELAY_RANGE;
    return percent;
}

void Motor_EnableForward(void) {
    set_direction(1);   // включает реле FWD через Relay_Update
}

void Motor_EnableReverse(void) {
    set_direction(2);   // включает реле REV
}

