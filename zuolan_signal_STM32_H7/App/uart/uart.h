#ifndef __UART_APP_H__
#define __UART_APP_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "usart.h"

#define UART_PRINTF_BUF_SIZE 256U
#define UART_DMA_RX_BUF_SIZE 256U
#define UART_RING_BUF_SIZE   1024U
#define UART_TX_BUF_SIZE     4096U  // 异步 TX 环形缓冲（每 UART 端口一份）

typedef enum
{
    UART_PORT_1 = 0U,
    UART_PORT_2,
    UART_PORT_COUNT
} uart_port_id_t;

void UART_Init(void);
int my_printf(UART_HandleTypeDef *huart, const char *format, ...);
void uart_proc(void);

// 把任意字节流推进 TX 环形缓冲，由 DMA 后台输出。绝不阻塞。
// 缓冲满时丢弃尾部多余字节并累加 dropped 计数。返回实际入队字节数。
int UART_WriteAsync(UART_HandleTypeDef *huart, const uint8_t *data, uint16_t length);

// TX 异步统计：丢字节数累计、当前空闲容量
uint32_t UART_GetTxDroppedBytes(uart_port_id_t port);
uint32_t UART_GetTxRingFreeBytes(uart_port_id_t port);

#ifdef __cplusplus
}
#endif

#endif /* __UART_APP_H__ */
