#include "ILI9341_text.h"
#include "stm32f103xx_CMSIS.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define MAX_FIELD 16   /* макс. символов в поле включая '\0' */
#define MAX_CACHE 10
#define MAX_FONT_WIDTH_SUPPORTED 34   // Поддерживаем до 64 пикселей в ширину
#define MAX_FONT_HEIGHT_SUPPORTED 37  // Поддерживаем до 64 пикселей в высоту

/* Структура для кеширования состояния отображения числа */
typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t last_value;
    uint8_t mode;
    uint8_t param;
    uint8_t width;
    uint8_t height;
    char last_buffer[MAX_FIELD];
} DisplayCache;

static DisplayCache display_cache[MAX_CACHE];
static uint8_t cache_count = 0;

/* Быстрое преобразование числа в строку с ведущими нулями */
static uint8_t uint_to_str_padded(char* out, uint32_t value, uint8_t width)
{
    char temp[6]; // Для чисел до 65535 (5 цифр) + \0
    uint8_t i = 0;
    
    // Преобразуем число в обратном порядке
    do {
        temp[i++] = '0' + (value % 10);
        value /= 10;
    } while (value > 0);
    
    // Дополняем ведущими нулями до нужной ширины
    while (i < width) {
        temp[i++] = '0';
    }
    
    // Разворачиваем в правильном порядке
    uint8_t j = 0;
    while (i > 0) {
        out[j++] = temp[--i];
    }
    
    out[j] = '\0';
    return j;
}

/* Быстрое преобразование числа в строку без форматирования */
static uint8_t uint_to_str_simple(char* out, uint32_t value)
{
    char temp[6];
    uint8_t i = 0;
    
    // Обработка нуля
    if (value == 0) {
        out[0] = '0';
        out[1] = '\0';
        return 1;
    }
    
    // Преобразуем число в обратном порядке
    while (value > 0) {
        temp[i++] = '0' + (value % 10);
        value /= 10;
    }
    
    // Разворачиваем в правильном порядке
    uint8_t j = 0;
    while (i > 0) {
        out[j++] = temp[--i];
    }
    
    out[j] = '\0';
    return j;
}

/* Оптимизированная версия format_uint (без sprintf) */
static uint8_t format_uint(char* out, uint16_t value, uint8_t mode, uint8_t param)
{
    if (mode == 0) {
        // Простой вывод без форматирования
        return uint_to_str_simple(out, value);
    }
    
    // Режим с ведущими нулями
    if (param < 1) param = 1;
    if (param > 5) param = 5;
    
    return uint_to_str_padded(out, value, param);
}

/* Оптимизированная версия format_float (без sprintf) */
static uint8_t format_float(char* out, float value, uint8_t decimals)
{
    if (decimals > 3) decimals = 3;
    
    char *p = out;
    
    // Обработка отрицательных чисел
    if (value < 0.0f) {
        *p++ = '-';
        value = -value;
    }
    
    uint32_t ipart = (uint32_t)value;
    float frac = value - (float)ipart;
    
    // Вычисляем множитель для дробной части
    uint32_t mul = 1;
    for (uint8_t i = 0; i < decimals; i++) {
        mul *= 10;
    }
    
    // Округляем дробную часть
    uint32_t fpart = (uint32_t)(frac * (float)mul + 0.5f);
    
    // Проверка на переполнение
    if (fpart >= mul) {
        ipart += 1;
        fpart -= mul;
    }
    
    // Вывод целой части
    uint8_t int_len = uint_to_str_simple(p, ipart);
    p += int_len;
    
    // Вывод дробной части с фиксированным количеством знаков
    if (decimals > 0) {
        *p++ = '.';
        
        // Выводим ровно decimals цифр, дополняя нулями
        if (fpart == 0) {
            for (uint8_t i = 0; i < decimals; i++) {
                *p++ = '0';
            }
        } else {
            // Преобразуем дробную часть с ведущими нулями
            char frac_buf[4];
            uint_to_str_padded(frac_buf, fpart, decimals);
            for (uint8_t i = 0; i < decimals; i++) {
                *p++ = frac_buf[i];
            }
        }
    }
    
    *p = '\0';
    return (uint8_t)(p - out);
}

/**Универсальная функция для вывода чисел без мерцания
 * 
 * @param x, y          координаты верхнего левого угла
 * @param float         выбираем формат: 0 - целое, 1 - с плавающей точкой
 * @param decimals      количество знаков после запятой для float
 * @param align_right   1 - выравнивание по правому краю, 0 - по левому
 * @param max_digits    максимальное количество цифр (без знаков и точки) для выравнивания
 * @param font          указатель на массив шрифта
 * @param width         ширина символа в пикселях
 * @param height        высота символа
 * @param color         цвет символа
 * @param bg            цвет фона
 */
void DrawNumber_NoFlicker(uint16_t x, uint16_t y,
                          float value,
                          uint8_t is_float,
                          uint8_t decimals,
                          uint8_t align_right,
                          uint8_t max_digits,
                          const uint8_t* font,
                          uint8_t width, uint8_t height,
                          uint16_t color, uint16_t bg)
{
    char new_buf[MAX_FIELD];
    uint8_t new_len = 0;

    if (is_float) {
        new_len = format_float(new_buf, value, decimals);
    } else {
        new_len = format_uint(new_buf, (uint16_t)value, 0, 0);
    }

    if (new_len >= (MAX_FIELD - 1)) new_len = MAX_FIELD - 1;
    new_buf[new_len] = '\0';

    /* Поиск в кеше по координатам и размеру символа */
    int8_t idx = -1;
    for (uint8_t i = 0; i < cache_count; i++) {
        if (display_cache[i].x == x &&
            display_cache[i].y == y &&
            display_cache[i].width == width &&
            display_cache[i].height == height &&
            display_cache[i].mode == 0) {
            idx = i;
            break;
        }
    }

    /* Создаём запись в кеше при необходимости */
    if (idx == -1) {
        if (cache_count >= MAX_CACHE) idx = 0;
        else idx = cache_count++;

        display_cache[idx].x = x;
        display_cache[idx].y = y;
        display_cache[idx].width = width;
        display_cache[idx].height = height;
        display_cache[idx].mode = 0;
        display_cache[idx].param = 0;
        display_cache[idx].last_value = 0;
        display_cache[idx].last_buffer[0] = '\0';
    }

    /* Копируем старую строку из кеша */
    char old_buf[MAX_FIELD];
    uint8_t old_len = 0;
    
    const char* src = display_cache[idx].last_buffer;
    while (old_len < (MAX_FIELD - 1) && src[old_len] != '\0') {
        old_buf[old_len] = src[old_len];
        old_len++;
    }
    old_buf[old_len] = '\0';

    /* Вычисляем длину визуального поля */
    uint8_t field_len;
    if (align_right && max_digits > 0) {
        uint8_t md = (max_digits > (MAX_FIELD - 1)) ? (MAX_FIELD - 1) : max_digits;
        field_len = md;
        if (new_len > field_len) field_len = new_len;
        if (old_len > field_len) field_len = old_len;
    } else {
        field_len = (old_len > new_len) ? old_len : new_len;
    }
    if (field_len == 0) field_len = 1;
    if (field_len > (MAX_FIELD - 1)) field_len = MAX_FIELD - 1;

    /* Формируем визуальные строки одинаковой длины field_len */
    char vis_old[MAX_FIELD], vis_new[MAX_FIELD];
    for (uint8_t i = 0; i < field_len; i++) { 
        vis_old[i] = ' '; 
        vis_new[i] = ' '; 
    }
    vis_old[field_len] = '\0';
    vis_new[field_len] = '\0';

    if (align_right && max_digits > 0) {
        int8_t shift_old = (int8_t)field_len - (int8_t)old_len;
        int8_t shift_new = (int8_t)field_len - (int8_t)new_len;
        for (uint8_t i = 0; i < field_len; i++) {
            if ((int8_t)i >= shift_old && (int8_t)(i - shift_old) < (int8_t)old_len)
                vis_old[i] = old_buf[i - shift_old];
            if ((int8_t)i >= shift_new && (int8_t)(i - shift_new) < (int8_t)new_len)
                vis_new[i] = new_buf[i - shift_new];
        }
    } else {
        for (uint8_t i = 0; i < field_len; i++) {
            if (i < old_len) vis_old[i] = old_buf[i];
            if (i < new_len) vis_new[i] = new_buf[i];
        }
    }

    int16_t start_x = x;

    // Один раз вычисляем параметры шрифта
    uint8_t bpr = (width + 7) / 8;   // bytes per row

    /* Если запись впервые — рисуем всё */
    if (old_len == 0) {
        for (uint8_t i = 0; i < field_len; i++) {
            uint32_t char_index = vis_new[i] - 32;
            const uint8_t *char_data = &font[char_index * height * bpr];
            ILI9341_DrawChar_Opaque_DMA_LineByLine(
                (uint16_t)(start_x + i * width), y,
                vis_new[i],
                char_data,
                width, height,
                color, bg
            );
            display_cache[idx].last_buffer[i] = vis_new[i];
        }
        display_cache[idx].last_buffer[field_len] = '\0';
        display_cache[idx].last_value = (uint16_t)value;
        return;
    }

    /* Если визуально совпадают — ничего не делаем */
    bool same = true;
    for (uint8_t i = 0; i < field_len; i++) {
        if (vis_old[i] != vis_new[i]) { 
            same = false; 
            break; 
        }
    }
    if (same) return;

    /* Перерисовываем только отличающиеся позиции */
    for (uint8_t i = 0; i < field_len; i++) {
        if (vis_old[i] != vis_new[i]) {
            uint32_t char_index = vis_new[i] - 32;
            const uint8_t *char_data = &font[char_index * height * bpr];
            ILI9341_DrawChar_Opaque_DMA_LineByLine(
                (uint16_t)(start_x + i * width), y,
                vis_new[i],
                char_data,
                width, height,
                color, bg
            );
            display_cache[idx].last_buffer[i] = vis_new[i];
        }
    }

    /* Нуль-терминируем и обновляем last_value */
    display_cache[idx].last_buffer[field_len] = '\0';
    display_cache[idx].last_value = (uint16_t)value;
}

// Функция отправки символа построчно с поддержкой любых размеров
void ILI9341_DrawChar_Opaque_DMA_LineByLine(uint16_t x, uint16_t y,
                                           char c,
                                           const uint8_t *font,
                                           uint8_t width, uint8_t height,
                                           uint16_t color, uint16_t bg)
{
    if (width > MAX_FONT_WIDTH_SUPPORTED || height > MAX_FONT_HEIGHT_SUPPORTED)
        return;

    uint16_t sw_color = (color << 8) | (color >> 8);
    uint16_t sw_bg    = (bg    << 8) | (bg    >> 8);

    // Быстрая заливка для пробела (опционально, можно оставить)
    if (c == ' ') {
        ILI9341_SetAddressWindow_DMA(x, y, x + width - 1, y + height - 1);
        GPIOB->BRR  = GPIO_BRR_BR12;
        GPIOB->BSRR = GPIO_BSRR_BS10;

        uint16_t bg_line[width];
        for (uint8_t i = 0; i < width; i++) bg_line[i] = sw_bg;

        for (uint8_t row = 0; row < height; row++) {
            SPI2_Transfer_DMA_NoCS((uint8_t*)bg_line, width * 2);
            if (ILI9341_WaitForDMA()) { /* обработка ошибки */ }
        }

        while (SPI2->SR & SPI_SR_BSY);
        GPIOB->BSRR = GPIO_BSRR_BS12;
        return;
    }

    // Для всех остальных символов просто используем переданный указатель font
    const uint8_t *bitmap = font;   // <-- ИЗМЕНЕНИЕ
    uint8_t bytes_per_row = (width + 7) / 8; // для 18 ширины это 3

    uint16_t line_buffer[width];

    ILI9341_SetAddressWindow_DMA(x, y, x + width - 1, y + height - 1);
    GPIOB->BRR  = GPIO_BRR_BR12;
    GPIOB->BSRR = GPIO_BSRR_BS10;

    for (uint8_t row = 0; row < height; row++) {
        for (uint8_t col = 0; col < width; col++) {
            uint32_t byte_idx = (uint32_t)row * bytes_per_row + (col / 8);
            uint8_t bit_idx = 7 - (col % 8);
            uint8_t bit = (bitmap[byte_idx] >> bit_idx) & 1;
            line_buffer[col] = bit ? sw_color : sw_bg;
        }
        SPI2_Transfer_DMA_NoCS((uint8_t*)line_buffer, width * 2);
        if (ILI9341_WaitForDMA()) { /* обработка ошибки */ }
    }

    while (SPI2->SR & SPI_SR_BSY);
    GPIOB->BSRR = GPIO_BSRR_BS12;
}

/**
 * Отрисовка символа из готового битового массива (строчный формат)
 * @param x, y    координаты верхнего левого угла
 * @param bitmap  указатель на битовые данные в строчном формате
 * @param width   ширина символа в пикселях
 * @param height  высота символа
 * @param color   цвет символа
 * @param bg      цвет фона
 */
void ILI9341_DrawChar_Bitmap(uint16_t x, uint16_t y,
                              const uint8_t *bitmap,
                              uint8_t width, uint8_t height,
                              uint16_t color, uint16_t bg)
{
    if (width > MAX_FONT_WIDTH_SUPPORTED || height > MAX_FONT_HEIGHT_SUPPORTED)
        return;

    uint16_t sw_color = (color << 8) | (color >> 8);
    uint16_t sw_bg    = (bg    << 8) | (bg    >> 8);

    uint8_t bytes_per_row = (width + 7) / 8;
    static uint16_t full_buffer[32 * 36]; // максимум 1152 элемента (2304 байта)
    uint16_t *ptr = full_buffer;

    for (uint8_t row = 0; row < height; row++) {
        const uint8_t *row_data = bitmap + row * bytes_per_row;
        for (uint8_t col = 0; col < width; col++) {
            uint8_t byte = row_data[col / 8];
            uint8_t bit = (byte >> (7 - (col % 8))) & 1;
            *ptr++ = bit ? sw_color : sw_bg;
        }
    }

    ILI9341_SetAddressWindow_DMA(x, y, x + width - 1, y + height - 1);
    GPIOB->BRR  = GPIO_BRR_BR12;  // CS = 0
    GPIOB->BSRR = GPIO_BSRR_BS10; // DC = 1

    SPI2_Transfer_DMA_NoCS((uint8_t*)full_buffer, width * height * 2);
    if (ILI9341_WaitForDMA()) { /* TODO: обработка ошибки DMA */ }

    while (SPI2->SR & SPI_SR_BSY);
    GPIOB->BSRR = GPIO_BSRR_BS12; // CS = 1
}

// принудительно сбрасывать кеш для этих координат.
void ResetNumberCache(uint16_t x, uint16_t y) {
    for (uint8_t i = 0; i < cache_count; i++) {
        if (display_cache[i].x == x && display_cache[i].y == y) {
            display_cache[i].last_buffer[0] = '\0';
            display_cache[i].last_value = 0;
            break;
        }
    }
}


//-----06,05,2026

/**
 * Отрисовка текста.
 * @param x, y    координаты верхнего левого угла
 * @param *str    техт
 * @param *font   шрифт
 * @param width   ширина символа в пикселях
 * @param height  высота символа
 * @param color   цвет символа
 * @param bg      цвет фона
 */
void ILI9341_PrintString(uint16_t x, uint16_t y,
                         const char *str,
                         const uint8_t *font,
                         uint8_t width, uint8_t height,
                         uint16_t color,
                         uint16_t bg)
{
    while (*str)
    {
        char c = *str++;

        if (c < 32 || c > 126)
            c = '?';

        uint32_t index = (c - 32);
        const uint8_t *char_data = &font[index * height * 3];

        ILI9341_DrawChar_Opaque_DMA_LineByLine(
            x, y,
            c,
            char_data,
            width, height,
            color, bg
        );

        x += width;
    }
}