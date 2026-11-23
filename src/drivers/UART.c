#include "UART.h"


// Буферы и флаги определены здесь (см. UART.h)
volatile uint8_t tx_buffer[UART_TX_BUFFER_SIZE]; // Буфер под исходящие данные
volatile uint8_t rx_buffer[UART_RX_BUFFER_SIZE]; // Буфер под входящие данные
// DMA-only reception: DMA заполняет rx_buffer в circular режиме.
// rx_len содержит число доступных байт, uart_rx_ready сигнализирует основному циклу.
volatile uint16_t rx_len = 0; // Количество доступных байт в rx_buffer (для совместимости)
// Для безопасной работы с circular буфером используем две позиции: head (DMA writes) и tail (reader)
volatile uint16_t rx_tail = 0; // индекс, до которого пользователь уже прочитал
volatile uint8_t uart_rx_ready = 0; // флаг: новые данные готовы к обработке
volatile uint8_t uart_tx_busy = 0; // флаг занятости TX DMA
volatile uint8_t uart_rx_error = 0; // флаг ошибки RX (DMA/USART)

static volatile uint16_t tx_idx = 0;
static volatile uint16_t tx_len = 0;

// Инициализация DMA для USART1: канал 3 — RX (DMA1_Channel3), канал 4 — TX (DMA1_Channel4)
void USART_DMA_Init(void)
{
	// Включаем тактирование DMA1
	RCC->AHBENR |= RCC_AHBENR_DMA1EN;

	// Настроим канал 3 (USART1_RX) - circular, memory increment, peripheral to memory
	DMA1_Channel3->CCR = 0;
	DMA1_Channel3->CPAR = (uint32_t)&(USART1->DR);
	DMA1_Channel3->CMAR = (uint32_t)rx_buffer;
	DMA1_Channel3->CNDTR = UART_RX_BUFFER_SIZE;
	// Включаем memory increment, transfer complete interrupt и half-transfer interrupt
	DMA1_Channel3->CCR = DMA_CCR_MINC | DMA_CCR_TCIE | DMA_CCR_HTIE; 
	// Установим circular bit отдельно (DMA_CCR_CIRC)
	DMA1_Channel3->CCR |= DMA_CCR_CIRC;

	// NVIC для DMA1_Channel3 (RX)
	NVIC_SetPriority(DMA1_Channel3_IRQn, 3);
	NVIC_EnableIRQ(DMA1_Channel3_IRQn);

	/*/ Настроим канал 4 (USART1_TX) - memory to peripheral, normal mode
	DMA1_Channel4->CCR = 0;
	DMA1_Channel4->CPAR = (uint32_t)&(USART1->DR);
	DMA1_Channel4->CMAR = (uint32_t)tx_buffer;
	DMA1_Channel4->CNDTR = 0;
	DMA1_Channel4->CCR = DMA_CCR_MINC | DMA_CCR_DIR | DMA_CCR_TCIE | DMA_CCR_TEIE;

	NVIC_SetPriority(DMA1_Channel4_IRQn, 3);
	NVIC_EnableIRQ(DMA1_Channel4_IRQn);
	*/
}

void UART_StartRxDMA(void)
{
	// Ensure RX DMA channel is disabled
	DMA1_Channel3->CCR &= ~DMA_CCR_EN;
	DMA1_Channel3->CMAR = (uint32_t)rx_buffer;
	DMA1_Channel3->CNDTR = UART_RX_BUFFER_SIZE;
	// Clear pending flags (including HT)
	DMA1->IFCR = DMA_IFCR_CGIF3 | DMA_IFCR_CTCIF3 | DMA_IFCR_CTEIF3 | DMA_IFCR_CHTIF3;
	// Enable channel
	DMA1_Channel3->CCR |= DMA_CCR_EN;
	// Also enable USART RX DMA request
	USART1->CR3 |= USART_CR3_DMAR;
}

/*
* Отправка данных с использованием DMA. Возвращает 0 в случае успеха, 
* ненулевое значение, если данные заняты/слишком большие.
*/
int UART_Send_DMA(const uint8_t *data, uint16_t len)
{
	if (len == 0 || len > UART_TX_BUFFER_SIZE) return 1; // invalid
	if (uart_tx_busy) return 2; // busy

	// копируем данные в tx_buffer
	for (uint16_t i = 0; i < len; i++) tx_buffer[i] = data[i];

    tx_idx = 0;
    tx_len = len;
	uart_tx_busy = 1;
	
    // Отключаем аппаратный DMA для USART, чтобы не конфликтовать с экраном
    USART1->CR3 &= ~USART_CR3_DMAT;
    
    // Включаем прерывание по опустошению регистра данных (TXE)
    USART1->CR1 |= USART_CR1_TXEIE;

	return 0;
	/*if (len == 0 || len > UART_TX_BUFFER_SIZE) return 1; // invalid
	if (uart_tx_busy) return 2; // busy

	// копируем данные в tx_buffer
	for (uint16_t i = 0; i < len; i++) tx_buffer[i] = data[i];

	// Настроим канал 4
	DMA1_Channel4->CCR &= ~DMA_CCR_EN;
	DMA1->IFCR = DMA_IFCR_CGIF4 | DMA_IFCR_CTCIF4 | DMA_IFCR_CTEIF4;
	DMA1_Channel4->CMAR = (uint32_t)tx_buffer;
	DMA1_Channel4->CNDTR = len;
	uart_tx_busy = 1;
	// Включаем канал
	DMA1_Channel4->CCR |= DMA_CCR_EN;
	// Включаем DMA запрос на передачу USART
	USART1->CR3 |= USART_CR3_DMAT;

	return 0;*/
}

uint8_t UART_IsTxBusy(void)
{
    if (uart_tx_busy) {
        // Если передача уже завершилась (CNDTR == 0), но прерывание не сработало
        if (DMA1_Channel4->CNDTR == 0 || !(DMA1_Channel4->CCR & DMA_CCR_EN)) {
            DMA1->IFCR = DMA_IFCR_CTCIF4;
            DMA1_Channel4->CCR &= ~DMA_CCR_EN;
            uart_tx_busy = 0;
            USART1->CR3 &= ~USART_CR3_DMAT;
        }
    }
    return uart_tx_busy;
}

void USART_Init (void) {

    USART_DMA_Init();    // настраивает каналы DMA1_Channel3 (RX) и DMA1_Channel4 (TX)
    UART_StartRxDMA();   // запускает циркулярный приём в буфер rx_buffer

	SET_BIT(RCC->APB2ENR, RCC_APB2ENR_IOPAEN); //Включение тактирование порта А
	SET_BIT(RCC->APB2ENR, RCC_APB2ENR_AFIOEN); //Включение альтернативных функций

		//Для конфигурирование ножек UART для Full Duplex нужно использовать Alternate function push-pull(См. п.п. 9.1.11 GPIO configurations for device peripherals стр.111 Reference Manual)
		//Tx - Alternative Function output Push-pull(Maximum output speed 50 MHz)
	MODIFY_REG(GPIOA->CRH, GPIO_CRH_CNF9_Msk, 0b10 << GPIO_CRH_CNF9_Pos);
	MODIFY_REG(GPIOA->CRH, GPIO_CRH_MODE9_Msk, 0b11 << GPIO_CRH_MODE9_Pos);
		//Rx - Input floating
	MODIFY_REG(GPIOA->CRH, GPIO_CRH_CNF10_Msk, 0b1 << GPIO_CRH_CNF10_Pos);
	MODIFY_REG(GPIOA->CRH, GPIO_CRH_MODE10_Msk, 0b00 << GPIO_CRH_MODE10_Pos);

		//Запустим тактирование USART1
	SET_BIT(RCC->APB2ENR, RCC_APB2ENR_USART1EN);

		///Расчет Fractional baud rate generation
		//есть формула:
		//Tx/Rx baud = fCK/(16*USARTDIV)
		//где fCK - Input clock to the peripheral (PCLK1 for USART2, 3, 4, 5 or PCLK2 for USART1)
		//в нашем случае fCK = 72000000
		//допустим нам нужна скорость 9600
		//9600 = 72000000/(16*USARTDIV)

		//Тогда USARTDIV = 72000000/9600*16 = 468.75
		//DIV_Mantissa в данном случае будет 468, что есть 0x1D4
		//DIV_Fraction будет, как 0.75*16 = 12, что есть 0xC

		//Тогда весь регистр USART->BRR для скорости 9600 будет выглядеть, как 0x1D4C.

		//для примера еще разберем скорость 115200:
		//115200 = 72000000/(16*USARTDIV)
		//Тогда USARTDIV = 72000000/115200*16 = 39.0625
		//DIV_Mantissa в данном случае будет 39, что есть 0x27
		//DIV_Fraction будет, как 0.0625*16 = 1, что есть 0x1

		//Тогда весь регистр USART->BRR для скорости 115200 будет выглядеть, как 0x271.

		

	MODIFY_REG(USART1->BRR, USART_BRR_DIV_Mantissa_Msk, 0x27 << USART_BRR_DIV_Mantissa_Pos);
	MODIFY_REG(USART1->BRR, USART_BRR_DIV_Fraction_Msk, 0x1 << USART_BRR_DIV_Fraction_Pos);
		//27.6.4 Control register 1(USART_CR1)(см. стр 821 Reference Manual)
	SET_BIT(USART1->CR1, USART_CR1_UE); //USART enable
	CLEAR_BIT(USART1->CR1, USART_CR1_M); //Длина слова 1 стартовый бит, 8 бит данных и стоповый бит
	CLEAR_BIT(USART1->CR1, USART_CR1_WAKE); //Просыпаться бездействующая линия
	CLEAR_BIT(USART1->CR1, USART_CR1_PCE); //Контроль четности отключен
		// настройка прерываний
		CLEAR_BIT(USART1->CR1, USART_CR1_PEIE); // прерывание ошибки четности отключено
		CLEAR_BIT(USART1->CR1, USART_CR1_TXEIE); // Прерывание TXE заблокировано
		CLEAR_BIT(USART1->CR1, USART_CR1_TCIE); // Прерывание завершения передачи отключено
		// В DMA-only режиме приёма отключаем RXNE и IDLE прерывания
		CLEAR_BIT(USART1->CR1, USART_CR1_RXNEIE);
		CLEAR_BIT(USART1->CR1, USART_CR1_IDLEIE);
	SET_BIT(USART1->CR1, USART_CR1_TE); //Передатчик включен
	SET_BIT(USART1->CR1, USART_CR1_RE); //Приемник включается и начинает поиск стартового бита.
	CLEAR_BIT(USART1->CR1, USART_CR1_RWU);
	CLEAR_BIT(USART1->CR1, USART_CR1_SBK);

		//Остальную настройку, не касающуюся стандартного USART, мы пока трогать не будем, но на всякий случай обнулим CR2 и GTPR
		USART1->CR2 = 0;
		USART1->GTPR = 0;

		// Установим приоритет обработчика USART и включим прерывания
		NVIC_SetPriority(USART1_IRQn, 3);
		NVIC_EnableIRQ(USART1_IRQn); //Включим прерывания по USART
}

void USART_Singl_Traslit (uint8_t *data) {
	// Ожидаем, пока TXE станет 1 (передатчик готов принять байт)
	while ((USART1->SR & USART_SR_TXE) == 0) ;
	USART1->DR = *data;
}


void CMSIS_USART_Transmit(USART_TypeDef *USART, uint8_t *data, uint16_t Size) {
	for (uint16_t i = 0; i < Size; i++) {
		while ((USART->SR & USART_SR_TXE) == 0); // Ждём, пока передатчик готов
		USART->DR = *data++; // Кидаем данные
}
}


// В DMA-only режиме USART IRQ используется только для ошибок (ORE/FE/NE)
void USART1_IRQHandler(void) {
	uint32_t sr = USART1->SR;

	// --- ОБРАБОТКА ПЕРЕДАЧИ ЧЕРЕЗ ПРЕРЫВАНИЯ ---
    if ((sr & USART_SR_TXE) && (USART1->CR1 & USART_CR1_TXEIE)) {
        if (tx_idx < tx_len) {
            USART1->DR = tx_buffer[tx_idx++]; // Отправляем следующий байт
        } else {
            USART1->CR1 &= ~USART_CR1_TXEIE;  // Данные кончились - выключаем прерывание
            uart_tx_busy = 0;                 // Освобождаем флаг
        }
    }

	if (sr & (USART_SR_ORE | USART_SR_FE | USART_SR_NE)) {
		// Сброс ошибок: чтение SR и DR
		volatile uint32_t tmp = USART1->DR;
		(void)tmp;
		// Помечаем ошибку приёма для основного цикла.
		// Примечание: при ORE/FE/NE перезапуск DMA не требуется — достаточно
		// вызвать UART_ClearRxError() в основном цикле и продолжить чтение данных
		// из rx_buffer через UART_Receive_GetData().
		uart_rx_error = 1;
	}
}

// DMA1 Channel3 IRQ handler (USART1_RX)
void DMA1_Channel3_IRQHandler(void)
{
	// Половинный перенос (HT) — полезно для низкой задержки обработки
	if (DMA1->ISR & DMA_ISR_HTIF3) {
		DMA1->IFCR = DMA_IFCR_CHTIF3;
		// Половина буфера заполнена: помечаем готовность и оставляем tail без изменений
		uart_rx_ready = 1;
		return;
	}

	// Transfer complete for RX (TC) — в circular режиме это означает, что указатель вернулся в начало
	if (DMA1->ISR & DMA_ISR_TCIF3) {
		DMA1->IFCR = DMA_IFCR_CTCIF3;
		// В circular режиме при TC указатель вернулся в начало.
		uart_rx_ready = 1;
		return;
	}

	// Transfer error
	if (DMA1->ISR & DMA_ISR_TEIF3) {
		DMA1->IFCR = DMA_IFCR_CTEIF3;
		// Попытка восстановить канал: перезапускаем, не затирая CMAR.
		DMA1_Channel3->CCR &= ~DMA_CCR_EN;
		DMA1_Channel3->CNDTR = UART_RX_BUFFER_SIZE;
		// Сброс флагов уже сделан выше
		DMA1_Channel3->CCR |= DMA_CCR_EN;
		// Сообщаем об ошибке (можно ставить флаг или логировать)
		uart_rx_error = 1;
		uart_rx_ready = 1; // сигнализируем, чтобы основной цикл мог проверить ошибку и перезапустить при необходимости
		return;
	}
}

// Получить данные из rx_buffer в пользовательский буфер.
// Возвращает количество скопированных байт.
// Функция защищена временным отключением прерываний для избежания гонок.
uint16_t UART_Receive_GetData(uint8_t *out, uint16_t max_len)
{
	uint16_t copied = 0;
	// 1) Коротко блокируем прерывания, снимаем snapshot head и tail
	uint32_t prim = __get_PRIMASK();
	__disable_irq();

	uint16_t head = UART_RX_BUFFER_SIZE - DMA1_Channel3->CNDTR;
	uint16_t tail = rx_tail;

	uint16_t available;
	if (head >= tail) available = head - tail;
	else available = UART_RX_BUFFER_SIZE - tail + head;

	uint16_t to_copy = (available < max_len) ? available : max_len;
	// Разрешаем прерывания — копирование будем делать без блокировки, опираясь на snapshot
	if (!prim) __enable_irq();

	if (to_copy > 0) {
		for (uint16_t i = 0; i < to_copy; i++) {
			uint16_t idx = tail + i;
			if (idx >= UART_RX_BUFFER_SIZE) idx -= UART_RX_BUFFER_SIZE;
			out[i] = rx_buffer[idx];
		}
		copied = to_copy;
	}

	// 2) Обновляем rx_tail и флаги под защитой
	prim = __get_PRIMASK();
	__disable_irq();
	// убедимся, что rx_tail не был изменён (он не меняется в IRQ), просто продвинем его
	rx_tail = rx_tail + copied;
	if (rx_tail >= UART_RX_BUFFER_SIZE) rx_tail -= UART_RX_BUFFER_SIZE;

	// Обновим rx_len для совместимости и флаг готовности
	// Пересчитаем head и available актуально
	head = UART_RX_BUFFER_SIZE - DMA1_Channel3->CNDTR;
	if (head >= rx_tail) rx_len = head - rx_tail;
	else rx_len = UART_RX_BUFFER_SIZE - rx_tail + head;
	if (rx_len == 0) uart_rx_ready = 0;

	if (!prim) __enable_irq();
	return copied;
}

// DMA1 Channel4 IRQ handler (USART1_TX)
void DMA1_Channel4_IRQHandler(void)
{
	if (DMA1->ISR & DMA_ISR_TCIF4) {
		DMA1->IFCR = DMA_IFCR_CTCIF4;
		// Отключаем канал и помечаем TX как свободный
		DMA1_Channel4->CCR &= ~DMA_CCR_EN;
		uart_tx_busy = 0;
		// Отключаем DMA запрос на передачу
		USART1->CR3 &= ~USART_CR3_DMAT;
		return;
	}
	if (DMA1->ISR & DMA_ISR_TEIF4) {
		DMA1->IFCR = DMA_IFCR_CTEIF4;
		DMA1_Channel4->CCR &= ~DMA_CCR_EN;
		uart_tx_busy = 0;
		USART1->CR3 &= ~USART_CR3_DMAT;
		return;
	}
}

// Очистить флаг ошибки приёма. Если была ошибка DMA/USART, основной цикл может сделать
// UART_ClearRxError(); и затем вызвать UART_StartRxDMA(); чтобы перезапустить приём.
void UART_ClearRxError(void)
{
	uart_rx_error = 0;
}
