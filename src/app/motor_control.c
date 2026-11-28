#include "motor_control.h"
#include "pid.h"
#include "motor.h"
#include "hardware.h"
#include "UART.h"
#include <stdio.h>
#include <inttypes.h>
#include <math.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

#define TRIAC_DELAY_MIN  500
#define TRIAC_DELAY_MAX  9500

#define MAX_POWER_INC   5
#define MAX_POWER_DEC   5
#define MAX_RPM_JUMP    2000

extern volatile uint32_t SysTimer_ms;
extern volatile uint16_t triac_delay;

static uint16_t power_to_delay_table[101];
static uint16_t startup_power_delay = TRIAC_DELAY_MAX;

typedef struct { uint16_t delay; uint16_t rpm; } Point;
static const Point points[] = {
    { 8200, 0 }, { 8180, 210 }, { 8160, 240 }, { 8140, 270 },
    { 8120, 300 }, { 8100, 330 }, { 8080, 470 }, { 8060, 480 },
    { 8040, 530 }, { 8020, 630 }, { 8000, 700 }, { 7980, 760 },
    { 7960, 850 }, { 7940, 930 }, { 7920, 990 }, { 7900, 1100 },
    { 7880, 1150 }, { 7840, 1350 }, { 7800, 1670 }, { 7780, 1750 },
    { 7740, 1990 }, { 7700, 2300 }, { 7680, 2320 }, { 7640, 2700 },
    { 7600, 2750 }, { 7580, 2790 }, { 7540, 2990 }, { 7500, 3100 },
    { 7480, 3110 }, { 7440, 3300 }, { 7400, 3500 }, { 7380, 3700 },
    { 7340, 3790 }, { 7300, 3980 }, { 7280, 4020 }, { 7240, 4200 },
    { 7200, 4320 }, { 7180, 4452 }, { 7140, 4650 }, { 7100, 4870 },
    { 7080, 4900 }, { 7040, 5070 }, { 7000, 5250 }, { 6980, 5230 },
    { 6940, 5310 }, { 6900, 5390 }, { 6880, 5490 }, { 6840, 5750 },
    { 6800, 5800 }, { 6680, 5930 }, { 6640, 6190 }, { 6600, 6350 },
    { 6580, 6480 }, { 6480, 6980 }, { 6380, 7351 }, { 6280, 7624 },
    { 6180, 7905 }, { 6080, 8233 }, { 5980, 8508 }, { 5880, 8767 },
    { 5780, 9033 }, { 5680, 9167 }, { 5580, 9404 }, { 5480, 9732 },
    { 5380, 10167 }, { 5180, 10458 }, { 5080, 10748 }, { 4980, 11038 },
    { 4880, 11267 }, { 4380, 11569 }, { 4280, 11769 }, { 4180, 11992 },
    { 4080, 12294 }, { 3980, 12519 }, { 3880, 12692 }, { 3780, 12966 },
    { 3680, 13222 }, { 3580, 13349 }, { 3480, 13521 }, { 3380, 13738 },
    { 3280, 13887 }, { 3180, 14078 }, { 3080, 14222 }, { 2980, 14356 },
    { 2880, 14425 }, { 2780, 14542 }, { 2680, 14685 }, { 2480, 14789 },
    { 2280, 15000 },
};
const int num_points = sizeof(points) / sizeof(points[0]);

static int32_t GetFFPercent(int32_t target_rpm) {
    if (target_rpm <= 0) return 0;
    for (int i = 0; i < num_points - 1; i++) {
        if (points[i].rpm <= target_rpm && target_rpm <= points[i+1].rpm) {
            int32_t delay = points[i].delay + (points[i+1].delay - points[i].delay) * (target_rpm - points[i].rpm) / (points[i+1].rpm - points[i].rpm);
            for (int p = 0; p <= 100; p++) {
                if (power_to_delay_table[p] <= delay) {
                    return p;
                }
            }
        }
    }
    return 100;
}

static void Linearization_Init(void)
{
    const uint16_t RPM_MAX = 15000;
    const uint16_t DELAY_MAX_POWER = 1000;

    for (int p = 0; p <= 100; p++) {
        uint16_t target_rpm = (uint16_t)((uint32_t)p * RPM_MAX / 100);
        if (target_rpm == 0) { power_to_delay_table[p] = TRIAC_DELAY_MAX; continue; }
        if (target_rpm >= RPM_MAX) { power_to_delay_table[p] = DELAY_MAX_POWER; continue; }
        uint16_t delay_low = 0, delay_high = 0, rpm_low = 0, rpm_high = 0;
        int found = 0;
        for (int i = 0; i < num_points - 1; i++) {
            if (points[i].rpm <= target_rpm && target_rpm <= points[i+1].rpm) {
                delay_low = points[i].delay;
                delay_high = points[i+1].delay;
                rpm_low = points[i].rpm;
                rpm_high = points[i+1].rpm;
                found = 1;
                break;
            }
        }
        if (!found) { power_to_delay_table[p] = TRIAC_DELAY_MAX; continue; }
        if (rpm_high == rpm_low) { power_to_delay_table[p] = delay_low; continue; }

        int32_t delta_delay = (int32_t)delay_high - (int32_t)delay_low;
        int32_t delta_rpm   = (int32_t)rpm_high   - (int32_t)rpm_low;
        int32_t target_delta = (int32_t)target_rpm - (int32_t)rpm_low;
        int32_t delay_signed = (int32_t)delay_low + delta_delay * target_delta / delta_rpm;

        if (delay_signed < TRIAC_DELAY_MIN) delay_signed = TRIAC_DELAY_MIN;
        if (delay_signed > TRIAC_DELAY_MAX) delay_signed = TRIAC_DELAY_MAX;
        power_to_delay_table[p] = (uint16_t)delay_signed;
    }

    for (int p = 1; p <= 100; p++) {
        if (power_to_delay_table[p] > power_to_delay_table[p-1]) {
            power_to_delay_table[p] = power_to_delay_table[p-1];
        }
    }
    power_to_delay_table[0] = TRIAC_DELAY_MAX;
    power_to_delay_table[100] = DELAY_MAX_POWER;
    for (int p = 0; p <= 100; p++) {
        if (power_to_delay_table[p] < TRIAC_DELAY_MIN) power_to_delay_table[p] = TRIAC_DELAY_MIN;
        if (power_to_delay_table[p] > TRIAC_DELAY_MAX) power_to_delay_table[p] = TRIAC_DELAY_MAX;
    }

#if ENABLE_UART_LOG
    char dbg[80];
    for (int p = 0; p <= 100; p += 10) {
        int n = snprintf(dbg, sizeof(dbg), "TBL[%d]=%d\n", p, power_to_delay_table[p]);
        if (n > 0 && n < (int)sizeof(dbg) && !UART_IsTxBusy()) {
            (void)UART_Send_DMA((const uint8_t*)dbg, (uint16_t)n);
        }
    }
#endif
}

static uint16_t PowerPercentToDelayLinear(int32_t percent)
{
    if (percent <= 0)   return power_to_delay_table[0];
    if (percent >= 100) return power_to_delay_table[100];
    return power_to_delay_table[percent];
}

PID_Handle motor_pid;
static uint32_t last_call_time = 0;
static uint16_t last_triac_delay = TRIAC_DELAY_MAX;

static uint32_t startup_delay_ms = 0;
static uint32_t startup_start_time = 0;
static uint8_t  startup_active = 0;
static uint8_t  saved_startup_power_percent = 0;

static int32_t last_power_percent = 0;
static uint8_t  first_rpm_call = 1;
static uint16_t prev_rpm = 0;

void MotorControl_Init(void)
{
    int32_t kp = eeprom_params.pid_kp;
    int32_t ki = eeprom_params.pid_ki;
    int32_t kd = eeprom_params.pid_kd;

    if (kp < 0 || kp > 5000) kp = 400;
    if (ki < 0 || ki > 2000) ki = 30;
    if (kd < 0 || kd > 5000) kd = 0;

    PID_Init(&motor_pid, kp, ki, kd, 20);
    motor_pid.output_min = -100;
    motor_pid.output_max = 100;

    PID_SetTarget(&motor_pid, 0, 0);
    Linearization_Init();
    last_triac_delay = TRIAC_DELAY_MAX;
    last_call_time = 0;
    startup_active = 0;
    saved_startup_power_percent = 0;
    last_power_percent = 0;
    first_rpm_call = 1;
    prev_rpm = 0;
}

void MotorControl_StartMotor(int32_t target_rpm, uint32_t ramp_rate,
                             uint8_t startup_power_percent, uint32_t delay_ms)
{
    motor_pid.integral_scaled = 0;
    motor_pid.prev_rpm = HW_GetRPM();
    PID_SetTarget(&motor_pid, target_rpm, ramp_rate);

    saved_startup_power_percent = startup_power_percent;
    startup_power_delay = PowerPercentToDelayLinear(startup_power_percent);

    if (delay_ms > 0) {
        triac_delay = startup_power_delay;
        last_triac_delay = triac_delay;
        startup_delay_ms = delay_ms;
        startup_start_time = SysTimer_ms;
        startup_active = 1;
    } else {
        startup_active = 0;
        motor_pid.integral_scaled = (int32_t)startup_power_percent * 1000;
        last_power_percent = startup_power_percent;
        last_call_time = SysTimer_ms - motor_pid.dt_ms;
    }
}

#if ENABLE_UART_LOG
static uint32_t log_sample = 0;
static uint32_t log_last_time_ms = 0;
static uint32_t log_dropped = 0;
static uint8_t log_has_started = 0;

#ifndef UART_LOG_PERIOD_MS
#define UART_LOG_PERIOD_MS 200U
#endif

static void UART_SendLog(uint16_t rpm, int32_t setpoint, int32_t error,
                         uint16_t delay, int32_t integral_scaled)
{
    const uint32_t now = SysTimer_ms;

    /* Throttle before checking UART or formatting: no snprintf on most PID ticks. */
    if (log_has_started && (uint32_t)(now - log_last_time_ms) < UART_LOG_PERIOD_MS) {
        return;
    }
    log_has_started = 1;
    log_last_time_ms = now;

    /* UART_Send_DMA is non-blocking, but only accepts data while TX is idle. */
    if (UART_IsTxBusy()) {
        log_dropped++;
        return;
    }

    char buf[80];
    const int32_t integral_percent = integral_scaled / 1000;
    const int len = snprintf(buf, sizeof(buf),
                             "%" PRIu32 ",%" PRIu32 ",%" PRIu16 ",%" PRId32 ",%" PRId32 ",%" PRIu16 ",%" PRId32 "\n",
                             log_sample, now, rpm, setpoint, error, delay,
                             integral_percent);

    if (len <= 0 || len >= (int)sizeof(buf)) {
        log_dropped++;
        return;
    }

    if (UART_Send_DMA((const uint8_t*)buf, (uint16_t)len) == 0) {
        log_sample++;
    } else {
        log_dropped++;
    }
}
#endif

void MotorControl_Task(void)
{
    uint32_t now = SysTimer_ms;
    uint16_t rpm = HW_GetRPM();

    if (first_rpm_call) {
        prev_rpm = rpm;
        first_rpm_call = 0;
    }

    static uint8_t reject_cnt = 0;
    if (abs((int32_t)rpm - (int32_t)prev_rpm) > MAX_RPM_JUMP && reject_cnt < 3) {
        rpm = prev_rpm;
        reject_cnt++;
    } else {
        reject_cnt = 0;
        prev_rpm = rpm;
    }

    if (startup_active) {
        if ((uint32_t)(now - startup_start_time) < startup_delay_ms) {
#if ENABLE_UART_LOG
            if (motor_pid.target_setpoint > 0 || rpm > 0) {
                UART_SendLog(rpm, motor_pid.setpoint, motor_pid.setpoint - rpm,
                             triac_delay, motor_pid.integral_scaled);
            }
#endif
            return;
        }

        startup_active = 0;
        motor_pid.setpoint = rpm;
        motor_pid.prev_rpm = rpm;
        last_power_percent = saved_startup_power_percent;
        last_call_time = now - motor_pid.dt_ms;

#if ENABLE_UART_LOG
        char dbg[40];
        const int n = snprintf(dbg, sizeof(dbg), "HANDOVER rpm=%u sp=%ld\n",
                               rpm, (long)motor_pid.setpoint);
        if (n > 0 && n < (int)sizeof(dbg) && !UART_IsTxBusy()) {
            (void)UART_Send_DMA((const uint8_t*)dbg, (uint16_t)n);
        }
#endif
    }

    if ((uint32_t)(now - last_call_time) > (motor_pid.dt_ms * 5)) {
        last_call_time = now - motor_pid.dt_ms;
    }

    if ((uint32_t)(now - last_call_time) >= motor_pid.dt_ms) {
        last_call_time += motor_pid.dt_ms;

        int32_t pid_correction = PID_Update(&motor_pid, rpm);
        int32_t ff_percent = GetFFPercent(motor_pid.setpoint);
        int32_t target_percent = ff_percent + pid_correction;

        if (target_percent < 0) target_percent = 0;
        if (target_percent > 100) target_percent = 100;

        int32_t diff = target_percent - last_power_percent;
        if (diff > MAX_POWER_INC) target_percent = last_power_percent + MAX_POWER_INC;
        else if (diff < -MAX_POWER_DEC) target_percent = last_power_percent - MAX_POWER_DEC;
        last_power_percent = target_percent;

        uint16_t new_delay = PowerPercentToDelayLinear(target_percent);
        triac_delay = new_delay;
        last_triac_delay = new_delay;

#if ENABLE_UART_LOG
        UART_SendLog(rpm, motor_pid.setpoint, motor_pid.setpoint - rpm,
                     triac_delay, motor_pid.integral_scaled);
#endif
    }
}

void MotorControl_ApplyPercent(int32_t percent) {
    if (percent < 0)   percent = 0;
    if (percent > 100) percent = 100;
    __disable_irq();
    triac_delay = PowerPercentToDelayLinear(percent);
    last_triac_delay = triac_delay;
    last_power_percent = percent;
    __enable_irq();
}
