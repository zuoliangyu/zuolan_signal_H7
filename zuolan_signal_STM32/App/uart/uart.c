#include "uart.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "main.h"

#include "cli.h"
#include "dac_app.h"
#include "led.h"

#define UART_PROC_TX_CHUNK_SIZE 64U

#if defined(__GNUC__)
#define UART_DMA_ALIGN __attribute__((aligned(32)))
#define UART_DMA_SECTION __attribute__((section(".dma_buffer")))
#else
#define UART_DMA_ALIGN
#define UART_DMA_SECTION
#endif

typedef struct
{
    const char *name;
    UART_HandleTypeDef *huart;
    uint8_t *dma_rx_buf;
    uint8_t ring_buf[UART_RING_BUF_SIZE];
    volatile uint16_t dma_last_pos;
    volatile uint16_t ring_head;
    volatile uint16_t ring_tail;
    volatile uint8_t overflow;
    volatile uint8_t idle_flag;
    volatile uint8_t ht_flag;
    volatile uint8_t tc_flag;
    volatile uint8_t error_flag;
    volatile uint8_t rx_ready;
} uart_app_port_t;

/* H7 上 UART DMA 不能访问 DTCM，接收缓冲区必须放到 DMA 可达 RAM。 */
static uint8_t s_uart1_dma_rx_buf[UART_DMA_RX_BUF_SIZE] UART_DMA_ALIGN UART_DMA_SECTION;
static uint8_t s_uart2_dma_rx_buf[UART_DMA_RX_BUF_SIZE] UART_DMA_ALIGN UART_DMA_SECTION;

static uart_app_port_t s_uart_ports[UART_PORT_COUNT] = {
    {
        .name = "USART1",
        .huart = &huart1,
        .dma_rx_buf = s_uart1_dma_rx_buf,
    },
    {
        .name = "USART2",
        .huart = &huart2,
        .dma_rx_buf = s_uart2_dma_rx_buf,
    },
};

static uint32_t UART_EnterCritical(void)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    return primask;
}

static void UART_ExitCritical(uint32_t primask)
{
    __set_PRIMASK(primask);
}

static uart_app_port_t *UART_FindPort(UART_HandleTypeDef *huart)
{
    for (uint32_t i = 0U; i < (uint32_t)UART_PORT_COUNT; i++)
    {
        if (s_uart_ports[i].huart == huart)
        {
            return &s_uart_ports[i];
        }
    }

    return NULL;
}

static void UART_ResetPort(uart_app_port_t *port)
{
    uint32_t primask;

    if (port == NULL)
    {
        return;
    }

    primask = UART_EnterCritical();
    port->dma_last_pos = 0U;
    port->ring_head = 0U;
    port->ring_tail = 0U;
    port->overflow = 0U;
    port->idle_flag = 0U;
    port->ht_flag = 0U;
    port->tc_flag = 0U;
    port->error_flag = 0U;
    port->rx_ready = 0U;
    (void)memset(port->dma_rx_buf, 0, UART_DMA_RX_BUF_SIZE);
    (void)memset(port->ring_buf, 0, sizeof(port->ring_buf));
    UART_ExitCritical(primask);
}

static void UART_PushToRingBuffer(uart_app_port_t *port, const uint8_t *data,
                                  uint16_t length)
{
    uint32_t primask;

    if ((port == NULL) || (data == NULL) || (length == 0U))
    {
        return;
    }

    primask = UART_EnterCritical();

    for (uint16_t i = 0U; i < length; i++)
    {
        uint16_t next_head = (uint16_t)((port->ring_head + 1U) % UART_RING_BUF_SIZE);

        if (next_head == port->ring_tail)
        {
            port->overflow = 1U;
            break;
        }

        port->ring_buf[port->ring_head] = data[i];
        port->ring_head = next_head;
    }

    UART_ExitCritical(primask);
}

static uint16_t UART_PopFromRingBuffer(uart_app_port_t *port, uint8_t *buffer,
                                       uint16_t max_length)
{
    uint16_t length = 0U;
    uint32_t primask;

    if ((port == NULL) || (buffer == NULL) || (max_length == 0U))
    {
        return 0U;
    }

    primask = UART_EnterCritical();

    while ((port->ring_tail != port->ring_head) && (length < max_length))
    {
        buffer[length++] = port->ring_buf[port->ring_tail];
        port->ring_tail = (uint16_t)((port->ring_tail + 1U) % UART_RING_BUF_SIZE);
    }

    UART_ExitCritical(primask);
    return length;
}

static uint8_t UART_TakeFlag(volatile uint8_t *flag)
{
    uint8_t value;
    uint32_t primask;

    if (flag == NULL)
    {
        return 0U;
    }

    primask = UART_EnterCritical();
    value = *flag;
    *flag = 0U;
    UART_ExitCritical(primask);

    return value;
}

static uint16_t UART_HandleRxEvent(uart_app_port_t *port, uint16_t size)
{
    uint16_t current_pos;
    uint16_t added_bytes = 0U;

    if ((port == NULL) || (size > UART_DMA_RX_BUF_SIZE))
    {
        return 0U;
    }

    current_pos = size;

    if (current_pos != port->dma_last_pos)
    {
        if (current_pos > port->dma_last_pos)
        {
            UART_PushToRingBuffer(port, &port->dma_rx_buf[port->dma_last_pos],
                                  (uint16_t)(current_pos - port->dma_last_pos));
            added_bytes = (uint16_t)(current_pos - port->dma_last_pos);
        }
        else
        {
            UART_PushToRingBuffer(port, &port->dma_rx_buf[port->dma_last_pos],
                                  (uint16_t)(UART_DMA_RX_BUF_SIZE - port->dma_last_pos));
            added_bytes = (uint16_t)(UART_DMA_RX_BUF_SIZE - port->dma_last_pos);

            if (current_pos > 0U)
            {
                UART_PushToRingBuffer(port, &port->dma_rx_buf[0], current_pos);
                added_bytes = (uint16_t)(added_bytes + current_pos);
            }
        }
    }

    port->dma_last_pos = (current_pos >= UART_DMA_RX_BUF_SIZE) ? 0U : current_pos;
    return added_bytes;
}

static HAL_StatusTypeDef UART_StartReceiveDMA(uart_app_port_t *port)
{
    HAL_StatusTypeDef status;

    if ((port == NULL) || (port->huart == NULL))
    {
        return HAL_ERROR;
    }

    port->dma_last_pos = 0U;
    status = HAL_UARTEx_ReceiveToIdle_DMA(port->huart, port->dma_rx_buf,
                                          UART_DMA_RX_BUF_SIZE);
    port->rx_ready = (status == HAL_OK) ? 1U : 0U;

    return status;
}

static void UART_PrintBootStatus(void)
{
    (void)my_printf(&huart1, "\r\n");
    (void)my_printf(&huart1, "System boot summary\r\n");
    (void)my_printf(&huart1, "USART1: mode=cli, rx_dma=%s, dma_buf=%u, ring_buf=%u\r\n",
                    (s_uart_ports[UART_PORT_1].rx_ready != 0U) ? "ready" : "error",
                    (unsigned int)UART_DMA_RX_BUF_SIZE,
                    (unsigned int)UART_RING_BUF_SIZE);
    (void)my_printf(&huart1, "USART2: mode=echo, rx_dma=%s, dma_buf=%u, ring_buf=%u\r\n",
                    (s_uart_ports[UART_PORT_2].rx_ready != 0U) ? "ready" : "error",
                    (unsigned int)UART_DMA_RX_BUF_SIZE,
                    (unsigned int)UART_RING_BUF_SIZE);
    (void)my_printf(&huart1, "LED0: state=%u, blink=%u, interval_ms=%u, active_level=low\r\n",
                    (unsigned int)ucLed[0], (unsigned int)LED_GetBlinkEnabled(0U),
                    (unsigned int)LED_GetBlinkIntervalMs(0U));
    (void)my_printf(&huart1,
                    "DAC1_CH1: state=%s, mode=%s, amp_mv=%u, offset_mv=%u, freq_hz=%lu, duty_percent=%u, raw=%u\r\n",
                    (DAC_APP_IsStarted() != 0U) ? "running" : "stopped",
                    DAC_APP_GetModeString(), (unsigned int)DAC_APP_GetAmpMv(),
                    (unsigned int)DAC_APP_GetOffsetMv(),
                    (unsigned long)DAC_APP_GetFreqHz(),
                    (unsigned int)DAC_APP_GetDutyPercent(),
                    (unsigned int)DAC_APP_GetCurrentRaw());
    (void)my_printf(&huart1, "Commands: help, echo, led, dac\r\n");
    (void)my_printf(&huart1, "\r\n");
}

void UART_Init(void)
{
    for (uint32_t i = 0U; i < (uint32_t)UART_PORT_COUNT; i++)
    {
        UART_ResetPort(&s_uart_ports[i]);
        (void)UART_StartReceiveDMA(&s_uart_ports[i]);
    }

    CLI_Init(&huart1);
    UART_PrintBootStatus();
    CLI_ShowPrompt(&huart1);
}

int my_printf(UART_HandleTypeDef *huart, const char *format, ...)
{
    char buffer[UART_PRINTF_BUF_SIZE];
    va_list arg;
    int len;

    va_start(arg, format);
    len = vsnprintf(buffer, sizeof(buffer), format, arg);
    va_end(arg);

    if (len < 0)
    {
        return len;
    }

    if (len >= (int)sizeof(buffer))
    {
        len = (int)sizeof(buffer) - 1;
    }

    if ((huart != NULL) && (len > 0))
    {
        (void)HAL_UART_Transmit(huart, (uint8_t *)buffer, (uint16_t)len, 0xFFFFU);
    }

    return len;
}

void uart_proc(void)
{
    uint8_t tx_buffer[UART_PROC_TX_CHUNK_SIZE];

    for (uint32_t i = 0U; i < (uint32_t)UART_PORT_COUNT; i++)
    {
        uart_app_port_t *port = &s_uart_ports[i];
        uint16_t length;

        if (port->rx_ready == 0U)
        {
            continue;
        }

        (void)UART_TakeFlag(&port->overflow);
        (void)UART_TakeFlag(&port->error_flag);

        if ((UART_TakeFlag(&port->idle_flag) == 0U) &&
            (UART_TakeFlag(&port->ht_flag) == 0U) &&
            (UART_TakeFlag(&port->tc_flag) == 0U) &&
            (port->ring_head == port->ring_tail))
        {
            continue;
        }

        length = UART_PopFromRingBuffer(port, tx_buffer, sizeof(tx_buffer));
        if (length > 0U)
        {
            if (port->huart == &huart1)
            {
                CLI_InputBuffer(port->huart, tx_buffer, length);
            }
            else
            {
                (void)HAL_UART_Transmit(port->huart, tx_buffer, length, 0xFFFFU);
            }
        }
    }
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    uart_app_port_t *port = UART_FindPort(huart);
    HAL_UART_RxEventTypeTypeDef event_type;

    if (port == NULL)
    {
        return;
    }

    event_type = HAL_UARTEx_GetRxEventType(huart);
    (void)UART_HandleRxEvent(port, Size);

    if (event_type == HAL_UART_RXEVENT_IDLE)
    {
        port->idle_flag = 1U;
    }
    else if (event_type == HAL_UART_RXEVENT_HT)
    {
        port->ht_flag = 1U;
    }
    else if (event_type == HAL_UART_RXEVENT_TC)
    {
        port->tc_flag = 1U;
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    uart_app_port_t *port = UART_FindPort(huart);

    if (port == NULL)
    {
        return;
    }

    port->error_flag = 1U;
    (void)HAL_UART_DMAStop(huart);
    (void)UART_StartReceiveDMA(port);
}
