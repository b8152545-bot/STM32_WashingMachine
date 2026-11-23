#ifndef PORT_MAPPING_H
#define PORT_MAPPING_H

// СОПОСТАВЛЕНИЕ ПОРТОВ

// Назначение выводов для исполнительных устройств
#define DOOR_PORT       GPIOA
#define DOOR_PIN        3

#define VALVE_1_PORT    GPIOA
#define VALVE_1_PIN     11

#define VALVE_2_PORT    GPIOA
#define VALVE_2_PIN     12

#define PUMP_CIRC_PORT  GPIOC
#define PUMP_CIRC_PIN   3

#define PUMP_DRAIN_PORT GPIOB
#define PUMP_DRAIN_PIN  5

#define TEN_PORT        GPIOA
#define TEN_PIN         5

#define DIR_FWD_PORT    GPIOA
#define DIR_FWD_PIN     15

#define DIR_REV_PORT    GPIOB
#define DIR_REV_PIN     4

#define RELAY_3_PORT    GPIOC
#define RELAY_3_PIN     4

#define POWER_MANAGEMENT_PORT   GPIOB
#define POWER_MANAGEMENT_PIN    3   

#define ZUMMER_PORT     GPIOB
#define ZUMMER_PIN      8

// Макросы для атомарной установки и сброса пинов через регистры BSRR/BRR
#define GPIO_SET_PIN(port, pin)   ((port)->BSRR = (uint32_t)(1 << (pin)))
#define GPIO_RESET_PIN(port, pin) ((port)->BRR  = (uint32_t)(1 << (pin)))

#define TRIAC_PIN_MODE_OUTPUT() do { GPIOA->CRH = (GPIOA->CRH & ~(0xF << 0)) | (0x1 << 0); } while(0)
#define TRIAC_PIN_MODE_AF()      do { GPIOA->CRH = (GPIOA->CRH & ~(0xF << 0)) | (0xA << 0); } while(0)


#endif