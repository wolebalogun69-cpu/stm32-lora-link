#include "main.h"
#include "usart.h"
#include "gpio.h"

#include <string.h>
#include <stdio.h>

extern UART_HandleTypeDef huart1;   // Debug
extern UART_HandleTypeDef huart2;   // LoRa

void SystemClock_Config(void);

#define CHUNK_SIZE_BYTES      2U
#define CHUNK_DELAY_MS        500U
#define ACK_TIMEOUT_MS        12000U
#define MAX_RETRIES           3U

static void dbg_print(const char *s)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)s, strlen(s), HAL_MAX_DELAY);
}

static uint16_t calc_data_checksum(uint16_t seq, uint16_t len, const char *payload)
{
    uint16_t checksum = 0;

    checksum += (seq & 0xFF);
    checksum += ((seq >> 8) & 0xFF);
    checksum += (len & 0xFF);
    checksum += ((len >> 8) & 0xFF);

    for (uint16_t i = 0; i < len; i++)
    {
        checksum += (uint8_t)payload[i];
    }

    return checksum;
}

static void send_pair(uint8_t a, uint8_t b)
{
    uint8_t pair[CHUNK_SIZE_BYTES] = {a, b};
    HAL_UART_Transmit(&huart2, pair, CHUNK_SIZE_BYTES, HAL_MAX_DELAY);
    HAL_Delay(CHUNK_DELAY_MS);
}

static void send_data_frame(uint16_t seq)
{
    const char msg[] = "HELLO123ABCD";
    uint16_t len = 12U;
    uint16_t checksum = calc_data_checksum(seq, len, msg);

    send_pair(0xA5, 0x5A);
    send_pair(seq & 0xFF, (seq >> 8) & 0xFF);
    send_pair(len & 0xFF, (len >> 8) & 0xFF);

    for (uint16_t i = 0; i < len; i += 2U)
    {
        send_pair((uint8_t)msg[i], (uint8_t)msg[i + 1U]);
    }

    send_pair(checksum & 0xFF, (checksum >> 8) & 0xFF);
}

static uint8_t wait_for_ack(uint16_t expected_seq)
{
    uint8_t b;
    uint8_t state = 0;
    uint8_t seq_lo = 0;
    uint8_t seq_hi = 0;
    uint16_t seq = 0;
    uint32_t start = HAL_GetTick();

    while ((HAL_GetTick() - start) < ACK_TIMEOUT_MS)
    {
        if (HAL_UART_Receive(&huart2, &b, 1, 1000) != HAL_OK)
        {
            continue;
        }

        {
            char rx_msg[24];
            sprintf(rx_msg, "RX:%02X\r\n", b);
            dbg_print(rx_msg);
        }

        switch (state)
        {
            case 0:
                if (b == 0xB6)
                {
                    state = 1;
                }
                break;

            case 1:
                if (b == 0x6B)
                {
                    state = 2;
                }
                else if (b == 0xB6)
                {
                    state = 1;
                }
                else
                {
                    state = 0;
                }
                break;

            case 2:
                seq_lo = b;
                state = 3;
                break;

            case 3:
                seq_hi = b;
                seq = ((uint16_t)seq_hi << 8) | seq_lo;

                if (seq == expected_seq)
                {
                    return 1U;
                }

                {
                    char msg[80];
                    sprintf(msg,
                            "ACK SEQ MISMATCH expected=%lu got=%lu\r\n",
                            (unsigned long)expected_seq,
                            (unsigned long)seq);
                    dbg_print(msg);
                }

                state = 0;
                break;

            default:
                state = 0;
                break;
        }
    }

    return 0U;
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_USART1_UART_Init();   // Debug
    MX_USART2_UART_Init();   // LoRa

    HAL_Delay(6000);

    dbg_print("\r\nNODE 1 V6.9 RELIABLE TX + ACK RAW DEBUG READY\r\n");

    uint16_t seq = 1U;

    while (1)
    {
        uint8_t ack_ok = 0U;

        for (uint8_t attempt = 1U; attempt <= MAX_RETRIES; attempt++)
        {
            char msg[100];

            sprintf(msg,
                    "\r\nTX SEQ=%lu ATTEMPT=%u\r\n",
                    (unsigned long)seq,
                    (unsigned int)attempt);
            dbg_print(msg);

            send_data_frame(seq);

            if (wait_for_ack(seq))
            {
                sprintf(msg, "ACK OK SEQ=%lu\r\n", (unsigned long)seq);
                dbg_print(msg);
                ack_ok = 1U;
                break;
            }

            sprintf(msg,
                    "ACK TIMEOUT SEQ=%lu ATTEMPT=%u\r\n",
                    (unsigned long)seq,
                    (unsigned int)attempt);
            dbg_print(msg);
        }

        if (ack_ok)
        {
            seq++;
        }
        else
        {
            char msg[80];
            sprintf(msg, "FRAME FAILED SEQ=%lu\r\n", (unsigned long)seq);
            dbg_print(msg);
        }

        HAL_Delay(2000);
    }
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PREDIV = RCC_PREDIV_DIV1;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL6;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType =
        RCC_CLOCKTYPE_HCLK |
        RCC_CLOCKTYPE_SYSCLK |
        RCC_CLOCKTYPE_PCLK1;

    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
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

