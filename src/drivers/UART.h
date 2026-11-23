#ifndef UART_H_
#define UART_H_

#include "stm32f1xx.h"

// Размеры буферов для UART (можно настроить)
#define UART_TX_BUFFER_SIZE 256
#define UART_RX_BUFFER_SIZE 128

// Внешние буферы и флаги (определены в UART.c)
extern volatile uint8_t tx_buffer[UART_TX_BUFFER_SIZE];
extern volatile uint8_t rx_buffer[UART_RX_BUFFER_SIZE];
// В DMA-only режиме используем только rx_len и uart_rx_ready
extern volatile uint16_t rx_len; // длина доступных данных в rx_buffer
extern volatile uint8_t uart_rx_ready; // флаг: данные готовы к обработке

// Флаг ошибки приёма (DMA/USART)
extern volatile uint8_t uart_rx_error;

// Получение данных из rx буфера. Возвращает число скопированных байт.
uint16_t UART_Receive_GetData(uint8_t *out, uint16_t max_len);

// Очистить флаг ошибки приёма (uart_rx_error).
// После возникновения ошибки приема (uart_rx_error == 1) рекомендуется вызвать
// UART_ClearRxError(); и затем UART_StartRxDMA(); для перезапуска приёма.
// Примечание по обработке ошибок:
// - При ошибках приемника USART (ORE/FE/NE) обычно перезапуск DMA не требуется.
//   Драйвер помечает `uart_rx_error == 1`. Приложение может вызвать
//   `UART_ClearRxError()` и продолжать читать доступные байты из буфера через
//   `UART_Receive_GetData()`. Перезапуск DMA (`UART_StartRxDMA()`) требуется
//   только если приложение решит явно восстановить канал после серьёзной ошибки.
// - При ошибках DMA (TEIF) драйвер также выставляет `uart_rx_error == 1` и
//   сигнализирует приложению; в этом случае рекомендуется перезапустить приём
//   вызовом `UART_StartRxDMA()` после очистки флага.
void UART_ClearRxError(void);

void USART_Init(void);
void USART_Singl_Traslit(uint8_t *data);
void CMSIS_USART_Transmit(USART_TypeDef *USART, uint8_t *data, uint16_t Size);
void USART1_IRQHandler(void);

// DMA helpers
void USART_DMA_Init(void);

// Начало заполнения буфера приема RX DMA (циклический режим)
void UART_StartRxDMA(void);

// Отправка данных с использованием DMA. Возвращает 0 в случае успеха, 
// ненулевое значение, если данные заняты/слишком большие.
int UART_Send_DMA(const uint8_t *data, uint16_t len);

// Проверьте, занят ли TX DMA.
uint8_t UART_IsTxBusy(void);


extern volatile uint8_t rx_buffer[UART_RX_BUFFER_SIZE];

#endif
