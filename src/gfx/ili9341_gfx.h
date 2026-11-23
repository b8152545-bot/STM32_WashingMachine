#pragma once
#include <stdint.h>




void ILI9341_FillScreen_DMA(uint16_t color);
void ILI9341_FillRect_DMA(uint16_t x, uint16_t y,
                          uint16_t w, uint16_t h,
                          uint16_t color);

void ILI9341_DrawRect_DMA(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t thickness, uint16_t color);

void ILI9341_DrawMonochromeImage(uint16_t x, uint16_t y,
                                 const uint8_t *bitmap,
                                 uint16_t width, uint16_t height,
                                 uint16_t color, uint16_t bg);  

void ProgressBar (uint16_t x, uint16_t y,
                  uint16_t width, uint16_t height,
                  uint8_t thickness,
                  uint16_t value, uint16_t max_val,
                  uint16_t color, uint16_t bg);                                                        
