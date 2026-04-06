#include "cli.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "dac_app.h"
#include "led.h"
#include "uart.h"

#define CLI_MAX_LINE_LENGTH 128U
#define CLI_MAX_TOKENS      8U

typedef void (*cli_handler_t)(UART_HandleTypeDef *huart, uint8_t argc, char *argv[]);

typedef struct
{
    const char *cmd;
    const char *help;
    cli_handler_t handler;
} cli_command_t;

typedef struct
{
    UART_HandleTypeDef *huart;
    char line[CLI_MAX_LINE_LENGTH];
    uint16_t length;
    uint8_t echo;
} cli_context_t;

static cli_context_t s_cli = {0};

static void CLI_Write(UART_HandleTypeDef *huart, const char *text)
{
    if ((huart == NULL) || (text == NULL))
    {
        return;
    }

    (void)HAL_UART_Transmit(huart, (uint8_t *)text, (uint16_t)strlen(text), 0xFFFFU);
}

static void CLI_WriteLine(UART_HandleTypeDef *huart, const char *text)
{
    if (text == NULL)
    {
        return;
    }

    (void)my_printf(huart, "%s\r\n", text);
}

void CLI_ShowPrompt(UART_HandleTypeDef *huart)
{
    CLI_Write(huart, "> ");
}

static void CLI_CmdHelp(UART_HandleTypeDef *huart, uint8_t argc, char *argv[]);
static void CLI_CmdEcho(UART_HandleTypeDef *huart, uint8_t argc, char *argv[]);
static void CLI_CmdLed(UART_HandleTypeDef *huart, uint8_t argc, char *argv[]);
static void CLI_CmdDac(UART_HandleTypeDef *huart, uint8_t argc, char *argv[]);

static const cli_command_t s_cli_commands[] = {
    {"help", "List all commands", CLI_CmdHelp},
    {"echo", "Echo parameters: echo <text>", CLI_CmdEcho},
    {"led", "Control LED: led on/off/toggle/blink", CLI_CmdLed},
    {"dac", "Control DAC: dac get/dc/wave/start/stop", CLI_CmdDac},
};

static uint8_t CLI_Tokenize(char *line, char *argv[], uint8_t max_tokens)
{
    uint8_t argc = 0U;
    char *p = line;

    while ((argc < max_tokens) && (p != NULL))
    {
        while ((*p != '\0') && (isspace((unsigned char)*p) != 0))
        {
            ++p;
        }

        if (*p == '\0')
        {
            break;
        }

        argv[argc++] = p;

        while ((*p != '\0') && (isspace((unsigned char)*p) == 0))
        {
            ++p;
        }

        if (*p == '\0')
        {
            break;
        }

        *p = '\0';
        ++p;
    }

    return argc;
}

static void CLI_ExecuteLine(cli_context_t *ctx)
{
    char *argv[CLI_MAX_TOKENS];
    uint8_t argc;
    uint8_t matched = 0U;

    if ((ctx == NULL) || (ctx->length == 0U))
    {
        return;
    }

    ctx->line[ctx->length] = '\0';
    argc = CLI_Tokenize(ctx->line, argv, CLI_MAX_TOKENS);

    if (argc == 0U)
    {
        return;
    }

    for (uint32_t i = 0U; i < (sizeof(s_cli_commands) / sizeof(s_cli_commands[0])); i++)
    {
        if (strcmp(argv[0], s_cli_commands[i].cmd) == 0)
        {
            s_cli_commands[i].handler(ctx->huart, argc, argv);
            matched = 1U;
            break;
        }
    }

    if (matched == 0U)
    {
        (void)my_printf(ctx->huart, "Unknown command: %s\r\n", argv[0]);
    }
}

static void CLI_CmdHelp(UART_HandleTypeDef *huart, uint8_t argc, char *argv[])
{
    (void)argc;
    (void)argv;

    CLI_WriteLine(huart, "Available commands:");
    for (uint32_t i = 0U; i < (sizeof(s_cli_commands) / sizeof(s_cli_commands[0])); i++)
    {
        (void)my_printf(huart, "  %s - %s\r\n", s_cli_commands[i].cmd,
                        s_cli_commands[i].help);
    }
}

static void CLI_CmdEcho(UART_HandleTypeDef *huart, uint8_t argc, char *argv[])
{
    if (argc <= 1U)
    {
        CLI_WriteLine(huart, "Usage: echo <text>");
        return;
    }

    for (uint8_t i = 1U; i < argc; i++)
    {
        CLI_Write(huart, argv[i]);
        if (i + 1U < argc)
        {
            CLI_Write(huart, " ");
        }
    }
    CLI_Write(huart, "\r\n");
}

static void CLI_CmdLed(UART_HandleTypeDef *huart, uint8_t argc, char *argv[])
{
    unsigned long interval_ms;
    char *end_ptr;

    if (argc <= 1U)
    {
        CLI_WriteLine(huart, "Usage: led on/off/toggle/blink [interval_ms]");
        return;
    }

    if (strcmp(argv[1], "on") == 0)
    {
        LED_SetBlinkEnabled(0U, 0U);
        LED_SetState(0U, 1U);
    }
    else if (strcmp(argv[1], "off") == 0)
    {
        LED_SetBlinkEnabled(0U, 0U);
        LED_SetState(0U, 0U);
    }
    else if (strcmp(argv[1], "toggle") == 0)
    {
        LED_SetBlinkEnabled(0U, 0U);
        LED_ToggleState(0U);
    }
    else if (strcmp(argv[1], "blink") == 0)
    {
        if (argc >= 3U)
        {
            interval_ms = strtoul(argv[2], &end_ptr, 10);
            if ((*argv[2] == '\0') || (*end_ptr != '\0') || (interval_ms == 0UL) ||
                (interval_ms > 65535UL))
            {
                CLI_WriteLine(huart, "Usage: led blink [interval_ms], interval_ms=1..65535");
                return;
            }

            LED_SetBlinkIntervalMs(0U, (uint16_t)interval_ms);
        }

        LED_SetBlinkEnabled(0U, 1U);
    }
    else
    {
        CLI_WriteLine(huart, "Usage: led on/off/toggle/blink [interval_ms]");
        return;
    }

    (void)my_printf(huart, "LED=%u, blink=%u, interval_ms=%u\r\n",
                    (unsigned int)ucLed[0], (unsigned int)LED_GetBlinkEnabled(0U),
                    (unsigned int)LED_GetBlinkIntervalMs(0U));
}

static void CLI_CmdDac(UART_HandleTypeDef *huart, uint8_t argc, char *argv[])
{
    unsigned long value;
    char *end_ptr;
    dac_app_waveform_t waveform;

    if (argc <= 1U)
    {
        CLI_WriteLine(huart, "Usage: dac get|dc <mv>|set <mv>|wave <sine|tri|square> <freq_hz>|start|stop");
        return;
    }

    if (strcmp(argv[1], "get") == 0)
    {
        if (DAC_APP_GetWaveform() == DAC_APP_WAVE_NONE)
        {
            (void)my_printf(huart, "DAC1_CH1: state=%s, mode=%s, mv=%u, raw=%u, ref_mv=%u\r\n",
                            (DAC_APP_IsStarted() != 0U) ? "running" : "stopped",
                            DAC_APP_GetModeString(), (unsigned int)DAC_APP_GetValueMv(),
                            (unsigned int)DAC_APP_GetValueRaw(),
                            (unsigned int)DAC_APP_REFERENCE_MV);
        }
        else
        {
            (void)my_printf(huart, "DAC1_CH1: state=%s, mode=%s, freq_hz=%lu, samples=%u\r\n",
                            (DAC_APP_IsStarted() != 0U) ? "running" : "stopped",
                            DAC_APP_GetModeString(),
                            (unsigned long)DAC_APP_GetWaveFrequencyHz(),
                            (unsigned int)DAC_APP_WAVE_SAMPLES);
        }
        return;
    }

    if ((strcmp(argv[1], "dc") == 0) || (strcmp(argv[1], "set") == 0))
    {
        if (argc < 3U)
        {
            CLI_WriteLine(huart, "Usage: dac dc <mv>, mv=0..3300");
            return;
        }

        value = strtoul(argv[2], &end_ptr, 10);
        if ((*argv[2] == '\0') || (*end_ptr != '\0') ||
            (value > (unsigned long)DAC_APP_REFERENCE_MV))
        {
            CLI_WriteLine(huart, "Usage: dac dc <mv>, mv=0..3300");
            return;
        }

        DAC_APP_SetValueMv((uint16_t)value);
        (void)my_printf(huart, "DAC1_CH1 dc: mv=%u, raw=%u\r\n",
                        (unsigned int)DAC_APP_GetValueMv(),
                        (unsigned int)DAC_APP_GetValueRaw());
        return;
    }

    if (strcmp(argv[1], "wave") == 0)
    {
        if (argc < 4U)
        {
            CLI_WriteLine(huart, "Usage: dac wave <sine|tri|square> <freq_hz>");
            return;
        }

        if (strcmp(argv[2], "sine") == 0)
        {
            waveform = DAC_APP_WAVE_SINE;
        }
        else if ((strcmp(argv[2], "tri") == 0) || (strcmp(argv[2], "triangle") == 0))
        {
            waveform = DAC_APP_WAVE_TRIANGLE;
        }
        else if (strcmp(argv[2], "square") == 0)
        {
            waveform = DAC_APP_WAVE_SQUARE;
        }
        else
        {
            CLI_WriteLine(huart, "Usage: dac wave <sine|tri|square> <freq_hz>");
            return;
        }

        value = strtoul(argv[3], &end_ptr, 10);
        if ((*argv[3] == '\0') || (*end_ptr != '\0') || (value == 0UL))
        {
            CLI_WriteLine(huart, "Usage: dac wave <sine|tri|square> <freq_hz>");
            return;
        }

        if (DAC_APP_StartWave(waveform, (uint32_t)value) == 0U)
        {
            CLI_WriteLine(huart, "DAC wave config invalid for current timer clock");
            return;
        }

        (void)my_printf(huart, "DAC1_CH1 wave: mode=%s, freq_hz=%lu, samples=%u\r\n",
                        DAC_APP_GetModeString(),
                        (unsigned long)DAC_APP_GetWaveFrequencyHz(),
                        (unsigned int)DAC_APP_WAVE_SAMPLES);
        return;
    }

    if (strcmp(argv[1], "start") == 0)
    {
        DAC_APP_Start();
        (void)my_printf(huart, "DAC1_CH1 started: mode=%s\r\n", DAC_APP_GetModeString());
        return;
    }

    if (strcmp(argv[1], "stop") == 0)
    {
        DAC_APP_Stop();
        CLI_WriteLine(huart, "DAC1_CH1 stopped");
        return;
    }

    CLI_WriteLine(huart, "Usage: dac get|dc <mv>|set <mv>|wave <sine|tri|square> <freq_hz>|start|stop");
}

void CLI_Init(UART_HandleTypeDef *huart)
{
    s_cli.huart = huart;
    s_cli.length = 0U;
    s_cli.echo = 1U;
}

void CLI_InputBuffer(UART_HandleTypeDef *huart, const uint8_t *data, uint16_t length)
{
    if ((huart == NULL) || (data == NULL) || (length == 0U))
    {
        return;
    }

    if (huart != s_cli.huart)
    {
        return;
    }

    for (uint16_t i = 0U; i < length; i++)
    {
        char ch = (char)data[i];

        if ((ch == '\r') || (ch == '\n'))
        {
            if (s_cli.echo != 0U)
            {
                CLI_Write(huart, "\r\n");
            }
            CLI_ExecuteLine(&s_cli);
            s_cli.length = 0U;
            CLI_ShowPrompt(huart);
            continue;
        }

        if ((ch == 0x08) || (ch == 0x7F))
        {
            if (s_cli.length > 0U)
            {
                s_cli.length--;
                if (s_cli.echo != 0U)
                {
                    CLI_Write(huart, "\b \b");
                }
            }
            continue;
        }

        if (isprint((unsigned char)ch) == 0)
        {
            continue;
        }

        if (s_cli.length < (CLI_MAX_LINE_LENGTH - 1U))
        {
            s_cli.line[s_cli.length++] = ch;
            if (s_cli.echo != 0U)
            {
                (void)HAL_UART_Transmit(huart, (uint8_t *)&ch, 1U, 0xFFFFU);
            }
        }
        else
        {
            CLI_WriteLine(huart, "Input too long. Buffer cleared.");
            s_cli.length = 0U;
            CLI_ShowPrompt(huart);
        }
    }
}
