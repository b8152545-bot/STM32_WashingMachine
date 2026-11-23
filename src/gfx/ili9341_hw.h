#pragma once
#include <stdint.h>


void ILI9341_SendCommand_DMA(uint8_t cmd);
void ILI9341_SendData_DMA(uint8_t data);
void ILI9341_SendPixels_DMA(uint16_t *pixels, uint32_t count);

// 📌 Почему отдельно?
// Если завтра будет другой дисплей или SPI, ты трогаешь один файл.