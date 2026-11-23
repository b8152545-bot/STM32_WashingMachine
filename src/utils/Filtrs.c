#include "Filtrs.h"

#define NUM_READ		40	// порядок медианы

#define IN_MIN			0		//	Верхний предел сигнала ADC
#define IN_MAX			4000	//	Нижний предел сигнала ADC
#define OUT_MIN			100		//	Новый дапозон ADC
#define OUT_MAX			0		//	Новый дапозон ADC

float k = 0.03;					// коэффициент фильтрации, 0.0-1.0

// медиана на N значений со своим буфером, ускоренный вариант
uint16_t findMedianN(uint16_t newVal) {
  static uint16_t buffer[NUM_READ];  // статический буфер
  static uint8_t count = 0;
  buffer[count] = newVal;
  if ((count < NUM_READ - 1) & (buffer[count] > buffer[count + 1])) {
    for (int i = count; i < NUM_READ - 1; i++) {
      if (buffer[i] > buffer[i + 1]) {
        float buff = buffer[i];
        buffer[i] = buffer[i + 1];
        buffer[i + 1] = buff;
      }
    }
  } else {
    if ((count > 0) & (buffer[count - 1] > buffer[count])) {
      for (int i = count; i > 0; i--) {
        if (buffer[i] < buffer[i - 1]) {
          uint16_t buff = buffer[i];
          buffer[i] = buffer[i - 1];
          buffer[i - 1] = buff;
        }
      }
    }
  }
  if (++count >= NUM_READ) count = 0;
  return buffer[(int)NUM_READ / 2];
}


/*
* "Сила" фильтра настраивается коэффициентом (0.0 - 1.0).
*  Чем он меньше, тем плавнее фильтр
*  &&
*  Втавленна функция map()
*  для изменения диапозона
*/
uint16_t Filtr (uint16_t newVal) {
	static uint16_t filVal = 0;
	filVal += (newVal - filVal) * k;

	return (filVal - IN_MIN) * (OUT_MAX - OUT_MIN) / (IN_MAX - IN_MIN) + OUT_MIN;
}

/*
*  Arduino - вская функцыя
*/
/*float constrain(float x, float l, float h) {
	return (x < l) ? l : (x > h) ? h : x;
}*/

