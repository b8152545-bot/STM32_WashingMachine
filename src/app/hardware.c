#include "hardware.h"

// Глобальные переменные для отслеживания длительности
static volatile uint8_t buzzer_active = 0;
static volatile uint32_t buzzer_remaining_ticks = 0;  // сколько осталось переполнений
static volatile uint8_t  buzzer_timer_running = 0;    // флаг, что идёт отсчёт

//----------------------------------------------------------
// Период обновления показаний температуры (в миллисекундах)
#define TEMP_UPDATE_MS      500

// Коэффициент фильтрации EWMA (чем больше, тем сильнее сглаживание)
// new_filtered = old_filtered + (raw - old_filtered) / TEMP_FILTER_COEF
#define TEMP_FILTER_COEF    8    // делитель (степень двойки для оптимизации)

// Калибровочные коэффициенты (из старого кода: при 15°C сырое значение 244, при 91°C – 837)
#define ADC_AT_10C          244
#define ADC_AT_91C          837

// Глобальный счётчик миллисекунд (должен обновляться в SysTick)
extern volatile uint32_t SysTimer_ms;

// Статические переменные для хранения состояния
static uint32_t last_update = 0;
static uint8_t last_temp = 20;               // начальное приближение (комнатная)
static uint16_t filtered_raw = 0;             // отфильтрованное сырое значение АЦП

// ========== Обработка сигнала двери (PA7) ==========
static volatile uint32_t last_door_edge_time = 0;   // время последнего фронта (мс)
static volatile uint8_t  door_initialized = 0;      // флаг инициализации

// ========== Такометр на TIM3_CH3 (PB0) ==========
static volatile uint32_t last_capture = 0;
static volatile uint32_t period_ticks = 0;
static volatile uint16_t rpm_filtered = 0;
static volatile uint32_t overflow_cnt = 0;

// Количество импульсов тахогенератора на один оборот (по вашим данным – 8)
#define TACHO_IMPULSES_PER_REV  8

// Инициазизация: ADC_init(); Motor_Init(); ZeroCross_Init(); Pressostat_Init(); HW_BuzzerInit();
// последовательность вызовов инициализации строго определена
void Hardware_Init (void) {
    ADC_init();
    Motor_Init();           //  PA8 TIM1_CH1
    ZeroCross_Init();       //  PA6 (TIM3_CH1)
    Pressostat_Init();      // TIM3_CH4 на PB1
    HW_BuzzerInit();        //  PB8 (TIM4_CH3)
    HW_DoorSensorInit();    //  EXTI на PA7
    // Загрузка параметров из EEPROM (включая коэффициенты PID)
    LoadProgramParams();
    // Теперь инициализация управления мотором с загруженными коэффициентами
    MotorControl_Init();
    
}

// Инициализация сенсора двери  (вызвать один раз в Hardware_Init)
void HW_DoorSensorInit(void)
{
    GPIO_TYPE door_sensor;
    door_sensor.port = PORTA;
    door_sensor.pin = 7;
    door_sensor.mode = INPUT_MODE;
    door_sensor.mode_type = INPUT_FLOATING;   // внешняя цепь сама формирует 0 и меандр
    door_sensor.speed = 0;
    
    gpio_init(door_sensor);
    
    // Настройка прерывания по обоим фронтам на PA7
    configure_gpio_interrupt(PORTA, 7, RISING_FALLING_EDGE);
    enable_gpio_interrupt(7, EXTI9_5_IRQn);
    
    // Запоминаем текущее время, чтобы не ложно определять открытое состояние до первого прерывания
    last_door_edge_time = SysTimer_ms;
    door_initialized = 1;
}

// Обработчик прерывания (линии EXTI9_5, т.к. PA7 входит в диапазон 5..9)
void EXTI9_5_IRQHandler(void)
{
    if (EXTI->PR & (1 << 7))          // проверяем, что прерывание пришло от PA7
    {
        clear_gpio_interrupt(7);      // обязательный сброс флага
        last_door_edge_time = SysTimer_ms;   // запоминаем время каждого фронта
    }
}

// Возвращает TRUE, если дверь закрыта (есть меандр), FALSE – открыта
bool HW_IsDoorClosed(void)
{
    if (!door_initialized) return false;   // инициализация ещё не выполнена
    
    uint32_t now = SysTimer_ms;
    uint32_t dt = now - last_door_edge_time;
    
    // Если последний фронт был более 25 мс назад – меандра нет, дверь открыта
    // (период 50 Гц = 20 мс, половина периода = 10 мс, запас 15 мс)
    return (dt <= 25);
}

// ------ открыть дверь ------
bool HW_OpenDoor(void) {

    uint8_t water_level = Pressostat_GetLevel();  // 0-100%
    
    // 1. Проверка уровня воды
    if (water_level > 2) {  // если вобы больше 2% - то дверь не открываем.
    
        // Вода есть – дверь не открываем, сигнал ошибки
        HW_IndicateError();   // ваша функция индикации ошибки
        return false;
    }

    // 2. Если дверь уже открыта – сразу успех
    if (!HW_IsDoorClosed())
    return true;

    // 3. Попытки открыть
    const uint8_t maxAttempts = 6;
    for (uint8_t attempt = 0; attempt < maxAttempts; attempt++)
    {
        // Подаём импульс 55 мс
        GPIO_RESET_PIN(DOOR_PORT, DOOR_PIN);
        delay_ms(65);
        GPIO_SET_PIN(DOOR_PORT, DOOR_PIN);

        // Ждём, пока механизм сработает (200 мс)
        delay_ms(200);

        // Проверяем обратную связь
        if (!HW_IsDoorClosed())
        return true;   // если ответ положительный, дверь открылась
    }

    // 4. Не удалось открыть после всех попыток – авария
    HW_IndicateError();
    // Ждём нажатия кнопки START для сброса (опционально)
    // Ждём длинного нажатия START (== 3). Пока ждём — мигаем индикацией ошибки.
    // ВНИМАНИЕ: цикл блокирующий — при отказе кнопки система зависнет.
    // TODO: вынести в неблокирующий автомат (см. ниже заметку).
    while (Button_GetEvent() != 3)
    {
        HW_Buzzer(1350, 50);
        HW_IndicateError();
        delay_ms(500);
    }
    return false;
}

// ------ Закрытие двери ------
bool HW_CloseDoor(void) {
    // Если дверь уже закрыта – успех
    if (HW_IsDoorClosed())
    return true;    

    const uint8_t maxAttempts = 4;  //количество попыток
    for (uint8_t attempt = 0; attempt < maxAttempts; attempt++)
    {
        GPIO_RESET_PIN(DOOR_PORT, DOOR_PIN);
        delay_ms(65);
        GPIO_SET_PIN(DOOR_PORT, DOOR_PIN);

        // Для закрытия пауза длиннее – 600 мс
        delay_ms(600);

        if (HW_IsDoorClosed())
        return true;
    }

    HW_IndicateError();
    // Ждём длинного нажатия START (== 3). Пока ждём — мигаем индикацией ошибки.
    // ВНИМАНИЕ: цикл блокирующий — при отказе кнопки система зависнет.
    // TODO: вынести в неблокирующий автомат (см. ниже заметку).
    while (Button_GetEvent() != 3)
    {
        HW_Buzzer(1350, 50);
        HW_IndicateError();
        delay_ms(500);
    }
    return false;
}

// Открыть впускной клапан1 — открыть подачу воды в барабан.  
void HW_OpenInletValve (void) {
    GPIO_RESET_PIN(VALVE_1_PORT,VALVE_1_PIN);
}

// Закрыть впускной клапан1 — перекрыть подачу воды.
void HW_CloseInletValve (void) {
    GPIO_SET_PIN(VALVE_1_PORT,VALVE_1_PIN);
}

// Открыть впускной клапан2 — (смываем ополаскиватель).  
void HW_OpenTheRinseAidInletValve (void) {
    GPIO_RESET_PIN(VALVE_2_PORT,VALVE_2_PIN);
}

// Закрыть впускной клапан2 — (прекратили смывать ополаскиватель).  
void HW_CloseTheRinseAidInletValve (void) {
    GPIO_SET_PIN(VALVE_2_PORT,VALVE_2_PIN);
}

// Включить циркуляционный насос 
void HW_StartPumpCirculation (void) {
     GPIO_RESET_PIN(PUMP_CIRC_PORT, PUMP_CIRC_PIN);
}

// Остановить циркуляционный насос;
void HW_StopPumpCirculation (void) {
    GPIO_SET_PIN(PUMP_CIRC_PORT, PUMP_CIRC_PIN);
}

// Включить дренажный насос
void HW_StartPumpDrain (void) {
    GPIO_RESET_PIN(PUMP_DRAIN_PORT, PUMP_DRAIN_PIN);
}

// Остановить дренажный насос;
void HW_StopPumpDrain (void) {
    GPIO_SET_PIN(PUMP_DRAIN_PORT, PUMP_DRAIN_PIN);
}

// Включить нагреватель (ТЭН) — начать нагрев воды PA5.
void HW_EnableHeater (void) {
    GPIO_SET_PIN(TEN_PORT,TEN_PIN);
}

// Отключить нагреватель — прекратить нагрев.
void HW_DisableHeater (void) {
    GPIO_RESET_PIN(TEN_PORT,TEN_PIN);
}

// Вспомогательная функция для преобразования сырого значения в градусы (uint8_t)
static uint8_t raw_to_celsius(uint16_t raw)
{
    if (raw <= ADC_AT_10C) return 10;
    if (raw >= ADC_AT_91C) return 91;
    uint32_t temp = 10 + (uint32_t)(raw - ADC_AT_10C) * (91 - 10) / (ADC_AT_91C - ADC_AT_10C);
    return (uint8_t)temp;
}

// Основная функция получения температуры
uint8_t HW_GetTemperature(void)
{
    uint32_t now = SysTimer_ms;
    
    // Проверяем, нужно ли обновить значение
    if (now - last_update >= TEMP_UPDATE_MS)
    {
        // 1. Читаем сырое значение с АЦП (канал 0 – PC0)
        uint16_t raw = read_ADC(0);
        
        // 2. Экспоненциальный фильтр (инициализация при первом вызове)
        if (last_update == 0) {
            filtered_raw = raw;                     // первое значение – как есть
        } else {
            // filtered_raw = filtered_raw + (raw - filtered_raw) / TEMP_FILTER_COEF
            int32_t diff = (int32_t)raw - (int32_t)filtered_raw;
            filtered_raw += diff / TEMP_FILTER_COEF;
        }
        
        // 3. Преобразуем отфильтрованное значение в градусы
        last_temp = raw_to_celsius(filtered_raw);
        
        // 4. Запоминаем время обновления
        last_update = now;
    }
    
    return last_temp;
}

// Запустить мотор в режиме стирки (медленное вращение с реверсом)
void HW_StartMotorWash(void) {
    Motor_StartWash();
}

// Запустить мотор в режиме отжима с заданными оборотами
void HW_StartMotorSpin(uint16_t rpm) {
    Motor_StartSpin(rpm);
}

// Остановить двигатель (выключить симистор и реле)
void HW_StopMotor(void) {
    Motor_Stop();
}

// Остановить всё оборудование — аварийная или безопасная остановка всех исполнительных устройств.
void HW_StopAll (void) {
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

// Сигнализировать об окончании цикла — включить индикатор/зуммер о завершении.
void HW_IndicateComplete (void) {

}

// Индикация ошибки
void HW_IndicateError(void) {

}

// Инициализация зуммера (настройка таймера и пина) PORTB 8
void HW_BuzzerInit(void)
{
    
    // Включить тактирование TIM4 и порта B
    RCC->APB1ENR |= RCC_APB1ENR_TIM4EN;
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN | RCC_APB2ENR_AFIOEN;

    // Настройка PB8 как альтернативная функция (TIM4_CH3)
    GPIOB->CRH &= ~(GPIO_CRH_CNF8 | GPIO_CRH_MODE8);
    GPIOB->CRH |= GPIO_CRH_CNF8_1;   // альтернативная функция push-pull
    GPIOB->CRH |= GPIO_CRH_MODE8_0;  // выход 10 МГц (можно 2 МГц)

    // Базовая настройка таймера: пока выключен
    TIM4->CR1 = 0;
    TIM4->CCER = 0;

    // Включить прерывание TIM4
    NVIC_EnableIRQ(TIM4_IRQn);
    NVIC_SetPriority(TIM4_IRQn, 4);   // приоритет, например, 1
    
}

// Включение звука на заданной частоте (freq > 0)
static void buzzer_set_freq(uint16_t freq)
{
    if (freq == 0) {
        // Выключить ШИМ и прерывания
        TIM4->CR1 &= ~TIM_CR1_CEN;
        TIM4->DIER &= ~TIM_DIER_UIE;   // запретить прерывания
        TIM4->CCER &= ~TIM_CCER_CC3E;
        buzzer_active = 0;
        buzzer_timer_running = 0;
        return;
    }

    // Расчёт PSC и ARR (как у вас)
    uint32_t psc = 3600 - 1;
    uint32_t arr = 10000 / freq;
    if (arr < 2) arr = 2;
    
    TIM4->PSC = psc;
    TIM4->ARR = arr - 1;
    TIM4->CCR3 = arr / 2;
    
    TIM4->CCMR2 &= ~TIM_CCMR2_OC3M;
    TIM4->CCMR2 |= TIM_CCMR2_OC3M_2 | TIM_CCMR2_OC3M_1;
    TIM4->CCMR2 |= TIM_CCMR2_OC3PE;
    TIM4->CCER |= TIM_CCER_CC3E;
    
    // Сбросить счётчик прерываний
    TIM4->CNT = 0;
    TIM4->SR &= ~TIM_SR_UIF;          // очистить флаг обновления
    TIM4->DIER |= TIM_DIER_UIE;       // разрешить прерывание по обновлению
    
    TIM4->EGR |= TIM_EGR_UG;
    TIM4->CR1 |= TIM_CR1_CEN;
    
    buzzer_active = 1;
}

/* Основная функция (вызывается из приложения)
* freq - частота звучания
* duration_ms - длительность звучаня
*/
void HW_Buzzer(uint16_t freq, uint16_t duration_ms)
{
    if (freq == 0 || duration_ms == 0) {
        buzzer_set_freq(0);
        buzzer_timer_running = 0;
        buzzer_remaining_ticks = 0;
        return;
    }
    
    // Расчёт количества периодов
    uint32_t ticks_needed = ((uint32_t)duration_ms * freq) / 1000;
    if (ticks_needed == 0) ticks_needed = 1;
    
    buzzer_set_freq(freq);          // запускает ШИМ и прерывания
    buzzer_remaining_ticks = ticks_needed;
    buzzer_timer_running = 1;       // теперь прерывание будет считать
}

// Функция для выключения (удобно вызывать из обработчика событий)
void HW_BuzzerStop(void)
{
    HW_Buzzer(0, 0);
}

void TIM4_IRQHandler(void)
{
    if (TIM4->SR & TIM_SR_UIF) {
        TIM4->SR &= ~TIM_SR_UIF;    // обязательно сбросить флаг!
        
        if (buzzer_timer_running && buzzer_remaining_ticks > 0) {
            buzzer_remaining_ticks--;
            if (buzzer_remaining_ticks == 0) {
                buzzer_set_freq(0);   // выключить звук
                buzzer_timer_running = 0;
            }
        }
    }
}

