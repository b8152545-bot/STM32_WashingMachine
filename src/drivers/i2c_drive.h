#include "stm32f1xx.h"
#include "gp_drive.h"


#define i2c_FM 0x2d // 400 кГц: 36MHz/(2*400kHz) = 45 + бит F/S
#define i2c_SM 0xB4 // 100 кГц: 36MHz/(2*100kHz) = 180


/*
*I2C1
*PB6 -> SCL
*PB7 -> SDA
*/
void i2c_init(char i2c, unsigned short speed_mode);
int i2c_start(char i2c);
int i2c_add(char i2c, char address, char RW);
int i2c_data(char i2c, char data);
void i2c_stop(char i2c);
void i2c_write(char i2c, char address, char data[]);
int i2c_write_mem(char i2c, char dev_addr, uint16_t mem_addr, const char *data, int len);
int i2c_read(char i2c, char address, uint16_t mem_addr, char *buffer, int len);
int i2c_read_eeprom(char i2c, char address, uint16_t mem_addr, char *buffer, int len);
// Read single byte from EEPROM memory address (returns 0 on success)
int i2c_read_byte(char i2c, char address, uint16_t mem_addr, uint8_t *out);