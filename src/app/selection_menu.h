#ifndef _SELECTION_MENU_H
#define _SELECTION_MENU_H

#include "ILI9341.h"
#include "encoder.h"
#include "stm32f103xx_CMSIS.h"
#include "stm32f1_rtc.h"
#include "i2c_drive.h"
#include "eeprom_images.h"
#include "hardware.h"
#include "zero_cross.h"
#include "motor_control.h"
#include "autotune.h"

// Перечисление состояний системы
typedef enum {
    STATE_POWER_ON,
    STATE_SELECT_PROGRAM,
    STATE_RUNNING,
    STATE_EDIT_PROGRAM,
    STATE_SETUP,
    STATE_RESUME_PROMPT,   // новое состояние
    STATE_ERROR
} SystemState;

// Перечисление программ стирки (если ещё не вынесено)
typedef enum {
    PROG_COTTON,
    PROG_SYNTHETICS,
    PROG_DELICATE,
    PROG_WOOL,
    PROG_FAST15,
    PROG_RINSE,
    PROG_SPIN,
    PROG_SETUP,
    PROG_COUNT_
} WashProgram;

// Глобальные переменные, используемые в main.c
extern SystemState system_state;
extern WashProgram current_program;
extern uint16_t power_on_timer;
extern uint8_t display;   // если нужен в main.c, но в вашем main.c display не используется, можно не объявлять

extern volatile uint32_t last_zero_cross_time;
extern volatile uint32_t SysTimer_ms;   // из вашего SysTick-обработчика

                                                                                          
void Setup_Time (void);
void DrawCurrentProgram(void);
void Menu_Update(uint16_t dt_ms);
void EditProgramParameters (uint16_t pr);
void Setup_ServiceMenu(void);

#endif