#include "ili9341_gfx.h"
#include "stm32f103xx_CMSIS.h"
#include "ili9341_Config.h"

// Заливка всего экрана одним цветом через DMA (8-битный SPI)
void ILI9341_FillScreen_DMA(uint16_t color)
{
    // 1. Установить окно на весь экран
    ILI9341_SetAddressWindow_DMA(
        0, 0,
        ILI9341_Width  - 1,
        ILI9341_Height - 1
    );

    // 2. Подготовить строку пикселей
    static uint16_t line[320];
    static uint8_t  buffer[640]; // 320 * 2 байта

    for (uint16_t i = 0; i < ILI9341_Width; i++)
        line[i] = color;

    // 3. Начало передачи пикселей
    if (ILI9341_WaitForDMA()) { /* TODO: обработка ошибки DMA */ }

    GPIOB->BRR  = GPIO_BRR_BR12; // CS ↓
    GPIOB->BSRR = GPIO_BSRR_BS10; // DC = 1 (DATA)

    // 4. Отправить ВСЕ строки без отпускания CS
    for (uint16_t y = 0; y < ILI9341_Height; y++)
    {
        // конвертация RGB565 → 8bit stream
        for (uint16_t i = 0; i < ILI9341_Width; i++)
        {
            buffer[2*i]     = line[i] >> 8;
            buffer[2*i + 1] = line[i] & 0xFF;
        }

        SPI2_Transfer_DMA_NoCS(buffer, ILI9341_Width * 2);
    if (ILI9341_WaitForDMA()) { /* TODO: обработка ошибки DMA */ }
    }

    // 5. Завершение транзакции
    while (SPI2->SR & SPI_SR_BSY);
    GPIOB->BSRR = GPIO_BSRR_BS12; // CS ↑
}

/* Заливка прямоугольника
* x, y - координаты верхнего левого угла
* w, h - ширина и высота прямоугольника
* color - цвет заливки
*/
void ILI9341_FillRect_DMA(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    if (w == 0 || h == 0) return;

    // 1. Установить окно
    ILI9341_SetAddressWindow_DMA(x, y, x + w - 1, y + h - 1);

    static uint8_t buffer[480]; // максимум 240 пикселей

    // 2. Подготовить одну строку
    for (uint16_t i = 0; i < w; i++) {
        buffer[2*i]     = color >> 8;
        buffer[2*i + 1] = color & 0xFF;
    }

    // 3. Начать RAMWR поток
    if (ILI9341_WaitForDMA()) { /* TODO: обработка ошибки DMA */ }
    GPIOB->BRR  = GPIO_BRR_BR12; // CS ↓
    GPIOB->BSRR = GPIO_BSRR_BS10; // DC = DATA

    // 4. Передать все строки подряд
    for (uint16_t row = 0; row < h; row++) {
        SPI2_Transfer_DMA_NoCS(buffer, w * 2);
    if (ILI9341_WaitForDMA()) { /* TODO: обработка ошибки DMA */ }
    }

    // 5. Завершить транзакцию
    while (SPI2->SR & SPI_SR_BSY);
    GPIOB->BSRR = GPIO_BSRR_BS12; // CS ↑
}

/* Рисование рамки (контур) Если нужен только контур, можно нарисовать 4 линии:
* x, y - координаты верхнего левого угла
* w, h - ширина и высота прямоугольника
* color - цвет заливки
*/
void ILI9341_DrawRect_DMA(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t thickness, uint16_t color)
{
    // Верхняя линия
    ILI9341_FillRect_DMA(x, y, w, thickness, color);

    // Нижняя линия
    ILI9341_FillRect_DMA(x, y + h - thickness, w, thickness, color);

    // Левая линия
    ILI9341_FillRect_DMA(x, y, thickness, h, color);

    // Правая линия
    ILI9341_FillRect_DMA(x + w - thickness, y, thickness, h, color);
}

/* Вывод монохромного изображения на дисплей.
 * bitmap - упакованные данные: каждая строка занимает bytes_per_row = (width+7)/8 байт,
 *          биты упакованы так, что первый пиксель строки (самый левый) соответствует старшему биту первого байта (бит 7).
 * x, y   - координаты верхнего левого угла.
 * width, height - размеры изображения в пикселях.
 * color  - цвет переднего плана (где бит = 1)
 * bg     - цвет фона (где бит = 0)
 */
void ILI9341_DrawMonochromeImage(uint16_t x, uint16_t y,
                                 const uint8_t *bitmap,
                                 uint16_t width, uint16_t height,
                                 uint16_t color, uint16_t bg)
{
    if (width == 0 || height == 0) return;

    // Количество байт на строку в исходном массиве
    uint8_t bytes_per_row = (width + 7) / 8;

    // Устанавливаем окно вывода
    ILI9341_SetAddressWindow_DMA(x, y, x + width - 1, y + height - 1);

    // Ожидаем готовности SPI
    if (ILI9341_WaitForDMA()) { /* TODO: обработка ошибки DMA */ }
    GPIOB->BRR  = GPIO_BRR_BR12;   // CS = 0
    GPIOB->BSRR = GPIO_BSRR_BS10;  // DC = 1 (данные)

    // Буфер для одной строки пикселей в формате RGB565, подготовленный для отправки (старший байт первым)
    static uint8_t line_buffer[640];  // максимум 320 пикселей * 2 байта

    // Для каждой строки изображения
    for (uint16_t row = 0; row < height; row++) {
        const uint8_t *row_data = bitmap + row * bytes_per_row;

        // Заполняем line_buffer пикселями текущей строки
        for (uint16_t col = 0; col < width; col++) {
            uint8_t byte = row_data[col / 8];
            uint8_t bit = (byte >> (7 - (col % 8))) & 1; // старший бит — левый пиксель
            uint16_t pixel = bit ? color : bg;

            // Старший байт пикселя отправляется первым
            line_buffer[2 * col]     = pixel >> 8;
            line_buffer[2 * col + 1] = pixel & 0xFF;
        }

        // Отправляем строку
        SPI2_Transfer_DMA_NoCS(line_buffer, width * 2);
    if (ILI9341_WaitForDMA()) { /* TODO: обработка ошибки DMA */ }
    }

    // Завершение передачи
    while (SPI2->SR & SPI_SR_BSY);
    GPIOB->BSRR = GPIO_BSRR_BS12;   // CS = 1
}

/* Виджет, прогрес полоса
*  x, y   - координаты верхнего левого угла.
*  width, height - размеры изображения в пикселях.
*  thickness - толщина рамки.
*  value - переменная.
*  max_val - максимальное значение, для пропорции
*  color  - цвет заливки
*  bg     - цвет рамки
*/ 
void ProgressBar (uint16_t x, uint16_t y,
                  uint16_t width, uint16_t height,
                  uint8_t thickness,
                  uint16_t value, uint16_t max_val,
                  uint16_t color, uint16_t bg)
{
    // Рисуем рамку цветом color
    ILI9341_DrawRect_DMA(x, y, width, height, thickness, color);

    // Внутренняя область рамки
    uint16_t inner_x = x + thickness;
    uint16_t inner_y = y + thickness;
    uint16_t inner_width = width - 2 * thickness;
    uint16_t inner_height = height - 2 * thickness;

    // Очищаем внутреннюю область чёрным (фон)
    ILI9341_FillRect_DMA(inner_x, inner_y, inner_width, inner_height, COLOR_BLACK);

    // Вычисляем ширину заполнения пропорционально значению
    uint32_t fill_width = 0;
    if (max_val > 0) {
        fill_width = ((uint32_t)value * inner_width) / max_val;
        if (fill_width > inner_width) fill_width = inner_width;
    }

    // Рисуем прогресс цветом bg
    if (fill_width > 0) {
        ILI9341_FillRect_DMA(inner_x, inner_y, fill_width, inner_height, bg);
    }
}
// 📌 Ни одного символа или RTC тут быть не должно.