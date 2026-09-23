#ifndef __UART_ADC_H
#define __UART_ADC_H

#include "main.h"

void ADC_read_send_uart(int pin, ADC_HandleTypeDef *hadc, UART_HandleTypeDef *huart);

#endif /* __UART_ADC_H */
