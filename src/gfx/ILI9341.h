#ifndef ILI9341_H
#define ILI9341_H

#pragma once

#include "SPI2.h"
#include "ili9341_Config.h"
#include "ili9341_hw.h"
#include "ili9341_cmd.h"
#include "ili9341_gfx.h"
#include "ili9341_text.h"
#include "ili9341_time.h"




const uint8_t Font_33x37[27*37*5]; // часовой курсив

const uint8_t Font_K_95_18x21[95*21*3]; // Тестовый список

const uint8_t Font_25x29[27*29*4]; // данные параметров стирки (температура - отжим)




#endif
