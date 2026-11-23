#pragma once
#ifndef ILI9341_CONFIG_H
#define ILI9341_CONFIG_H


// Выбери одну ориентацию 0°-90°-180°-270°
#define ILI9341_ORIENTATION_270

#ifdef ILI9341_ORIENTATION_0
    #define ILI9341_Width  240
    #define ILI9341_Height 320
    #define ILI9341_MADCTL 0x48
#endif
#ifdef ILI9341_ORIENTATION_90
    #define ILI9341_Width  320
    #define ILI9341_Height 240
    #define ILI9341_MADCTL 0x28
#endif
#ifdef ILI9341_ORIENTATION_180
    #define ILI9341_Width  240
    #define ILI9341_Height 320
    #define ILI9341_MADCTL 0x88
#endif
#ifdef ILI9341_ORIENTATION_270
    #define ILI9341_Width  320
    #define ILI9341_Height 240
    #define ILI9341_MADCTL 0xE8
#endif


#define	COLOR_BLACK   0x0000
#define	COLOR_BLUE    0x001F
#define	COLOR_RED     0xF800
#define	COLOR_GREEN   0x07E0
#define	COLOR_CYAN    0x07FF
#define	COLOR_MAGENTA 0xF81F
#define COLOR_YELLOW  0xFFE0
#define COLOR_WHITE   0xFFFF


#define MAX_00001    5      // например: 5 → 00123
#define MAX_0001     4      // например: 4 → 0042
#define MAX_001      3      // например: 3 → 001
#define MAX_01       2      // например: 2 → 07
#define ALIGN_LEFT      0      // прибить слева
#define ALIGN_RIGHT     1      // прибить справа
#define MODE_ALIGN      0      // обычный вывод
#define MODE_PADDED     1      // (режим ведущие нули)


#define IS_FLOAT_YAS    1      // выодитть данные как float
#define IS_FLOAT_NO     0      // выодитть данные как целое число

#define MAX_ZNAK_01     1      // например: 1 → 0.7
#define MAX_ZNAK_02     2      // например: 2 → 0.07
#define MAX_ZNAK_03     3      // например: 3 → 0.007
#define MAX_ZNAK_04     4      // например: 4 → 0.0007

#include <stdint.h>

// Таймаут для ожидания завершения SPI DMA передачи
#ifndef SPI_DMA_TIMEOUT
#define SPI_DMA_TIMEOUT 200000u
#endif

// Маленькая обёртка ожидания завершения DMA с таймаутом.
// Возвращает 0 при успешном завершении, 1 при таймауте.
static inline int ILI9341_WaitForDMA(void)
{
    extern volatile uint8_t spi2_dma_tx_done; // флаг в SPI2.c
    uint32_t t = 0;
    while (!spi2_dma_tx_done && t++ < SPI_DMA_TIMEOUT) {
#if defined(__ARMCC_VERSION) || defined(__GNUC__)
        __asm volatile ("nop");
#endif
    }
    return spi2_dma_tx_done ? 0 : 1;
}

#endif
