#include "ADC.h"
#include <stdint.h>

#define DMA_BUFF_SIZE 2

volatile uint16_t samples[2] = {0,0}; // обязательно volatile

void delay_ms(uint32_t ms); // объявление внешней функции

void ADC_init(void)
{
    // Предделитель АЦП: ADCPRE = DIV6 (12 МГц при 72 МГц)
    RCC->CFGR |= RCC_CFGR_ADCPRE_DIV6;

    // Включаем тактирование: ADC1, AFIO, GPIOС, DMA1
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN | RCC_APB2ENR_AFIOEN | RCC_APB2ENR_IOPCEN;
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;

    // Настройка PС0 и PС1 как аналоговые входы (CNF=00, MODE=00)
    GPIOC->CRL &= ~(GPIO_CRL_CNF0 | GPIO_CRL_MODE0);
    GPIOC->CRL &= ~(GPIO_CRL_CNF1 | GPIO_CRL_MODE1);

    // Максимальное время выборки 239.5 циклов для каналов 0 и 1
    ADC1->SMPR2 |= ADC_SMPR2_SMP0 | ADC_SMPR2_SMP1; // SMP5[2:0] = 111, SMP7[2:0] = 111

    // Длина последовательности: 2 преобразования (L = 1)
    ADC1->SQR1 = (2 - 1) << 20;

    // Последовательность: первый канал 0, второй канал 1
    ADC1->SQR3 = (10 << 0) | (11 << 5);  // канал 10 (PC0) и канал 11 (PC1)

    // Включаем сканирование и DMA
    ADC1->CR1 |= ADC_CR1_SCAN;
    ADC1->CR2 |= ADC_CR2_DMA;

    // Настройка DMA1, канал 1
    DMA1_Channel1->CPAR = (uint32_t)&ADC1->DR;
    DMA1_Channel1->CMAR = (uint32_t)samples;
    DMA1_Channel1->CNDTR = DMA_BUFF_SIZE;
    DMA1_Channel1->CCR |= DMA_CCR_CIRC    // циклический режим
                        | DMA_CCR_MINC    // инкремент адреса памяти
                        | DMA_CCR_PSIZE_0 // размер периферии 16 бит
                        | DMA_CCR_MSIZE_0 // размер памяти 16 бит
                        | DMA_CCR_EN;     // включить DMA

    // Включение АЦП: сначала ADON и CONT, затем повторный ADON после задержки
    ADC1->CR2 |= ADC_CR2_ADON | ADC_CR2_CONT;
    delay_ms(1);
    ADC1->CR2 |= ADC_CR2_ADON;
    delay_ms(1);

    // Калибровка (опционально)
    // Запуск калибровки
    ADC1->CR2 |= ADC_CR2_CAL;
    while (ADC1->CR2 & ADC_CR2_CAL); // ждём окончания калибровки
    delay_ms(2);
}

// Возвращает значение канала (0 - PС0, 1 - PС1)
/*uint16_t read_ADC(uint16_t ch)
{
    if (ch < DMA_BUFF_SIZE)
        return samples[ch];
    else
        return 0;
}*/
uint16_t read_ADC(uint16_t ch)
{
    if (ch == 0) return samples[0];  // samples[0] будет канал 10 (PC0)
    else return samples[1];          // канал 11 (PC1)
}