
#include "ILI9341_time.h"
#include "stm32f1_rtc.h"
#include "timestamp.h"
#include <string.h>


static char old_time[9] = "        "; // 8 символов + \0
static char old_date[11] = "          "; // 10 символов + \0


// Быстрая функция форматирования времени (без snprintf)
static void format_time_fast(char* buf, uint8_t hours, uint8_t minutes, uint8_t seconds, uint8_t blink)
{
    // Часы: десятки и единицы
    buf[0] = '0' + (hours / 10);
    buf[1] = '0' + (hours % 10);
    
    // Разделитель (двоеточие или пробел при мигании)
    uint8_t show_colon = !(blink && (seconds & 1));
    buf[2] = show_colon ? ':' : ' ';
    
    // Минуты
    buf[3] = '0' + (minutes / 10);
    buf[4] = '0' + (minutes % 10);
    
    // Второй разделитель
    buf[5] = show_colon ? ':' : ' ';
    
    // Секунды
    buf[6] = '0' + (seconds / 10);
    buf[7] = '0' + (seconds % 10);
    
    buf[8] = '\0';
}

/*void UpdateRTC_TimeFull(
    uint16_t x, uint16_t y,
    const uint8_t *font,
    uint8_t width, uint8_t height,
    uint16_t color, uint16_t bg)
{
    static char old_time[9] = "        ";
    RTC_UNIT dt = rtc_GetTime();
    char new_time[9];
    format_time_fast(new_time, dt.hours, dt.minutes, dt.seconds, 1);

    uint8_t bytes_per_row = (width + 7) / 8;
    uint32_t char_size = height * bytes_per_row;

    uint16_t cx = x;
    for (uint8_t i = 0; i < 8; i++) {
        if (new_time[i] != old_time[i]) {
            if (new_time[i] == ' ') {
                ILI9341_FillRect_DMA(cx, y, width, height, bg);
            } else {
                uint32_t idx = (uint8_t)new_time[i] - 32;
                const uint8_t* char_bitmap = &font[idx * char_size];
                ILI9341_DrawChar_Opaque_DMA_LineByLine(cx, y, new_time[i],
                                           char_bitmap, width, height,
                                           color, bg);
            }
            old_time[i] = new_time[i];
        }
        cx += width;
    }
    old_time[8] = '\0';
}*/
void UpdateRTC_TimeFull(
    uint16_t x, uint16_t y,
    const uint8_t *font,
    uint8_t width, uint8_t height,
    uint16_t color, uint16_t bg)
{
    // УДАЛИТЬ строку: static char old_time[9] = "        ";
    // Используем глобальную old_time из начала файла
    RTC_UNIT dt = rtc_GetTime();
    char new_time[9];
    format_time_fast(new_time, dt.hours, dt.minutes, dt.seconds, 1);

    uint8_t bytes_per_row = (width + 7) / 8;
    uint32_t char_size = height * bytes_per_row;

    uint16_t cx = x;
    for (uint8_t i = 0; i < 8; i++) {
        if (new_time[i] != old_time[i]) {
            if (new_time[i] == ' ') {
                ILI9341_FillRect_DMA(cx, y, width, height, bg);
            } else {
                uint32_t idx = (uint8_t)new_time[i] - 32;
                const uint8_t* char_bitmap = &font[idx * char_size];
                ILI9341_DrawChar_Opaque_DMA_LineByLine(cx, y, new_time[i],
                                           char_bitmap, width, height,
                                           color, bg);
            }
            old_time[i] = new_time[i];
        }
        cx += width;
    }
    old_time[8] = '\0';
}

static void format_date_fast(char* buf, uint8_t day, uint8_t month, uint8_t year) {
    buf[0] = '0' + (day / 10);
    buf[1] = '0' + (day % 10);
    buf[2] = '.';
    buf[3] = '0' + (month / 10);
    buf[4] = '0' + (month % 10);
    buf[5] = '.';
    buf[6] = '2';          // первая цифра века (предполагаем 2000-2099)
    buf[7] = '0';          // вторая цифра века
    buf[8] = '0' + (year / 10);
    buf[9] = '0' + (year % 10);
    buf[10] = '\0';
}


/*void UpdateRTC_DateFull(uint16_t x, uint16_t y, const uint8_t *font,
                        uint8_t width, uint8_t height,
                        uint16_t color, uint16_t bg) {
    static char old_date[11] = "          ";   // обязательно static
    RTC_UNIT dt = rtc_GetTime();
    char new_date[11];
    format_date_fast(new_date, dt.date, dt.month, dt.year);

    uint8_t bytes_per_row = (width + 7) / 8;
    uint32_t char_size = height * bytes_per_row;

    uint16_t cx = x;
    for (uint8_t i = 0; i < 10; i++) {          // исправлено: 10 вместо 8
        if (new_date[i] != old_date[i]) {
            if (new_date[i] == ' ') {
                ILI9341_FillRect_DMA(cx, y, width, height, bg);
            } else {
                uint32_t idx = (uint8_t)new_date[i] - 32;
                const uint8_t* char_bitmap = &font[idx * char_size];
                ILI9341_DrawChar_Opaque_DMA_LineByLine(cx, y, new_date[i],
                                           char_bitmap, width, height,
                                           color, bg);
            }
            old_date[i] = new_date[i];          // исправлено: old_date вместо old_time
        }
        cx += width;
    }
    old_date[10] = '\0';
}*/
void UpdateRTC_DateFull(uint16_t x, uint16_t y, const uint8_t *font,
                        uint8_t width, uint8_t height,
                        uint16_t color, uint16_t bg) {

    // Используем глобальную old_date
    RTC_UNIT dt = rtc_GetTime();
    char new_date[11];
    format_date_fast(new_date, dt.date, dt.month, dt.year);

    uint8_t bytes_per_row = (width + 7) / 8;
    uint32_t char_size = height * bytes_per_row;

    uint16_t cx = x;
    for (uint8_t i = 0; i < 10; i++) {          // 10 символов
        if (new_date[i] != old_date[i]) {
            if (new_date[i] == ' ') {
                ILI9341_FillRect_DMA(cx, y, width, height, bg);
            } else {
                uint32_t idx = (uint8_t)new_date[i] - 32;
                const uint8_t* char_bitmap = &font[idx * char_size];
                ILI9341_DrawChar_Opaque_DMA_LineByLine(cx, y, new_date[i],
                                           char_bitmap, width, height,
                                           color, bg);
            }
            old_date[i] = new_date[i];
        }
        cx += width;
    }
    old_date[10] = '\0';
}

void DisplayTime(uint16_t x, uint16_t y, const uint8_t* font,
                 uint8_t w, uint8_t h, uint16_t color, uint16_t bg,
                 struct RTC_UNIT time, uint8_t active_field) {
    static uint8_t last_active = 0xFF;
    const uint8_t underline_h = 4;
    uint16_t underline_color = COLOR_RED;

    // Стираем подчёркивание предыдущего активного поля
    if (last_active <= 2 && last_active != active_field) {
        uint16_t x_clear = x + ((last_active == 0) ? 0 : (last_active == 1) ? 3*w : 6*w);
        ILI9341_FillRect_DMA(x_clear, y + h, 2*w, underline_h, bg);
    }

    // Формируем строку времени без мигания
    char buf[9];
    format_time_fast(buf, time.hours, time.minutes, time.seconds, 0);

    // Вычисляем размер битмапа одного символа в байтах
    uint8_t bytes_per_row = (w + 7) / 8;
    uint32_t char_size = h * bytes_per_row;

    // Рисуем все символы
    uint16_t cx = x;
    for (uint8_t i = 0; i < 8; i++) {
        char c = buf[i];
        uint32_t idx = (uint8_t)c - 32;         // индекс в таблице шрифта
        const uint8_t* char_bitmap = &font[idx * char_size];
        ILI9341_DrawChar_Opaque_DMA_LineByLine(cx, y, c, char_bitmap, w, h, color, bg);
        cx += w;
    }

    // Рисуем подчёркивание для активного поля
    uint16_t x_act = x + ((active_field == 0) ? 0 : (active_field == 1) ? 3*w : 6*w);
    ILI9341_FillRect_DMA(x_act, y + h, 2*w, underline_h, underline_color);
    last_active = active_field;
}

void DisplayDate(uint16_t x, uint16_t y, const uint8_t* font,
                 uint8_t w, uint8_t h, uint16_t color, uint16_t bg,
                 RTC_UNIT date, uint8_t active_field) {
    static uint8_t last_active = 0xFF;
    const uint8_t underline_h = 4;
    uint16_t underline_color = COLOR_RED;

    if (last_active <= 2 && last_active != active_field) {
        uint16_t x_clear;
        uint16_t clear_width;
        if (last_active == 0) {
            x_clear = x;
            clear_width = 2 * w;
        } else if (last_active == 1) {
            x_clear = x + 3 * w;
            clear_width = 2 * w;
        } else {
            x_clear = x + 6 * w;
            clear_width = 4 * w;
        }
        ILI9341_FillRect_DMA(x_clear, y + h, clear_width, underline_h, bg);
    }

    char buf[11];
    format_date_fast(buf, date.date, date.month, date.year);

    uint8_t bytes_per_row = (w + 7) / 8;
    uint32_t char_size = h * bytes_per_row;

    uint16_t cx = x;
    for (uint8_t i = 0; i < 10; i++) {
        char c = buf[i];
        uint32_t idx = (uint8_t)c - 32;
        const uint8_t* char_bitmap = &font[idx * char_size];
        ILI9341_DrawChar_Opaque_DMA_LineByLine(cx, y, c, char_bitmap, w, h, color, bg);
        cx += w;
    }

    uint16_t x_act;
    uint16_t act_width;
    if (active_field == 0) {
        x_act = x;
        act_width = 2 * w;
    } else if (active_field == 1) {
        x_act = x + 3 * w;
        act_width = 2 * w;
    } else {
        x_act = x + 6 * w;
        act_width = 4 * w;
    }
    ILI9341_FillRect_DMA(x_act, y + h, act_width, underline_h, underline_color);
    last_active = active_field;
}

// Функции сброса:
void ResetTimeDisplayCache(void) {
    for (uint8_t i = 0; i < 8; i++) old_time[i] = ' ';
    old_time[8] = '\0';
}
// Функции сброса:
void ResetDateDisplayCache(void) {
    for (uint8_t i = 0; i < 10; i++) old_date[i] = ' ';
    old_date[10] = '\0';
}