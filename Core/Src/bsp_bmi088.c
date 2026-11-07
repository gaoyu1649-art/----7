/**
  ******************************************************************************
  * @file    bsp_bmi088.c
  * @brief   BMI088 六轴 IMU 驱动实现（SPI1 + 软件片选，轮询读取）
  * @note    本文件内的所有 SPI 收发、寄存器读写、片选操作都封在内部，
  *          外部只需调用 bmi088_init / bmi088_read_raw。
  ******************************************************************************
  */
#include "main.h"
#include "bsp_bmi088.h"

extern SPI_HandleTypeDef hspi1;

/* ---------------- 片选（C 型板固定接法） ---------------- */
#define ACC_CS_PORT     GPIOA
#define ACC_CS_PIN      GPIO_PIN_4      /* CS1_Accel */
#define GYRO_CS_PORT    GPIOB
#define GYRO_CS_PIN     GPIO_PIN_0      /* CS1_Gyro  */

#define ACC_CS_LOW()    HAL_GPIO_WritePin(ACC_CS_PORT,  ACC_CS_PIN,  GPIO_PIN_RESET)
#define ACC_CS_HIGH()   HAL_GPIO_WritePin(ACC_CS_PORT,  ACC_CS_PIN,  GPIO_PIN_SET)
#define GYRO_CS_LOW()   HAL_GPIO_WritePin(GYRO_CS_PORT, GYRO_CS_PIN, GPIO_PIN_RESET)
#define GYRO_CS_HIGH()  HAL_GPIO_WritePin(GYRO_CS_PORT, GYRO_CS_PIN, GPIO_PIN_SET)

#define BMI088_SPI_TIMEOUT  10U         /* ms */

/* ---------------- 寄存器地址 ---------------- */
/* 加速度计 */
#define ACC_CHIP_ID      0x00U
#define ACC_DATA_X_LSB   0x12U
#define ACC_CONF         0x40U
#define ACC_RANGE        0x41U
#define ACC_PWR_CONF     0x7CU
#define ACC_PWR_CTRL     0x7DU
#define ACC_SOFTRESET    0x7EU

/* 陀螺仪 */
#define GYRO_CHIP_ID     0x00U
#define GYRO_RATE_X_LSB  0x02U
#define GYRO_RANGE       0x0FU
#define GYRO_BANDWIDTH   0x10U
#define GYRO_LPM1        0x11U
#define GYRO_SOFTRESET   0x14U

#define SOFT_RESET_CMD   0xB6U

/* ---------------- 内部状态 ---------------- */
static float s_acc_lsb_per_g   = 5460.0f;     /* 默认 6g  */
static float s_gyro_lsb_per_dps = 32.768f;    /* 默认 1000dps */

/* ================= 底层 SPI ================= */

/**
  * @brief  读多字节。
  * @note   加速度计读操作必须先发 1 个 dummy 字节，数据从第 3 个字节开始；
  *         陀螺仪不需要 dummy，数据从第 2 个字节开始。这是 BMI088 两个
  *         芯片的固有差异，写错会整体错一个字节。
  */
static int imu_read(GPIO_TypeDef *cs_port, uint16_t cs_pin,
                    uint8_t reg, uint8_t *out, uint16_t len, uint8_t need_dummy)
{
    uint8_t tx[12] = {0};
    uint8_t rx[12] = {0};
    uint16_t n = (uint16_t)(len + (need_dummy ? 2U : 1U));
    HAL_StatusTypeDef st;

    if (len > 10U) return BMI088_ERR_SPI;

    tx[0] = (uint8_t)(reg | 0x80U);            /* 最高位置 1 表示读 */

    HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_RESET);
    st = HAL_SPI_TransmitReceive(&hspi1, tx, rx, n, BMI088_SPI_TIMEOUT);
    HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);

    if (st != HAL_OK) return BMI088_ERR_SPI;

    /* need_dummy=1：rx[0]=dummy，rx[1]=dummy，数据从 rx[2] 起 */
    for (uint16_t i = 0; i < len; i++)
        out[i] = rx[i + (need_dummy ? 2U : 1U)];

    return BMI088_OK;
}

/** @brief 写单字节（最高位清 0 表示写） */
static int imu_write(GPIO_TypeDef *cs_port, uint16_t cs_pin, uint8_t reg, uint8_t val)
{
    uint8_t tx[2];
    HAL_StatusTypeDef st;

    tx[0] = (uint8_t)(reg & 0x7FU);
    tx[1] = val;

    HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_RESET);
    st = HAL_SPI_Transmit(&hspi1, tx, 2U, BMI088_SPI_TIMEOUT);
    HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);

    return (st == HAL_OK) ? BMI088_OK : BMI088_ERR_SPI;
}

/* ================= 对外接口 ================= */

/* 写加速度计寄存器并回读校验；不生效则重试，最多 3 次 */
static void acc_write_verified(uint8_t reg, uint8_t val)
{
    uint8_t rb = 0;
    for (int i = 0; i < 3; i++) {
        imu_write(ACC_CS_PORT, ACC_CS_PIN, reg, val);
        HAL_Delay(2);
        imu_read(ACC_CS_PORT, ACC_CS_PIN, reg, &rb, 1U, 1U);
        if (rb == val) return;
    }
}

int bmi088_read_id(uint8_t *acc_id, uint8_t *gyro_id)
{
    uint8_t v = 0;
    int r;

    r = imu_read(ACC_CS_PORT, ACC_CS_PIN, ACC_CHIP_ID, &v, 1U, 1U);
    if (r != BMI088_OK) return r;
    *acc_id = v;

    r = imu_read(GYRO_CS_PORT, GYRO_CS_PIN, GYRO_CHIP_ID, &v, 1U, 0U);
    if (r != BMI088_OK) return r;
    *gyro_id = v;

    return BMI088_OK;
}

int bmi088_init(uint8_t acc_range, uint8_t gyro_range)
{
    uint8_t acc_id = 0, gyro_id = 0;
    int r;

    /* 片选全部拉高（空闲态），此时两颗芯片都不响应 */
    ACC_CS_HIGH();
    GYRO_CS_HIGH();
    HAL_Delay(50);                      /* 等加速度计上电稳定 */

    /* ---- 1. 先验 ID，确认 SPI 通路通了 ---- */
    r = bmi088_read_id(&acc_id, &gyro_id);
    if (r != BMI088_OK) return r;
    if (acc_id  != BMI088_ACC_CHIP_ID)  return BMI088_ERR_ACC_ID;
    if (gyro_id != BMI088_GYRO_CHIP_ID) return BMI088_ERR_GYRO_ID;

    /* ---- 2. 加速度计 ---- */
    imu_write(ACC_CS_PORT, ACC_CS_PIN, ACC_SOFTRESET, SOFT_RESET_CMD);
    HAL_Delay(50);
    /* 顺序：强制 suspend -> 写参数 -> 使能 -> 切 active。
       每一步都回读校验，写不进去就重试，避免"以为配好了其实没配"。 */
    imu_write(ACC_CS_PORT, ACC_CS_PIN, ACC_PWR_CONF, 0x03U);   /* 强制 suspend，保证配置可写 */
    HAL_Delay(5);
    acc_write_verified(ACC_RANGE, acc_range);
    acc_write_verified(ACC_CONF,  0x28U);                      /* acc_bwp=NORMAL, ODR=100Hz */
    HAL_Delay(2);
    acc_write_verified(ACC_PWR_CTRL, 0x04U);                   /* 使能加速度计 */
    HAL_Delay(5);
    acc_write_verified(ACC_PWR_CONF, 0x00U);                   /* 切 active */
    HAL_Delay(50);

    {   /* 上电后第一组数据可能无效，先空读一次丢弃 */
        uint8_t discard[6];
        imu_read(ACC_CS_PORT, ACC_CS_PIN, ACC_DATA_X_LSB, discard, 6U, 1U);
    }

    switch (acc_range) {
        case BMI088_ACC_3G:  s_acc_lsb_per_g = 10920.0f; break;
        case BMI088_ACC_6G:  s_acc_lsb_per_g = 5460.0f;  break;
        case BMI088_ACC_12G: s_acc_lsb_per_g = 2730.0f;  break;
        case BMI088_ACC_24G: s_acc_lsb_per_g = 1365.0f;  break;
        default:             s_acc_lsb_per_g = 5460.0f;  break;
    }

    /* ---- 3. 陀螺仪 ---- */
    imu_write(GYRO_CS_PORT, GYRO_CS_PIN, GYRO_SOFTRESET, SOFT_RESET_CMD);
    HAL_Delay(30);
    imu_write(GYRO_CS_PORT, GYRO_CS_PIN, GYRO_RANGE,     gyro_range);
    imu_write(GYRO_CS_PORT, GYRO_CS_PIN, GYRO_BANDWIDTH, 0x02U);
    imu_write(GYRO_CS_PORT, GYRO_CS_PIN, GYRO_LPM1,      0x00U); /* 正常模式 */

    switch (gyro_range) {
        case BMI088_GYRO_2000DPS: s_gyro_lsb_per_dps = 16.384f;  break;
        case BMI088_GYRO_1000DPS: s_gyro_lsb_per_dps = 32.768f;  break;
        case BMI088_GYRO_500DPS:  s_gyro_lsb_per_dps = 65.536f;  break;
        case BMI088_GYRO_250DPS:  s_gyro_lsb_per_dps = 131.072f; break;
        case BMI088_GYRO_125DPS:  s_gyro_lsb_per_dps = 262.144f; break;
        default:                  s_gyro_lsb_per_dps = 32.768f;  break;
    }

    return BMI088_OK;
}

int bmi088_read_raw(int16_t acc[3], int16_t gyro[3])
{
    uint8_t b[6];
    int r;

    /* 加速度计：起始地址 0x12，连续 6 字节 */
    r = imu_read(ACC_CS_PORT, ACC_CS_PIN, ACC_DATA_X_LSB, b, 6U, 1U);
    if (r != BMI088_OK) return r;
    acc[0] = (int16_t)(((uint16_t)b[1] << 8) | b[0]);
    acc[1] = (int16_t)(((uint16_t)b[3] << 8) | b[2]);
    acc[2] = (int16_t)(((uint16_t)b[5] << 8) | b[4]);

    /* 陀螺仪：起始地址 0x02，连续 6 字节 */
    r = imu_read(GYRO_CS_PORT, GYRO_CS_PIN, GYRO_RATE_X_LSB, b, 6U, 0U);
    if (r != BMI088_OK) return r;
    gyro[0] = (int16_t)(((uint16_t)b[1] << 8) | b[0]);
    gyro[1] = (int16_t)(((uint16_t)b[3] << 8) | b[2]);
    gyro[2] = (int16_t)(((uint16_t)b[5] << 8) | b[4]);

    return BMI088_OK;
}

float bmi088_acc_lsb_per_g(void)    { return s_acc_lsb_per_g; }
float bmi088_gyro_lsb_per_dps(void) { return s_gyro_lsb_per_dps; }
