#include "pid.h"

void PID_Reset(pid_t *pid)
{
    pid->ref      = 0.0f;
    pid->fdb      = 0.0f;
    pid->err      = 0.0f;
    pid->last_err = 0.0f;
    pid->p_out    = 0.0f;
    pid->i_out    = 0.0f;
    pid->d_out    = 0.0f;
    pid->output   = 0.0f;
}

void PID_Init(pid_t *pid, float kp, float ki, float kd, float i_max, float out_max)
{
    pid->kp      = kp;
    pid->ki      = ki;
    pid->kd      = kd;
    pid->i_max   = i_max;
    pid->out_max = out_max;
    PID_Reset(pid);
}

float PID_Calc(pid_t *pid, float ref, float fdb)
{
    pid->ref = ref;
    pid->fdb = fdb;
    pid->err = ref - fdb;

    pid->p_out = pid->kp * pid->err;

    pid->i_out += pid->ki * pid->err;
    if (pid->i_out >  pid->i_max) pid->i_out =  pid->i_max;
    if (pid->i_out < -pid->i_max) pid->i_out = -pid->i_max;

    pid->d_out    = pid->kd * (pid->err - pid->last_err);
    pid->last_err = pid->err;

    pid->output = pid->p_out + pid->i_out + pid->d_out;
    if (pid->output >  pid->out_max) pid->output =  pid->out_max;
    if (pid->output < -pid->out_max) pid->output = -pid->out_max;

    return pid->output;
}
