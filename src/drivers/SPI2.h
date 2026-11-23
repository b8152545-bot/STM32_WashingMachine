#ifndef SPI2_H_
#define SPI2_H_
//------------------------------------------------
#include "stm32f1xx.h"
//------------------------------------------------
#define CS_IDLE	        *((uint32_t *)(PERIPH_BB_BASE+32*(GPIOB_BASE-PERIPH_BASE+0xC) + 12*4)) = 1
#define RESET_ACTIVE	*((uint32_t *)(PERIPH_BB_BASE+32*(GPIOB_BASE-PERIPH_BASE+0xC) + 10*4)) = 1
#define DC_COMMAND		*((uint32_t *)(PERIPH_BB_BASE+32*(GPIOB_BASE-PERIPH_BASE+0xC) + 11*4)) = 1
//------------------------------------------------
// Флаги состояния (экспортируемые в main)
extern volatile uint8_t spi2_dma_tx_done;
// Флаг ошибки DMA (устанавливается в IRQ при ошибке передачи)
extern volatile uint8_t spi2_dma_error;

// Высокоуровневая функция отправки с ожиданием и попытками восстановления.
// Возвращает 0 при успешной передаче, !=0 при ошибке.
int SPI2_SendAndWait(uint8_t *tx, uint16_t size, int attempts);

// Функции инициализации/транзакции
void SPI2_GPIO_Init(void);
void SPI2_Init(void);
void DMA_SPI2_Init(void);
void SPI2_Transfer_DMA_NoCS(uint8_t *tx, uint16_t size);
void ILI9341_DrawRect_DMA(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t thickness, uint16_t color);
void DMA1_Channel5_IRQHandler(void);
//------------------------------------------------
#endif /* SPI2_H_ */