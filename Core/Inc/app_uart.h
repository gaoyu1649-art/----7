#ifndef __APP_UART_H
#define __APP_UART_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

void AppUart_Init(void);
void AppUart_SendString(const char *str);
void AppUart_RxByte(uint8_t byte);
uint8_t AppUart_GetCommand(char *buf, uint16_t maxlen);

#ifdef __cplusplus
}
#endif

#endif
