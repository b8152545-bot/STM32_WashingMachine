
#include "selection_menu.h"
#include <stdio.h>
#include <string.h>

//Вывод отладочной информации
#define ENABLE_OUT_INFO   0

//--06,05,2026
#define SETUP_MODE_TIME     0
#define SETUP_MODE_SERVICE  1
#define SETUP_MODE_DIAG     2   // новый пункт


// --- НАСТРОЙКИ СИСТЕМЫ И ЖЕЛЕЗА ---
#define EEPROM_I2C_NUM        1
#define EEPROM_I2C_ADDR       (0x50 << 1)   // исправлено
#define EEPROM_PAGE_SIZE      64

#define SCREEN_WIDTH          320           // можно определить в общем заголовке
#define SCREEN_HEIGHT         240

// --- КООРДИНАТЫ И РАЗМЕРЫ UI ---
#define X_TIME 27
#define Y_TIME 186
#define X_DATE 35
#define Y_DATE 140

#define FONT_TIME_W 33
#define FONT_TIME_H 37
#define FONT_DATE_W 25
#define FONT_DATE_H 29

#define PP_X 1      // ориентация прогресс бара
#define PP_Y 200    // ориентация прогресс бара

//--06,05,2026
static uint8_t setup_main_selection = 0;   // 0 - время, 1 - сервис
static uint8_t service_item = 0;           // текущий пункт в сервисном меню
static uint8_t in_calibration_mode = 0;   // флаг режима калибровки
static uint16_t calib_delay = 9500;       // текущая задержка в калибровке

// Состояние автотюна
static uint8_t calib_mode = 0; // 0 – выбор, 1 – manual, 2 – auto-tune
static uint8_t auto_tune_started = 0;

// --- ПЕРЕМЕННЫЕ ЖЕЛЕЗА И СИСТЕМЫ ---
RTC_UNIT datetime;
RTC_UNIT edit_time;
int8_t enc_dir = 0;
uint8_t btn_event = 0;
uint8_t display = 1;   // будущий zero-cross. 0 - нет сигнала, 1 - есть сигнал

//--06,05,2026
// Список устройств для сервисного меню
typedef enum {
    SVC_DOOR,
    SVC_VALVE1,
    SVC_VALVE2,
    SVC_PUMP_CIRC,
    SVC_PUMP_DRAIN,
    SVC_TRIAC,
    SVC_HEATER,
    SVC_DIR_FWD,
    SVC_DIR_REV,
    SVC_RELAY3,
    SVC_POWER_MGMT,
    SVC_BUZZER,
    SVC_COUNT
} ServiceDevice;

static uint8_t service_active[SVC_COUNT] = {0};   // 1 = включено

//--06,05,2026
const char* const service_names[SVC_COUNT] = {
    "Door       ",
    "Valve 1    ",
    "Valve 2    ",
    "Pump circul",
    "Pump drain ",
    "Triac      ",
    "Heater     ",
    "Dir FWD    ",
    "Dir REV    ",
    "Relay 3    ",
    "Power Mgmt ",
    "Buzzer     "
};

// Прототипы функций
void DrawCurrentProgram(void);
void ResetProgramDisplayCache(void);

//--06,05,2026
// Прототипы тестовых функций (новые)
static void TestDoor_On(void);
static void TestDoor_Off(void);
static void TestValve1_On(void);
static void TestValve1_Off(void);
static void TestValve2_On(void);
static void TestValve2_Off(void);
static void TestPumpCirc_On(void);
static void TestPumpCirc_Off(void);
static void TestPumpDrain_On(void);
static void TestPumpDrain_Off(void);
static void TestTriac_On(void);
static void TestTriac_Off(void);
static void TestHeater_On(void);
static void TestHeater_Off(void);
static void TestDirFWD_On(void);
static void TestDirFWD_Off(void);
static void TestDirREV_On(void);
static void TestDirREV_Off(void);
static void TestRelay3_On(void);
static void TestRelay3_Off(void);
static void TestPowerMgmt_On(void);
static void TestPowerMgmt_Off(void);
static void TestBuzzer_On(void);
static void TestBuzzer_Off(void);

// Массивы указателей на функции 
static void (*test_func_on[SVC_COUNT])(void) = {
    TestDoor_On,
    TestValve1_On,
    TestValve2_On,
    TestPumpCirc_On,
    TestPumpDrain_On,
    TestTriac_On,
    TestHeater_On,
    TestDirFWD_On,
    TestDirREV_On,
    TestRelay3_On,
    TestPowerMgmt_On,
    TestBuzzer_On
};

static void (*test_func_off[SVC_COUNT])(void) = {
    TestDoor_Off,
    TestValve1_Off,
    TestValve2_Off,
    TestPumpCirc_Off,
    TestPumpDrain_Off,
    TestTriac_Off,
    TestHeater_Off,
    TestDirFWD_Off,
    TestDirREV_Off,
    TestRelay3_Off,
    TestPowerMgmt_Off,
    TestBuzzer_Off
};

// Глобальная (файловая) переменная для DrawCurrentProgram
static WashProgram last_program = PROG_COUNT_;

// Прототип функции
void DrawProgramParameters(WashProgram prog);
void ResetProgramDisplayCache(void); 

// Глобальные переменные автомата
SystemState system_state = STATE_POWER_ON;
WashProgram current_program = PROG_COTTON;
uint16_t power_on_timer = 0;

// ---------------------------------------------------------
// Экраны редактирования даты и времени
// ---------------------------------------------------------
typedef enum {
    STATE_MAIN,       // Главный экран: время и дата
    STATE_SET_TIME,   // Редактирование времени (ЧЧ:ММ:СС)
    STATE_SET_DATE    // Редактирование даты (ДД.ММ.ГГ)
} AppState;

static AppState app_state = STATE_MAIN; // Локально для меню
static uint8_t current_field = 0;

// ------------------------------------------------------------------
//  Основная функция настройки (выбор режима + подменю)
// ------------------------------------------------------------------
void Setup_ServiceMenu(void)
{
    static uint8_t need_redraw = 1;
    static uint8_t in_time_menu = 0;
    static uint8_t in_service_menu = 0;
    static uint8_t in_diag_menu = 0;
    static uint32_t last_diag_update = 0;
    static uint8_t prev_level = 255, prev_rpm = 255;
    static uint8_t prev_zerocross = 255, prev_door = 255;
    static uint16_t prev_temp = 65535;

    // ---------------------------------------------------------------
    // 1. Режим выбора (Время или Сервис или Вывод данных)
    // ---------------------------------------------------------------
    if (!in_time_menu && !in_service_menu && !in_diag_menu)
    {
        if (need_redraw)
        {
            // Очищаем всю область экрана
            ILI9341_FillRect_DMA(1, 1, SCREEN_WIDTH - 2, SCREEN_HEIGHT - 2, COLOR_BLACK);

            ILI9341_PrintString(80, 30, "SETTING", Font_K_95_18x21, 18, 21, COLOR_WHITE, COLOR_BLACK);

            ILI9341_PrintString(50, 80, "1.Time/Date", Font_K_95_18x21, 18, 21, COLOR_WHITE, COLOR_BLACK);
            ILI9341_PrintString(50, 120, "2.Service test", Font_K_95_18x21, 18, 21, COLOR_WHITE, COLOR_BLACK);
            ILI9341_PrintString(50, 160, "3.Diagnostics", Font_K_95_18x21, 18, 21, COLOR_WHITE, COLOR_BLACK);
            ILI9341_PrintString(50, 200, "4.Calibration", Font_K_95_18x21, 18, 21, COLOR_WHITE, COLOR_BLACK);
            need_redraw = 0;
        }

        // Курсор выбора
        static uint8_t last_sel = 0xFF;
        if (last_sel != setup_main_selection)
        {
            ILI9341_PrintString(20, 80 + last_sel * 40, "$", 
                                Font_25x29, 25, 29, COLOR_BLACK, COLOR_BLACK);
            ILI9341_PrintString(20, 80 + setup_main_selection * 40, "$", 
                                Font_25x29, 25, 29, COLOR_YELLOW, COLOR_BLACK);
            last_sel = setup_main_selection;
        }

        if (enc_dir != 0) // Проверка границ
        {
            int8_t new_sel = setup_main_selection + enc_dir;
            if (new_sel < 0) new_sel = 3;
            if (new_sel > 3) new_sel = 0;
            setup_main_selection = new_sel;
            HW_Buzzer(1200, 7);
        }

        if (btn_event == 1)
        {
            if (setup_main_selection == SETUP_MODE_TIME) {
                in_time_menu = 1;
                app_state = STATE_MAIN;
                current_field = 0;
                edit_time = rtc_GetTime();
                need_redraw = 1;
                ILI9341_FillRect_DMA(1, 1, SCREEN_WIDTH - 2, SCREEN_HEIGHT - 2, COLOR_BLACK);
            } else if (setup_main_selection == 3) {
                in_calibration_mode = 1;
                need_redraw = 1;
                ILI9341_FillRect_DMA(1, 1, SCREEN_WIDTH - 2, SCREEN_HEIGHT - 2, COLOR_BLACK);
                calib_delay = 9500;          // <-- сброс на минимальную мощность
                triac_delay = calib_delay;   // <-- применить сразу
                Motor_EnableForward();
            } else if (setup_main_selection == SETUP_MODE_SERVICE) {
                in_service_menu = 1;
                service_item = 0;
                need_redraw = 1;
            } else {
                in_diag_menu = 1;   // новый флаг
                need_redraw = 1;
            }
        }
        else if (btn_event == 3)
        {
            ILI9341_FillRect_DMA(1, 1, SCREEN_WIDTH - 2, SCREEN_HEIGHT - 2, COLOR_BLACK);
            ResetProgramDisplayCache();   // СБРОС КЕША ПРОГРАММЫ
            DrawCurrentProgram();
            display = 1;
            DrawProgramParameters(current_program);
            system_state = STATE_SELECT_PROGRAM;
            need_redraw = 1;
            in_time_menu = 0;
            in_service_menu = 0;

            in_calibration_mode = 0;
            need_redraw = 1;
            Motor_Stop();   // <-- выключить реле и обнулить задержку
        }
        return;
    }

    // ---------------------------------------------------------------
    // 2. Режим установки времени 
    // ---------------------------------------------------------------
    if (in_time_menu)
    {
        static uint8_t need_redraw_time = 1;
        static RTC_UNIT edit_time_local;

        if (need_redraw_time)
        {
            edit_time_local = rtc_GetTime();
            ResetTimeDisplayCache();
            ResetDateDisplayCache();
            need_redraw_time = 0;
        }

        switch (app_state)
        {
            case STATE_MAIN:
                UpdateRTC_TimeFull(X_TIME, Y_TIME, Font_33x37, FONT_TIME_W, FONT_TIME_H, COLOR_WHITE, COLOR_BLACK);
                UpdateRTC_DateFull(X_DATE, Y_DATE, Font_25x29, FONT_DATE_W, FONT_DATE_H, COLOR_WHITE, COLOR_BLACK);

                if (btn_event == 1 || btn_event == 2)
                {
                    edit_time_local = rtc_GetTime();
                    current_field = 0;
                    app_state = (btn_event == 1) ? STATE_SET_TIME : STATE_SET_DATE;
                    if (app_state == STATE_SET_TIME) {
                        DisplayTime(X_TIME, Y_TIME, Font_33x37, FONT_TIME_W, FONT_TIME_H, COLOR_WHITE, COLOR_BLACK, edit_time_local, current_field);
                    } else {
                        DisplayDate(X_DATE, Y_DATE, Font_25x29, FONT_DATE_W, FONT_DATE_H, COLOR_WHITE, COLOR_BLACK, edit_time_local, current_field);
                    }
                }
                else if (btn_event == 3)
                {
                    ILI9341_FillRect_DMA(1, 1, SCREEN_WIDTH - 2, SCREEN_HEIGHT - 2, COLOR_BLACK);
                    in_time_menu = 0;
                    need_redraw = 1;
                    app_state = STATE_MAIN;
                    need_redraw_time = 1;        // <-- ДОБАВИТЬ ЭТУ СТРОКУ, для повторного вывода
                }
                break;

            case STATE_SET_TIME:
                if (enc_dir != 0) {
                    if (current_field == 0) edit_time_local.hours   = (edit_time_local.hours   + enc_dir + 24) % 24;
                    if (current_field == 1) edit_time_local.minutes = (edit_time_local.minutes + enc_dir + 60) % 60;
                    if (current_field == 2) edit_time_local.seconds = (edit_time_local.seconds + enc_dir + 60) % 60;
                    DisplayTime(X_TIME, Y_TIME, Font_33x37, FONT_TIME_W, FONT_TIME_H, COLOR_WHITE, COLOR_BLACK, edit_time_local, current_field);
                }
                if (btn_event == 1 || btn_event == 3) {
                    if (btn_event == 3 || current_field == 2) {
                        if (btn_event == 1) rtc_SetTime(edit_time_local);
                        uint16_t x_clear = X_TIME + ((current_field == 0) ? 0 : (current_field == 1) ? 3 * FONT_TIME_W : 6 * FONT_TIME_W);
                        ILI9341_FillRect_DMA(x_clear, Y_TIME + FONT_TIME_H, 2 * FONT_TIME_W, 4, COLOR_BLACK);
                        app_state = STATE_MAIN;
                    } else {
                        current_field++;
                        DisplayTime(X_TIME, Y_TIME, Font_33x37, FONT_TIME_W, FONT_TIME_H, COLOR_WHITE, COLOR_BLACK, edit_time_local, current_field);
                    }
                }
                break;

            case STATE_SET_DATE:
                if (enc_dir != 0) {
                    if (current_field == 0) edit_time_local.date  = (edit_time_local.date  - 1 + enc_dir + 31) % 31 + 1;
                    if (current_field == 1) edit_time_local.month = (edit_time_local.month - 1 + enc_dir + 12) % 12 + 1;
                    if (current_field == 2) edit_time_local.year += enc_dir;
                    DisplayDate(X_DATE, Y_DATE, Font_25x29, FONT_DATE_W, FONT_DATE_H, COLOR_WHITE, COLOR_BLACK, edit_time_local, current_field);
                }
                if (btn_event == 1 || btn_event == 3) {
                    if (btn_event == 3 || current_field == 2) {
                        if (btn_event == 1) rtc_SetTime(edit_time_local);
                        uint16_t x_clear = X_DATE + ((current_field == 0) ? 0 : (current_field == 1) ? 3 * FONT_DATE_W : 6 * FONT_DATE_W);
                        uint16_t clear_width = (current_field == 2) ? 4 * FONT_DATE_W : 2 * FONT_DATE_W;
                        ILI9341_FillRect_DMA(x_clear, Y_DATE + FONT_DATE_H, clear_width, 4, COLOR_BLACK);
                        app_state = STATE_MAIN;
                    } else {
                        current_field++;
                        DisplayDate(X_DATE, Y_DATE, Font_25x29, FONT_DATE_W, FONT_DATE_H, COLOR_WHITE, COLOR_BLACK, edit_time_local, current_field);
                    }
                }
                break;
        }
        return;
    }

    // ---------------------------------------------------------------
    // 3. Сервисное меню (список устройств) – СКРОЛЛИНГ
    // ---------------------------------------------------------------
    if (in_service_menu)
    {
        #define VISIBLE_ITEMS 9
        #define ITEM_HEIGHT   20

        if (need_redraw)
        {
            ILI9341_FillRect_DMA(1, 1, SCREEN_WIDTH - 2, SCREEN_HEIGHT - 2, COLOR_BLACK);
            ILI9341_PrintString(15, 10, "TEST: 2-ON 1-OFF", Font_K_95_18x21, 18, 21, COLOR_WHITE, COLOR_BLACK);
            need_redraw = 0;
        }

        // Скроллинг: first_visible – индекс первого отображаемого пункта
        static int8_t first_visible = 0;
        if (service_item < first_visible)
            first_visible = service_item;
        else if (service_item >= first_visible + VISIBLE_ITEMS)
            first_visible = service_item - VISIBLE_ITEMS + 1;

        // База для списка (сдвинута вниз на 40 пикселей)
        uint16_t y_base = 40;
        for (uint8_t i = first_visible; i < first_visible + VISIBLE_ITEMS && i < SVC_COUNT; i++)
        {
            uint16_t y = y_base + (i - first_visible) * ITEM_HEIGHT;
            uint16_t color = service_active[i] ? COLOR_GREEN : COLOR_WHITE; // менем цвет включенного пункта
            ILI9341_PrintString(60, y, (char*)service_names[i], Font_K_95_18x21, 18, 21, color, COLOR_BLACK);
        }

        // Курсор – исправленное переключение без дублирования
        static int8_t last_item = -1;
        static uint16_t last_cursor_y = 0;
        if (last_item != service_item || need_redraw)
        {
            // Стираем предыдущий курсор (если он был нарисован)
            if (last_item != -1)
            {
                //ILI9341_FillRect_DMA(30, last_cursor_y, 16, 16, COLOR_BLACK);
                ILI9341_PrintString(20, last_cursor_y, "$", Font_25x29, 25, 29, COLOR_BLACK, COLOR_BLACK);
            }
            // Рисуем новый курсор, только если текущий пункт видим
            if (service_item >= first_visible && service_item < first_visible + VISIBLE_ITEMS)
            {
                uint16_t new_y = y_base + (service_item - first_visible) * ITEM_HEIGHT;
                //ILI9341_FillRect_DMA(30, new_y, 16, 16, COLOR_YELLOW);
                ILI9341_PrintString(20, new_y, "$", Font_25x29, 25, 29, COLOR_YELLOW, COLOR_BLACK);
                last_cursor_y = new_y;
                last_item = service_item;
            }
            else
            {
                // Пункт вне экрана – курсор не рисуем, но запоминаем, что он неактивен
                last_item = -1;
            }
        }

        if (enc_dir != 0)
        {
            int16_t new_item = service_item + enc_dir;
            if (new_item < 0) new_item = SVC_COUNT - 1;
            if (new_item >= SVC_COUNT) new_item = 0;
            service_item = new_item;
            HW_Buzzer(1350, 10);
        }

        if (btn_event == 2) {
            if (test_func_on[service_item] != NULL) {
                test_func_on[service_item]();
                service_active[service_item] = 1;
            }
        }
        else if (btn_event == 1) {
            if (test_func_off[service_item] != NULL) {
                test_func_off[service_item]();
                service_active[service_item] = 0;
            }
        }
        else if (btn_event == 3)   // длинное нажатие – выход (без изменений)
        {
            ILI9341_FillRect_DMA(1, 1, SCREEN_WIDTH - 2, SCREEN_HEIGHT - 2, COLOR_BLACK);
            in_service_menu = 0;
            need_redraw = 1;
            // Выключаем всё, если что-то осталось активно
            for (uint8_t i = 0; i < SVC_COUNT; i++) {
                if (service_active[i] && test_func_off[i] != NULL) {
                    test_func_off[i]();
                    service_active[i] = 0;
                }
            }
        }
        return;
    }

    // ---------------------------------------------------------------
    // 4. Вывод данных
    // ---------------------------------------------------------------
    if (in_diag_menu) {
        if (need_redraw) {
            ILI9341_FillRect_DMA(1, 1, SCREEN_WIDTH-2, SCREEN_HEIGHT-2, COLOR_BLACK);
            ILI9341_PrintString(20, 18, "= DIAGNOSTICS =", Font_K_95_18x21, 18, 21, COLOR_WHITE, COLOR_BLACK);
            // подписи полей
            ILI9341_PrintString(20, 58, "Temp:", Font_K_95_18x21, 18, 21, COLOR_WHITE, COLOR_BLACK);
            ILI9341_PrintString(20, 98, "Water:", Font_K_95_18x21, 18, 21, COLOR_WHITE, COLOR_BLACK);
            ILI9341_PrintString(20, 138, "ZeroCross:", Font_K_95_18x21, 18, 21, COLOR_WHITE, COLOR_BLACK);
            ILI9341_PrintString(20, 178, "Motor RPM:", Font_K_95_18x21, 18, 21, COLOR_WHITE, COLOR_BLACK);
            ILI9341_PrintString(20, 218, "Door:", Font_K_95_18x21, 18, 21, COLOR_WHITE, COLOR_BLACK);
            need_redraw = 0;
            last_diag_update = 0;
        }

        // Обновляем данные не чаще 200 мс, чтобы не было мерцания
        uint32_t now = SysTimer_ms;
        if (now - last_diag_update >= 200) {
            last_diag_update = now;

            uint16_t temp =  read_ADC(0); //HW_GetTemperature();

            uint8_t level = Pressostat_GetLevel();
            // Проверка zero-cross – если last_zero_cross_time не устарело
            uint8_t zc_ok = (now - last_zero_cross_time < 50) ? 1 : 0;
            uint8_t door_closed = HW_IsDoorClosed();
           
            uint16_t rpm = HW_GetRPM();// 

            // Выводим только изменившиеся значения (без мерцания всей строки)
            if (temp != prev_temp) {
                DrawNumber_NoFlicker(140, 58, temp, IS_FLOAT_NO, MAX_ZNAK_01, ALIGN_LEFT, MAX_01,
                                    Font_25x29, 25, 29, COLOR_WHITE, COLOR_BLACK);
                prev_temp = temp;
            }
            if (level != prev_level) {
                DrawNumber_NoFlicker(140, 98, level, IS_FLOAT_NO, MAX_ZNAK_01, ALIGN_LEFT, MAX_01,
                                    Font_25x29, 25, 29, COLOR_WHITE, COLOR_BLACK);
                prev_level = level;
            }
            if (zc_ok != prev_zerocross) {
                const char* zc_str = zc_ok ? " OK!" : "ERROR";
                ILI9341_FillRect_DMA(210, 138, 100, 29, COLOR_BLACK);
                ILI9341_PrintString(210, 138, zc_str, Font_K_95_18x21, 18, 21, COLOR_GREEN, COLOR_BLACK);
                prev_zerocross = zc_ok;
            }
            if (rpm != prev_rpm) {
                DrawNumber_NoFlicker(210, 178, rpm, IS_FLOAT_NO, MAX_ZNAK_04, ALIGN_LEFT, MAX_0001,
                                    Font_25x29, 25, 29, COLOR_WHITE, COLOR_BLACK);
                prev_rpm = rpm;
            }
            if (door_closed != prev_door) {
                const char* door_str = door_closed ? "closed" : "open";
                ILI9341_FillRect_DMA(200, 218, 100, 29, COLOR_BLACK);
                ILI9341_PrintString(200, 218, door_str, Font_K_95_18x21, 18, 21, COLOR_GREEN, COLOR_BLACK);
                prev_door = door_closed;
            }
        }

        // Выход по длинному нажатию (btn_event == 3)
        if (btn_event == 3) {
            ILI9341_FillRect_DMA(1, 1, SCREEN_WIDTH-2, SCREEN_HEIGHT-2, COLOR_BLACK);
            in_diag_menu = 0;
            need_redraw = 1;
            // сбросить кеш предыдущих значений, чтобы при следующем входе всё перерисовалось
            //prev_temp = prev_level = prev_rpm = 0;    // - оригинал
            //prev_zerocross = prev_door = 0;           // - оригинал

            // Фикс из аудита неполный: 
            // нужно сбросить все пять кеш-переменных к «заведомо чужим» значениям, 
            // не только prev_temp:
            prev_temp      = 65535;
            prev_level     = 255;
            prev_rpm       = 255;
            prev_zerocross = 255;
            prev_door      = 255;
        }
        return;
    }
}

// Режим калибровки: ручное управление задержкой для снятия характеристики
void Calibration_Mode(void)
{
    static uint8_t need_redraw = 1;
    static uint32_t last_display_update = 0;
    static uint8_t selected = 0;           // 0 – manual, 1 – auto
    static uint8_t finished_displayed = 0; // <--- ЕДИНСТВЕННАЯ статическая переменная
    uint16_t rpm = 0;
    char buf[32];

    #if ENABLE_UART_LOG
    char msg[64];
    sprintf(msg, "Auto-tune started\n");
    UART_Send_DMA((const uint8_t*)msg, strlen(msg));
    /*Лог покажет:
    st= – текущее состояние (0-3)
    acc= – точность (accuracy)
    out= – текущая задержка (triac_delay)
    rpm= – текущие обороты (фильтрованные)
    steady=, max=, min= – для анализа пересечений*/
    #endif

    // --- 1. Если только вошли в калибровку и ещё не выбрали режим, показываем меню выбора ---
        if (need_redraw && calib_mode == 0) {
        ILI9341_FillRect_DMA(1, 1, SCREEN_WIDTH - 2, SCREEN_HEIGHT - 2, COLOR_BLACK);
        ILI9341_PrintString(10, 10, "== CALIBRATION ==", Font_K_95_18x21, 18, 21, COLOR_WHITE, COLOR_BLACK);
        ILI9341_PrintString(20, 60, "1. Manual", Font_K_95_18x21, 18, 21, COLOR_WHITE, COLOR_BLACK);
        ILI9341_PrintString(20, 100, "2. Auto-tune", Font_K_95_18x21, 18, 21, COLOR_WHITE, COLOR_BLACK);
        
        // ДОБАВЛЕНО: Третий пункт
        ILI9341_PrintString(20, 140, "3. Edit PID", Font_K_95_18x21, 18, 21, COLOR_WHITE, COLOR_BLACK);
        
        ILI9341_PrintString(20, 180, "Use ENC, START to confirm", Font_K_95_18x21, 18, 21, COLOR_WHITE, COLOR_BLACK);
        
        ILI9341_PrintString(5, 60, ">", Font_K_95_18x21, 18, 21, COLOR_YELLOW, COLOR_BLACK);
        need_redraw = 0;
        calib_mode = 0;
        auto_tune_started = 0;
        selected = 0;
        AutoTune_Reset();
    }

    // --- 2. Режим выбора (calib_mode == 0) ---
    if (calib_mode == 0) {
        // Обработка энкодера
        if (enc_dir != 0) {
            // Стираем старый курсор
            ILI9341_PrintString(5, 60 + selected * 40, " ", Font_K_95_18x21, 18, 21, COLOR_BLACK, COLOR_BLACK);
            // ИЗМЕНЕНО: теперь 3 пункта (0, 1, 2)
            selected = (selected + enc_dir + 3) % 3; 
            // Рисуем новый курсор
            ILI9341_PrintString(5, 60 + selected * 40, ">", Font_K_95_18x21, 18, 21, COLOR_YELLOW, COLOR_BLACK);
            HW_Buzzer(1200, 5);
        }
        // Подтверждение выбора
        if (btn_event == 1) {
            // ИЗМЕНЕНО: выбор 3-го режима
            calib_mode = (selected == 0) ? 1 : ((selected == 1) ? 2 : 3);
            need_redraw = 1;   // чтобы перерисовать экран для выбранного режима
            if (calib_mode == 2) {
                // steady=15% (около 500-700 об/мин), step=2% (мягкий импульс), window=40 RPM
                AutoTune_Init(NORMAL, 15, 2, 8000, 40.0f, 6000, 20);
                
                // ИСПРАВЛЕНО: ставим флаг запуска и подаем начальную мощность!
                auto_tune_started = 1;
                //MotorControl_ApplyPercent(30);   // -- баг — забыли, что steady уже 15
                Motor_EnableForward();
                
                // Сразу очищаем экран для вывода статуса
                ILI9341_FillRect_DMA(1, 1, SCREEN_WIDTH - 2, SCREEN_HEIGHT - 2, COLOR_BLACK);
                ILI9341_PrintString(10, 10, "== AUTO-TUNE ==", Font_K_95_18x21, 18, 21, COLOR_WHITE, COLOR_BLACK);
                last_display_update = 0;
            }
        }
        return; // выходим, не переходя к остальным режимам
    }

    // --- 3. Ручной режим (calib_mode == 1) ---
    if (calib_mode == 1) {
        if (need_redraw) {
            ILI9341_FillRect_DMA(1, 1, SCREEN_WIDTH - 2, SCREEN_HEIGHT - 2, COLOR_BLACK);
            ILI9341_PrintString(10, 10, "== MANUAL ==", Font_K_95_18x21, 18, 21, COLOR_WHITE, COLOR_BLACK);
            need_redraw = 0;
        }
        // Обработка энкодера (изменение задержки)
        if (enc_dir != 0) {
            int16_t new_delay = calib_delay + enc_dir * 20;
            if (new_delay < 500) new_delay = 500;
            if (new_delay > 9500) new_delay = 9500;
            calib_delay = new_delay;
            triac_delay = calib_delay;
            HW_Buzzer(1200, 5);
            last_display_update = 0;
        }
        // Обновление информации
        uint32_t now = SysTimer_ms;
        if (now - last_display_update >= 200) {
            last_display_update = now;
            rpm = HW_GetRPM();
            ILI9341_PrintString(10, 60, "Delay:", Font_K_95_18x21, 18, 21, COLOR_WHITE, COLOR_BLACK);
            sprintf(buf, "%4d us", calib_delay);
            ILI9341_PrintString(100, 60, buf, Font_K_95_18x21, 18, 21, COLOR_YELLOW, COLOR_BLACK);
            ILI9341_PrintString(10, 100, "RPM:", Font_K_95_18x21, 18, 21, COLOR_WHITE, COLOR_BLACK);
            sprintf(buf, "%4d", rpm);
            ILI9341_PrintString(100, 100, buf, Font_K_95_18x21, 18, 21, COLOR_YELLOW, COLOR_BLACK);
            ILI9341_PrintString(10, 140, "Step:", Font_K_95_18x21, 18, 21, COLOR_WHITE, COLOR_BLACK);
            ILI9341_PrintString(100, 140, "20 us", Font_K_95_18x21, 18, 21, COLOR_YELLOW, COLOR_BLACK);
            ILI9341_PrintString(10, 200, "Hold START to exit", Font_K_95_18x21, 18, 21, COLOR_GREEN, COLOR_BLACK);
        }
        // Выход по длинному нажатию
        if (btn_event == 3) {
            ILI9341_FillRect_DMA(1, 1, SCREEN_WIDTH - 2, SCREEN_HEIGHT - 2, COLOR_BLACK);
            in_calibration_mode = 0;
            need_redraw = 1;
            Motor_Stop();
            setup_main_selection = 0;
            ResetProgramDisplayCache();
            DrawCurrentProgram();
            display = 1;
            DrawProgramParameters(current_program);
            system_state = STATE_SELECT_PROGRAM;
            calib_mode = 0;
            auto_tune_started = 0; // <--- ДОБАВИТЬ СЮДА
        }
        return;
    }

    // --- 4. Режим автотюна (calib_mode == 2) ---
    if (calib_mode == 2) {
        uint32_t now = SysTimer_ms;

        // Если только вошли в режим автотюна – сбрасываем флаг
        if (auto_tune_started && !finished_displayed) {
            finished_displayed = 0; // на случай, если уже был завершён
        }

        // Вызов тюнера – каждый цикл
        if (auto_tune_started) {
            // Фильтрация теперь внутри AutoTune_SetInput (сбрасывается через AutoTune_Init/Reset).
            // Локальный static filtered_rpm удалён — именно он не сбрасывался между запусками.
            AutoTune_SetInput((float)HW_GetRPM());

            AutoTune_Compute();
            MotorControl_ApplyPercent(AutoTune_GetOutput());
        }

        // Обновление экрана (не чаще 200 мс) – только если автонастройка ещё НЕ завершена
        if (!AutoTune_IsFinished() && (now - last_display_update >= 200)) {
            ILI9341_PrintString(10, 60, "St:", Font_K_95_18x21, 18, 21, COLOR_WHITE, COLOR_BLACK);
            sprintf(buf, "%d", AutoTune_GetState());
            ILI9341_PrintString(100, 60, buf, Font_K_95_18x21, 18, 21, COLOR_YELLOW, COLOR_BLACK);
            last_display_update = now;
            ILI9341_PrintString(10, 90, "Acc:", Font_K_95_18x21, 18, 21, COLOR_WHITE, COLOR_BLACK);
            uint8_t st = AutoTune_GetAccuracy();
            sprintf(buf, "%d%%", st);
            ILI9341_PrintString(100, 90, buf, Font_K_95_18x21, 18, 21, COLOR_YELLOW, COLOR_BLACK);
            ILI9341_PrintString(10, 120, "Delay:", Font_K_95_18x21, 18, 21, COLOR_WHITE, COLOR_BLACK);
            sprintf(buf, "%4d", triac_delay);
            ILI9341_PrintString(100, 120, buf, Font_K_95_18x21, 18, 21, COLOR_YELLOW, COLOR_BLACK);
            ILI9341_PrintString(10, 150, "RPM:", Font_K_95_18x21, 18, 21, COLOR_WHITE, COLOR_BLACK);
            sprintf(buf, "%4d", HW_GetRPM());
            ILI9341_PrintString(100, 150, buf, Font_K_95_18x21, 18, 21, COLOR_YELLOW, COLOR_BLACK);
            // Дополнительные строки при желании
        }

        // Проверка завершения (выполнится один раз)
        if (AutoTune_IsFinished()) {
            Motor_Stop(); // останавливаем двигатель
            if (!finished_displayed) {
                finished_displayed = 1;   // чтобы не повторять вывод

                float kp, ki, kd;
                AutoTune_GetPID(&kp, &ki, &kd);

                // Сохраняем коэффициенты в EEPROM
                eeprom_params.pid_kp = (int32_t)(kp * 1000);
                eeprom_params.pid_ki = (int32_t)(ki * 1000);
                eeprom_params.pid_kd = (int32_t)(kd * 1000);
                SaveProgramParams();
                // Применяем новые коэффициенты к работающему регулятору без перезагрузки
                MotorControl_Init();

                // Очищаем экран и выводим финальную информацию
                ILI9341_FillRect_DMA(1, 1, SCREEN_WIDTH - 2, SCREEN_HEIGHT - 2, COLOR_BLACK);
                ILI9341_PrintString(10, 10, "TUNE COMPLETE!", Font_K_95_18x21, 18, 21, COLOR_GREEN, COLOR_BLACK);

                // без float printf (в newlib-nano без флага -u _printf_float %.2f выводит пустоту):
                int kp_x100 = (int)(kp * 100.0f + 0.5f);
                int ki_x100 = (int)(ki * 100.0f + 0.5f);
                int kd_x100 = (int)(kd * 100.0f + 0.5f);
                // ИСПРАВЛЕНО: Выводим Kp, Ki, Kd каждый на своей строке (чтобы влезло в экран)
                char buf2[32];
                snprintf(buf2, sizeof(buf2), "Kp=%d.%02d", kp_x100/100, kp_x100%100);
                ILI9341_PrintString(10, 50, buf2, Font_K_95_18x21, 18, 21, COLOR_YELLOW, COLOR_BLACK);
                
                snprintf(buf2, sizeof(buf2), "Ki=%d.%02d", ki_x100/100, ki_x100%100);
                ILI9341_PrintString(10, 75, buf2, Font_K_95_18x21, 18, 21, COLOR_YELLOW, COLOR_BLACK);
                
                snprintf(buf2, sizeof(buf2), "Kd=%d.%02d", kd_x100/100, kd_x100%100);
                ILI9341_PrintString(10, 100, buf2, Font_K_95_18x21, 18, 21, COLOR_YELLOW, COLOR_BLACK);
                
                ILI9341_PrintString(10, 130, "Saved to EEPROM:", Font_K_95_18x21, 18, 21, COLOR_WHITE, COLOR_BLACK);
                
                // Выводим сохраненное значение Kp (умноженное на 1000, т.е. 760)
                DrawNumber_NoFlicker(120, 160, (uint32_t)eeprom_params.pid_kp, IS_FLOAT_NO, MAX_ZNAK_04, ALIGN_RIGHT, MAX_0001,
                            Font_25x29, 25, 29, COLOR_YELLOW, COLOR_BLACK);
                            
                // ИСПРАВЛЕНО: укорачиваем текст, чтобы влез в экран
                ILI9341_PrintString(10, 200, "Hold START: exit", Font_K_95_18x21, 18, 21, COLOR_GREEN, COLOR_BLACK);

                auto_tune_started = 0;
                #if ENABLE_UART_LOG
                char msg[80];
                // БЫЛО:
                //sprintf(msg, "Auto-tune finished, Kp=%.2f Ki=%.2f Kd=%.2f\n", kp, ki, kd);
                // СТАЛО:
                sprintf(msg, "Auto-tune finished, Kp=%d.%02d Ki=%d.%02d Kd=%d.%02d\n",
                kp_x100/100, kp_x100%100,
                ki_x100/100, ki_x100%100,
                kd_x100/100, kd_x100%100);
                if (!UART_IsTxBusy()) UART_Send_DMA((const uint8_t*)msg, strlen(msg));
                #endif
            }

            // Ждём выхода по длинному нажатию
            if (btn_event == 3) {
                ILI9341_FillRect_DMA(1, 1, SCREEN_WIDTH - 2, SCREEN_HEIGHT - 2, COLOR_BLACK);
                in_calibration_mode = 0;
                need_redraw = 1;
                Motor_Stop();
                setup_main_selection = 0;
                ResetProgramDisplayCache();
                DrawCurrentProgram();
                display = 1;
                DrawProgramParameters(current_program);
                system_state = STATE_SELECT_PROGRAM;
                AutoTune_Reset();
                calib_mode = 0;
                finished_displayed = 0; // сброс для следующего запуска
                auto_tune_started = 0; // <--- ДОБАВИТЬ 
            }
            return; // выходим, чтобы не обрабатывать другие состояния
        }

        // Если автонастройка ещё не завершена, проверяем выход по длинному нажатию
        if (btn_event == 3) {
            // Принудительный выход
            ILI9341_FillRect_DMA(1, 1, SCREEN_WIDTH - 2, SCREEN_HEIGHT - 2, COLOR_BLACK);
            in_calibration_mode = 0;
            need_redraw = 1;
            Motor_Stop();
            setup_main_selection = 0;
            ResetProgramDisplayCache();
            DrawCurrentProgram();
            display = 1;
            DrawProgramParameters(current_program);
            system_state = STATE_SELECT_PROGRAM;
            AutoTune_Reset();
            calib_mode = 0;
            finished_displayed = 0;
            auto_tune_started = 0; // <--- ДОБАВИТЬ 
        }
        return;
    }

    // --- 5. Режим ручного редактирования PID (calib_mode == 3) ---
    
    /*Как это будет работать:
    *Заходишь в калибровку, выбираешь 3. Edit PID.
    *Экран покажет текущие значения (например, Kp: 0.755, Ki: 0.140, Kd: 1.040).
    *Вращение энкодера меняет значение выбранного параметра. Выбранный параметр выделен желтым цветом.
    *Короткое нажатие кнопки (START) переключает тебя с Kp на Ki, потом на Kd, потом обратно.
    *Длинное нажатие кнопки (START) сохраняет новые значения в EEPROM и выходит в главное меню.
    */
    if (calib_mode == 3) {
        static uint8_t pid_idx = 0; // 0 = Kp, 1 = Ki, 2 = Kd
        char buf[32];

        if (need_redraw) {
            ILI9341_FillRect_DMA(1, 1, SCREEN_WIDTH - 2, SCREEN_HEIGHT - 2, COLOR_BLACK);
            ILI9341_PrintString(10, 10, "== EDIT PID ==", Font_K_95_18x21, 18, 21, COLOR_WHITE, COLOR_BLACK);
            ILI9341_PrintString(10, 200, "1-Next  HOLD-Save/Exit", Font_K_95_18x21, 18, 21, COLOR_GREEN, COLOR_BLACK);
            need_redraw = 0;
        }

        // Обработка энкодера (изменение выбранного значения)
        if (enc_dir != 0) {
            if (pid_idx == 0) {
                eeprom_params.pid_kp += enc_dir * 1; // Шаг 1 (0.001)
                if (eeprom_params.pid_kp < 0) eeprom_params.pid_kp = 0;
            } else if (pid_idx == 1) {
                eeprom_params.pid_ki += enc_dir * 1; // Шаг 1 (0.001)
                if (eeprom_params.pid_ki < 0) eeprom_params.pid_ki = 0;
            } else if (pid_idx == 2) {
                eeprom_params.pid_kd += enc_dir * 1; // Шаг 1 (0.001)
                if (eeprom_params.pid_kd < 0) eeprom_params.pid_kd = 0;
            }
            HW_Buzzer(1200, 5);
            last_display_update = 0;
        }

        // Обработка кнопок
        if (btn_event == 1) {
            // Короткое нажатие - переключить параметр (Kp -> Ki -> Kd -> Kp)
            pid_idx = (pid_idx + 1) % 3;
            HW_Buzzer(1350, 10);
            last_display_update = 0;
        }
        else if (btn_event == 3) {
            // Длинное нажатие - сохранить в EEPROM и выйти
            SaveProgramParams(); 
            ILI9341_FillRect_DMA(1, 1, SCREEN_WIDTH - 2, SCREEN_HEIGHT - 2, COLOR_BLACK);
            in_calibration_mode = 0;
            need_redraw = 1;
            Motor_Stop();
            setup_main_selection = 0;
            ResetProgramDisplayCache();
            DrawCurrentProgram();
            display = 1;
            DrawProgramParameters(current_program);
            system_state = STATE_SELECT_PROGRAM;
            calib_mode = 0;
            return;
        }

        // Обновление экрана (не чаще 200 мс)
        uint32_t now = SysTimer_ms;
        if (now - last_display_update >= 200) {
            last_display_update = now;

            // Правильный фикс расширить FillRect влево:так,старый курсор стирается
            ILI9341_FillRect_DMA(1, 50, 259, 100, COLOR_BLACK);

            // Выводим Kp
            snprintf(buf, sizeof(buf), "Kp: %d.%03d", (int)(eeprom_params.pid_kp / 1000), (int)(eeprom_params.pid_kp % 1000));
            ILI9341_PrintString(20, 50, buf, Font_K_95_18x21, 18, 21, (pid_idx == 0) ? COLOR_YELLOW : COLOR_WHITE, COLOR_BLACK);

            // Выводим Ki
            snprintf(buf, sizeof(buf), "Ki: %d.%03d", (int)(eeprom_params.pid_ki / 1000), (int)(eeprom_params.pid_ki % 1000));
            ILI9341_PrintString(20, 80, buf, Font_K_95_18x21, 18, 21, (pid_idx == 1) ? COLOR_YELLOW : COLOR_WHITE, COLOR_BLACK);

            // Выводим Kd
            snprintf(buf, sizeof(buf), "Kd: %d.%03d", (int)(eeprom_params.pid_kd / 1000), (int)(eeprom_params.pid_kd % 1000));
            ILI9341_PrintString(20, 110, buf, Font_K_95_18x21, 18, 21, (pid_idx == 2) ? COLOR_YELLOW : COLOR_WHITE, COLOR_BLACK);

            // Курсор (указывает на редактируемый параметр)
            ILI9341_PrintString(5, 50 + pid_idx * 30, "$", Font_K_95_18x21, 18, 21, COLOR_YELLOW, COLOR_BLACK);
        }
        return;
    }
}

// ------------------------------------------------------------------
//  Тестовые функции для сервисного меню
// ------------------------------------------------------------------
static void TestDoor_On(void) {HW_CloseDoor();}
static void TestDoor_Off(void) {HW_OpenDoor();}
//-----------------------------------------
static void TestValve1_On(void) {HW_OpenInletValve();}
static void TestValve1_Off(void) {HW_CloseInletValve();}
//-----------------------------------------
static void TestValve2_On(void) {HW_OpenTheRinseAidInletValve();}
static void TestValve2_Off(void) {HW_CloseTheRinseAidInletValve();}
//-----------------------------------------
static void TestPumpCirc_On(void) {HW_StartPumpCirculation();}
static void TestPumpCirc_Off(void) {HW_StopPumpCirculation();}
//-----------------------------------------
static void TestPumpDrain_On(void) {HW_StartPumpDrain();}
static void TestPumpDrain_Off(void) {HW_StopPumpDrain();}
//-----------------------------------------
static void TestTriac_On(void) {
    TRIAC_PIN_MODE_OUTPUT();     // переключаем в режим выхода
    GPIO_SET_PIN(TRIAC_PORT, TRIAC_PIN);
}
static void TestTriac_Off(void) {
    GPIO_RESET_PIN(TRIAC_PORT, TRIAC_PIN);
    TRIAC_PIN_MODE_AF();         // возвращаем альтернативную функцию
}
//-----------------------------------------
static void TestHeater_On(void) {HW_EnableHeater();}
static void TestHeater_Off(void) {HW_DisableHeater();}
//-----------------------------------------
static void TestDirFWD_On(void) {GPIO_SET_PIN(DIR_FWD_PORT,DIR_FWD_PIN);}
static void TestDirFWD_Off(void) {GPIO_RESET_PIN(DIR_FWD_PORT,DIR_FWD_PIN);}
//-----------------------------------------
static void TestDirREV_On(void) {GPIO_SET_PIN(DIR_REV_PORT,DIR_REV_PIN);}
static void TestDirREV_Off(void) {GPIO_RESET_PIN(DIR_REV_PORT,DIR_REV_PIN);}
//-----------------------------------------
static void TestRelay3_On(void) {GPIO_SET_PIN(RELAY_3_PORT,RELAY_3_PIN);}
static void TestRelay3_Off(void) {GPIO_RESET_PIN(RELAY_3_PORT,RELAY_3_PIN);}
//-----------------------------------------
static void TestPowerMgmt_On(void) {GPIO_SET_PIN(POWER_MANAGEMENT_PORT,POWER_MANAGEMENT_PIN);}
static void TestPowerMgmt_Off(void) {GPIO_RESET_PIN(POWER_MANAGEMENT_PORT,POWER_MANAGEMENT_PIN);}
//-----------------------------------------
static void TestBuzzer_On(void) { HW_Buzzer(1350, 65000);}
static void TestBuzzer_Off(void) { HW_BuzzerStop();}



// ---------------------------------------------------------
// Функция для вывода параметров на экран.
// ---------------------------------------------------------
void DrawProgramParameters(WashProgram prog) {
    static uint8_t last_type = 0xFF; 
    uint8_t temp = eeprom_params.programs[prog].temperature;
    uint16_t spin = eeprom_params.programs[prog].spin_speed;

    uint8_t type = (prog == PROG_SETUP) ? 0 : ((prog == PROG_SPIN) ? 1 : 2);

    if (type != last_type || display) {
        ILI9341_FillRect_DMA(1, 120, 318, 110, COLOR_BLACK); // Стираем данные
        last_type = type;

        if (type == 0) {
            DrawImage_FromEEPROM(130, 120, IMAGE_TIM, 60, 60);
        } else if (type == 1) {
            DrawImage_FromEEPROM(130, 120, IMAGE_OBR, 60, 44);
        } else {
            DrawImage_FromEEPROM(40, 120, IMAGE_TEMP, 60, 42);
            DrawImage_FromEEPROM(95, 198, IMAGE_C, 42, 30);
            DrawImage_FromEEPROM(220, 120, IMAGE_OBR, 60, 44);
        }
        display = 0;
    }

    if (type == 1) {
        ResetNumberCache(105, 200);
        DrawNumber_NoFlicker(105, 200, spin, IS_FLOAT_NO, MAX_ZNAK_01, ALIGN_RIGHT, MAX_0001,
                            Font_25x29, 25, 29, COLOR_WHITE, COLOR_BLACK);
    } else if (type == 2) {
        ResetNumberCache(30, 200);
        ResetNumberCache(185, 200);
        DrawNumber_NoFlicker(30, 200, temp, IS_FLOAT_NO, MAX_ZNAK_01, ALIGN_RIGHT, MAX_01,
                            Font_25x29, 25, 29, COLOR_WHITE, COLOR_BLACK);
        DrawNumber_NoFlicker(185, 200, spin, IS_FLOAT_NO, MAX_ZNAK_01, ALIGN_RIGHT, MAX_0001,
                            Font_25x29, 25, 29, COLOR_WHITE, COLOR_BLACK);
    }
}

// ---------------------------------------------------------
// Таблица редактирования режимов
// ---------------------------------------------------------
typedef enum {
    MODE_MAIN,      // Главный экран
    MODE_T,         // Редактирование температуры
    MODE_PRM        // Редактирование оборотов отжима
} ModeState;

static ModeState edit_mode = MODE_MAIN;

// ---------------------------------------------------------
//  Функция редактирования режимов стирки
// ---------------------------------------------------------
void EditProgramParameters(uint16_t pr) {
    static uint16_t current_edit_prog = 0;  
    static uint16_t rpm_value = 0, temperature_value = 0;
    static uint8_t need_redraw = 1; 
    static uint8_t locked = 0;  

    switch (edit_mode) {
        case MODE_MAIN: {
            if (need_redraw) {
                current_edit_prog = pr;   
                temperature_value = eeprom_params.programs[current_edit_prog].temperature;
                rpm_value = eeprom_params.programs[current_edit_prog].spin_speed;
                
                ILI9341_FillRect_DMA(1, 100, 318, 139, COLOR_BLACK);
                DrawImage_FromEEPROM(30, 100, IMAGE_SETUP, 246, 30);
                
                if (current_edit_prog == PROG_SPIN) {
                    DrawImage_FromEEPROM(220, 170, IMAGE_OBR, 60, 44);
                    DrawImage_FromEEPROM(125, 170, IMAGE_ENC, 60, 42);
                } else {
                    DrawImage_FromEEPROM(30, 170, IMAGE_TEMP, 60, 42);
                    DrawImage_FromEEPROM(125, 170, IMAGE_ENC, 60, 42);
                    DrawImage_FromEEPROM(220, 170, IMAGE_OBR, 60, 44);
                }
                need_redraw = 0;  
            }

            if (btn_event == 3) {
                SaveProgramParams();
                ILI9341_FillRect_DMA(1, 70, 318, 169, COLOR_BLACK); 
                DrawCurrentProgram();
                display = 1;
                DrawProgramParameters(current_program);
                system_state = STATE_SELECT_PROGRAM;
                edit_mode = MODE_MAIN;
                need_redraw = 1;  
                break;
            }

            // Однократное переключение при повороте
            if (!locked && enc_dir != 0) {
                if (enc_dir == -1) {
                    edit_mode = (current_edit_prog == PROG_SPIN) ? MODE_PRM : MODE_T;
                } else {
                    edit_mode = MODE_PRM;
                }
                need_redraw = 1;
                locked = 1; 
            }
            if (enc_dir == 0) locked = 0;
            break;
        }

        case MODE_T: {
            if (need_redraw) {
                ILI9341_FillRect_DMA(1, 100, 318, 139, COLOR_BLACK);
                ProgressBar(PP_X, PP_Y, 318, 30, 1, temperature_value, 90, COLOR_WHITE, COLOR_GREEN);
                DrawImage_FromEEPROM(80, 155, IMAGE_TEMPERATURE, 12, 30);
                DrawImage_FromEEPROM(190, 155, IMAGE_C, 42, 30);
                ResetNumberCache(120, 157);
                DrawNumber_NoFlicker(120, 157, temperature_value, IS_FLOAT_NO, MAX_ZNAK_01, ALIGN_RIGHT, MAX_01, 
                                    Font_25x29, 25, 29, COLOR_WHITE, COLOR_BLACK);
                need_redraw = 0;
            }

            if (enc_dir != 0) {
                int16_t new_temp = temperature_value + (enc_dir * 10);
                temperature_value = (new_temp < 0) ? 0 : ((new_temp > 90) ? 90 : new_temp);
                eeprom_params.programs[current_edit_prog].temperature = temperature_value;
                
                ProgressBar(PP_X, PP_Y, 318, 30, 1, temperature_value, 90, COLOR_WHITE, COLOR_GREEN);
                DrawNumber_NoFlicker(120, 157, temperature_value, IS_FLOAT_NO, MAX_ZNAK_01, ALIGN_RIGHT, MAX_01, 
                                     Font_25x29, 25, 29, COLOR_WHITE, COLOR_BLACK);
            }

            if (btn_event == 3) { 
                ILI9341_FillRect_DMA(1, 100, 318, 139, COLOR_BLACK);
                edit_mode = MODE_MAIN;
                need_redraw = 1;
            }
            break;
        }

        case MODE_PRM: {
            if (need_redraw) {
                ILI9341_FillRect_DMA(1, 100, 318, 139, COLOR_BLACK);
                ProgressBar(PP_X, PP_Y, 318, 30, 1, rpm_value, 1200, COLOR_WHITE, COLOR_GREEN);
                DrawImage_FromEEPROM(130, 155, IMAGE_OBMIN, 119, 30);
                ResetNumberCache(30, 157);
                DrawNumber_NoFlicker(30, 157, rpm_value, IS_FLOAT_NO, MAX_ZNAK_01, ALIGN_RIGHT, MAX_0001, 
                                     Font_25x29, 25, 29, COLOR_WHITE, COLOR_BLACK);
                need_redraw = 0;
            }

            if (enc_dir != 0) {
                int16_t new_rpm = rpm_value + (enc_dir * 300);
                rpm_value = (new_rpm < 0) ? 0 : ((new_rpm > 1200) ? 1200 : new_rpm);
                eeprom_params.programs[current_edit_prog].spin_speed = rpm_value;
                
                ProgressBar(PP_X, PP_Y, 318, 30, 1, rpm_value, 1200, COLOR_WHITE, COLOR_GREEN);
                DrawNumber_NoFlicker(30, 157, rpm_value, IS_FLOAT_NO, MAX_ZNAK_01, ALIGN_RIGHT, MAX_0001, 
                                    Font_25x29, 25, 29, COLOR_WHITE, COLOR_BLACK);
            }

            if (btn_event == 3) {
                ILI9341_FillRect_DMA(1, 120, 318, 108, COLOR_BLACK); 
                edit_mode = MODE_MAIN;
                need_redraw = 1;
            }
            break;
        }   
    }
}

// ---------------------------------------------------------
// Таблица картинок для программ и ширины картинок
// ---------------------------------------------------------
const ProgImage program_images[PROG_COUNT] = {
    {IMAGE_COTON,     175},
    {IMAGE_SYNTETICS, 237},
    {IMAGE_DELICATE,  269},
    {IMAGE_WOOL,      167},
    {IMAGE_FAST_15,   269},
    {IMAGE_RINSING,   282},
    {IMAGE_SPIN,      154},
    {IMAGE_SETUP,     246} 
};

// ---------------------------------------------------------
//  Функция отрисовки текущей программы
// ---------------------------------------------------------
void DrawCurrentProgram(void) {

    static uint16_t last_x = 0;                   
    static uint16_t last_width = 0;                

    if (last_program != current_program) {
        if (last_program != PROG_COUNT_) {
            ILI9341_FillRect_DMA(last_x, 40, last_width, 30, COLOR_BLACK);
        }

        uint16_t new_width = program_images[current_program].width;
        uint16_t new_x = (SCREEN_WIDTH - new_width) / 2;

        DrawImage_FromEEPROM(new_x, 40, program_images[current_program].addr, new_width, 30);

        last_program = current_program;
        last_x = new_x;
        last_width = new_width;
    }
}

//---10,05,2026
void ResetProgramDisplayCache(void) {
    last_program = PROG_COUNT_;
}

// ---------------------------------------------------------
//  Основная функция меню
// ---------------------------------------------------------
void Menu_Update(uint16_t dt_ms) {

    CheckPowerFailure();
    Relay_Update();
    // В основном коде теперь можно просто писать:
    // uint8_t current_temp = HW_GetTemperature();
    // использовать current_temp для отображения или управления
    // Вызываем только если мы не в режиме настроек (калибровка/меню)
    if (system_state != STATE_SETUP) {
        MotorControl_Task();
    }

    enc_dir = Encoder_GetDirection();
    btn_event = Button_GetEvent();
    static WashProgram editing_program;

    switch (system_state) {
        case STATE_POWER_ON: {
            delay_ms(dt_ms); 
            ILI9341_FillRect_DMA(1, 20, 318, 219, COLOR_BLACK);
            DrawCurrentProgram();
            DrawProgramParameters(current_program);
            system_state = STATE_SELECT_PROGRAM;
            triac_delay = TRIAC_DELAY_MAX;   // или PowerPercentToDelayLinear(0)
            triac_pulse = 500;    // длительность управляющего импульс 750 us
            break;
        }

        case STATE_SELECT_PROGRAM: {
            /*DrawNumber_NoFlicker(60, 164, 
            current_temp, IS_FLOAT_NO, // выводим температуру
            MAX_ZNAK_01, ALIGN_RIGHT, MAX_001, 
            image_data_Font_23x28, 23, 28, 
            COLOR_GREEN, COLOR_BLACK);*/
            if (enc_dir != 0) {
                HW_Buzzer(1350, 15);   // короткий писк при повороте
                int new_prog = current_program + enc_dir;
                current_program = (new_prog < 0) ? (PROG_COUNT - 1) : ((new_prog >= PROG_COUNT) ? 0 : new_prog);
                DrawProgramParameters(current_program); 
                DrawCurrentProgram();
            }

            if (btn_event == 1) {   // короткое нажатие
                if (current_program == PROG_SETUP) {    
                    system_state = STATE_SETUP;         
                    ILI9341_FillRect_DMA(1, 80, 318, 159, COLOR_BLACK);
                    app_state = STATE_MAIN;                 
                } else {                                
                    system_state = STATE_RUNNING;
                    ILI9341_FillRect_DMA(1, 170, 318, 69, COLOR_BLACK);
                    
                    wash_state.magic = 0xA5A5A5A5;
                    wash_state.program = current_program; 
                    wash_state.stage = 1;
                    wash_state.stage_time = 100;        
                    wash_state.crc = 0;                 
                    SaveWashState();
                }
            }
            else if (btn_event == 3) {   // длинное нажатие                                    
                if (current_program != PROG_SETUP) {
                    editing_program = current_program;
                    system_state = STATE_EDIT_PROGRAM;
                }
            }
            break;
        }

        case STATE_RUNNING: {
            static uint8_t motor_started = 0;
            if (!motor_started) {
                // Читаем обороты из настроек выбранной программы
                int32_t target_rpm = (int32_t)eeprom_params.programs[current_program].spin_speed;
                
                // Если пользователь случайно выставил 0 (например, программа без отжима)
                if (target_rpm > 0) {
                    // Передаем цель (target_rpm), ramp=500, старт=12%, задержка=200 мс
                    MotorControl_StartMotor(target_rpm, 200, 28, 400); 
                    Motor_StartWash();   // включает реле направления
                } else {
                    Motor_Stop(); // На всякий случай, если цель 0
                }
                motor_started = 1;
            }

            // Восстанавливаем инкремент таймера и прогресс-бар
            power_on_timer++;
            wash_state.stage_time = power_on_timer;
            ProgressBar(PP_X, PP_Y, 318, 30, 1, power_on_timer, 400, COLOR_WHITE, COLOR_GREEN);

            // Отладочный вывод – столбиком
            #if ENABLE_OUT_INFO
                char buf[16];
                int16_t y = 30;          // начальная Y-координата

                // 1. Текущие обороты
                ILI9341_PrintString(10, y, "RPM:", Font_K_95_18x21, 18, 21, COLOR_WHITE, COLOR_BLACK);
                //sprintf(buf, "%4ld", (long)HW_GetRPM());
                //ILI9341_PrintString(80, y, buf, Font_K_95_18x21, 18, 21, COLOR_YELLOW, COLOR_BLACK);
                DrawNumber_NoFlicker(80, 20, HW_GetRPM(), IS_FLOAT_NO, MAX_ZNAK_01, ALIGN_RIGHT, MAX_0001,
                                Font_25x29, 25, 29, COLOR_YELLOW, COLOR_BLACK);
                y += 25;  // отступ между строками

                // 2. Ошибка
                ILI9341_PrintString(10, y, "Err:", Font_K_95_18x21, 18, 21, COLOR_WHITE, COLOR_BLACK);
                sprintf(buf, "%4ld", (long)(motor_pid.setpoint - HW_GetRPM()));
                ILI9341_PrintString(80, y, buf, Font_K_95_18x21, 18, 21, COLOR_YELLOW, COLOR_BLACK);
                y += 25;

                // 3. Задержка (triac_delay)
                ILI9341_PrintString(10, y, "Dly:", Font_K_95_18x21, 18, 21, COLOR_WHITE, COLOR_BLACK);
                sprintf(buf, "%4d", triac_delay);
                ILI9341_PrintString(80, y, buf, Font_K_95_18x21, 18, 21, COLOR_YELLOW, COLOR_BLACK);
                y += 25;

                // 4. Задание (setpoint)
                ILI9341_PrintString(10, y, "Set:", Font_K_95_18x21, 18, 21, COLOR_WHITE, COLOR_BLACK);
                sprintf(buf, "%4ld", (long)motor_pid.setpoint);
                ILI9341_PrintString(80, y, buf, Font_K_95_18x21, 18, 21, COLOR_YELLOW, COLOR_BLACK);
            #endif

            if (power_on_timer > 1000) {
                power_on_timer = 0;
                ILI9341_FillRect_DMA(PP_X, PP_Y, 318, 30, COLOR_BLACK);
                wash_state.magic = 0;
                wash_state.stage = 0;
                SaveWashState();
                DrawCurrentProgram();
                DrawProgramParameters(current_program);
                system_state = STATE_SELECT_PROGRAM;
                motor_started = 0;
                Motor_Stop();
                ILI9341_FillRect_DMA(10, 210, 300, 21, COLOR_BLACK);
            }
            break;
        }

        case STATE_EDIT_PROGRAM: {
            EditProgramParameters(editing_program);
            break;
        }    

        case STATE_SETUP: {
            if (in_calibration_mode) {
                Calibration_Mode();
            } else {
                Setup_ServiceMenu();
            }
            break;
        }

        case STATE_RESUME_PROMPT: {
            // Координаты и размеры кнопки START
            #define START_BTN_X  96
            #define START_BTN_Y  210      // 320/2 - 64 = 96
            #define START_BTN_W  128
            #define START_BTN_H  30

            static uint32_t last_blink = 0;
            static uint8_t  blink_state = 0;

            // Мигание каждые 500 мс
            if (SysTimer_ms - last_blink >= 500) {
                last_blink = SysTimer_ms;
                blink_state = !blink_state;
                if (blink_state) {
                    HW_Buzzer(1550, 5);   // короткий писк
                    DrawImage_FromEEPROM(START_BTN_X, START_BTN_Y, IMAGE_START, START_BTN_W, START_BTN_H);
                } else {
                    ILI9341_FillRect_DMA(START_BTN_X, START_BTN_Y, START_BTN_W, START_BTN_H, COLOR_BLACK);
                }
            }

            // Обработка кнопок
            if (btn_event == 1) {   // короткое нажатие – продолжить стирку
                // Переходим в состояние выполнения стирки
                system_state = STATE_RUNNING;
                // Очищаем экран и перерисовываем интерфейс RUNNING
                //ILI9341_FillScreen_DMA(COLOR_BLACK);
                ILI9341_FillRect_DMA(START_BTN_X, START_BTN_Y, START_BTN_W, START_BTN_H, COLOR_BLACK);
                DrawCurrentProgram();
                display = 1;   // флаг принудительной перерисовки параметров
                DrawProgramParameters(current_program);
                // Прогресс-бар будет рисоваться в STATE_RUNNING, но можно сразу показать текущее состояние
                ProgressBar(PP_X, PP_Y, 318, 30, 1, power_on_timer, 400, COLOR_WHITE, COLOR_GREEN);
                // Сбрасываем таймер мигания (не обязательно)
                last_blink = 0;
            }
            else if (btn_event == 3) {   // длинное нажатие – отменить, начать заново
                // Сбрасываем сохранённое состояние в EEPROM
                wash_state.magic = 0;
                wash_state.stage = 0;
                SaveWashState();
                // Переходим в режим выбора программы (как после загрузки лого)
                system_state = STATE_SELECT_PROGRAM;
                // Очищаем экран и рисуем обычный главный экран
                //ILI9341_FillScreen_DMA(COLOR_BLACK);
                ILI9341_FillRect_DMA(START_BTN_X, START_BTN_Y, START_BTN_W, START_BTN_H, COLOR_BLACK);
                DrawCurrentProgram();
                display = 1;
                DrawProgramParameters(current_program);
                // Сбрасываем мигание
                last_blink = 0;
            }
            break;
        }

        case STATE_ERROR: {
            ILI9341_FillScreen_DMA(COLOR_RED);
            break;
        }
    }
}