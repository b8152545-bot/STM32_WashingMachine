#pragma once
#include <stdint.h>
#include "rtc_unit.h"



void UpdateRTC_TimeFull(
    uint16_t x, uint16_t y,
    const uint8_t *font,
    uint8_t width, uint8_t height,
    uint16_t color, uint16_t bg);

void UpdateRTC_DateFull(
    uint16_t x, uint16_t y, const uint8_t *font,
    uint8_t width, uint8_t height,
    uint16_t color, uint16_t bg);

void DisplayDate(uint16_t x, uint16_t y, const uint8_t* font,
                 uint8_t w, uint8_t h, uint16_t color, uint16_t bg,
                 struct RTC_UNIT date, uint8_t active_field);

void DisplayTime(uint16_t x, uint16_t y, const uint8_t* font,
                 uint8_t w, uint8_t h, uint16_t color, uint16_t bg,
                 struct RTC_UNIT time, uint8_t active_field);

void ResetTimeDisplayCache(void);

void ResetDateDisplayCache(void);

/*void UpdateRTC_TimeFull_FromEEPROM(
    uint16_t x, uint16_t y,
    uint8_t width, uint8_t height,
    uint16_t color, uint16_t bg);*/
