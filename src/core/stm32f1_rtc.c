#include "stm32f1_rtc.h"
#include "timestamp.h"

//	Деинициализировать часы
void rtc_Reset(void) {
	if (RTC->PRLL != 0x7FFF) { // Если RTC настроены не правильно (это может быть при первом включении девайса)
		RCC->BDCR &= ~RCC_BDCR_RTCEN; // Сбить настройки
	}
}

// Инициализация часов реального времени. Вернет 0 если все ок, вернет !0 - плохо
uint8_t rtc_Init(void) {
	uint32_t cntr = 0; // Счетчик времени ожидания
	rtc_Reset();
	if (!(RCC->BDCR & RCC_BDCR_RTCEN)) { // Если часы реального времени не инициализированы
		RCC->APB1ENR |= RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN; 
		PWR->CR |= PWR_CR_DBP;                                    //	Разрешить доступ к Backup области
		RCC->BDCR |= RCC_BDCR_BDRST;                              // 	Сбросить Backup область
		RCC->BDCR &= ~RCC_BDCR_BDRST;
		RCC->BDCR |= RCC_BDCR_RTCEN | RCC_BDCR_RTCSEL_LSE;        //	Выбрать LSE источник (кварц 32768) и подать тактирование
		RCC->BDCR |= RCC_BDCR_LSEON;                              //	Включить LSE
		while ((RCC->BDCR & RCC_BDCR_LSEON) != RCC_BDCR_LSEON){		//	Дождаться включения
			if (++cntr == 144000000) return 1;
		} 	
		BKP->RTCCR |= 0;                                          //	калибровка RTC было |= 3
		cntr = 0;
		while (!(RTC->CRL & RTC_CRL_RTOFF)) {                			//	проверить закончены ли изменения регистров RTC
			if (++cntr == 144000000) return 2;
		}
		RTC->CRL  |=  RTC_CRL_CNF;                                //	Разрешить Запись в регистры RTC
		RTC->PRLL  =  0x7FFF;                                      //	Настроит делитель на 32768 (32767+1)
		RTC->CRL  &=  ~RTC_CRL_CNF;                               //	Запретить запись в регистры RTC
		cntr = 0;
		while (!(RTC->CRL & RTC_CRL_RTOFF)) {                     //	Дождаться окончания записи
			if (++cntr == 144000000) return 3;
		}
		RTC->CRL &= (uint16_t)~RTC_CRL_RSF;                       //	Синхронизировать RTC
		cntr = 0;
		while((RTC->CRL & RTC_CRL_RSF) != RTC_CRL_RSF) {          //	Дождаться синхронизации
			if (++cntr == 144000000) return 4;
		}
		PWR->CR &= ~PWR_CR_DBP;                                   //	запретить доступ к Backup области

		/*RCC->APB1ENR |= RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN;  //Включить тактирование PWR и Backup
		PWR->CR |= PWR_CR_DBP;  //Разрешить доступ к Backup области
		RCC->BDCR |= RCC_BDCR_BDRST;  //Сбросить Backup область
		RCC->BDCR &= ~RCC_BDCR_BDRST;
		RCC->BDCR |= RCC_BDCR_RTCEN | RCC_BDCR_RTCSEL_LSE;  //Выбрать LSE источник (кварц 32768) и подать тактирование
		RCC->BDCR |= RCC_BDCR_LSEON;  //Включить LSE
		while ((RCC->BDCR & RCC_BDCR_LSEON) != RCC_BDCR_LSEON){} //Дождаться включения
		BKP->RTCCR |= 3;  //калибровка RTC
		while (!(RTC->CRL & RTC_CRL_RTOFF));  //проверить закончены ли изменения регистров RTC
		RTC->CRL  |=  RTC_CRL_CNF;  //Разрешить Запись в регистры RTC
		RTC->PRLL  = 0x7FFF;  //Настроит делитель на 32768 (32767+1)
		BKP->RTCCR |= BKP_RTCCR_CCO;  //Включение вывода Temper
		RTC->CRL  &=  ~RTC_CRL_CNF;  //Запретить запись в регистры RTC
		while (!(RTC->CRL & RTC_CRL_RTOFF));  //Дождаться окончания записи
		RTC->CRL &= (uint16_t)~RTC_CRL_RSF;  //Синхронизировать RTC
		while((RTC->CRL & RTC_CRL_RSF) != RTC_CRL_RSF){}  //Дождаться синхронизации
		PWR->CR &= ~PWR_CR_DBP;  //запретить доступ к Backup области*/

		
	}
	return 0;
}

//	Получить значение счетчика
uint32_t rtc_GetCounter(void) {
	return (uint32_t)((RTC->CNTH << 16) | RTC->CNTL); 
} 

//	Записать новое значение счетчика
void rtc_SetCounter(uint32_t count)	{
	RCC->APB1ENR |= RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN;  //	включить тактирование PWR и Backup
	PWR->CR |= PWR_CR_DBP;                  //	разрешить доступ к Backup области
	while (!(RTC->CRL & RTC_CRL_RTOFF));    //	проверить закончены ли изменения регистров RTC
	RTC->CRL |= RTC_CRL_CNF;                //	Разрешить Запись в регистры RTC
	RTC->CNTH = count>>16;                  //	записать новое значение счетного регистра
	RTC->CNTL = count;
	RTC->CRL &= ~RTC_CRL_CNF;               //	Запретить запись в регистры RTC
	while (!(RTC->CRL & RTC_CRL_RTOFF));    //	Дождаться окончания записи
	PWR->CR &= ~PWR_CR_DBP;                	//	запретить доступ к Backup области
}

RTC_UNIT rtc_GetTime(void) {
	RTC_UNIT dt;
	TS_GetDateTime(rtc_GetCounter(), &dt.year, &dt.month, &dt.date, &dt.hours, &dt.minutes, &dt.seconds);
	return dt;
}

void rtc_SetTime(RTC_UNIT dt) {
	uint32_t temp = TS_GetSeconds(dt.year, dt.month, dt.date, dt.hours, dt.minutes, dt.seconds);
	rtc_SetCounter(temp);
}

// Испортить данные в структуре так, чтобы в ней оказалось нереальное время
void ResetStructDateTime(RTC_UNIT * unit) {
	unit->year = 25;
	unit->month = 2;
	unit->date = 5;
	unit->hours = 19;
	unit->minutes = 04;
	unit->seconds = 10;
}
	