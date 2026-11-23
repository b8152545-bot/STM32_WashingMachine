#ifndef EEPROM_IMAGES_H
#define EEPROM_IMAGES_H

#include <stdint.h>
#include "i2c_drive.h"
#include "ILI9341.h"
#include "stm32f103xx_CMSIS.h"
#include <string.h>

#define PROG_COUNT 8

#define EEPROM_I2C_NUM        1
#define EEPROM_I2C_ADDR       (0x50 << 1)
#define EEPROM_PAGE_SIZE      64

// Адреса изображений (оставьте как есть)
#define ELECTROLUX_LOGO     0x0000
#define IMAGE_C             0x0A00
#define IMAGE_COTON         0x0AC0
#define IMAGE_DELICATE      0x0D80
#define IMAGE_FAST_15       0x1180
#define IMAGE_NOO           0x1580
#define IMAGE_OBMIN         0x1700
#define IMAGE_PAUSE         0x1900
#define IMAGE_RINSING       0x1B00
#define IMAGE_SETUP         0x1F40
#define IMAGE_SPIN          0x2300
#define IMAGE_START         0x2580
#define IMAGE_STOP          0x2780
#define IMAGE_SYNTETICS     0x2940
#define IMAGE_TEMPERATURE   0x2D00
#define IMAGE_WATER         0x2D40
#define IMAGE_WOOL          0x2F40
#define IMAGE_YAS           0x31C0
// пиктограммы
#define IMAGE_ENC           0x3340
#define IMAGE_TEMP          0x3480
#define IMAGE_OBR           0x35C0
#define IMAGE_TIM           0x6C80  // следующий свободный адрес 0х6Е80

#define PARAMS_EEPROM_ADDR  0x6E80  // страница под хранения параметров
#define STATE_EEPROM_ADDR   0x6EC0   // страница под лог процесса стирки


// Структуры
typedef struct {
    uint8_t temperature;
    uint16_t spin_speed;
} ProgramParam;

typedef struct {
    uint16_t addr;
    uint16_t width;
} ProgImage;

// Параметры программ + PID коэффициенты
typedef struct {
    uint8_t magic;                      // маркер валидности
    ProgramParam programs[PROG_COUNT];
    int32_t pid_kp;                     // масштабированные коэффициенты (*1000)
    int32_t pid_ki;
    int32_t pid_kd;
    uint16_t crc;
} EepromParams;

extern EepromParams eeprom_params;

// Структура для логирования стирки
typedef struct {
    uint32_t magic;
    uint16_t program;
    uint16_t stage;
    uint16_t stage_time;
    uint16_t total_time;
    uint16_t crc;
} WashState;

extern WashState wash_state;

// Прототипы функций
uint8_t eeprom_test_connection(void);
void DrawImage_FromEEPROM(uint16_t x, uint16_t y, uint16_t image_addr,
                          uint16_t width, uint16_t height);
void EEPROM_WriteImage(uint16_t start_addr, const uint8_t *image_data, uint16_t image_size);
void SaveProgramParams(void);
void LoadProgramParams(void);
void SaveWashState(void);
void LoadWashState(void);

// Массив изображений программ
extern const ProgImage program_images[];

#endif