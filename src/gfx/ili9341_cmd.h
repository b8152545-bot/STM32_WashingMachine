#pragma once
#include <stdint.h>


void ILI9341_Init(void);
void ILI9341_SetAddressWindow_DMA(uint16_t x0, uint16_t y0,
                                  uint16_t x1, uint16_t y1);

void ILI9341_SendCommandWithData_DMA_OneCS(uint8_t cmd,
                                          const uint8_t *data,
                                          uint16_t size);

// 📌 Здесь только datasheet ILI9341, никакой графики