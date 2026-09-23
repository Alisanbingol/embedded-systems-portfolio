#include "uart_adc.h"
#include <stdio.h>
#include <string.h>


void ADC_read_send_uart(int pin, ADC_HandleTypeDef *hadc, UART_HandleTypeDef *huart) {
    HAL_ADC_Start(hadc);
    HAL_ADC_PollForConversion(hadc, 1000);
    uint32_t data = HAL_ADC_GetValue(hadc);
    HAL_ADC_Stop(hadc);
    char buffer[20];
    sprintf(buffer, "PIN_%d= %lu\n", pin, data);
    HAL_UART_Transmit(huart, (uint8_t*) buffer, strlen(buffer), HAL_MAX_DELAY);
}
