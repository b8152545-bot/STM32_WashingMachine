#include "autotune.h"
#include "hardware.h"   // для SysTimer_ms
#include "UART.h"
#include <stdio.h>      // для snprintf
#include <math.h>
#include <stdlib.h>

#define BUF_SIZE 16
#define PI 3.14159265359f

// Коэффициенты пересчёта (Ziegler-Nichols)
#define TUNE_P_PI   0.4f
#define TUNE_I_PI   0.48f
#define TUNE_P_PID  0.6f
#define TUNE_I_PID  1.2f
#define TUNE_D_PID  0.075f

extern volatile uint32_t SysTimer_ms;

// Состояния автомата
enum {
    STATE_IDLE = 0,
    STATE_STABILIZE,
    STATE_WAIT_PULSE,
    STATE_ANALYSIS
};

static struct {
    uint8_t direction;
    int steady, step;
    uint32_t wait_ms, pulse_ms, period_ms;
    float window;
    float steady_value;
    float this_value, prev_value, real_value, derivative, b;
    uint32_t start_time, timer;
    int output;
    uint8_t state;
    bool trigger, change_dir;
    float max_val, min_val;
    float buf[BUF_SIZE];
    uint32_t osc_time, prev_osc_time;
    float PI_k[2], PID_k[3];
    uint8_t accuracy;
    bool finished;
    float prev_val;
    bool first_analysis;
} tuner;

static uint32_t last_log_time = 0;   // для логирования

static void derivative_calc(void) {
    float sumX=0, sumY=0, sumX2=0, sumXY=0;
    for (int i=0; i<BUF_SIZE; i++) {
        sumX += i;
        sumY += tuner.buf[i];
        sumX2 += i*i;
        sumXY += tuner.buf[i] * i;
    }
    tuner.derivative = (BUF_SIZE*sumXY - sumX*sumY) / (BUF_SIZE*sumX2 - sumX*sumX);
    tuner.b = (sumY - tuner.derivative*sumX) / BUF_SIZE;
}

void AutoTune_Init(uint8_t direction, int steady, int step, uint32_t wait_ms,
                   float window, uint32_t pulse_ms, uint32_t period_ms)
{
    tuner.direction = direction;
    tuner.steady = steady;
    tuner.step = step;  // ИСПРАВЛЕНО: step всегда положителен (амплитуда мощности)
    tuner.wait_ms = wait_ms;
    tuner.window = window;
    tuner.pulse_ms = pulse_ms;
    tuner.period_ms = period_ms;
    tuner.state = STATE_IDLE;
    tuner.finished = false;
    tuner.accuracy = 0;
    for (int i=0; i<BUF_SIZE; i++) tuner.buf[i] = 0;
    tuner.timer = SysTimer_ms - period_ms; // чтобы сразу начать
    last_log_time = 0;
    tuner.prev_val = 0;
    tuner.first_analysis = true;
}

void AutoTune_SetInput(float input) {
    tuner.this_value = input;
}

void AutoTune_Compute(void) {
    uint32_t now = SysTimer_ms;
    if (now - tuner.timer < tuner.period_ms) return;
    tuner.timer = now;

    // Логирование (как у вас)
    #if AUTOTUNE_LOG_ENABLE
    if (now - last_log_time >= AUTOTUNE_LOG_PERIOD) {
        last_log_time = now;
        char buf[120];
        int len = snprintf(buf, sizeof(buf),
                           "AT: st=%d acc=%d out=%d rpm=%.1f steady=%.1f max=%.1f min=%.1f\n",
                           tuner.state, tuner.accuracy, tuner.output,
                           tuner.real_value, tuner.steady_value,
                           tuner.max_val, tuner.min_val);
        if (len > 0 && !UART_IsTxBusy()) {
            UART_Send_DMA((const uint8_t*)buf, (uint16_t)len);
        }
    }
    #endif

    // IDLE -> STABILIZE
    if (tuner.state == STATE_IDLE) {
        tuner.state = STATE_STABILIZE;
        tuner.start_time = now;
        tuner.output = tuner.steady;
        tuner.trigger = true;
        tuner.real_value = tuner.this_value;
        tuner.prev_val = tuner.this_value;
        for (int i=0; i<BUF_SIZE; i++) tuner.buf[i] = tuner.this_value;
        return;
    }

    // Обновление буфера и расчёт сглаженного значения
    for (int i=0; i<BUF_SIZE-1; i++) tuner.buf[i] = tuner.buf[i+1];
    tuner.buf[BUF_SIZE-1] = tuner.this_value;
    derivative_calc();
    tuner.real_value = tuner.derivative * (BUF_SIZE-1) + tuner.b;

    switch (tuner.state) {
        case STATE_STABILIZE: {
            if (now - tuner.start_time >= tuner.wait_ms) {
                tuner.steady_value = tuner.real_value;
                tuner.max_val = tuner.steady_value;
                tuner.min_val = tuner.steady_value;
                tuner.state = STATE_WAIT_PULSE;
                tuner.start_time = now;
                
                // ИСПРАВЛЕНО: для разгона УВЕЛИЧИВАЕМ мощность (проценты)
                tuner.output = tuner.steady + tuner.step;
                tuner.trigger = true;
                tuner.osc_time = 0;
                tuner.prev_osc_time = 0;
            }
            break;
        }

        case STATE_WAIT_PULSE: {
            if (now - tuner.start_time >= tuner.pulse_ms) {
                tuner.state = STATE_ANALYSIS;
                tuner.start_time = now;
                
                // ИСПРАВЛЕНО: для торможения УМЕНЬШАЕМ мощность (проценты)
                tuner.output = tuner.steady - tuner.step;
                tuner.trigger = false;   // ждём падения
                tuner.prev_val = tuner.real_value;
                tuner.first_analysis = true;
            }
            break;
        }

        case STATE_ANALYSIS: {
            float current = tuner.real_value;
            if (tuner.first_analysis) {
                tuner.first_analysis = false;
                tuner.prev_val = current;
                tuner.max_val = current;
                tuner.min_val = current;
                break;
            }

            // Обновляем экстремумы
            if (current > tuner.max_val) tuner.max_val = current;
            if (current < tuner.min_val) tuner.min_val = current;

            // --- Переключение при падении ниже steady (снова разгоняем) ---
            // ИСПРАВЛЕНО: убран diff, так как на инерционных системах он блокирует переключение
            if (!tuner.trigger && (current < (tuner.steady_value - tuner.window))) {
                tuner.trigger = true;
                tuner.output = tuner.steady + tuner.step;  // увеличиваем мощность
            }

            // --- Переключение при подъёме выше steady (начинаем тормозить) ---
            if (tuner.trigger && (current > (tuner.steady_value + tuner.window))) {
                tuner.trigger = false;
                tuner.output = tuner.steady - tuner.step;  // уменьшаем мощность

                // Расчёт периода и коэффициентов
                if (tuner.osc_time > 0) {
                    tuner.prev_osc_time = tuner.osc_time;
                    tuner.osc_time = now - tuner.start_time;
                    if (tuner.prev_osc_time > 0) {
                        tuner.accuracy = (uint8_t)(100.0f * (tuner.osc_time < tuner.prev_osc_time ?
                            (float)tuner.osc_time / tuner.prev_osc_time :
                            (float)tuner.prev_osc_time / tuner.osc_time));
                    }
                    float amplitude_pp = tuner.max_val - tuner.min_val; // Полный размах
                    if (amplitude_pp > 1.0f) {
                        // Формула релейного метода: Ku = 4 * d / (pi * a), 
                        // где a - амплитуда от центра до пика (amplitude_pp / 2).
                        // Итого: Ku = 8 * d / (pi * amplitude_pp)
                        float Ku = fabsf((8.0f * tuner.step) / (amplitude_pp * PI));
                        
                        float Pu = 2.0f * tuner.osc_time / 1000.0f; // ИСПРАВЛЕНО: полный период!
                        tuner.PI_k[0] = TUNE_P_PI * Ku;
                        tuner.PI_k[0] = TUNE_P_PI * Ku;
                        tuner.PI_k[1] = TUNE_I_PI * Ku / Pu;
                        tuner.PID_k[0] = TUNE_P_PID * Ku;
                        tuner.PID_k[1] = TUNE_I_PID * Ku / Pu;
                        tuner.PID_k[2] = TUNE_D_PID * Ku * Pu;
                        if (tuner.accuracy > 95) tuner.finished = true;
                    }
                } else {
                    tuner.osc_time = now - tuner.start_time;
                }
                tuner.start_time = now;
                // Сброс экстремумов для следующего полупериода
                tuner.max_val = current;
                tuner.min_val = current;
            }
            break;
        }
        default: break;
    }
}

int AutoTune_GetOutput(void) {
    return tuner.output;
}

uint8_t AutoTune_GetAccuracy(void) {
    return tuner.accuracy;
}

bool AutoTune_IsFinished(void) {
    return tuner.finished;
}

void AutoTune_GetPI(float *kp, float *ki) {
    *kp = tuner.PI_k[0];
    *ki = tuner.PI_k[1];
}

void AutoTune_GetPID(float *kp, float *ki, float *kd) {
    *kp = tuner.PID_k[0];
    *ki = tuner.PID_k[1];
    *kd = tuner.PID_k[2];
}

void AutoTune_Reset(void) {
    tuner.state = STATE_IDLE;
    tuner.finished = false;
    tuner.accuracy = 0;
}

uint8_t AutoTune_GetState(void) {
    return tuner.state;
}