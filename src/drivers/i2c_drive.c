#include "i2c_drive.h"
#include "stm32f103xx_CMSIS.h"

/*
I2C2
PB10 -> SCL
PB11 -> SDA

I2C1
PB6 -> SCL
PB7 -> SDA

*/


/*
*I2C1
*PB6 -> SCL
*PB7 -> SDA
*/
void i2c_init(char i2c, unsigned short speed_mode)
{
    RCC->APB2ENR |= 1; 
    
    if(i2c == 1)
    {
        RCC->APB1ENR |= 0x200000;
        // Pin enable 
        init_GP(PB, 6, OUT50, O_AF_OD);
        init_GP(PB, 7, OUT50, O_AF_OD);
        
        // Сброс I2C
        I2C1->CR1 |= 0x8000;
        I2C1->CR1 &= ~0x8000;
        
        // Частота тактирования I2C: 36 МГц (APB1 = 36 МГц при SysClk 72 МГц)
        I2C1->CR2 = 36;
        
        // Настройка скорости
        I2C1->CCR = speed_mode;
        
        // Настройка времени нарастания
        if(speed_mode & 0x8000) {
            // Быстрый режим (400 кГц): TRISE = 36MHz * 300ns / 1000 + 1 = 12
            I2C1->TRISE = 12;
        } else {
            // Стандартный режим (100 кГц): TRISE = 36MHz * 1000ns / 1000 + 1 = 37
            I2C1->TRISE = 37;
        }
        
        // Включение I2C
        I2C1->CR1 |= 1;

        // Включение I2C
        I2C1->CR1 |= 1;

        // Включение подтверждения (ACK) для приёма
        I2C1->CR1 |= 0x0400;   // бит 10 = ACK
    }
    else if(i2c == 2)
    {
        RCC->APB1ENR |= 0x400000;
        // Pin enable 
        init_GP(PB, 10, OUT50, O_AF_OD);
        init_GP(PB, 11, OUT50, O_AF_OD);
        
        I2C2->CR1 |= 0x8000;
        I2C2->CR1 &= ~0x8000;
        I2C2->CR2 = 36;
        I2C2->CCR = speed_mode;
        
        if(speed_mode & 0x8000) {
            I2C2->TRISE = 12;
        } else {
            I2C2->TRISE = 37;
        }
        
        I2C2->CR1 |= 1;

        // Включение I2C
        I2C1->CR1 |= 1;

        // Включение подтверждения (ACK) для приёма
        I2C1->CR1 |= 0x0400;   // бит 10 = ACK
            }
}

// Start step
int i2c_start(char i2c)
{
    uint32_t timeout = 100000;

    if(i2c == 1)
    {
        I2C1->CR1 |= 0x100;
        while (!(I2C1->SR1 & 1))
        {
            if(--timeout == 0)
                return 1;
        }
    }
    else if(i2c == 2)
    {
        I2C2->CR1 |= 0x100;
        while (!(I2C2->SR1 & 1))
        {
            if(--timeout == 0)
                return 1;
        }
    }

    return 0;
}

// Sending the address + R or Write    
// Возвращает 0 при успехе, 1 при таймауте
int i2c_add(char i2c, char address, char RW)
{
    volatile int tmp;
    uint32_t timeout = 100000;

    if(i2c == 1)
    {
        I2C1->DR = (address | RW);

        while((I2C1->SR1 & 2) == 0)
        {
            if(--timeout == 0)
                return 1; // Таймаут
        }

        tmp = I2C1->SR1;
        tmp = I2C1->SR2;
        (void)tmp;
    }
    else if(i2c == 2)
    {
        I2C2->DR = (address | RW);

        while((I2C2->SR1 & 2) == 0)
        {
            if(--timeout == 0)
                return 1;
        }

        tmp = I2C2->SR1;
        tmp = I2C2->SR2;
        (void)tmp;
    }

    return 0;
}

// Аналогично для i2c_data
// Исправленная функция отправки данных
int i2c_data(char i2c,char data)
{
	if(i2c==1)
	{
		while((I2C1->SR1 & 0x80) == 0){}
			I2C1->DR = data;
		while((I2C1->SR1 & 0x80) == 0){}
	}
	else if(i2c==2)
	{
		while((I2C2->SR1 & 0x80) == 0){}
			I2C2->DR = data;
		while((I2C2->SR1 & 0x80) == 0){}
	}
    return 0;
}


// Stop step
void i2c_stop(char i2c)
{
    volatile int tmp;
    if(i2c == 1)
    {
        tmp = I2C1->SR1;
        tmp = I2C1->SR2;
        (void)tmp; // Явно указываем, что переменная не используется
        I2C1->CR1 |= 0x200;
    }
    else if(i2c == 2)
    {
        tmp = I2C2->SR1;
        tmp = I2C2->SR2;
        (void)tmp; // Явно указываем, что переменная не используется
        I2C2->CR1 |= 0x200;
    }
}

// i2c_write()
void i2c_write(char i2c, char address, char data[])
{
    int i = 0;
    
    i2c_start(i2c);
    
    i2c_add(i2c, address, 0);
    
    while(data[i] != '\0')
    {
        i2c_data(i2c, data[i]);
        i++;
    }
    i2c_stop(i2c);
}

// Функция записи в EEPROM с указанием адреса памяти
int i2c_write_mem(char i2c, char dev_addr, uint16_t mem_addr, const char *data, int len)
{
    const int page_size = 64; // размер страницы EEPROM
    int offset = 0;

    while (offset < len) {
        // Определяем размер текущего куска (не больше страницы)
        int chunk = len - offset;
        if (chunk > page_size) chunk = page_size;

        // Начинаем транзакцию для этого куска
    if (i2c_start(i2c)) return 1;
    if (i2c_add(i2c, dev_addr, 0)) { i2c_stop(i2c); return 1; } // режим записи

        // Отправляем адрес памяти (с учётом смещения)
        uint16_t current_addr = mem_addr + offset;
    if (i2c_data(i2c, (current_addr >> 8) & 0xFF)) { i2c_stop(i2c); return 1; }
    if (i2c_data(i2c, current_addr & 0xFF)) { i2c_stop(i2c); return 1; }

        // Отправляем данные текущего куска
        for (int i = 0; i < chunk; i++) {
            if (i2c_data(i2c, data[offset + i])) { i2c_stop(i2c); return 1; }
        }

        // Завершаем транзакцию
    i2c_stop(i2c);

        // Пауза для внутреннего программирования EEPROM
    delay_ms(10);

        offset += chunk;
    }
    return 0;
}

// Функция чтения из EEPROM
int i2c_read(char i2c, char address, uint16_t mem_addr, char *buffer, int len)
{
    // 1) Записываем в устройство адрес памяти (адресирование для последующего чтения)
    if (i2c_start(i2c)) return 1;
    if (i2c_add(i2c, address, 0)) { i2c_stop(i2c); return 1; }  // режим записи
    if (i2c_data(i2c, (mem_addr >> 8) & 0xFF)) { i2c_stop(i2c); return 1; } // Старший байт адреса
    if (i2c_data(i2c, mem_addr & 0xFF)) { i2c_stop(i2c); return 1; }        // Младший байт адреса

    // 2) Повторный START для чтения
    if (i2c_start(i2c)) return 1;
    if (i2c_add(i2c, address, 1)) { i2c_stop(i2c); return 1; }  // Режим чтения
    // Убедимся, что ACK включён перед началом многобайтового приёма
    if(i2c == 1)
        I2C1->CR1 |= 0x0400;  // ACK
    else if(i2c == 2)
        I2C2->CR1 |= 0x0400;  // ACK
    
    // Чтение данных
    for(int i = 0; i < len; i++)
    {
        // Для последнего байта отправляем NACK
        if(i == len - 1)
        {
            if(i2c == 1)
                I2C1->CR1 &= ~0x0400;  // NACK
            else if(i2c == 2)
                I2C2->CR1 &= ~0x0400;  // NACK
        }
        
        // Ожидание данных
        if(i2c == 1)
        {
            while((I2C1->SR1 & 0x40) == 0){}  // Ждем RxNE
            buffer[i] = I2C1->DR;
        }
        else if(i2c == 2)
        {
            while((I2C2->SR1 & 0x40) == 0){}
            buffer[i] = I2C2->DR;
        }
    }
    
    i2c_stop(i2c);
    // Восстановим ACK в привычное состояние (включён)
    if(i2c == 1)
        I2C1->CR1 |= 0x0400;
    else if(i2c == 2)
        I2C2->CR1 |= 0x0400;
    return 0;
}


void i2c_dma_init(char i2c)
{
    // Включение тактирования DMA
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;
    
    if(i2c == 1)
    {
        // Канал 6 для I2C1_TX
        DMA1_Channel6->CPAR = (uint32_t)&(I2C1->DR);
        DMA1_Channel6->CMAR = 0;  // Установится позже при передаче
        DMA1_Channel6->CNDTR = 0;
        
        // Настройка канала 6 (TX)
        DMA1_Channel6->CCR =
            DMA_CCR_MINC |       // Increment memory
            DMA_CCR_DIR |        // Memory to peripheral
            DMA_CCR_TCIE |       // Transfer complete interrupt
            DMA_CCR_PL_1;        // High priority
            
        // Канал 7 для I2C1_RX
        DMA1_Channel7->CPAR = (uint32_t)&(I2C1->DR);
        DMA1_Channel7->CMAR = 0;  // Установится позже при приеме
        DMA1_Channel7->CNDTR = 0;
        
        // Настройка канала 7 (RX)
        DMA1_Channel7->CCR =
            DMA_CCR_MINC |       // Increment memory
            DMA_CCR_TCIE;        // Transfer complete interrupt
    }
}

// 
int i2c_read_eeprom(char i2c, char address, uint16_t mem_addr, char *buffer, int len)
{
    // 1. Записываем адрес памяти в EEPROM
    if (i2c_start(i2c)) return 1;
    if (i2c_add(i2c, address, 0)) { i2c_stop(i2c); return 1; }  // Режим записи
    if (i2c_data(i2c, (mem_addr >> 8) & 0xFF)) { i2c_stop(i2c); return 1; } // Старший байт адреса
    if (i2c_data(i2c, mem_addr & 0xFF)) { i2c_stop(i2c); return 1; }        // Младший байт адреса
    i2c_stop(i2c);  // ОБЯЗАТЕЛЬНО! EEPROM ждет STOP перед чтением
    
    // 2. Небольшая задержка для EEPROM
    delay_ms(5);
    
    // 3. Теперь читаем данные
    if (i2c_start(i2c)) return 1;
    if (i2c_add(i2c, address, 1)) { i2c_stop(i2c); return 1; }  // Режим чтения
    // Убедимся, что ACK включён перед приёмом
    if(i2c == 1)
        I2C1->CR1 |= 0x0400;  // ACK
    else if(i2c == 2)
        I2C2->CR1 |= 0x0400;  // ACK
    
    // Чтение данных
    for(int i = 0; i < len; i++)
    {
        // Для последнего байта отправляем NACK
        if(i == len - 1)
        {
            if(i2c == 1)
                I2C1->CR1 &= ~0x0400;  // NACK
            else if(i2c == 2)
                I2C2->CR1 &= ~0x0400;  // NACK
        }
        
        // Ожидание данных
    uint32_t timeout = 100000;  // Таймаут
        if(i2c == 1)
        {
            while((I2C1->SR1 & 0x40) == 0)
            {
                if(--timeout == 0)
                {
                    i2c_stop(i2c);
                    return 1;  // Выход по таймауту
                }
            }
            buffer[i] = I2C1->DR;
        }
        else if(i2c == 2)
        {
            while((I2C2->SR1 & 0x40) == 0)
            {
                if(--timeout == 0)
                {
                    i2c_stop(i2c);
                    return 1;
                }
            }
            buffer[i] = I2C2->DR;
        }
    }
    
    i2c_stop(i2c);
    // Восстановим ACK (на всякий случай)
    if(i2c == 1)
        I2C1->CR1 |= 0x0400;
    else if(i2c == 2)
        I2C2->CR1 |= 0x0400;
    return 0;
}

// Read a single byte from EEPROM memory address into *out. Returns 0 on success, 1 on failure.
int i2c_read_byte(char i2c, char address, uint16_t mem_addr, uint8_t *out)
{
    // Write memory address then STOP
    i2c_start(i2c);
    if (i2c_add(i2c, address, 0)) { i2c_stop(i2c); return 1; }
    i2c_data(i2c, (mem_addr >> 8) & 0xFF);
    i2c_data(i2c, mem_addr & 0xFF);
    i2c_stop(i2c);

    // Small delay for EEPROM internal operations
    delay_ms(2);

    // Read single byte
    i2c_start(i2c);
    if (i2c_add(i2c, address, 1)) { i2c_stop(i2c); return 1; }

    // For single byte read, send NACK after reception
    if(i2c == 1)
        I2C1->CR1 &= ~0x0400; // NACK
    else
        I2C2->CR1 &= ~0x0400;

    // Wait for data
    uint32_t timeout = 100000;
    if(i2c == 1)
    {
        while((I2C1->SR1 & 0x40) == 0) { if(--timeout == 0) { i2c_stop(i2c); return 1; } }
        *out = I2C1->DR;
    }
    else
    {
        while((I2C2->SR1 & 0x40) == 0) { if(--timeout == 0) { i2c_stop(i2c); return 1; } }
        *out = I2C2->DR;
    }

    i2c_stop(i2c);
    // Восстановим ACK (обычно включён)
    if(i2c == 1)
        I2C1->CR1 |= 0x0400;
    else
        I2C2->CR1 |= 0x0400;

    return 0;
}