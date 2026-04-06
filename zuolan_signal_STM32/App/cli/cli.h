#ifndef __CLI_H__
#define __CLI_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "usart.h"

void CLI_Init(UART_HandleTypeDef *huart);
void CLI_ShowPrompt(UART_HandleTypeDef *huart);
void CLI_InputBuffer(UART_HandleTypeDef *huart, const uint8_t *data, uint16_t length);

#ifdef __cplusplus
}
#endif

#endif /* __CLI_H__ */
