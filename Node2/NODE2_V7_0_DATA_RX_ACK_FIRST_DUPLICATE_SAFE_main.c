#include "main.h"
#include "usart.h"
#include "gpio.h"

#include <string.h>
#include <stdio.h>

extern UART_HandleTypeDef huart2;   // LoRa
extern UART_HandleTypeDef huart4;   // Debug

void SystemClock_Config(void);

#define MAX_PAYLOAD      32U
#define CHUNK_DELAY_MS   500U

static void dbg_print(const char *s)
{
    HAL_UART_Transmit(&huart4, (uint8_t *)s, strlen(s), HAL_MAX_DELAY);
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

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_USART2_UART_Init();   // LoRa
    MX_USART4_UART_Init();   // Debug

    HAL_Delay(1000);

    dbg_print("\r\nNODE 2 V7.0 DATA RX + ACK FIRST + DUPLICATE SAFE READY\r\n");

    uint8_t b;
    uint8_t state = 0;

    uint8_t seq_lo = 0, seq_hi = 0;
    uint16_t sequence = 0;

    uint8_t len_lo = 0, len_hi = 0;
    uint16_t length = 0;

    char payload[MAX_PAYLOAD + 1];
    uint16_t index = 0;

    uint8_t checksum_lo = 0, checksum_hi = 0;
    uint16_t rx_checksum = 0;
    uint16_t calc_checksum = 0;

    uint8_t have_last_sequence = 0U;
    uint16_t last_sequence = 0U;

    while (1)
    {
        if (HAL_UART_Receive(&huart2, &b, 1, 5000) != HAL_OK)
        {
            continue;
        }

        switch (state)
        {
            case 0:
                if (b == 0xA5)
                {
                    state = 1;
                }
                break;

            case 1:
                if (b == 0x5A)
                {
                    state = 2;
                }
                else if (b == 0xA5)
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
                sequence = ((uint16_t)seq_hi << 8) | seq_lo;
                state = 4;
                break;

            case 4:
                len_lo = b;
                state = 5;
                break;

            case 5:
                len_hi = b;
                length = ((uint16_t)len_hi << 8) | len_lo;

                if (length == 0U || length > MAX_PAYLOAD)
                {
                    dbg_print("BAD LENGTH - RESYNC\r\n");
                    state = 0;
                    index = 0;
                }
                else
                {
                    index = 0;
                    state = 6;
                }
                break;

            case 6:
                payload[index++] = (char)b;

                if (index >= length)
                {
                    state = 7;
                }
                break;

            case 7:
                checksum_lo = b;
                state = 8;
                break;

            case 8:
            {
                checksum_hi = b;
                rx_checksum = ((uint16_t)checksum_hi << 8) | checksum_lo;

                calc_checksum = 0;
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
                        sprintf(msg,
                                "\r\nDUPLICATE SEQ=%lu - ACK RESENT\r\n\r\n",
                                (unsigned long)sequence);
                        dbg_print(msg);
                    }
                    else
                    {
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

                    sprintf(msg,
                            "\r\nCHECKSUM FAIL SEQ=%lu calc=%lu rx=%lu\r\n\r\n",
                            (unsigned long)sequence,
                            (unsigned long)calc_checksum,
                            (unsigned long)rx_checksum);

                    dbg_print(msg);
                }

                state = 0;
                index = 0;
                break;
            }

            default:
                state = 0;
                index = 0;
                break;
        }
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
