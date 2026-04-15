#include "cli.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "adc_app.h"
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
static void CLI_CmdAdc(UART_HandleTypeDef *huart, uint8_t argc, char *argv[]);
static void CLI_CmdDac(UART_HandleTypeDef *huart, uint8_t argc, char *argv[]);

static const cli_command_t s_cli_commands[] = {
    {"help", "List all commands", CLI_CmdHelp},
    {"echo", "Echo parameters: echo <text>", CLI_CmdEcho},
    {"led", "Control LED: led on/off/toggle/blink", CLI_CmdLed},
    {"adc", "ADC: get/raw/mv/avg/rate/frame/stream/block", CLI_CmdAdc},
    {"dac", "Control DAC: dac get/mode/amp/offset/freq/duty/start/stop", CLI_CmdDac},
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

    if (strcmp(argv[1], "help") == 0)
    {
        CLI_WriteLine(huart, "LED commands:");
        CLI_WriteLine(huart, "  led on           - turn LED on and disable blink");
        CLI_WriteLine(huart, "  led off          - turn LED off and disable blink");
        CLI_WriteLine(huart, "  led toggle       - toggle LED once and disable blink");
        CLI_WriteLine(huart, "  led blink        - enable blink with current interval");
        CLI_WriteLine(huart, "  led blink <ms>   - set blink toggle interval in ms");
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
    uint8_t is_query = 0U;

    if ((argc >= 3U) && (strcmp(argv[2], "?") == 0))
    {
        is_query = 1U;
    }

    if (argc <= 1U)
    {
        CLI_WriteLine(huart, "Usage: dac get|mode <value|?>|amp <mv|?>|offset <mv|?>|freq <hz|?>|duty <0..100|?>|start|stop");
        return;
    }

    if (strcmp(argv[1], "help") == 0)
    {
        CLI_WriteLine(huart, "DAC commands:");
        CLI_WriteLine(huart, "  dac get          - show full DAC state");
        CLI_WriteLine(huart, "  dac mode <...>   - set mode: dc/sine/tri/square");
        CLI_WriteLine(huart, "  dac mode ?       - show current mode");
        CLI_WriteLine(huart, "  dac amp <mv>     - set waveform amplitude");
        CLI_WriteLine(huart, "  dac amp ?        - show current amplitude");
        CLI_WriteLine(huart, "  dac offset <mv>  - set DC level / waveform center");
        CLI_WriteLine(huart, "  dac offset ?     - show current offset");
        CLI_WriteLine(huart, "  dac freq <hz>    - set waveform frequency");
        CLI_WriteLine(huart, "  dac freq ?       - show current frequency");
        CLI_WriteLine(huart, "  dac duty <0..100>- set square-wave duty percent");
        CLI_WriteLine(huart, "  dac duty ?       - show square-wave duty percent");
        CLI_WriteLine(huart, "  dac start/stop   - start or stop output");
        return;
    }

    if (strcmp(argv[1], "get") == 0)
    {
        (void)my_printf(huart,
                        "DAC1_CH1: state=%s, mode=%s, amp_mv=%u, offset_mv=%u, freq_hz=%lu, duty=%u, raw=%u, samples=%u\r\n",
                        (DAC_APP_IsStarted() != 0U) ? "running" : "stopped",
                        DAC_APP_GetModeString(), (unsigned int)DAC_APP_GetAmpMv(),
                        (unsigned int)DAC_APP_GetOffsetMv(),
                        (unsigned long)DAC_APP_GetFreqHz(),
                        (unsigned int)DAC_APP_GetDutyPercent(),
                        (unsigned int)DAC_APP_GetCurrentRaw(),
                        (unsigned int)DAC_APP_WAVE_SAMPLES);
        return;
    }

    if (strcmp(argv[1], "mode") == 0)
    {
        if (is_query != 0U)
        {
            (void)my_printf(huart, "mode=%s\r\n", DAC_APP_GetModeString());
            return;
        }

        if (argc < 3U)
        {
            CLI_WriteLine(huart, "Usage: dac mode <dc|sine|tri|square|?>");
            return;
        }

        if (strcmp(argv[2], "dc") == 0)
        {
            DAC_APP_SetMode(DAC_APP_MODE_DC);
        }
        else if (strcmp(argv[2], "sine") == 0)
        {
            DAC_APP_SetMode(DAC_APP_MODE_SINE);
        }
        else if ((strcmp(argv[2], "tri") == 0) || (strcmp(argv[2], "triangle") == 0))
        {
            DAC_APP_SetMode(DAC_APP_MODE_TRIANGLE);
        }
        else if (strcmp(argv[2], "square") == 0)
        {
            DAC_APP_SetMode(DAC_APP_MODE_SQUARE);
        }
        else
        {
            CLI_WriteLine(huart, "Usage: dac mode <dc|sine|tri|square|?>");
            return;
        }

        (void)my_printf(huart, "mode=%s\r\n", DAC_APP_GetModeString());
        return;
    }

    if (strcmp(argv[1], "amp") == 0)
    {
        if (is_query != 0U)
        {
            (void)my_printf(huart, "amp_mv=%u\r\n", (unsigned int)DAC_APP_GetAmpMv());
            return;
        }

        if (argc < 3U)
        {
            CLI_WriteLine(huart, "Usage: dac amp <mv|?>, mv=0..3300");
            return;
        }

        value = strtoul(argv[2], &end_ptr, 10);
        if ((*argv[2] == '\0') || (*end_ptr != '\0') ||
            (value > (unsigned long)DAC_APP_REFERENCE_MV))
        {
            CLI_WriteLine(huart, "Usage: dac amp <mv|?>, mv=0..3300");
            return;
        }

        if (DAC_APP_SetAmpMv((uint16_t)value) == 0U)
        {
            (void)my_printf(huart,
                            "amp out of range: requested=%lu, valid=0..%u, current_offset_mv=%u\r\n",
                            value, (unsigned int)DAC_APP_GetAmpMaxMv(),
                            (unsigned int)DAC_APP_GetOffsetMv());
            return;
        }

        (void)my_printf(huart, "amp_mv=%u\r\n", (unsigned int)DAC_APP_GetAmpMv());
        return;
    }

    if (strcmp(argv[1], "offset") == 0)
    {
        if (is_query != 0U)
        {
            (void)my_printf(huart, "offset_mv=%u\r\n", (unsigned int)DAC_APP_GetOffsetMv());
            return;
        }

        if (argc < 3U)
        {
            CLI_WriteLine(huart, "Usage: dac offset <mv|?>, mv=0..3300");
            return;
        }

        value = strtoul(argv[2], &end_ptr, 10);
        if ((*argv[2] == '\0') || (*end_ptr != '\0') ||
            (value > (unsigned long)DAC_APP_REFERENCE_MV))
        {
            CLI_WriteLine(huart, "Usage: dac offset <mv|?>, mv=0..3300");
            return;
        }

        if (DAC_APP_SetOffsetMv((uint16_t)value) == 0U)
        {
            (void)my_printf(huart,
                            "offset out of range: requested=%lu, valid=%u..%u, current_amp_mv=%u\r\n",
                            value, (unsigned int)DAC_APP_GetOffsetMinMv(),
                            (unsigned int)DAC_APP_GetOffsetMaxMv(),
                            (unsigned int)DAC_APP_GetAmpMv());
            return;
        }

        (void)my_printf(huart, "offset_mv=%u, raw=%u\r\n",
                        (unsigned int)DAC_APP_GetOffsetMv(),
                        (unsigned int)DAC_APP_GetCurrentRaw());
        return;
    }

    if (strcmp(argv[1], "freq") == 0)
    {
        if (is_query != 0U)
        {
            (void)my_printf(huart, "freq_hz=%lu\r\n", (unsigned long)DAC_APP_GetFreqHz());
            return;
        }

        if (argc < 3U)
        {
            CLI_WriteLine(huart, "Usage: dac freq <hz|?>");
            return;
        }

        value = strtoul(argv[2], &end_ptr, 10);
        if ((*argv[2] == '\0') || (*end_ptr != '\0') || (value == 0UL))
        {
            CLI_WriteLine(huart, "Usage: dac freq <hz|?>");
            return;
        }

        if (DAC_APP_SetFreqHz((uint32_t)value) == 0U)
        {
            CLI_WriteLine(huart, "Invalid frequency");
            return;
        }

        (void)my_printf(huart, "freq_hz=%lu\r\n", (unsigned long)DAC_APP_GetFreqHz());
        return;
    }

    if (strcmp(argv[1], "duty") == 0)
    {
        if (is_query != 0U)
        {
            (void)my_printf(huart, "duty_percent=%u\r\n",
                            (unsigned int)DAC_APP_GetDutyPercent());
            return;
        }

        if (argc < 3U)
        {
            CLI_WriteLine(huart, "Usage: dac duty <0..100|?>");
            return;
        }

        value = strtoul(argv[2], &end_ptr, 10);
        if ((*argv[2] == '\0') || (*end_ptr != '\0') || (value > 100UL))
        {
            CLI_WriteLine(huart, "Usage: dac duty <0..100|?>");
            return;
        }

        DAC_APP_SetDutyPercent((uint8_t)value);
        (void)my_printf(huart, "duty_percent=%u\r\n",
                        (unsigned int)DAC_APP_GetDutyPercent());
        return;
    }

    if (strcmp(argv[1], "start") == 0)
    {
        DAC_APP_Start();
        if (DAC_APP_IsStarted() == 0U)
        {
            CLI_WriteLine(huart, "DAC1_CH1 start failed with current config");
            return;
        }

        (void)my_printf(huart, "DAC1_CH1 started: mode=%s\r\n", DAC_APP_GetModeString());
        return;
    }

    if (strcmp(argv[1], "stop") == 0)
    {
        DAC_APP_Stop();
        CLI_WriteLine(huart, "DAC1_CH1 stopped");
        return;
    }

    CLI_WriteLine(huart, "Usage: dac get|mode <value|?>|amp <mv|?>|offset <mv|?>|freq <hz|?>|duty <0..100|?>|start|stop");
}

static void CLI_CmdAdc(UART_HandleTypeDef *huart, uint8_t argc, char *argv[])
{
    static uint16_t frame_buffer[ADC_APP_DMA_SAMPLES];
    adc_app_block_t block;
    unsigned long interval_ms;
    unsigned long sample_rate_hz;
    char *end_ptr;
    uint32_t frame_seq;

    if (argc <= 1U)
    {
        CLI_WriteLine(huart, "Usage: adc get|raw|mv|avg|rate <hz|?>|frame|frame ?|stream on [ms]|stream off|stream ?|block on|off|?|next|help");
        return;
    }

    if (strcmp(argv[1], "help") == 0)
    {
        CLI_WriteLine(huart, "ADC commands:");
        CLI_WriteLine(huart, "  adc get             - show full ADC state");
        CLI_WriteLine(huart, "  adc raw             - show latest raw sample");
        CLI_WriteLine(huart, "  adc mv              - show latest sample in mV");
        CLI_WriteLine(huart, "  adc avg             - show averaged raw and mV");
        CLI_WriteLine(huart, "  adc rate <hz>       - set ADC sample rate from TIM2");
        CLI_WriteLine(huart, "  adc rate ?          - show current ADC sample rate");
        CLI_WriteLine(huart, "  adc frame           - dump latest full frame as index,raw,mv");
        CLI_WriteLine(huart, "  adc frame ?         - show frame size and latest frame seq");
        CLI_WriteLine(huart, "  adc stream on [ms]  - start continuous CSV output: raw,mv");
        CLI_WriteLine(huart, "  adc stream off      - stop continuous output");
        CLI_WriteLine(huart, "  adc stream ?        - show stream state");
        CLI_WriteLine(huart, "  adc block on        - stream half/full DMA blocks as raw lines");
        CLI_WriteLine(huart, "  adc block off       - stop block streaming");
        CLI_WriteLine(huart, "  adc block ?         - show block streaming status");
        CLI_WriteLine(huart, "  adc next            - dump one queued half/full DMA block");
        return;
    }

    if (strcmp(argv[1], "get") == 0)
    {
        (void)my_printf(huart,
                        "ADC1: state=%s, pin=PA0, channel=16, rate_hz=%lu, dma_samples=%u, block_samples=%u, latest_raw=%u, latest_mv=%u, avg_raw=%u, avg_mv=%u, frame_seq=%lu, half_events=%lu, full_events=%lu, dropped_blocks=%lu, monitor=%s, interval_ms=%u, block_stream=%s\r\n",
                        (ADC_APP_IsStarted() != 0U) ? "running" : "error",
                        (unsigned long)ADC_APP_GetSampleRateHz(),
                        (unsigned int)ADC_APP_GetBufferSamples(),
                        (unsigned int)ADC_APP_GetBlockSamples(),
                        (unsigned int)ADC_APP_GetLatestRaw(),
                        (unsigned int)ADC_APP_GetLatestMv(),
                        (unsigned int)ADC_APP_GetAverageRaw(),
                        (unsigned int)ADC_APP_GetAverageMv(),
                        (unsigned long)ADC_APP_GetLatestFrameSeq(),
                        (unsigned long)ADC_APP_GetHalfEventCount(),
                        (unsigned long)ADC_APP_GetFullEventCount(),
                        (unsigned long)ADC_APP_GetDroppedBlockCount(),
                        (ADC_APP_GetStreamEnabled() != 0U) ? "on" : "off",
                        (unsigned int)ADC_APP_GetStreamIntervalMs(),
                        (ADC_APP_GetBlockStreamEnabled() != 0U) ? "on" : "off");
        return;
    }

    if (strcmp(argv[1], "raw") == 0)
    {
        (void)my_printf(huart, "raw=%u\r\n", (unsigned int)ADC_APP_GetLatestRaw());
        return;
    }

    if (strcmp(argv[1], "mv") == 0)
    {
        (void)my_printf(huart, "mv=%u\r\n", (unsigned int)ADC_APP_GetLatestMv());
        return;
    }

    if (strcmp(argv[1], "avg") == 0)
    {
        (void)my_printf(huart, "avg_raw=%u, avg_mv=%u\r\n",
                        (unsigned int)ADC_APP_GetAverageRaw(),
                        (unsigned int)ADC_APP_GetAverageMv());
        return;
    }

    if (strcmp(argv[1], "rate") == 0)
    {
        if ((argc >= 3U) && (strcmp(argv[2], "?") == 0))
        {
            (void)my_printf(huart, "rate_hz=%lu\r\n",
                            (unsigned long)ADC_APP_GetSampleRateHz());
            return;
        }

        if (argc < 3U)
        {
            CLI_WriteLine(huart, "Usage: adc rate <hz|?>");
            return;
        }

        sample_rate_hz = strtoul(argv[2], &end_ptr, 10);
        if ((*argv[2] == '\0') || (*end_ptr != '\0') || (sample_rate_hz == 0UL))
        {
            CLI_WriteLine(huart, "Usage: adc rate <hz|?>");
            return;
        }

        if (ADC_APP_SetSampleRateHz((uint32_t)sample_rate_hz) == 0U)
        {
            CLI_WriteLine(huart, "Invalid ADC sample rate");
            return;
        }

        (void)my_printf(huart, "rate_hz=%lu\r\n",
                        (unsigned long)ADC_APP_GetSampleRateHz());
        return;
    }

    if (strcmp(argv[1], "frame") == 0)
    {
        if ((argc >= 3U) && (strcmp(argv[2], "?") == 0))
        {
            (void)my_printf(huart, "frame_samples=%u, frame_seq=%lu\r\n",
                            (unsigned int)ADC_APP_GetBufferSamples(),
                            (unsigned long)ADC_APP_GetLatestFrameSeq());
            return;
        }

        frame_seq = ADC_APP_GetLatestFrameSeq();
        ADC_APP_CopyLatestFrame(frame_buffer, ADC_APP_DMA_SAMPLES);
        (void)my_printf(huart, "# frame_seq=%lu, sample_rate_hz=%lu, samples=%u, format=index,raw,mv\r\n",
                        (unsigned long)frame_seq,
                        (unsigned long)ADC_APP_GetSampleRateHz(),
                        (unsigned int)ADC_APP_GetBufferSamples());
        for (uint32_t i = 0U; i < ADC_APP_DMA_SAMPLES; ++i)
        {
            uint16_t raw = frame_buffer[i];
            uint32_t mv = (((uint32_t)raw * (uint32_t)ADC_APP_REFERENCE_MV) +
                           ((uint32_t)ADC_APP_MAX_RAW_VALUE / 2U)) /
                          (uint32_t)ADC_APP_MAX_RAW_VALUE;

            (void)my_printf(huart, "%lu,%u,%lu\r\n", (unsigned long)i,
                            (unsigned int)raw, (unsigned long)mv);
        }
        return;
    }

    if (strcmp(argv[1], "stream") == 0)
    {
        if (argc < 3U)
        {
            CLI_WriteLine(huart, "Usage: adc stream on [ms]|off|?");
            return;
        }

        if (strcmp(argv[2], "?") == 0)
        {
            (void)my_printf(huart, "stream=%s, interval_ms=%u, format=raw,mv\r\n",
                            (ADC_APP_GetStreamEnabled() != 0U) ? "on" : "off",
                            (unsigned int)ADC_APP_GetStreamIntervalMs());
            return;
        }

        if (strcmp(argv[2], "on") == 0)
        {
            if (argc >= 4U)
            {
                interval_ms = strtoul(argv[3], &end_ptr, 10);
                if ((*argv[3] == '\0') || (*end_ptr != '\0') ||
                    (interval_ms < (unsigned long)ADC_APP_MIN_STREAM_INTERVAL_MS) ||
                    (interval_ms > 65535UL))
                {
                    CLI_WriteLine(huart, "Usage: adc stream on [ms], ms=2..65535");
                    return;
                }

                if (ADC_APP_SetStreamIntervalMs((uint16_t)interval_ms) == 0U)
                {
                    CLI_WriteLine(huart, "Usage: adc stream on [ms], ms=2..65535");
                    return;
                }
            }

            if (ADC_APP_IsStarted() == 0U)
            {
                CLI_WriteLine(huart, "ADC1 is not running");
                return;
            }

            ADC_APP_SetStreamEnabled(1U);
            (void)my_printf(huart,
                            "ADC stream started: interval_ms=%u, format=raw,mv\r\n",
                            (unsigned int)ADC_APP_GetStreamIntervalMs());
            return;
        }

        if (strcmp(argv[2], "off") == 0)
        {
            ADC_APP_SetStreamEnabled(0U);
            CLI_WriteLine(huart, "ADC stream stopped");
            return;
        }

        CLI_WriteLine(huart, "Usage: adc stream on [ms]|off|?");
        return;
    }

    if (strcmp(argv[1], "block") == 0)
    {
        if (argc < 3U)
        {
            CLI_WriteLine(huart, "Usage: adc block on|off|?");
            return;
        }

        if (strcmp(argv[2], "?") == 0)
        {
            (void)my_printf(huart,
                            "block_stream=%s, block_samples=%u, half_events=%lu, full_events=%lu, dropped_blocks=%lu\r\n",
                            (ADC_APP_GetBlockStreamEnabled() != 0U) ? "on" : "off",
                            (unsigned int)ADC_APP_GetBlockSamples(),
                            (unsigned long)ADC_APP_GetHalfEventCount(),
                            (unsigned long)ADC_APP_GetFullEventCount(),
                            (unsigned long)ADC_APP_GetDroppedBlockCount());
            return;
        }

        if (strcmp(argv[2], "on") == 0)
        {
            ADC_APP_SetBlockStreamEnabled(1U);
            CLI_WriteLine(huart, "ADC block stream started");
            return;
        }

        if (strcmp(argv[2], "off") == 0)
        {
            ADC_APP_SetBlockStreamEnabled(0U);
            CLI_WriteLine(huart, "ADC block stream stopped");
            return;
        }

        CLI_WriteLine(huart, "Usage: adc block on|off|?");
        return;
    }

    if (strcmp(argv[1], "next") == 0)
    {
        if (ADC_APP_PopBlock(&block) == 0U)
        {
            CLI_WriteLine(huart, "No ADC block ready");
            return;
        }

        (void)my_printf(huart,
                        "# block_seq=%lu, part=%s, sample_rate_hz=%lu, samples=%u, format=raw\r\n",
                        (unsigned long)block.seq,
                        (block.part == ADC_APP_BLOCK_PART_HALF) ? "half" : "full",
                        (unsigned long)ADC_APP_GetSampleRateHz(),
                        (unsigned int)ADC_APP_GetBlockSamples());
        for (uint32_t i = 0U; i < ADC_APP_BLOCK_SAMPLES; ++i)
        {
            (void)my_printf(huart, "%u\r\n", (unsigned int)block.samples[i]);
        }
        return;
    }

    CLI_WriteLine(huart, "Usage: adc get|raw|mv|avg|rate <hz|?>|frame|frame ?|stream on [ms]|stream off|stream ?|block on|off|?|next|help");
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
