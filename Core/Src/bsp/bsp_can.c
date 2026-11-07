/**
 ******************************************************************************
 * @file    bsp_can.c
 * @brief   CAN 总线驱动实现（bsp 层）
 ******************************************************************************
 */
#include "bsp_can.h"
#include "can.h"

/** 发送邮箱满时的返回值 */
#define BSP_CAN_ERR_MAILBOX_FULL    (-1)
/** 发送失败时的返回值 */
#define BSP_CAN_ERR_SEND_FAIL       (-2)
/** 接收调试记录的不同 ID 个数 */
#define BSP_CAN_ID_HISTORY_NUM      4U
/** CAN1 接收中断优先级，需 >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY */
#define BSP_CAN_IRQ_PRIORITY        5U

/** 累计发送帧数，调试用 */
volatile uint32_t g_can_tx_count = 0;
/** 累计接收帧数，调试用 */
volatile uint32_t g_can_rx_count = 0;
/** 最近一次错误码，调试用 */
volatile int8_t   g_can_last_error = 0;
/** 最近出现过的不同接收 ID，调试用 */
volatile uint32_t g_can_rx_id_slot[BSP_CAN_ID_HISTORY_NUM] = {0};
/** 最近一帧实际发出的数据与 ID，调试用 */
volatile uint8_t  g_can_tx_debug[BSP_CAN_DLC] = {0};
volatile uint32_t g_can_tx_debug_id = 0;

static BSP_CAN_Rx_Handler_t s_rx_handler = 0;

/**
 * @brief 配置 CAN1 过滤器为全通，并启动外设与接收中断
 * @note  仅在 BSP_CAN_Init 内部使用
 */
static void BSP_CAN_Filter_Init(void)
{
    CAN_FilterTypeDef filter = {0};

    filter.FilterActivation     = ENABLE;
    filter.FilterMode           = CAN_FILTERMODE_IDMASK;
    filter.FilterScale          = CAN_FILTERSCALE_32BIT;
    filter.FilterIdHigh         = 0x0000;
    filter.FilterIdLow          = 0x0000;
    filter.FilterMaskIdHigh     = 0x0000;
    filter.FilterMaskIdLow      = 0x0000;
    filter.FilterBank           = 0;
    filter.FilterFIFOAssignment = CAN_RX_FIFO0;

    (void)HAL_CAN_ConfigFilter(&hcan1, &filter);
    (void)HAL_CAN_Start(&hcan1);
    (void)HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);
}

void BSP_CAN_Init(BSP_CAN_Rx_Handler_t handler)
{
    s_rx_handler = handler;
    BSP_CAN_Filter_Init();

    HAL_NVIC_SetPriority(CAN1_RX0_IRQn, BSP_CAN_IRQ_PRIORITY, 0);
    HAL_NVIC_EnableIRQ(CAN1_RX0_IRQn);
}

int BSP_CAN_Send(uint32_t std_id, const uint8_t data[BSP_CAN_DLC])
{
    CAN_TxHeaderTypeDef header;
    uint32_t mailbox = 0;
    HAL_StatusTypeDef status;

    header.StdId              = std_id;
    header.ExtId              = 0;
    header.IDE                = CAN_ID_STD;
    header.RTR                = CAN_RTR_DATA;
    header.DLC                = BSP_CAN_DLC;
    header.TransmitGlobalTime = DISABLE;

    if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) == 0U)
    {
        g_can_last_error = BSP_CAN_ERR_MAILBOX_FULL;
        return BSP_CAN_ERR_MAILBOX_FULL;
    }

    for (uint8_t i = 0; i < BSP_CAN_DLC; i++)
    {
        g_can_tx_debug[i] = data[i];
    }
    g_can_tx_debug_id = std_id;

    status = HAL_CAN_AddTxMessage(&hcan1, &header, (uint8_t *)data, &mailbox);
    if (status != HAL_OK)
    {
        g_can_last_error = BSP_CAN_ERR_SEND_FAIL;
        return BSP_CAN_ERR_SEND_FAIL;
    }

    g_can_tx_count++;
    return 0;
}

void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *hcan)
{
    if (hcan->Instance != CAN1)
    {
        return;
    }
    g_can_last_error = (int8_t)hcan->ErrorCode;
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CAN_RxHeaderTypeDef rx;
    uint8_t data[BSP_CAN_DLC];

    if (hcan->Instance != CAN1)
    {
        return;
    }
    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx, data) != HAL_OK)
    {
        return;
    }

    g_can_rx_count++;

    if (g_can_rx_id_slot[0] != rx.StdId)
    {
        for (uint8_t i = BSP_CAN_ID_HISTORY_NUM - 1U; i > 0U; i--)
        {
            g_can_rx_id_slot[i] = g_can_rx_id_slot[i - 1U];
        }
        g_can_rx_id_slot[0] = rx.StdId;
    }

    if (s_rx_handler != 0)
    {
        s_rx_handler(rx.StdId, data, (uint8_t)rx.DLC);
    }
}
