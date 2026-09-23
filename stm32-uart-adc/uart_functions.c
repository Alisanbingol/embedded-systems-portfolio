// uart_functions.c

#include "uart_functions.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void UART_SEND_TXT(UART_HandleTypeDef *huart, char buffer[], int m) {
    HAL_UART_Transmit(huart, (uint8_t*) buffer, strlen(buffer), HAL_MAX_DELAY);
    if (m == 1)
        HAL_UART_Transmit(huart, (uint8_t*) "\n\r", 2, HAL_MAX_DELAY);
}

void UART_SEND_CHR(UART_HandleTypeDef *huart, char c, int m) {
    HAL_UART_Transmit(huart, (uint8_t*) &c, 1, HAL_MAX_DELAY);
    if (m == 1)
        HAL_UART_Transmit(huart, (uint8_t*) "\n\r", 2, HAL_MAX_DELAY);
}

void UART_SEND_NL(UART_HandleTypeDef *huart) {
    HAL_UART_Transmit(huart, (uint8_t*) "\n\r", 2, HAL_MAX_DELAY);
}

void UART_SEND_INT(UART_HandleTypeDef *huart, int i, int m) {
    char buffer[10];
    sprintf(buffer, "%d", i);
    HAL_UART_Transmit(huart, (uint8_t*) buffer, strlen(buffer), HAL_MAX_DELAY);
    if (m == 1)
        HAL_UART_Transmit(huart, (uint8_t*) "\n\r", 2, HAL_MAX_DELAY);
}

int ReadInt(UART_HandleTypeDef *huart) {
    int n, N = 0;
    char readBuf[1];
    while (1) {
        HAL_UART_Receive(huart, (uint8_t*) readBuf, 1, HAL_MAX_DELAY);
        UART_SEND_CHR(huart, readBuf[0], 0);
        if (readBuf[0] == '\r')
            break;
        n = atoi(readBuf);
        N = 10 * N + n;
    }
    return N;
}
