/**
  ******************************************************************************
  * @file    bsp_bmi088.h
  * @brief   BMI088 六轴 IMU 驱动（SPI1，RoboMaster 开发板 C 型）
  *
  * 硬件连接（见《RoboMaster 开发板 C 型用户手册》附表）：
  *   SPI1_CLK   -> PB3      SPI1_MOSI  -> PA7      SPI1_MISO -> PB4
  *   CS1_Accel  -> PA4      CS1_Gyro   -> PB0
  *   INT1_Accel -> PC4      INT1_Gyro  -> PC5   （本期未使用，轮询采样）
  *
  * 对外只暴露本头文件，主函数中不应出现任何 SPI 调用或寄存器操作。
  ******************************************************************************
  */
#ifndef __BSP_BMI088_H
#define __BSP_BMI088_H

#include <stdint.h>

/* ---------------- 量程选项 ---------------- */
#define BMI088_ACC_3G        0x00U
#define BMI088_ACC_6G        0x01U
#define BMI088_ACC_12G       0x02U
#define BMI088_ACC_24G       0x03U

#define BMI088_GYRO_2000DPS  0x00U
#define BMI088_GYRO_1000DPS  0x01U
#define BMI088_GYRO_500DPS   0x02U
#define BMI088_GYRO_250DPS   0x03U
#define BMI088_GYRO_125DPS   0x04U

/* ---------------- 返回码 ---------------- */
#define BMI088_OK            0
#define BMI088_ERR_ACC_ID   -1      /* 加速度计 WHO_AM_I 不是 0x1E */
#define BMI088_ERR_GYRO_ID  -2      /* 陀螺仪   WHO_AM_I 不是 0x0F */
#define BMI088_ERR_SPI      -3      /* SPI 传输超时/失败 */

/* 芯片固定 ID，用于验证 SPI 通路 */
#define BMI088_ACC_CHIP_ID   0x1EU
#define BMI088_GYRO_CHIP_ID  0x0FU

/**
  * @brief  初始化 BMI088
  * @param  acc_range   BMI088_ACC_3G / 6G / 12G / 24G
  * @param  gyro_range  BMI088_GYRO_2000DPS / 1000 / 500 / 250 / 125
  * @retval BMI088_OK 或负值错误码
  */
int bmi088_init(uint8_t acc_range, uint8_t gyro_range);

/**
  * @brief  读取六轴原始值
  * @param  acc   长度 3 的数组，单位 LSB
  * @param  gyro  长度 3 的数组，单位 LSB
  * @retval BMI088_OK 或负值错误码
  */
int bmi088_read_raw(int16_t acc[3], int16_t gyro[3]);

/**
  * @brief  仅读取两颗芯片的 WHO_AM_I（用于最小化验证 SPI 通路）
  */
int bmi088_read_id(uint8_t *acc_id, uint8_t *gyro_id);

/* 量程换算系数（由 bmi088_init 记录） */
float bmi088_acc_lsb_per_g(void);
float bmi088_gyro_lsb_per_dps(void);

#endif /* __BSP_BMI088_H */
