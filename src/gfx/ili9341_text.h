

#pragma once
#include <stdint.h>



extern void ILI9341_DrawChar_Opaque_DMA_LineByLine(uint16_t x, uint16_t y,
                                           char c,
                                           const uint8_t *font,
                                           uint8_t width, uint8_t height,
                                           uint16_t color, uint16_t bg);                                 

void DrawNumber_NoFlicker(uint16_t x, uint16_t y,
                          float value, uint8_t is_float,
                          uint8_t decimals,
                          uint8_t align_right, uint8_t max_digits,
                          const uint8_t* font,
                          uint8_t width, uint8_t height,
                          uint16_t color, uint16_t bg);

void ILI9341_DrawChar_Bitmap(uint16_t x, uint16_t y,
                              const uint8_t *bitmap,
                              uint8_t width, uint8_t height,
                              uint16_t color, uint16_t bg);

void ResetNumberCache(uint16_t x, uint16_t y);

//---06,05,2026

void ILI9341_PrintString(uint16_t x, uint16_t y,
                         const char *str,
                         const uint8_t *font,
                         uint8_t width, uint8_t height,
                         uint16_t color,
                         uint16_t bg);