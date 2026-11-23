#include "ILI9341.h"
#include "stm32f103xx_CMSIS.h"


// 1. Отправка команды через DMA
void ILI9341_SendCommand_DMA(uint8_t cmd)
{
    if (ILI9341_WaitForDMA()) { /* TODO: обработка ошибки DMA */ }

    GPIOB->BRR = GPIO_BRR_BR12;   // CS ↓
    GPIOB->BRR = GPIO_BRR_BR10;   // DC = 0

    SPI2_Transfer_DMA_NoCS(&cmd, 1);

    if (ILI9341_WaitForDMA()) { /* TODO: обработка ошибки DMA */ }
    while (SPI2->SR & SPI_SR_BSY);

    GPIOB->BSRR = GPIO_BSRR_BS12; // CS ↑
}

// 2. Отправка одного байта данных через DMA
void ILI9341_SendData_DMA(uint8_t data)
{
    // DC = 1 → данные
    GPIOB->BSRR = GPIO_BSRR_BS10;

    SPI2_Transfer_DMA_NoCS(&data, 1);

    // ⬇️ дождаться окончания
    if (ILI9341_WaitForDMA()) { /* TODO: обработка ошибки DMA */ }
    while (SPI2->SR & SPI_SR_BSY);
}

// 3. Отправка массива пикселей (RGB565) через DMA в 8-битном режиме
void ILI9341_SendPixels_DMA(uint16_t *pixels, uint32_t count)
{
    // Локальный буфер для преобразования 16-бит → 2×8-бит
    // Размер должен быть >= count*2
    static uint8_t buffer[512]; // пример: 256 пикселей за раз

    // Ждём завершения предыдущей передачи
    if (ILI9341_WaitForDMA()) { /* TODO: обработка ошибки DMA */ }

    // DC = 1 → данные
    GPIOB->BSRR = GPIO_BSRR_BS10;

    // CS = 0 → начало транзакции
    GPIOB->BRR = GPIO_BRR_BR12;

    // Преобразуем массив пикселей в байты
    for (uint32_t i = 0; i < count; i++)
    {
        buffer[2*i]     = pixels[i] >> 8;   // старший байт
        buffer[2*i + 1] = pixels[i] & 0xFF; // младший байт
    }

    // Запускаем DMA на 2*count байт
    SPI2_Transfer_DMA_NoCS((uint8_t*)buffer, count * 2);

    // Ждём окончания
    if (ILI9341_WaitForDMA()) { /* TODO: обработка ошибки DMA */ }

    // CS = 1 → конец транзакции
    GPIOB->BSRR = GPIO_BSRR_BS12;
}