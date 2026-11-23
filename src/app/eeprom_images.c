#include "eeprom_images.h"
#include <string.h>

#define PARAMS_MAGIC 0xA5

#define BLOCK_ROWS 16   // число строк в блоке
static uint8_t block_data[BLOCK_ROWS * 40];   // буфер для блока
static uint8_t line_buffer[640];

WashState wash_state;

// структура для редактирования параметров режимлв
ProgramParam default_params[PROG_COUNT] = {
    {60, 1200},     // хлопок
    {40, 1000},     // синтетика
    {30, 800},      // деликатные
    {30, 400},      // шерсть
    {30, 400},      // быстрая 15
    {30, 400},      // полоскание
    {0, 800},       // отжим
    {0, 0}          // настройки (можно оставить нули)
};
EepromParams eeprom_params;

//функция проверки соединения с EEPROM
uint8_t eeprom_test_connection(void)
{
    uint32_t timeout = 100000;   
    
    i2c_start(1);
    
    // Пытаемся обратиться к устройству
    I2C1->DR = EEPROM_I2C_ADDR;  // Адрес + запись
    
    while((I2C1->SR1 & 2) == 0)
    {
        if(--timeout == 0)
        {
            i2c_stop(1);
            return 0;  // Нет ответа
        }
    }
    
    // Получили ответ - устройство присутствует
    volatile int tmp = I2C1->SR1;
    tmp = I2C1->SR2;
    (void)tmp;
    
    i2c_stop(1);
    return 1;  // Устройство ответило
}

/*
* Универсальная функция для записи любого изображения в EEPROM
*/
void EEPROM_WriteImage(uint16_t start_addr, const uint8_t *image_data, uint16_t image_size)
{
    if (!eeprom_test_connection()) {
        ILI9341_FillScreen_DMA(COLOR_RED);
        while(1);
    }
    ILI9341_FillScreen_DMA(COLOR_BLACK);
    // Попытки записи с ретраями
    {
        const int attempts = 3;
        int rc = 1;
        for (int a = 0; a < attempts; a++) {
            rc = i2c_write_mem(EEPROM_I2C_NUM, EEPROM_I2C_ADDR, start_addr, (const char*)image_data, image_size);
            if (rc == 0) break;
            delay_ms(10);
        }
        if (rc != 0) {
            // Критическая ошибка записи EEPROM — показать красный экран и остановиться
            ILI9341_FillScreen_DMA(COLOR_RED);
            while (1);
        }
    }
    ILI9341_FillScreen_DMA(COLOR_GREEN);
    delay_ms(500);
}

/* Функцмя чтения картинки из EEPROM
 * @param x, y          координаты верхнего левого угла
 * @param image_addr    указатель на адрес в EEPROM
 * @param width         ширина в пикселях
 * @param height        высота в пикселях
 */
void DrawImage_FromEEPROM(uint16_t x, uint16_t y,
                          uint16_t image_addr,
                          uint16_t width, uint16_t height)
{
    uint8_t bytes_per_row = (width + 7) / 8;
    int center = height / 2;
    int total_blocks = (height + BLOCK_ROWS - 1) / BLOCK_ROWS; // общее количество блоков
    int center_block = center / BLOCK_ROWS;                    // блок, содержащий центр

    // Перебираем блоки, начиная с центрального, расходясь к краям
    for (int offset = 0; offset <= total_blocks; offset++) {
        // Блок выше центра (если существует)
        int block_above = center_block - offset;
        if (block_above >= 0) {
            uint16_t start_row = block_above * BLOCK_ROWS;
            uint16_t end_row = start_row + BLOCK_ROWS - 1;
            if (end_row >= height) end_row = height - 1;
            uint16_t rows_in_block = end_row - start_row + 1;

            // Читаем блок из EEPROM
            uint16_t addr = image_addr + start_row * bytes_per_row;
            // Чтение с ретраями
            {
                const int attempts = 3;
                int rc = 1;
                for (int a = 0; a < attempts; a++) {
                    rc = i2c_read_eeprom(EEPROM_I2C_NUM, EEPROM_I2C_ADDR, addr,
                                         (char*)block_data, rows_in_block * bytes_per_row);
                    if (rc == 0) break;
                    delay_ms(5);
                }
                if (rc != 0) {
                    // Ошибка чтения — можно пропустить блок или индицировать ошибку
                    ILI9341_FillScreen_DMA(COLOR_RED);
                    while (1);
                }
            }

            // Выводим строки блока (от начала блока к концу)
            for (uint16_t r = 0; r < rows_in_block; r++) {
                uint16_t row = start_row + r;
                const uint8_t *row_data = block_data + r * bytes_per_row;

                // Устанавливаем окно на одну строку
                ILI9341_SetAddressWindow_DMA(x, y + row, x + width - 1, y + row);
                if (ILI9341_WaitForDMA()) {
                    // таймаут DMA — можно добавить логику восстановления или повторной передачи
                    // TODO: обработать ошибку DMA (перезапуск SPI/DMA)
                }
                GPIOB->BRR = GPIO_BRR_BR12;
                GPIOB->BSRR = GPIO_BSRR_BS10;

                // Конвертация в RGB565
                for (uint16_t col = 0; col < width; col++) {
                    uint8_t byte = row_data[col / 8];
                    uint8_t bit = (byte >> (7 - (col % 8))) & 1;
                    uint16_t pixel = bit ? COLOR_YELLOW : COLOR_BLACK;
                    line_buffer[2 * col]     = pixel >> 8;
                    line_buffer[2 * col + 1] = pixel & 0xFF;
                }

                SPI2_Transfer_DMA_NoCS(line_buffer, width * 2);
                if (ILI9341_WaitForDMA()) {
                    // TODO: обработать ошибку DMA
                }
            }
        }

        // Блок ниже центра (если есть и не совпадает с только что выведенным)
        int block_below = center_block + offset;
        if (offset > 0 && block_below < total_blocks) {
            uint16_t start_row = block_below * BLOCK_ROWS;
            uint16_t end_row = start_row + BLOCK_ROWS - 1;
            if (end_row >= height) end_row = height - 1;
            uint16_t rows_in_block = end_row - start_row + 1;

            uint16_t addr = image_addr + start_row * bytes_per_row;
            // Чтение с ретраями
            {
                const int attempts = 3;
                int rc = 1;
                for (int a = 0; a < attempts; a++) {
                    rc = i2c_read_eeprom(EEPROM_I2C_NUM, EEPROM_I2C_ADDR, addr,
                                         (char*)block_data, rows_in_block * bytes_per_row);
                    if (rc == 0) break;
                    delay_ms(5);
                }
                if (rc != 0) {
                    ILI9341_FillScreen_DMA(COLOR_RED);
                    while (1);
                }
            }

            for (uint16_t r = 0; r < rows_in_block; r++) {
                uint16_t row = start_row + r;
                const uint8_t *row_data = block_data + r * bytes_per_row;

                ILI9341_SetAddressWindow_DMA(x, y + row, x + width - 1, y + row);
                if (ILI9341_WaitForDMA()) {
                    // TODO: обработка ошибки
                }
                GPIOB->BRR = GPIO_BRR_BR12;
                GPIOB->BSRR = GPIO_BSRR_BS10;

                for (uint16_t col = 0; col < width; col++) {
                    uint8_t byte = row_data[col / 8];
                    uint8_t bit = (byte >> (7 - (col % 8))) & 1;
                    uint16_t pixel = bit ? COLOR_YELLOW : COLOR_BLACK;
                    line_buffer[2 * col]     = pixel >> 8;
                    line_buffer[2 * col + 1] = pixel & 0xFF;
                }

                SPI2_Transfer_DMA_NoCS(line_buffer, width * 2);
                if (ILI9341_WaitForDMA()) {
                    // TODO: обработка ошибки
                }
            }
        }
    }

    while (SPI2->SR & SPI_SR_BSY);
    GPIOB->BSRR = GPIO_BSRR_BS12;
}

// функции для чтения блока данных в EEPROM
void LoadProgramParams(void) {
    i2c_read_eeprom(EEPROM_I2C_NUM, EEPROM_I2C_ADDR, PARAMS_EEPROM_ADDR,
                    (char*)&eeprom_params, sizeof(eeprom_params));

    // Проверяем валидность (например, по magic)
    if (eeprom_params.magic != PARAMS_MAGIC) {
        // При первом запуске или сбое - инициализируем дефолтными значениями
        eeprom_params.magic = PARAMS_MAGIC;
        memcpy(eeprom_params.programs, default_params, sizeof(default_params));
        // Сохраняем в EEPROM
        i2c_write_mem(EEPROM_I2C_NUM, EEPROM_I2C_ADDR, PARAMS_EEPROM_ADDR,
                      (const char*)&eeprom_params, sizeof(eeprom_params));
    }
    
}

// функции для записи блока данных в EEPROM
void SaveProgramParams(void) {
    
    static EepromParams previous;   // статическая копия последних сохранённых данных
    static uint8_t first = 1;

    if (!first && memcmp(&previous, &eeprom_params, sizeof(eeprom_params)) == 0) {
        return;
    }
    if (memcmp(&previous, &eeprom_params, sizeof(eeprom_params)) == 0) {
        return;  // ничего не изменилось
    }
    // иначе сохраняем
    i2c_write_mem(EEPROM_I2C_NUM, EEPROM_I2C_ADDR, PARAMS_EEPROM_ADDR,
                  (const char*)&eeprom_params, sizeof(eeprom_params));
    // обновляем копию
    memcpy(&previous, &eeprom_params, sizeof(eeprom_params));
}

// записали точку программы
void SaveWashState(void) {
    // Обновляем CRC перед записью
    wash_state.crc = 0; // или вычислить реальную CRC
    i2c_write_mem(EEPROM_I2C_NUM, EEPROM_I2C_ADDR, STATE_EEPROM_ADDR,
                  (const char*)&wash_state, sizeof(wash_state));
    // ILI9341_FillRect_DMA(170, 1, 20, 20, COLOR_GREEN); // использовалось для отладки
}

void LoadWashState(void) {
    i2c_read_eeprom(EEPROM_I2C_NUM, EEPROM_I2C_ADDR, STATE_EEPROM_ADDR,
                    (char*)&wash_state, sizeof(wash_state));
    // ILI9341_FillRect_DMA(130, 1, 20, 20, COLOR_MAGENTA); // использовалось для отладки                
    // Проверка magic и CRC (опционально)
}