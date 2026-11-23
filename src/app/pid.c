#include "pid.h"

void PID_Init(PID_Handle *pid, int32_t Kp, int32_t Ki, int32_t Kd, uint32_t dt_ms)
{
    pid->Kp = Kp;
    pid->Ki = Ki;
    pid->Kd = Kd;
    pid->dt_ms = dt_ms;
    pid->setpoint = 0;
    pid->target_setpoint = 0;
    pid->ramp_rate = 0;
    pid->ramp_enabled = 0;
    pid->prev_rpm = 0;
    pid->integral_scaled = 0;
    pid->output_min = 0;
    pid->output_max = 100;
    pid->integral_min = -500000;
    pid->integral_max =  500000;
}


void PID_SetTarget(PID_Handle *pid, int32_t target, uint32_t ramp_rate)
{
    pid->target_setpoint = target;
    pid->ramp_rate = ramp_rate;
    pid->ramp_enabled = (ramp_rate > 0);
    if (!pid->ramp_enabled) pid->setpoint = target;
}

static void PID_UpdateRamp(PID_Handle *pid)
{
    if (!pid->ramp_enabled) return;
    if (pid->setpoint == pid->target_setpoint) return;
    // Защита от переполнения при больших ramp_rate
    int32_t max_step = (int32_t)(((uint64_t)pid->ramp_rate * pid->dt_ms) / 1000);
    if (max_step < 1) max_step = 1;
    int32_t diff = pid->target_setpoint - pid->setpoint;
    if (diff > 0) {
        if (diff > max_step) pid->setpoint += max_step;
        else pid->setpoint = pid->target_setpoint;
    } else {
        if (-diff > max_step) pid->setpoint -= max_step;
        else pid->setpoint = pid->target_setpoint;
    }
}

int32_t PID_Update(PID_Handle *pid, int32_t current_rpm)
{
    PID_UpdateRamp(pid);

    int32_t error = pid->setpoint - current_rpm;
    
    // Зона нечувствительности (deadband) — не дергаем мотор из-за шума в 5 RPM
    if (error > -5 && error < 5) {
        error = 0;
    }

    // ----- P -----
    int32_t p_part = (pid->Kp * error) / 1000;

    // ----- D -----
    // СТАЛО (защита от переполнения):
    int32_t d_error = (current_rpm - pid->prev_rpm) * 1000 / (int32_t)pid->dt_ms;
    int32_t d_part = (int32_t)(-((int64_t)pid->Kd * d_error) / 1000);
    pid->prev_rpm = current_rpm;

    // ----- I -----
    // ИЗМЕНЕНО: интеграл теперь накапливается с учётом dt_ms.
    // Поведение регулятора больше не зависит от частоты вызова PID_Update().
    //
    // ВНИМАНИЕ (важно при миграции!):
    // по сравнению со старой формулой (без множителя dt_ms) эффективность
    // интегратора выросла примерно в dt_ms раз. При dt_ms = 20 это ~20x.
    // Сохранённый в EEPROM Ki нужно либо уменьшить примерно в dt_ms раз,
    // либо (рекомендуется) перетюнить PI с нуля — коэффициенты всё равно
    // придётся пересчитывать после остальных правок из плана.
    // СТАЛО (защита от переполнения):
    //int64_t temp_int_scaled = (int64_t)pid->integral_scaled 
    //                        + (int64_t)pid->Ki * error * (int32_t)pid->dt_ms;                        
    //if (temp_int_scaled > pid->integral_max) temp_int_scaled = pid->integral_max;
    //if (temp_int_scaled < pid->integral_min) temp_int_scaled = pid->integral_min;
    //int32_t i_part = (int32_t)(temp_int_scaled / 1000);
    //int32_t raw_output = p_part + i_part + d_part;
    
        // ----- I -----
    // ИСПРАВЛЕНО: добавляем деление на 1000, так как dt_ms в миллисекундах, 
    // а Ki хранится с масштабом 1000. Иначе интеграл будет расти в 1000 раз быстрее.
    int64_t temp_int_scaled = (int64_t)pid->integral_scaled 
                            + ((int64_t)pid->Ki * error * (int32_t)pid->dt_ms) / 1000;
                            
    if (temp_int_scaled > pid->integral_max) temp_int_scaled = pid->integral_max;
    if (temp_int_scaled < pid->integral_min) temp_int_scaled = pid->integral_min;

    // ИСПРАВЛЕНО: возвращаем расчет предварительного выхода
    int32_t i_part = (int32_t)(temp_int_scaled / 1000);
    int32_t raw_output = p_part + i_part + d_part;

    // ----- Anti-windup -----
    // Условия исправлены (были перевёрнуты).
    // Интегрирование блокируется, только если выход УЖЕ в насыщении
    // и ошибка толкает его дальше в ту же сторону.
    int can_integrate = 1;
    if (raw_output >= pid->output_max && error > 0) can_integrate = 0;
    if (raw_output <= pid->output_min && error < 0) can_integrate = 0;

    if (can_integrate) {
        pid->integral_scaled = (int32_t)temp_int_scaled;
    }

    // Страховочное ограничение (если интеграл уже был вне диапазона).
    if (pid->integral_scaled > pid->integral_max) pid->integral_scaled = pid->integral_max;
    if (pid->integral_scaled < pid->integral_min) pid->integral_scaled = pid->integral_min;

    int32_t output = p_part + pid->integral_scaled / 1000 + d_part;
    if (output < pid->output_min) output = pid->output_min;
    if (output > pid->output_max) output = pid->output_max;

    return output;
}