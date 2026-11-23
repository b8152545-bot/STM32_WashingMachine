#include "main.h"
#include "stm32f103xx_CMSIS.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "stm32f1_rtc.h"
#include "selection_menu.h"


RTC_UNIT datetime;

/*
#define EEPROM_WRITE_DELAY_MS 5
#define FONT_34X37_START_ADDR  0x0000   // начальный адрес в EEPROM
#define FONT_32X36_CHAR_COUNT  13       // количество символов (0-9, :, -, .)
*/
/*
 * Запись шрифта Font_34x37 в EEPROM
 * Вызывать ОДИН раз после инициализации I2C
 *
void EEPROM_WriteFont_34x37(uint16_t start_addr)
{
    const uint8_t* symbols[] = {
        image_data_Font_0x30, // '0'
        image_data_Font_0x31, // '1'
        image_data_Font_0x32, // '2'
        image_data_Font_0x33, // '3'
        image_data_Font_0x34, // '4'
        image_data_Font_0x35, // '5'
        image_data_Font_0x36, // '6'
        image_data_Font_0x37, // '7'
        image_data_Font_0x38, // '8'
        image_data_Font_0x39, // '9'
        image_data_Font_0x3a, // ':'
        image_data_Font_0x2d, // '-'
        image_data_Font_0x2e  // '.'
    };
    const uint16_t char_size = 185;  
    uint16_t addr = start_addr;
    uint16_t addr_step_bytes = 0; // видна после цикла

    ILI9341_FillScreen_DMA(COLOR_BLACK);

    for (int i = 0; i < 13; i++) {
        // Записываем весь символ (функция разбивает по страницам)
        i2c_write_mem(EEPROM_I2C_NUM, EEPROM_I2C_ADDR, addr,
                      (const char*)symbols[i], char_size);

        uint16_t pages_per_char = (char_size + EEPROM_PAGE_SIZE - 1) / EEPROM_PAGE_SIZE;
        addr_step_bytes = pages_per_char * EEPROM_PAGE_SIZE;
        addr += addr_step_bytes; // сдвиг на выровненное количество байт

        // Прогресс
        uint8_t percent = (i * 100) / 13;
        DrawNumber_NoFlicker(10, 50, percent, IS_FLOAT_NO,
                             MAX_ZNAK_03, ALIGN_LEFT, MAX_001,
                             image_data_Font_23x28, 23, 28,
                             COLOR_YELLOW, COLOR_BLACK);
    }

    ILI9341_FillScreen_DMA(COLOR_GREEN);
    // Выводим адрес следующей страницы (для проверки правильности расчёта)
    DrawNumber_NoFlicker(26, 104, 
               addr_step_bytes, IS_FLOAT_NO, 
                MAX_ZNAK_01, ALIGN_LEFT, MAX_001, 
                image_data_Font_23x28, 23, 28, 
                COLOR_WHITE, COLOR_GREEN);
    delay_ms(5000);
}

void DrawChar_Font34x37_EEPROM(uint16_t x, uint16_t y, char c,
                         uint16_t color, uint16_t bg) {
    uint8_t index;
    if (c >= '0' && c <= '9') index = c - '0';
    else if (c == ':') index = 10;
    else if (c == '-') index = 11;
    else if (c == '.') index = 12;
    else return;

    uint16_t addr = index * 192;    // каждый символ занимает 144 байта
                                    // но так как массив симвала занивает  2,25 страницы памяти
                                    // напамним, что одна страница 64. То отдаём под массив 3 стрпаницы
                                    // что равно: 64 х 3 = 192
                                    // соответственно сдвиг адреса на 192
    uint8_t bitmap[185];
    // Read byte-by-byte using i2c_read_byte to avoid possible multi-byte read issues
    for (int i = 0; i < 185; ++i) {
        uint8_t b = 0xFF;
        if (i2c_read_byte(EEPROM_I2C_NUM, EEPROM_I2C_ADDR, addr + i, &b) != 0) {
            // read failed, leave 0xFF
            b = 0xFF;
        }
        bitmap[i] = b;
    }

    ILI9341_DrawChar_Bitmap(x, y, bitmap, 34, 37, color, bg);
}

void TestDrawString_34x37(void)
{
    ILI9341_FillScreen_DMA(COLOR_BLACK);

    char *text = "12:45";
    uint16_t x = 50;
    uint16_t y = 100;

    while (*text) {
        DrawChar_Font34x37_EEPROM(x, y, *text, COLOR_WHITE, COLOR_BLACK);
        x += 32;  // моноширинный шрифт, ширина 32 пикселя
        text++;
    }
}*/



int main(void) 
{
	CMSIS_Debug_init();
	CMSIS_RCC_SystemClock_72MHz();
	CMSIS_SysTick_Timer_init();
    port_init();
	rtc_Init();
    Encoder_Init();
    USART_Init();        // настраивает пины PA9(TX), PA10(RX), скорость 115200
    SPI2_Init();
    DMA_SPI2_Init();
	ILI9341_Init();
    i2c_init(1, i2c_FM);
    Hardware_Init();
    __enable_irq();


    /*EEPROM_WriteImage(ELECTROLUX_LOGO, image_Electrolux_logo1, ELECTROLUX_LOGO_SIZE);
    EEPROM_WriteImage(IMAGE_C, image_C, C_SIZE);
    EEPROM_WriteImage(IMAGE_COTON, image_Coton, COTON_SIZE);
    EEPROM_WriteImage(IMAGE_DELICATE, image_Delicate, DELICATE_SIZE);
    EEPROM_WriteImage(IMAGE_FAST_15, image_Fast_15, FAST_15_SIZE);
    EEPROM_WriteImage(IMAGE_NOO, image_Noo, NOO_SIZE);
    EEPROM_WriteImage(IMAGE_OBMIN, image_Ob_min, OBMIN_SIZE);
    EEPROM_WriteImage(IMAGE_PAUSE, image_Psuse, PAUSE_SIZE);
    EEPROM_WriteImage(IMAGE_RINSING, image_Rinsing, RINSING_SIZE);
    EEPROM_WriteImage(IMAGE_SETUP, image_Setup, SETUP_SIZE);
    EEPROM_WriteImage(IMAGE_SPIN, image_Spin, SPIN_SIZE);
    EEPROM_WriteImage(IMAGE_START, image_Start, START_SIZE);
    EEPROM_WriteImage(IMAGE_STOP, image_Stop, STOP_SIZE);
    EEPROM_WriteImage(IMAGE_SYNTETICS, image_Syntetics, SYNTETICS_SIZE);
    EEPROM_WriteImage(IMAGE_TEMPERATURE, image_Temperature, TEMPERATURE_SIZE);
    EEPROM_WriteImage(IMAGE_WATER, image_Water, WATER_SIZE);
    EEPROM_WriteImage(IMAGE_WOOL, image_Wool, WOOL_SIZE);
    EEPROM_WriteImage(IMAGE_YAS, image_Yas, YAS_SIZE);
    EEPROM_WriteImage(IMAGE_ENC, image_enc, ENC_SIZE);
    EEPROM_WriteImage(IMAGE_TEMP, image_temp, TEMP_SIZE);
    EEPROM_WriteImage(IMAGE_OBR, image_obr, OBR_SIZE);
    EEPROM_WriteImage(IMAGE_TIM, image_tim, TIM_SIZE);*/


 
    ILI9341_FillScreen_DMA(COLOR_BLACK);
    ILI9341_DrawRect_DMA(0, 0, 320, 240, 1, COLOR_YELLOW);
  
    /*__enable_irq();

    // --- ТЕСТ UART ---
    // Отправляем приветствие блокирующим методом (без DMA и прерываний)
    const char *boot_msg = "\r\n[BOOT] UART1 OK! Hello from STM32!\r\n";
    CMSIS_USART_Transmit(USART1, (uint8_t*)boot_msg, strlen(boot_msg));
    delay_ms(50); // Пауза, чтобы сообщение точно ушло до следующего кода
    // -----------------*/

    if(!eeprom_test_connection()) { 
        ILI9341_FillScreen_DMA(COLOR_RED);
    } else {
        delay_ms(1);
        LoadWashState();  // загрузить состояние из EEPROM
        if (wash_state.magic == 0xA5A5A5A5 && wash_state.stage != 0) {
            // Незавершённая стирка найдена – переходим в режим предложения продолжить
            system_state = STATE_RESUME_PROMPT;
            // Сохраняем параметры прерванной стирки в глобальные переменные (они уже есть: current_program, power_on_timer)
            current_program = wash_state.program;
            power_on_timer = wash_state.stage_time;
            // Можно сразу очистить экран (или это сделаем в самом состоянии)
            ILI9341_FillScreen_DMA(COLOR_BLACK);
        } else {
            DrawImage_FromEEPROM(5, 80, ELECTROLUX_LOGO, 310, 65);
        }
        LoadProgramParams(); // прочли параметры из EEPROM
            // --- ТЕСТ UART + ВЫВОД PID ---
        char boot_msg[128];
        // eeprom_params уже прочитана в Hardware_Init()
        snprintf(boot_msg, sizeof(boot_msg),
                "\r\n[BOOT] UART1 OK! Hello from STM32!\r\n"
                "[PID] Kp=%ld.%03ld Ki=%ld.%03ld Kd=%ld.%03ld\r\n",
                (long)(eeprom_params.pid_kp / 1000), (long)(eeprom_params.pid_kp % 1000),
                (long)(eeprom_params.pid_ki / 1000), (long)(eeprom_params.pid_ki % 1000),
                (long)(eeprom_params.pid_kd / 1000), (long)(eeprom_params.pid_kd % 1000));
                
        CMSIS_USART_Transmit(USART1, (uint8_t*)boot_msg, strlen(boot_msg));
        delay_ms(50);

    }

    while (1) {
        Menu_Update(1499);
    }
}


