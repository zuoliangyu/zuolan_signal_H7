#include "cli.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "adc.h"
#include "adc_app.h"
#include "dac_app.h"
#include "dsp.h"
#include "dsp_pipeline.h"
#include "led.h"
#include "tim.h"
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

    (void)UART_WriteAsync(huart, (const uint8_t *)text, (uint16_t)strlen(text));
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
static void CLI_CmdFft(UART_HandleTypeDef *huart, uint8_t argc, char *argv[]);
static void CLI_CmdFftDump(UART_HandleTypeDef *huart, uint8_t argc, char *argv[]);
static void CLI_CmdFilter(UART_HandleTypeDef *huart, uint8_t argc, char *argv[]);
static void CLI_CmdPipeline(UART_HandleTypeDef *huart, uint8_t argc, char *argv[]);
static void CLI_CmdPipelineWindowTest(UART_HandleTypeDef *huart);
static void CLI_CmdDacRate(UART_HandleTypeDef *huart, uint8_t argc, char *argv[]);
static void CLI_CmdAdcRate(UART_HandleTypeDef *huart, uint8_t argc, char *argv[]);
static void CLI_CmdAdcDump(UART_HandleTypeDef *huart, uint8_t argc, char *argv[]);
static void CLI_CmdUartTx(UART_HandleTypeDef *huart, uint8_t argc, char *argv[]);
static void CLI_CmdReport(UART_HandleTypeDef *huart, uint8_t argc, char *argv[]);
static void CLI_CmdClearStats(UART_HandleTypeDef *huart, uint8_t argc, char *argv[]);

static const cli_command_t s_cli_commands[] = {
    {"help", "List all commands", CLI_CmdHelp},
    {"echo", "Echo parameters: echo <text>", CLI_CmdEcho},
    {"led", "Control LED: led on/off/toggle/blink", CLI_CmdLed},
    {"adc", "ADC: get/raw/mv/avg/rate/frame/stream/block", CLI_CmdAdc},
    {"dac", "Control DAC: dac get/mode/amp/offset/freq/duty/start/stop/test", CLI_CmdDac},
    {"fft", "FFT self-test: fft selftest", CLI_CmdFft},
    {"fftdump", "Dump latest pipeline mag spectrum as CSV (oneshot capture)", CLI_CmdFftDump},
    {"filter", "Filter self-test: filter selftest", CLI_CmdFilter},
    {"pipeline", "ADC->filter->FFT pipeline: pipeline status/filter/window [test]/fft/dc/rate/run/stream/selftest", CLI_CmdPipeline},
    {"dacrate", "Measure DAC actual output Hz over 1 second", CLI_CmdDacRate},
    {"adcrate", "Measure ADC actual sample rate over 1 second", CLI_CmdAdcRate},
    {"adcdump", "Dump TIM2 register state for ADC trigger debug", CLI_CmdAdcDump},
    {"uarttx", "Show UART async TX ring buffer stats", CLI_CmdUartTx},
    {"report", "Pause pipeline and dump all health metrics in one go", CLI_CmdReport},
    {"clearstats", "Reset ADC event/drop counters and DAC DMA counter", CLI_CmdClearStats},
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
        CLI_WriteLine(huart, "  dac test         - quick: 1V/1.65V/1kHz sine + start (pipeline test preset)");
        return;
    }

    if (strcmp(argv[1], "test") == 0)
    {
        DAC_APP_Stop();
        (void)DAC_APP_SetAmpMv(1000U);
        (void)DAC_APP_SetOffsetMv(1650U);
        (void)DAC_APP_SetFreqHz(1000U);
        DAC_APP_SetMode(DAC_APP_MODE_SINE);
        DAC_APP_Start();
        (void)my_printf(huart,
            "DAC test preset applied: sine 1 kHz, amp=1000 mV, offset=1650 mV, started\r\n");
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

static void CLI_CmdFft(UART_HandleTypeDef *huart, uint8_t argc, char *argv[])
{
    if ((argc >= 2U) && (strcmp(argv[1], "selftest") == 0))
    {
        DSP_SelfTest(huart);
        return;
    }

    CLI_WriteLine(huart, "Usage: fft selftest");
}

// 同步发送 helper：vsnprintf 到本地 buffer，再 HAL_UART_Transmit 阻塞发送。
// 绕开 my_printf 的 ring buffer，绝不丢字节。代价：调用方阻塞到字节全部发出。
// fftdump 是一次性 dump 大量数据，阻塞模式最稳。
static void cli_fftdump_sync_printf(UART_HandleTypeDef *huart, const char *fmt, ...)
{
    char buf[96];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (len <= 0) { return; }
    if (len >= (int)sizeof(buf)) { len = (int)sizeof(buf) - 1; }
    (void)HAL_UART_Transmit(huart, (uint8_t *)buf, (uint16_t)len, 0xFFFFU);
}

// 触发一次 oneshot，把当前 pipeline 配置（filter/window/fft）下的整段 magnitude
// 谱以 CSV 格式 dump 到串口。每行：bin,freq_hz,mag。
// 全程用 HAL_UART_Transmit 阻塞同步发送，避免 ring buffer 丢字节。
// 进入前会先等 TX ring 排空，让前面的异步内容（如命令回显）先发完。
static void CLI_CmdFftDump(UART_HandleTypeDef *huart, uint8_t argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (ADC_APP_IsStarted() == 0U)
    {
        CLI_WriteLine(huart, "ADC not running");
        return;
    }

    // 静默触发一帧
    dsp_pipeline_result_t r;
    int rc = DSP_Pipeline_RunOneshotBlocking(2000U, 1U, &r);
    if (rc != 0)
    {
        (void)my_printf(huart, "fftdump: oneshot failed rc=%d\r\n", rc);
        return;
    }

    uint32_t mag_len = 0U;
    const float32_t *mag = DSP_Pipeline_GetLastMag(&mag_len);
    if ((mag == NULL) || (mag_len == 0U))
    {
        CLI_WriteLine(huart, "fftdump: no mag buffer");
        return;
    }

    const float32_t bin_hz = (float32_t)r.fs_hz / (float32_t)r.fft_size;

    // 切到阻塞同步模式之前先等 TX ring 排空（让命令回显之类的异步内容先发完）。
    // 否则 HAL_UART_Transmit 会跟正在跑的 DMA 抢 huart->gState。
    while (UART_GetTxRingFreeBytes(UART_PORT_1) < (UART_TX_BUF_SIZE - 1U))
    {
        // busy-wait
    }

    cli_fftdump_sync_printf(huart,
        "# fftdump fft=%lu fs=%lu Hz window=%s seq=%lu bin_hz=%.4f peak_bin=%lu peak_freq=%.2f\r\n",
        (unsigned long)r.fft_size,
        (unsigned long)r.fs_hz,
        DSP_Window_Name(r.window),
        (unsigned long)r.frame_seq,
        (double)bin_hz,
        (unsigned long)r.peak_bin,
        (double)r.peak_freq_hz);
    cli_fftdump_sync_printf(huart, "bin,freq_hz,mag\r\n");

    for (uint32_t i = 0U; i < mag_len; i++)
    {
        cli_fftdump_sync_printf(huart, "%lu,%.2f,%.2f\r\n",
                                (unsigned long)i,
                                (double)((float32_t)i * bin_hz),
                                (double)mag[i]);
    }

    cli_fftdump_sync_printf(huart, "# fftdump end\r\n");
}

static void CLI_CmdFilter(UART_HandleTypeDef *huart, uint8_t argc, char *argv[])
{
    if ((argc >= 2U) && (strcmp(argv[1], "selftest") == 0))
    {
        DSP_Filter_SelfTest(huart);
        return;
    }

    CLI_WriteLine(huart, "Usage: filter selftest");
}

static void CLI_CmdPipeline(UART_HandleTypeDef *huart, uint8_t argc, char *argv[])
{
    if (argc < 2U)
    {
        CLI_WriteLine(huart, "Usage: pipeline status|filter|window|fft|dc|rate|run|stream|selftest");
        return;
    }

    if (strcmp(argv[1], "status") == 0)
    {
        DSP_Pipeline_PrintStatus(huart);
        return;
    }

    if (strcmp(argv[1], "filter") == 0)
    {
        if ((argc < 3U) || (strcmp(argv[2], "?") == 0))
        {
            DSP_Pipeline_PrintFilters(huart);
            return;
        }
        if (DSP_Pipeline_SetFilterByName(argv[2]) != 0)
        {
            (void)my_printf(huart, "Unknown filter: %s\r\n", argv[2]);
            return;
        }
        (void)my_printf(huart, "filter -> %s\r\n", argv[2]);
        return;
    }

    if (strcmp(argv[1], "window") == 0)
    {
        if ((argc < 3U) || (strcmp(argv[2], "?") == 0))
        {
            DSP_Pipeline_PrintWindows(huart);
            return;
        }
        if (strcmp(argv[2], "test") == 0)
        {
            CLI_CmdPipelineWindowTest(huart);
            return;
        }
        if (DSP_Pipeline_SetWindowByName(argv[2]) != 0)
        {
            (void)my_printf(huart, "Unknown window: %s\r\n", argv[2]);
            return;
        }
        (void)my_printf(huart, "window -> %s\r\n", argv[2]);
        return;
    }

    if (strcmp(argv[1], "fft") == 0)
    {
        if ((argc < 3U) || (strcmp(argv[2], "?") == 0))
        {
            (void)my_printf(huart, "fft = %lu (allowed: 256/512/1024)\r\n",
                            (unsigned long)DSP_Pipeline_GetFftLen());
            return;
        }
        char *end_ptr;
        unsigned long len = strtoul(argv[2], &end_ptr, 10);
        if ((*end_ptr != '\0') || (DSP_Pipeline_SetFftLen((uint32_t)len) != 0))
        {
            CLI_WriteLine(huart, "Usage: pipeline fft 256|512|1024|?");
            return;
        }
        (void)my_printf(huart, "fft -> %lu\r\n", len);
        return;
    }

    if (strcmp(argv[1], "rate") == 0)
    {
        if ((argc < 3U) || (strcmp(argv[2], "?") == 0))
        {
            (void)my_printf(huart, "rate = %u (every N frames printed; 1=every frame)\r\n",
                            (unsigned)DSP_Pipeline_GetOutputRate());
            return;
        }
        char *end_ptr;
        unsigned long n = strtoul(argv[2], &end_ptr, 10);
        if ((*end_ptr != '\0') || (DSP_Pipeline_SetOutputRate((uint16_t)n) != 0))
        {
            CLI_WriteLine(huart, "Usage: pipeline rate <1..1000>|?");
            return;
        }
        (void)my_printf(huart, "rate -> %lu\r\n", n);
        return;
    }

    if (strcmp(argv[1], "dc") == 0)
    {
        if ((argc < 3U) || (strcmp(argv[2], "?") == 0))
        {
            (void)my_printf(huart, "dc = %s\r\n",
                            (DSP_Pipeline_GetDcRemove() != 0U) ? "on" : "off");
            return;
        }
        if (strcmp(argv[2], "on") == 0)        { DSP_Pipeline_SetDcRemove(1U); }
        else if (strcmp(argv[2], "off") == 0)  { DSP_Pipeline_SetDcRemove(0U); }
        else { CLI_WriteLine(huart, "Usage: pipeline dc on|off|?"); return; }
        (void)my_printf(huart, "dc -> %s\r\n", argv[2]);
        return;
    }

    if (strcmp(argv[1], "run") == 0)
    {
        if (DSP_Pipeline_RunOneshot() != 0)
        {
            CLI_WriteLine(huart, "ADC not running");
            return;
        }
        CLI_WriteLine(huart, "pipeline oneshot armed");
        return;
    }

    if (strcmp(argv[1], "selftest") == 0)
    {
        (void)DSP_Pipeline_SelfTest(huart);
        return;
    }

    if (strcmp(argv[1], "stream") == 0)
    {
        if (argc < 3U)
        {
            CLI_WriteLine(huart, "Usage: pipeline stream on|off");
            return;
        }
        if (strcmp(argv[2], "on") == 0)
        {
            if (DSP_Pipeline_SetStream(1U) != 0)
            {
                CLI_WriteLine(huart, "ADC not running");
                return;
            }
            CLI_WriteLine(huart, "pipeline stream started");
            return;
        }
        if (strcmp(argv[2], "off") == 0)
        {
            (void)DSP_Pipeline_SetStream(0U);
            CLI_WriteLine(huart, "pipeline stream stopped");
            return;
        }
        CLI_WriteLine(huart, "Usage: pipeline stream on|off");
        return;
    }

    CLI_WriteLine(huart, "Usage: pipeline status|filter|window|fft|dc|rate|run|stream|selftest");
}

// 自动测试：依次跑 none/hann/hamming/blackman 各一次 oneshot，
// 统计 peak_bin / mag / amp 并出 PASS/FAIL 判定。
// 使用前提：DAC 已输出已知正弦（推荐 dac test），杜邦线把 PA4 接到 PA0。
static void CLI_CmdPipelineWindowTest(UART_HandleTypeDef *huart)
{
    static const char *const k_windows[] = {"none", "hann", "hamming", "blackman"};
    static const uint32_t     k_count    = (uint32_t)(sizeof(k_windows) / sizeof(k_windows[0]));

    if (ADC_APP_IsStarted() == 0U)
    {
        CLI_WriteLine(huart, "ADC not running");
        return;
    }

    // 强制把 stream 关掉，避免抢资源
    (void)DSP_Pipeline_SetStream(0U);

    // 保存现场，结束后恢复
    dsp_window_type_t prev_window = DSP_Pipeline_GetWindow();

    dsp_pipeline_result_t results[4];
    (void)memset(results, 0, sizeof(results));

    CLI_WriteLine(huart, "[WINDOW-TEST] warmup ...");

    // warmup：丢 2 帧，让 DAC / ADC / DMA 队列都稳定下来。
    // 否则刚 dac test 之后立刻测试，第一帧会采到 DAC 启动瞬态信号。
    (void)DSP_Pipeline_SetWindowByName("none");
    for (uint32_t w = 0U; w < 2U; w++) {
        dsp_pipeline_result_t discard;
        (void)DSP_Pipeline_RunOneshotBlocking(2000U, 1U, &discard);
    }

    CLI_WriteLine(huart, "[WINDOW-TEST] running 4 windows x 1 oneshot each ...");

    for (uint32_t i = 0U; i < k_count; i++)
    {
        if (DSP_Pipeline_SetWindowByName(k_windows[i]) != 0)
        {
            (void)my_printf(huart, "  %-9s: window unsupported\r\n", k_windows[i]);
            continue;
        }
        int rc = DSP_Pipeline_RunOneshotBlocking(2000U, 1U, &results[i]);
        if (rc == -2)
        {
            (void)my_printf(huart, "  %-9s: timeout\r\n", k_windows[i]);
        }
        else if (rc != 0)
        {
            (void)my_printf(huart, "  %-9s: error rc=%d\r\n", k_windows[i], rc);
        }
    }

    // 还原窗
    (void)DSP_Pipeline_SetWindowByName(DSP_Window_Name(prev_window));

    // ---- 打印对比表 ----
    CLI_WriteLine(huart, "");
    CLI_WriteLine(huart, "  window     cgain  peak_bin  peak_freq      mag           amp           amp/none");
    CLI_WriteLine(huart, "  ---------  -----  --------  -------------  ------------  ------------  --------");

    float32_t amp_none = (results[0].valid != 0U) ? results[0].peak_amp : 0.0f;
    if (amp_none < 1.0f) { amp_none = 1.0f; }

    for (uint32_t i = 0U; i < k_count; i++)
    {
        if (results[i].valid == 0U)
        {
            (void)my_printf(huart, "  %-9s  (no result)\r\n", k_windows[i]);
            continue;
        }
        (void)my_printf(huart,
            "  %-9s  %.3f  %8lu  %9.2f Hz  %12.0f  %12.0f  %.3f\r\n",
            k_windows[i],
            (double)results[i].cgain,
            (unsigned long)results[i].peak_bin,
            (double)results[i].peak_freq_hz,
            (double)results[i].peak_mag,
            (double)results[i].peak_amp,
            (double)(results[i].peak_amp / amp_none));
    }

    // ---- 判定：peak_bin 一致（±2）+ amp 在 ±15% ----
    if (results[0].valid == 0U)
    {
        CLI_WriteLine(huart, "  verdict: SKIP (no baseline result for 'none')");
        return;
    }

    uint32_t base_bin = results[0].peak_bin;
    uint8_t  pass     = 1U;
    const char *fail_reason = "";

    for (uint32_t i = 1U; i < k_count; i++)
    {
        if (results[i].valid == 0U) { continue; }

        int32_t db = (int32_t)results[i].peak_bin - (int32_t)base_bin;
        if ((db < -2) || (db > 2))
        {
            pass = 0U;
            fail_reason = "peak_bin shifted";
            break;
        }
        float32_t ratio = results[i].peak_amp / amp_none;
        if ((ratio < 0.85f) || (ratio > 1.15f))
        {
            pass = 0U;
            fail_reason = "amp deviation > 15%";
            break;
        }
    }

    if (pass != 0U)
    {
        (void)my_printf(huart,
            "  verdict: PASS  (peak_bin within +/-2 of base=%lu, amp within +/-15%%)\r\n",
            (unsigned long)base_bin);
    }
    else
    {
        (void)my_printf(huart, "  verdict: FAIL  (%s)\r\n", fail_reason);
    }
}

static void CLI_CmdDacRate(UART_HandleTypeDef *huart, uint8_t argc, char *argv[])
{
    (void)argc;
    (void)argv;
    uint32_t before = DAC_APP_GetDmaFullCount();
    HAL_Delay(1000);
    uint32_t after  = DAC_APP_GetDmaFullCount();
    uint32_t delta  = after - before;
    (void)my_printf(huart,
        "[DAC-RATE] dma_full_callbacks_per_sec=%lu (= DAC output Hz)\r\n",
        (unsigned long)delta);
}

static void CLI_CmdAdcRate(UART_HandleTypeDef *huart, uint8_t argc, char *argv[])
{
    (void)argc;
    (void)argv;
    uint32_t half_before = ADC_APP_GetHalfEventCount();
    uint32_t full_before = ADC_APP_GetFullEventCount();
    HAL_Delay(1000);
    uint32_t half_after  = ADC_APP_GetHalfEventCount();
    uint32_t full_after  = ADC_APP_GetFullEventCount();
    // 每个 half/full 事件对应 ADC_APP_BLOCK_SAMPLES (=128) 个样本
    uint32_t total_events = (half_after - half_before) + (full_after - full_before);
    uint32_t actual_sps   = total_events * 128U;
    uint32_t claimed_sps  = ADC_APP_GetSampleRateHz();
    (void)my_printf(huart,
        "[ADC-RATE] events/sec=%lu actual_sps=%lu claimed_sps=%lu ratio=%.3f\r\n",
        (unsigned long)total_events,
        (unsigned long)actual_sps,
        (unsigned long)claimed_sps,
        (claimed_sps > 0U) ? ((double)actual_sps / (double)claimed_sps) : 0.0);
}

static void CLI_CmdAdcDump(UART_HandleTypeDef *huart, uint8_t argc, char *argv[])
{
    (void)argc;
    (void)argv;
    uint32_t psc = htim2.Instance->PSC;
    uint32_t arr = htim2.Instance->ARR;
    uint32_t cnt = htim2.Instance->CNT;
    uint32_t cr1 = htim2.Instance->CR1;
    uint32_t cr2 = htim2.Instance->CR2;
    uint32_t adc_cr  = hadc1.Instance->CR;
    uint32_t adc_cfgr = hadc1.Instance->CFGR;
    (void)my_printf(huart,
        "[TIM2] PSC=%lu ARR=%lu CNT=%lu CR1=0x%lx CR2=0x%lx\r\n",
        (unsigned long)psc, (unsigned long)arr, (unsigned long)cnt,
        (unsigned long)cr1, (unsigned long)cr2);
    (void)my_printf(huart,
        "[ADC1] CR=0x%lx CFGR=0x%lx (EXTSEL=bits[9:5] of CFGR)\r\n",
        (unsigned long)adc_cr, (unsigned long)adc_cfgr);
    // 用当前硬件 PSC/ARR 估算实际触发频率（假定 timer_clk = HCLK）
    uint32_t timer_clk = HAL_RCC_GetHCLKFreq();
    if ((psc + 1U) * (arr + 1U) > 0U) {
        uint32_t hw_rate = (uint32_t)((uint64_t)timer_clk / ((uint64_t)(psc + 1U) * (uint64_t)(arr + 1U)));
        (void)my_printf(huart,
            "[ADC-CALC] timer_clk=%lu hw_trigger_rate=%lu Hz\r\n",
            (unsigned long)timer_clk, (unsigned long)hw_rate);
    }
}

static void CLI_CmdUartTx(UART_HandleTypeDef *huart, uint8_t argc, char *argv[])
{
    (void)argc;
    (void)argv;
    (void)my_printf(huart,
        "[UART-TX] USART1 dropped=%lu free=%lu/%u USART2 dropped=%lu free=%lu/%u\r\n",
        (unsigned long)UART_GetTxDroppedBytes(UART_PORT_1),
        (unsigned long)UART_GetTxRingFreeBytes(UART_PORT_1),
        (unsigned)UART_TX_BUF_SIZE,
        (unsigned long)UART_GetTxDroppedBytes(UART_PORT_2),
        (unsigned long)UART_GetTxRingFreeBytes(UART_PORT_2),
        (unsigned)UART_TX_BUF_SIZE);
}

static void CLI_CmdReport(UART_HandleTypeDef *huart, uint8_t argc, char *argv[])
{
    (void)argc;
    (void)argv;
    // 1) 暂停 pipeline，让串口安静
    (void)DSP_Pipeline_SetStream(0U);
    // 等待 ~50 ms 让 TX ring 排空
    HAL_Delay(50);
    // 2) 一次性打印所有健康指标
    CLI_WriteLine(huart, "=== HEALTH REPORT ===");
    DSP_Pipeline_PrintStatus(huart);
    (void)my_printf(huart,
        "[ADC] sample_rate=%lu Hz half=%lu full=%lu dropped=%lu queued=%lu\r\n",
        (unsigned long)ADC_APP_GetSampleRateHz(),
        (unsigned long)ADC_APP_GetHalfEventCount(),
        (unsigned long)ADC_APP_GetFullEventCount(),
        (unsigned long)ADC_APP_GetDroppedBlockCount(),
        (unsigned long)(ADC_APP_GetHalfEventCount() + ADC_APP_GetFullEventCount() - ADC_APP_GetDroppedBlockCount()));
    (void)my_printf(huart,
        "[UART-TX] USART1 dropped=%lu free=%lu/%u\r\n",
        (unsigned long)UART_GetTxDroppedBytes(UART_PORT_1),
        (unsigned long)UART_GetTxRingFreeBytes(UART_PORT_1),
        (unsigned)UART_TX_BUF_SIZE);
    (void)my_printf(huart,
        "[DAC] freq=%lu Hz dma_full_cb_count=%lu\r\n",
        (unsigned long)DAC_APP_GetFreqHz(),
        (unsigned long)DAC_APP_GetDmaFullCount());
    CLI_WriteLine(huart, "=== END REPORT ===");
}

static void CLI_CmdClearStats(UART_HandleTypeDef *huart, uint8_t argc, char *argv[])
{
    (void)argc;
    (void)argv;
    ADC_APP_ClearEventStats();
    CLI_WriteLine(huart, "ADC event stats cleared (half/full/dropped = 0)");
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
                (void)UART_WriteAsync(huart, (const uint8_t *)&ch, 1U);
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
