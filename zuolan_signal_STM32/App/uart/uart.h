#ifndef __UART_APP_H__
#define __UART_APP_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "usart.h"

#define UART_PRINTF_BUF_SIZE 128U
#define UART_DMA_RX_BUF_SIZE 256U
#define UART_RING_BUF_SIZE   1024U

typedef enum
{
    UART_PORT_1 = 0U,
    UART_PORT_2,
    UART_PORT_COUNT
} uart_port_id_t;

void UART_Init(void);
int my_printf(UART_HandleTypeDef *huart, const char *format, ...);
void uart_proc(void);

#ifdef __cplusplus
}
#endif

#endif /* __UART_APP_H__ */
