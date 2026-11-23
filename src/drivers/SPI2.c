#include "SPI2.h"
#include "ILI9341.h"

#define SPEED_2MHz   0b10   // выход 2 МГц
#define SPEED_10MHz  0b01   // выход 10 МГц
#define SPEED_50MHz  0b11   // выход 50 МГц
#define GPIO_SPEED   SPEED_10MHz   // теперь действительно ** МГц

// Флаги состояния передачи/приёма через DMA (можно читать из main/task)
// убрать static, чтобы main мог видеть эти переменные через extern в заголовке

volatile uint8_t spi2_dma_tx_done = 0;
// Флаг ошибки DMA (0 — нет ошибки, 1 — ошибка)
volatile uint8_t spi2_dma_error = 0;

//Инициализация GPIO для SPI2
void SPI2_GPIO_Init(void)
{
  // Включаем тактирование порта B
  SET_BIT(RCC->APB2ENR, RCC_APB2ENR_IOPBEN);

  // PB10 — RESET (Output push‑pull) 
  MODIFY_REG(GPIOB->CRH, 
              GPIO_CRH_MODE10_Msk | GPIO_CRH_CNF10_Msk, 
              (GPIO_SPEED << GPIO_CRH_MODE10_Pos) |
              (0b00 << GPIO_CRH_CNF10_Pos)); // push‑pull 
    
  // Вывести RESET = 1 (нормальная работа) 
  GPIOB->BSRR = GPIO_BSRR_BS10;

  // PB11 — DC (Output push‑pull) 
  MODIFY_REG(GPIOB->CRH, 
              GPIO_CRH_MODE11_Msk | GPIO_CRH_CNF11_Msk, 
              (GPIO_SPEED << GPIO_CRH_MODE11_Pos) |
              (0b00 << GPIO_CRH_CNF11_Pos)); // push‑pull 
    
  // Установить DC = 1 (режим данных) 
  GPIOB->BSRR = GPIO_BSRR_BS11;

  // PB12 — CS (Output push‑pull)
  MODIFY_REG(GPIOB->CRH, 
              GPIO_CRH_MODE12_Msk | GPIO_CRH_CNF12_Msk, 
              (GPIO_SPEED << GPIO_CRH_MODE12_Pos) | 
              (0b00 << GPIO_CRH_CNF12_Pos)); // CNF12 = 00 → push‑pull

  // Установить CS = 1 (не выбрано) 
  GPIOB->BSRR = GPIO_BSRR_BS12;      

  // PB13 — SCK (AF push‑pull)
  MODIFY_REG(GPIOB->CRH,
              GPIO_CRH_MODE13_Msk | GPIO_CRH_CNF13_Msk,
              (GPIO_SPEED << GPIO_CRH_MODE13_Pos) | 
              (0b10 << GPIO_CRH_CNF13_Pos)); // CNF13 = 10 → AF push‑pull 

   // PB14 — MISO (input floating)
   MODIFY_REG(GPIOB->CRH,
               GPIO_CRH_MODE14_Msk | GPIO_CRH_CNF14_Msk, 
               (0b00 << GPIO_CRH_MODE14_Pos) | // MODE14 = 00 → вход 
               (0b01 << GPIO_CRH_CNF14_Pos)); // CNF14 = 01 → вход плавающий
   
   // PB15 — MOSI (AF push‑pull)
   MODIFY_REG(GPIOB->CRH, 
              GPIO_CRH_MODE15_Msk | GPIO_CRH_CNF15_Msk, 
              (GPIO_SPEED << GPIO_CRH_MODE15_Pos) | 
              (0b10 << GPIO_CRH_CNF15_Pos)); // CNF15 = 10 → AF push‑pull            
}

void SPI2_Init(void)
{
    SPI2_GPIO_Init(); // MOSI/SCK/CS/DC/RESET
    

    // Тактирование SPI2
    RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;
    RCC->APB1RSTR |= RCC_APB1RSTR_SPI2RST;
    RCC->APB1RSTR &= ~RCC_APB1RSTR_SPI2RST;

    // SPI2: Master, 8 бит, CPOL=1, CPHA=1, SSM/SSI, BR = Fpclk/4
    SPI2->CR1 = 0;
    SPI2->CR1 = SPI_CR1_MSTR    // мастер
              | SPI_CR1_SSM     // программное NSS
              | SPI_CR1_SSI
              | SPI_CR1_CPOL    // полярность
              | SPI_CR1_CPHA    // фаза
              | SPI_CR1_BR_0;   // делитель /4 (APB1=36 МГц → 9 МГц)

    SPI2->CR2 = 0; // пока без DMA

    SPI2->CR1 |= SPI_CR1_SPE; // включить SPI
}

void DMA_SPI2_Init(void)
{
    // Тактирование DMA1
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;

    // TX: SPI2_TX → DMA1 Channel5
    DMA1_Channel5->CCR = 0;
    DMA1_Channel5->CPAR = (uint32_t)&SPI2->DR; // периферия — SPI2->DR

    // 8 бит периферия и память, направление: память → периферия
    DMA1_Channel5->CCR = DMA_CCR_MINC      // инкремент памяти
                       | DMA_CCR_DIR       // из памяти в периферию
                       | DMA_CCR_TCIE      // прерывание по завершению
                       | DMA_CCR_TEIE;     // прерывание по ошибке

    // Разрешаем TX DMA в SPI2
    SPI2->CR2 |= SPI_CR2_TXDMAEN;

    // NVIC для DMA1_Channel5
    NVIC_SetPriority(DMA1_Channel5_IRQn, 3);
    NVIC_EnableIRQ(DMA1_Channel5_IRQn);

    // Канал готов к первой передаче
    spi2_dma_tx_done = 1;
}

// Передача данных без выбора CS
// Низкоуровневая передача: запускает DMA и не ждёт; возвращает 0 всегда (оставлено для обратной совместимости)
void SPI2_Transfer_DMA_NoCS(uint8_t *tx, uint16_t size)
{
    while (!spi2_dma_tx_done);
    while (SPI2->SR & SPI_SR_BSY);

    spi2_dma_tx_done = 0;
    spi2_dma_error = 0;

    DMA1->IFCR = DMA_IFCR_CGIF5 | DMA_IFCR_CTCIF5 |
                 DMA_IFCR_CHTIF5 | DMA_IFCR_CTEIF5;

    DMA1_Channel5->CCR &= ~DMA_CCR_EN;
    DMA1_Channel5->CMAR  = (uint32_t)tx;
    DMA1_Channel5->CNDTR = size;
    DMA1_Channel5->CCR |= DMA_CCR_EN;
}

// Высокоуровневая передача с повторными попытками при ошибке/таймауте
// attempts: число попыток (>=1). Возвращает 0 при успехе, !=0 при неудаче.
int SPI2_SendAndWait(uint8_t *tx, uint16_t size, int attempts)
{
    for (int a = 0; a < attempts; a++) {
        spi2_dma_error = 0;
        SPI2_Transfer_DMA_NoCS(tx, size);
        // Ожидаем завершения DMA (использует ILI9341_WaitForDMA)
        if (!ILI9341_WaitForDMA() && !spi2_dma_error) {
            return 0; // успешно
        }

        // В случае ошибки/таймаута — попробуем выполнить восстановление: сбросить DMA, очистить флаги
        DMA1_Channel5->CCR &= ~DMA_CCR_EN;
        DMA1->IFCR = DMA_IFCR_CGIF5 | DMA_IFCR_CTCIF5 | DMA_IFCR_CTEIF5;
        // Небольшая пауза перед повтором
        for (volatile int i = 0; i < 1000; i++) __asm volatile ("nop");
    }
    return 1; // не удалось после attempts
}

//Обновлённый IRQ
void DMA1_Channel5_IRQHandler(void)
{
    // Transfer complete
    if (DMA1->ISR & DMA_ISR_TCIF5)
    {
        DMA1->IFCR = DMA_IFCR_CTCIF5;
        DMA1_Channel5->CCR &= ~DMA_CCR_EN;

        while (SPI2->SR & SPI_SR_BSY);

        spi2_dma_tx_done = 1;
        return;
    }

    // Transfer error
    if (DMA1->ISR & DMA_ISR_TEIF5)
    {
        DMA1->IFCR = DMA_IFCR_CTEIF5;
        DMA1_Channel5->CCR &= ~DMA_CCR_EN;
        spi2_dma_error = 1;
        spi2_dma_tx_done = 1; // разблокируем ожидающие
        return;
    }
}




