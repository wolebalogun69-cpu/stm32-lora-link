#include "main.h"
#include "usart.h"
#include "gpio.h"

#include <string.h>
#include <stdio.h>

extern UART_HandleTypeDef huart2;   // LoRa
extern UART_HandleTypeDef huart4;   // Debug

void SystemClock_Config(void);

#define MAX_PAYLOAD              32U
#define CHUNK_DELAY_MS           500U
#define RX_RING_SIZE             256U
#define PARSER_TIMEOUT_MS        3000U
#define DIAG_REPORT_INTERVAL_MS  10000U

static volatile uint8_t rx_ring[RX_RING_SIZE];
static volatile uint16_t rx_head = 0U;
static volatile uint16_t rx_tail = 0U;
static volatile uint32_t rx_overflow_count = 0U;
static volatile uint32_t uart_error_count = 0U;
static uint8_t lora_rx_byte = 0U;

static uint32_t frames_ok_count = 0U;
static uint32_t checksum_fail_count = 0U;
static uint32_t bad_length_count = 0U;
static uint32_t duplicate_count = 0U;
static uint32_t parser_timeout_count = 0U;

static void dbg_print(const char *s)
{
    HAL_UART_Transmit(&huart4, (uint8_t *)s, strlen(s), HAL_MAX_DELAY);
}

static void rx_ring_push_from_isr(uint8_t b)
{
    uint16_t next_head = (uint16_t)((rx_head + 1U) % RX_RING_SIZE);

    if (next_head == rx_tail)
    {
        rx_overflow_count++;
        return;
    }

    rx_ring[rx_head] = b;
    rx_head = next_head;
}

static uint8_t rx_ring_pop(uint8_t *b)
{
    if (rx_head == rx_tail)
    {
        return 0U;
    }

    *b = rx_ring[rx_tail];
    rx_tail = (uint16_t)((rx_tail + 1U) % RX_RING_SIZE);

    return 1U;
}

static void start_lora_rx_it(void)
{
    if (HAL_UART_Receive_IT(&huart2, &lora_rx_byte, 1U) != HAL_OK)
    {
        Error_Handler();
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        rx_ring_push_from_isr(lora_rx_byte);
        (void)HAL_UART_Receive_IT(&huart2, &lora_rx_byte, 1U);
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        uart_error_count++;
        (void)HAL_UART_Receive_IT(&huart2, &lora_rx_byte, 1U);
    }
}

static void send_pair(uint8_t a, uint8_t b)
{
    uint8_t pair[2] = {a, b};
    HAL_UART_Transmit(&huart2, pair, 2, HAL_MAX_DELAY);
    HAL_Delay(CHUNK_DELAY_MS);
}

static void send_ack(uint16_t seq)
{
    send_pair(0xB6, 0x6B);
    send_pair(seq & 0xFF, (seq >> 8) & 0xFF);
}

static void reset_parser(uint8_t *state, uint16_t *index, uint32_t *parser_started_at)
{
    *state = 0U;
    *index = 0U;
    *parser_started_at = 0U;
}

static void mark_parser_active(uint8_t state, uint32_t *parser_started_at)
{
    if (state != 0U && *parser_started_at == 0U)
    {
        *parser_started_at = HAL_GetTick();
    }
}

static void report_diagnostics(void)
{
    char msg[220];

    sprintf(msg,
            "\r\nDIAG ok=%lu checksum_fail=%lu bad_len=%lu dup=%lu parser_timeout=%lu ring_overflow=%lu uart_error=%lu\r\n",
            (unsigned long)frames_ok_count,
            (unsigned long)checksum_fail_count,
            (unsigned long)bad_length_count,
            (unsigned long)duplicate_count,
            (unsigned long)parser_timeout_count,
            (unsigned long)rx_overflow_count,
            (unsigned long)uart_error_count);
    dbg_print(msg);
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_USART2_UART_Init();   // LoRa
    MX_USART4_UART_Init();   // Debug

    HAL_Delay(1000);

    start_lora_rx_it();

    dbg_print("\r\nNODE 2 V8.1 IRQ RING RX DIAGNOSTIC HARDENED READY\r\n");

    uint8_t b;
    uint8_t state = 0U;
    uint32_t parser_started_at = 0U;
    uint32_t last_diag_report = HAL_GetTick();

    uint8_t seq_lo = 0U, seq_hi = 0U;
    uint16_t sequence = 0U;

    uint8_t len_lo = 0U, len_hi = 0U;
    uint16_t length = 0U;

    char payload[MAX_PAYLOAD + 1U];
    uint16_t index = 0U;

    uint8_t checksum_lo = 0U, checksum_hi = 0U;
    uint16_t rx_checksum = 0U;
    uint16_t calc_checksum = 0U;

    uint8_t have_last_sequence = 0U;
    uint16_t last_sequence = 0U;

    while (1)
    {
        if (state != 0U &&
            parser_started_at != 0U &&
            (HAL_GetTick() - parser_started_at) > PARSER_TIMEOUT_MS)
        {
            parser_timeout_count++;
            dbg_print("PARSER TIMEOUT - RESYNC\r\n");
            reset_parser(&state, &index, &parser_started_at);
        }

        while (rx_ring_pop(&b))
        {
            switch (state)
            {
                case 0:
                    if (b == 0xA5)
                    {
                        state = 1U;
                        parser_started_at = HAL_GetTick();
                    }
                    break;

                case 1:
                    if (b == 0x5A)
                    {
                        state = 2U;
                    }
                    else if (b == 0xA5)
                    {
                        state = 1U;
                        parser_started_at = HAL_GetTick();
                    }
                    else
                    {
                        reset_parser(&state, &index, &parser_started_at);
                    }
                    break;

                case 2:
                    seq_lo = b;
                    state = 3U;
                    break;

                case 3:
                    seq_hi = b;
                    sequence = ((uint16_t)seq_hi << 8) | seq_lo;
                    state = 4U;
                    break;

                case 4:
                    len_lo = b;
                    state = 5U;
                    break;

                case 5:
                    len_hi = b;
                    length = ((uint16_t)len_hi << 8) | len_lo;

                    if (length == 0U || length > MAX_PAYLOAD)
                    {
                        bad_length_count++;
                        dbg_print("BAD LENGTH - RESYNC\r\n");
                        reset_parser(&state, &index, &parser_started_at);
                    }
                    else
                    {
                        index = 0U;
                        state = 6U;
                    }
                    break;

                case 6:
                    payload[index++] = (char)b;

                    if (index >= length)
                    {
                        state = 7U;
                    }
                    break;

                case 7:
                    checksum_lo = b;
                    state = 8U;
                    break;

                case 8:
                {
                    checksum_hi = b;
                    rx_checksum = ((uint16_t)checksum_hi << 8) | checksum_lo;

                    calc_checksum = 0U;
                    calc_checksum += (sequence & 0xFF);
                    calc_checksum += ((sequence >> 8) & 0xFF);
                    calc_checksum += (length & 0xFF);
                    calc_checksum += ((length >> 8) & 0xFF);

                    for (uint16_t i = 0; i < length; i++)
                    {
                        calc_checksum += (uint8_t)payload[i];
                    }

                    payload[length] = '\0';

                    if (calc_checksum == rx_checksum)
                    {
                        char msg[120];
                        uint8_t is_duplicate =
                            (have_last_sequence && (sequence == last_sequence));

                        send_ack(sequence);

                        if (is_duplicate)
                        {
                            duplicate_count++;
                            sprintf(msg,
                                    "\r\nDUPLICATE SEQ=%lu - ACK RESENT\r\n\r\n",
                                    (unsigned long)sequence);
                            dbg_print(msg);
                        }
                        else
                        {
                            frames_ok_count++;
                            last_sequence = sequence;
                            have_last_sequence = 1U;

                            sprintf(msg, "\r\nFRAME OK SEQ=%lu\r\n",
                                    (unsigned long)sequence);
                            dbg_print(msg);

                            dbg_print("MESSAGE: ");
                            dbg_print(payload);
                            dbg_print("\r\nACK SENT\r\n\r\n");
                        }
                    }
                    else
                    {
                        char msg[120];

                        checksum_fail_count++;
                        sprintf(msg,
                                "\r\nCHECKSUM FAIL SEQ=%lu calc=%lu rx=%lu\r\n\r\n",
                                (unsigned long)sequence,
                                (unsigned long)calc_checksum,
                                (unsigned long)rx_checksum);

                        dbg_print(msg);
                    }

                    reset_parser(&state, &index, &parser_started_at);
                    break;
                }

                default:
                    reset_parser(&state, &index, &parser_started_at);
                    break;
            }

            mark_parser_active(state, &parser_started_at);
        }

        if ((HAL_GetTick() - last_diag_report) >= DIAG_REPORT_INTERVAL_MS)
        {
            last_diag_report = HAL_GetTick();
            report_diagnostics();
        }

        HAL_Delay(1);
    }
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
    RCC_OscInitStruct.MSIState = RCC_MSI_ON;
    RCC_OscInitStruct.MSICalibrationValue = 0;
    RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_5;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType =
        RCC_CLOCKTYPE_HCLK |
        RCC_CLOCKTYPE_SYSCLK |
        RCC_CLOCKTYPE_PCLK1 |
        RCC_CLOCKTYPE_PCLK2;

    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
    {
        Error_Handler();
    }
}

void Error_Handler(void)
{
    __disable_irq();

    while (1)
    {
    }
}

