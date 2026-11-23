#ifndef PID_H
#define PID_H

#include <stdint.h>

typedef struct {
    int32_t Kp, Ki, Kd;
    int32_t setpoint;
    int32_t target_setpoint;
    uint32_t ramp_rate;
    uint8_t  ramp_enabled;
    int32_t prev_rpm;
    int32_t integral_scaled;   // масштабированный интеграл (Ki * Σerror)
    int32_t output_min;        // 0%
    int32_t output_max;        // 100%
    int32_t integral_min;      // -100000
    int32_t integral_max;      //  100000
    uint32_t dt_ms;
} PID_Handle;

void PID_Init(PID_Handle *pid, int32_t Kp, int32_t Ki, int32_t Kd, uint32_t dt_ms);
void PID_SetTarget(PID_Handle *pid, int32_t target, uint32_t ramp_rate);
int32_t PID_Update(PID_Handle *pid, int32_t current_rpm);

#endif