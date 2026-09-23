// uart_functions.h

#ifndef UART_FUNCTIONS_H
#define UART_FUNCTIONS_H

#include "stm32f4xx_hal.h"

void UART_SEND_TXT(UART_HandleTypeDef *huart, char buffer[], int m);
void UART_SEND_CHR(UART_HandleTypeDef *huart, char c, int m);
void UART_SEND_NL(UART_HandleTypeDef *huart);
void UART_SEND_INT(UART_HandleTypeDef *huart, int i, int m);
int ReadInt(UART_HandleTypeDef *huart);

#endif /* UART_FUNCTIONS_H */
