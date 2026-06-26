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

#define AUDIO_MSG_START       0x30U
#define AUDIO_MSG_DATA        0x31U
#define AUDIO_MSG_END         0x32U
#define AUDIO_CLIP_BYTES      256U
#define AUDIO_SAMPLE_RATE     8000U
#define AUDIO_DATA_BYTES      20U

static const uint8_t tone_table[32] =
{
    128, 153, 177, 199, 218, 234, 245, 253,
    255, 253, 245, 234, 218, 199, 177, 153,
    128, 103,  79,  57,  38,  22,  11,   3,
      1,   3,  11,  22,  38,  57,  79, 103
};

static void dbg_print(const char *s)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)s, strlen(s), HAL_MAX_DELAY);
}

static uint8_t audio_sample_at(uint16_t index)
{
    return tone_table[index % 32U];
}

static uint16_t calc_clip_checksum(void)
{
    uint16_t checksum = 0U;

    for (uint16_t i = 0U; i < AUDIO_CLIP_BYTES; i++)
    {
        checksum += audio_sample_at(i);
    }

    return checksum;
}

static uint16_t calc_frame_checksum(uint16_t seq,
                                    uint16_t len,
                                    const uint8_t *payload)
{
    uint16_t checksum = 0U;

    checksum += (seq & 0xFF);
    checksum += ((seq >> 8) & 0xFF);
    checksum += (len & 0xFF);
    checksum += ((len >> 8) & 0xFF);

    for (uint16_t i = 0U; i < len; i++)
    {
        checksum += payload[i];
    }

    return checksum;
}

static void send_pair(uint8_t a, uint8_t b)
{
    uint8_t pair[CHUNK_SIZE_BYTES] = {a, b};
    HAL_UART_Transmit(&huart2, pair, CHUNK_SIZE_BYTES, HAL_MAX_DELAY);
    HAL_Delay(CHUNK_DELAY_MS);
}

static void send_frame(uint16_t seq, const uint8_t *payload, uint16_t len)
{
    uint16_t checksum = calc_frame_checksum(seq, len, payload);

    send_pair(0xA5, 0x5A);
    send_pair(seq & 0xFF, (seq >> 8) & 0xFF);
    send_pair(len & 0xFF, (len >> 8) & 0xFF);

    for (uint16_t i = 0U; i < len; i += 2U)
    {
        uint8_t second = 0U;

        if ((i + 1U) < len)
        {
            second = payload[i + 1U];
        }

        send_pair(payload[i], second);
    }

    send_pair(checksum & 0xFF, (checksum >> 8) & 0xFF);
}

static uint8_t wait_for_ack(uint16_t expected_seq)
{
    uint8_t b;
    uint8_t state = 0U;
    uint8_t seq_lo = 0U;
    uint8_t seq_hi = 0U;
    uint16_t seq = 0U;
    uint32_t start = HAL_GetTick();

    while ((HAL_GetTick() - start) < ACK_TIMEOUT_MS)
    {
        if (HAL_UART_Receive(&huart2, &b, 1U, 1000U) != HAL_OK)
        {
            continue;
        }

        switch (state)
        {
            case 0:
                if (b == 0xB6)
                {
                    state = 1U;
                }
                break;

            case 1:
                if (b == 0x6B)
                {
                    state = 2U;
                }
                else if (b == 0xB6)
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
                seq = ((uint16_t)seq_hi << 8) | seq_lo;

                if (seq == expected_seq)
                {
                    return 1U;
                }

                state = 0U;
                break;

            default:
                state = 0U;
                break;
        }
    }

    return 0U;
}

static uint8_t send_reliable_frame(uint16_t *seq,
                                   const uint8_t *payload,
                                   uint16_t len,
                                   const char *label)
{
    for (uint8_t attempt = 1U; attempt <= MAX_RETRIES; attempt++)
    {
        char msg[100];

        sprintf(msg,
                "\r\nTX %s SEQ=%lu ATTEMPT=%u\r\n",
                label,
                (unsigned long)*seq,
                (unsigned int)attempt);
        dbg_print(msg);

        send_frame(*seq, payload, len);

        if (wait_for_ack(*seq))
        {
            sprintf(msg, "ACK OK SEQ=%lu\r\n", (unsigned long)*seq);
            dbg_print(msg);
            (*seq)++;
            return 1U;
        }

        sprintf(msg,
                "ACK TIMEOUT SEQ=%lu ATTEMPT=%u\r\n",
                (unsigned long)*seq,
                (unsigned int)attempt);
        dbg_print(msg);
    }

    dbg_print("FRAME FAILED\r\n");
    return 0U;
}

static uint8_t send_audio_clip(uint16_t *seq)
{
    uint8_t payload[32];
    uint16_t clip_checksum = calc_clip_checksum();

    payload[0] = AUDIO_MSG_START;
    payload[1] = AUDIO_CLIP_BYTES & 0xFF;
    payload[2] = (AUDIO_CLIP_BYTES >> 8) & 0xFF;
    payload[3] = AUDIO_SAMPLE_RATE & 0xFF;
    payload[4] = (AUDIO_SAMPLE_RATE >> 8) & 0xFF;
    payload[5] = 0U;

    if (!send_reliable_frame(seq, payload, 6U, "AUDIO_START"))
    {
        return 0U;
    }

    for (uint16_t offset = 0U; offset < AUDIO_CLIP_BYTES; offset += AUDIO_DATA_BYTES)
    {
        uint8_t count = AUDIO_DATA_BYTES;

        if ((offset + count) > AUDIO_CLIP_BYTES)
        {
            count = (uint8_t)(AUDIO_CLIP_BYTES - offset);
        }

        payload[0] = AUDIO_MSG_DATA;
        payload[1] = offset & 0xFF;
        payload[2] = (offset >> 8) & 0xFF;
        payload[3] = count;

        for (uint8_t i = 0U; i < count; i++)
        {
            payload[4U + i] = audio_sample_at(offset + i);
        }

        if (!send_reliable_frame(seq, payload, (uint16_t)(4U + count), "AUDIO_DATA"))
        {
            return 0U;
        }
    }

    payload[0] = AUDIO_MSG_END;
    payload[1] = AUDIO_CLIP_BYTES & 0xFF;
    payload[2] = (AUDIO_CLIP_BYTES >> 8) & 0xFF;
    payload[3] = clip_checksum & 0xFF;
    payload[4] = (clip_checksum >> 8) & 0xFF;
    payload[5] = 0U;

    if (!send_reliable_frame(seq, payload, 6U, "AUDIO_END"))
    {
        return 0U;
    }

    return 1U;
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_USART1_UART_Init();   // Debug
    MX_USART2_UART_Init();   // LoRa

    HAL_Delay(6000);

    dbg_print("\r\nNODE 1 AUDIO CLIP TX V1 READY\r\n");

    uint16_t seq = 1U;

    while (1)
    {
        if (send_audio_clip(&seq))
        {
            dbg_print("\r\nAUDIO CLIP TRANSFER COMPLETE\r\n");
        }
        else
        {
            dbg_print("\r\nAUDIO CLIP TRANSFER FAILED\r\n");
        }

        HAL_Delay(10000);
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
