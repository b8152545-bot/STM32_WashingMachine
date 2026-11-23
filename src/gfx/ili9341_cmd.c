#include "ili9341_cmd.h"
#include "stm32f103xx_CMSIS.h"
#include "ili9341_Config.h"




void ILI9341_Init(void)
{
    //ILI9341_SetRotation(0); // Установка ориентации по умолчанию
   
    // --- Аппаратный RESET ---
    GPIOB->BRR = GPIO_BRR_BR11;   // RESET = 0
    delay_ms(20);
    GPIOB->BSRR = GPIO_BSRR_BS11; // RESET = 1
    delay_ms(150);

    // --- Power Control A ---
    {
        static const uint8_t pwrA[] = {0x39, 0x2C, 0x00, 0x34, 0x02};
        ILI9341_SendCommandWithData_DMA_OneCS(0xCB, pwrA, 5);
    if (ILI9341_WaitForDMA()) { /* TODO: обработка ошибки DMA */ }
    }

    // --- Power Control B ---
    {
        static const uint8_t pwrB[] = {0x00, 0xC1, 0x30};
        ILI9341_SendCommandWithData_DMA_OneCS(0xCF, pwrB, 3);
    if (ILI9341_WaitForDMA()) { /* TODO: обработка ошибки DMA */ }
    }

    // --- Driver timing control A ---
    {
        static const uint8_t dtca[] = {0x85, 0x00, 0x78};
        ILI9341_SendCommandWithData_DMA_OneCS(0xE8, dtca, 3);
    if (ILI9341_WaitForDMA()) { /* TODO: обработка ошибки DMA */ }
    }

    // --- Driver timing control B ---
    {
        static const uint8_t dtcb[] = {0x00, 0x00};
        ILI9341_SendCommandWithData_DMA_OneCS(0xEA, dtcb, 2);
    if (ILI9341_WaitForDMA()) { /* TODO: обработка ошибки DMA */ }
    }

    // --- Power on sequence control ---
    {
        static const uint8_t posc[] = {0x64, 0x03, 0x12, 0x81};
        ILI9341_SendCommandWithData_DMA_OneCS(0xED, posc, 4);
    if (ILI9341_WaitForDMA()) { /* TODO: обработка ошибки DMA */ }
    }

    // --- Pump ratio control ---
    {
        static const uint8_t prc[] = {0x20};
        ILI9341_SendCommandWithData_DMA_OneCS(0xF7, prc, 1);
    if (ILI9341_WaitForDMA()) { /* TODO: обработка ошибки DMA */ }
    }

    // --- Power Control 1 ---
    {
        static const uint8_t pwr1[] = {0x23};
        ILI9341_SendCommandWithData_DMA_OneCS(0xC0, pwr1, 1);
    if (ILI9341_WaitForDMA()) { /* TODO: обработка ошибки DMA */ }
    }

    // --- Power Control 2 ---
    {
        static const uint8_t pwr2[] = {0x10};
        ILI9341_SendCommandWithData_DMA_OneCS(0xC1, pwr2, 1);
        while (!spi2_dma_tx_done);
    }

    // --- VCOM Control 1 ---
    {
        static const uint8_t vcom1[] = {0x3E, 0x28};
        ILI9341_SendCommandWithData_DMA_OneCS(0xC5, vcom1, 2);
        while (!spi2_dma_tx_done);
    }

    // --- VCOM Control 2 ---
    {
        static const uint8_t vcom2[] = {0x86};
        ILI9341_SendCommandWithData_DMA_OneCS(0xC7, vcom2, 1);
        while (!spi2_dma_tx_done);
    }

    // --- Memory Access Control ---
    {
        static const uint8_t madctl[] = {ILI9341_MADCTL};
        ILI9341_SendCommandWithData_DMA_OneCS(0x36, madctl, 1);
        while (!spi2_dma_tx_done);
    }

    // --- Pixel Format ---
    {
        static const uint8_t pixfmt[] = {0x55};
        ILI9341_SendCommandWithData_DMA_OneCS(0x3A, pixfmt, 1);
        while (!spi2_dma_tx_done);
    }

    // --- Frame Rate Control ---
    {
        static const uint8_t frctrl[] = {0x00, 0x18};
        ILI9341_SendCommandWithData_DMA_OneCS(0xB1, frctrl, 2);
        while (!spi2_dma_tx_done);
    }

    // --- Display Function Control ---
    {
        static const uint8_t dfc[] = {0x08, 0x82, 0x27};
        ILI9341_SendCommandWithData_DMA_OneCS(0xB6, dfc, 3);
        while (!spi2_dma_tx_done);
    }

    // --- Gamma Function Disable ---
    {
        static const uint8_t gamma_disable[] = {0x00};
        ILI9341_SendCommandWithData_DMA_OneCS(0xF2, gamma_disable, 1);
        while (!spi2_dma_tx_done);
    }

    // --- Gamma Set ---
    {
        static const uint8_t gamma_set[] = {0x01};
        ILI9341_SendCommandWithData_DMA_OneCS(0x26, gamma_set, 1);
        while (!spi2_dma_tx_done);
    }

    // --- Positive Gamma Correction ---
    {
        static const uint8_t pgamma[] = {
            0x0F, 0x31, 0x2B, 0x0C, 0x0E,
            0x08, 0x4E, 0xF1, 0x37, 0x07,
            0x10, 0x03, 0x0E, 0x09, 0x00
        };
        ILI9341_SendCommandWithData_DMA_OneCS(0xE0, pgamma, 15);
        while (!spi2_dma_tx_done);
    }

    // --- Negative Gamma Correction ---
    {
        static const uint8_t ngamma[] = {
            0x00, 0x0E, 0x14, 0x03, 0x11,
            0x07, 0x31, 0xC1, 0x48, 0x08,
            0x0F, 0x0C, 0x31, 0x36, 0x0F
        };
        ILI9341_SendCommandWithData_DMA_OneCS(0xE1, ngamma, 15);
        while (!spi2_dma_tx_done);
    }

    // --- Exit Sleep ---
    ILI9341_SendCommand_DMA(0x11);
    while (!spi2_dma_tx_done);
    delay_ms(120);

    // --- Display ON ---
    ILI9341_SendCommand_DMA(0x29);
    while (!spi2_dma_tx_done);
    delay_ms(20);
}

//  Установка окна для последующей передачи пикселей через DMA
void ILI9341_SetAddressWindow_DMA(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    // CASET
    uint8_t data1[4] = { x0 >> 8, x0 & 0xFF, x1 >> 8, x1 & 0xFF };
    ILI9341_SendCommandWithData_DMA_OneCS(0x2A, data1, 4);
    while (!spi2_dma_tx_done);

    // RASET
    uint8_t data2[4] = { y0 >> 8, y0 & 0xFF, y1 >> 8, y1 & 0xFF };
    ILI9341_SendCommandWithData_DMA_OneCS(0x2B, data2, 4);
    while (!spi2_dma_tx_done);

    // RAMWR
    ILI9341_SendCommand_DMA(0x2C);
    while (!spi2_dma_tx_done);
}


//  Отправка команды с данными через DMA
// Command + Data ОДНИМ CS
void ILI9341_SendCommandWithData_DMA_OneCS(uint8_t cmd,const uint8_t *data, uint16_t size)
{
    while (!spi2_dma_tx_done);

    GPIOB->BRR = GPIO_BRR_BR12;   // CS ↓

    // CMD
    GPIOB->BRR = GPIO_BRR_BR10;   // DC = 0
    SPI2_Transfer_DMA_NoCS(&cmd, 1);
    while (!spi2_dma_tx_done);

    // DATA
    GPIOB->BSRR = GPIO_BSRR_BS10; // DC = 1
    SPI2_Transfer_DMA_NoCS((uint8_t*)data, size);
    while (!spi2_dma_tx_done);

    GPIOB->BSRR = GPIO_BSRR_BS12; // CS ↑
}
// 📌 Здесь только datasheet ILI9341, никакой графики