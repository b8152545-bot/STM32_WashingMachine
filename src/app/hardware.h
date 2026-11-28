#ifndef HARDWARE_H
#define HARDWARE_H

#include "stm32f103xx_CMSIS.h"
#include "port_mapping.h" // СОПОСТАВЛЕНИЕ ПОРТОВ
#include "zero_cross.h"
#include "ILI9341.h"
#include "pressostat.h"
#include "ADC.h"  
#include "motor.h"
#include "HAL_GPIO.h"
#include "pid.h"
#include "motor_control.h"


void Hardware_Init (void);
void HW_DoorSensorInit(void);
bool HW_IsDoorClosed(void);
bool HW_OpenDoor(void);
bool HW_CloseDoor(void);
void HW_OpenInletValve (void);
void HW_CloseInletValve (void);
void HW_OpenTheRinseAidInletValve (void);
void HW_CloseTheRinseAidInletValve (void);
void HW_StartPumpCirculation (void);
void HW_StopPumpCirculation (void);
void HW_StartPumpDrain (void);
void HW_StopPumpDrain (void);
void HW_EnableHeater (void);
void HW_DisableHeater (void);
uint8_t HW_GetTemperature(void);
void HW_StartMotorWash (void);
void HW_StartMotorSpin(uint16_t rpm);
void HW_StopMotor (void);
void HW_StopAll (void);
void HW_IndicateComplete (void);
void HW_IndicateError(void);
void HW_BuzzerInit(void);
void HW_Buzzer(uint16_t freq, uint16_t duration_ms);
void HW_BuzzerStop(void);


#endif