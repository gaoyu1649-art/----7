#ifndef __PID_H
#define __PID_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    float kp;
    float ki;
    float kd;

    float i_max;
    float out_max;

    float ref;
    float fdb;
    float err;
    float last_err;

    float p_out;
    float i_out;
    float d_out;
    float output;
} pid_t;

void  PID_Init(pid_t *pid, float kp, float ki, float kd, float i_max, float out_max);
void  PID_Reset(pid_t *pid);
float PID_Calc(pid_t *pid, float ref, float fdb);

#ifdef __cplusplus
}
#endif

#endif
