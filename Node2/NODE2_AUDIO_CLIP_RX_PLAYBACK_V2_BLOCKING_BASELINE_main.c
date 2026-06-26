#include "main.h"
#include "dac.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

#include <string.h>
#include <stdio.h>

extern DAC_HandleTypeDef hdac;
extern TIM_HandleTypeDef htim2;
extern UART_HandleTypeDef huart2;   // LoRa
extern UART_HandleTypeDef huart4;   // Debug

void SystemClock_Config(void);

#define MAX_PAYLOAD          32U
#define CHUNK_DELAY_MS       500U

#define AUDIO_MSG_START      0x30U
#define AUDIO_MSG_DATA       0x31U
#define AUDIO_MSG_END        0x32U
#define AUDIO_BUFFER_BYTES   256U
#define AUDIO_PLAY_REPEATS   32U

static uint8_t audio_buffer[AUDIO_BUFFER_BYTES];
static uint16_t audio_expected_len = 0U;
static uint16_t audio_received_len = 0U;
static uint8_t audio_transfer_active = 0U;

static volatile uint8_t audio_playing = 0U;
static volatile uint16_t audio_play_len = 0U;
static volatile uint16_t audio_play_index = 0U;
static volatile uint16_t audio_play_repeat = 0U;

static void dbg_print(const char *s)
{
    HAL_UART_Transmit(&huart4, (uint8_t *)s, strlen(s), HAL_MAX_DELAY);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2)
    {
        uint16_t dac_value = 2048U;

        if (audio_playing && audio_play_len > 0U)
        {
            dac_value = ((uint16_t)audio_buffer[audio_play_index]) << 4;

            audio_play_index++;
            if (audio_play_index >= audio_play_len)
            {
                audio_play_index = 0U;
                audio_play_repeat++;

                if (audio_play_repeat >= AUDIO_PLAY_REPEATS)
                {
                    audio_playing = 0U;
                    audio_play_repeat = 0U;
                }
            }
        }

        HAL_DAC_SetValue(&hdac, DAC_CHANNEL_1, DAC_ALIGN_12B_R, dac_value);
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

static uint16_t calc_audio_checksum(uint16_t len)
{
    uint16_t checksum = 0U;

    for (uint16_t i = 0U; i < len; i++)
    {
        checksum += audio_buffer[i];
    }

    return checksum;
}

static uint8_t start_audio_playback(uint16_t len)
{
    if (len == 0U || len > AUDIO_BUFFER_BYTES)
    {
        return 0U;
    }

    __disable_irq();
    audio_play_len = len;
    audio_play_index = 0U;
    audio_play_repeat = 0U;
    audio_playing = 1U;
    __enable_irq();

    return 1U;
}

static uint8_t process_audio_payload(const uint8_t *payload, uint16_t len)
{
    uint8_t type;

    if (len == 0U)
    {
        return 0U;
    }

    type = payload[0];

    if (type == AUDIO_MSG_START)
    {
        uint16_t clip_len;
        uint16_t sample_rate;

        if (len != 6U)
        {
            return 0U;
        }

        clip_len = ((uint16_t)payload[2] << 8) | payload[1];
        sample_rate = ((uint16_t)payload[4] << 8) | payload[3];

        if (clip_len == 0U || clip_len > AUDIO_BUFFER_BYTES)
        {
            dbg_print("AUDIO START BAD LENGTH\r\n");
            return 0U;
        }

        audio_transfer_active = 1U;
        audio_expected_len = clip_len;
        audio_received_len = 0U;
        audio_playing = 0U;
        memset(audio_buffer, 128, sizeof(audio_buffer));

        {
            char msg[100];
            sprintf(msg,
                    "\r\nAUDIO START len=%lu rate=%lu\r\n",
                    (unsigned long)clip_len,
                    (unsigned long)sample_rate);
            dbg_print(msg);
        }

        return 1U;
    }

    if (type == AUDIO_MSG_DATA)
    {
        uint16_t offset;
        uint8_t count;

        if (len < 5U)
        {
            return 0U;
        }

        offset = ((uint16_t)payload[2] << 8) | payload[1];
        count = payload[3];

        if (!audio_transfer_active ||
            count == 0U ||
            (uint16_t)(4U + count) != len ||
            offset != audio_received_len ||
            (offset + count) > audio_expected_len ||
            (offset + count) > AUDIO_BUFFER_BYTES)
        {
            dbg_print("AUDIO DATA BAD OFFSET/COUNT\r\n");
            return 0U;
        }

        memcpy(&audio_buffer[offset], &payload[4], count);
        audio_received_len = (uint16_t)(audio_received_len + count);

        {
            char msg[80];
            sprintf(msg,
                    "AUDIO DATA offset=%lu count=%u received=%lu\r\n",
                    (unsigned long)offset,
                    (unsigned int)count,
                    (unsigned long)audio_received_len);
            dbg_print(msg);
        }

        return 1U;
    }

    if (type == AUDIO_MSG_END)
    {
        uint16_t clip_len;
        uint16_t expected_checksum;
        uint16_t actual_checksum;

        if (len != 6U)
        {
            return 0U;
        }

        clip_len = ((uint16_t)payload[2] << 8) | payload[1];
        expected_checksum = ((uint16_t)payload[4] << 8) | payload[3];
        actual_checksum = calc_audio_checksum(clip_len);

        if (!audio_transfer_active ||
            clip_len != audio_expected_len ||
            audio_received_len != audio_expected_len ||
            actual_checksum != expected_checksum)
        {
            char msg[120];
            sprintf(msg,
                    "AUDIO END FAIL len=%lu rx=%lu calc=%lu exp=%lu\r\n",
                    (unsigned long)clip_len,
                    (unsigned long)audio_received_len,
                    (unsigned long)actual_checksum,
                    (unsigned long)expected_checksum);
            dbg_print(msg);
            return 0U;
        }

        audio_transfer_active = 0U;
        dbg_print("\r\nAUDIO COMPLETE - PLAYING\r\n");
        return start_audio_playback(clip_len);
    }

    dbg_print("UNKNOWN AUDIO MSG\r\n");
    return 0U;
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_USART2_UART_Init();   // LoRa
    MX_USART4_UART_Init();   // Debug
    MX_DAC_Init();
    MX_TIM2_Init();

    HAL_Delay(1000);

    if (HAL_DAC_Start(&hdac, DAC_CHANNEL_1) != HAL_OK)
    {
        Error_Handler();
    }

    HAL_DAC_SetValue(&hdac, DAC_CHANNEL_1, DAC_ALIGN_12B_R, 2048U);

    if (HAL_TIM_Base_Start_IT(&htim2) != HAL_OK)
    {
        Error_Handler();
    }

    dbg_print("\r\nNODE 2 AUDIO CLIP RX + PLAYBACK V2 BLOCKING READY\r\n");

    uint8_t b;
    uint8_t state = 0U;

    uint8_t seq_lo = 0U, seq_hi = 0U;
    uint16_t sequence = 0U;

    uint8_t len_lo = 0U, len_hi = 0U;
    uint16_t length = 0U;

    uint8_t payload[MAX_PAYLOAD];
    uint16_t index = 0U;

    uint8_t checksum_lo = 0U, checksum_hi = 0U;
    uint16_t rx_checksum = 0U;
    uint16_t calc_checksum = 0U;

    uint8_t have_last_sequence = 0U;
    uint16_t last_sequence = 0U;

    while (1)
    {
        if (HAL_UART_Receive(&huart2, &b, 1U, 5000U) != HAL_OK)
        {
            continue;
        }

        switch (state)
        {
            case 0:
                if (b == 0xA5)
                {
                    state = 1U;
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
                }
                else
                {
                    state = 0U;
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
                    char msg[100];
                    sprintf(msg,
                            "BAD LENGTH - RESYNC len=%lu seq=%lu\r\n",
                            (unsigned long)length,
                            (unsigned long)sequence);
                    dbg_print(msg);
                    state = 0U;
                    index = 0U;
                }
                else
                {
                    index = 0U;
                    state = 6U;
                }
                break;

            case 6:
                payload[index++] = b;

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

                for (uint16_t i = 0U; i < length; i++)
                {
                    calc_checksum += payload[i];
                }

                if (calc_checksum == rx_checksum)
                {
                    uint8_t is_duplicate =
                        (have_last_sequence && (sequence == last_sequence));

                    if (is_duplicate)
                    {
                        send_ack(sequence);
                        dbg_print("DUPLICATE - ACK RESENT\r\n");
                    }
                    else if (process_audio_payload(payload, length))
                    {
                        last_sequence = sequence;
                        have_last_sequence = 1U;
                        send_ack(sequence);
                        dbg_print("ACK SENT\r\n");
                    }
                    else
                    {
                        dbg_print("PAYLOAD REJECTED - NO ACK\r\n");
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

                state = 0U;
                index = 0U;
                break;
            }

            default:
                state = 0U;
                index = 0U;
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

