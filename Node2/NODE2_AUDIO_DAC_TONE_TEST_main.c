#include "main.h"
#include "dac.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

#include <string.h>

extern DAC_HandleTypeDef hdac;
extern TIM_HandleTypeDef htim2;
extern UART_HandleTypeDef huart4;   // Debug

void SystemClock_Config(void);

/*
 * TIM2 is configured with:
 *   Prescaler = 0
 *   Period    = 261
 *
 * With the current MSI range 5 clock, this is approximately an 8 kHz
 * update rate. A 32-sample wavetable therefore produces about 250 Hz.
 */
#define DAC_MIDPOINT_12B       2048U
#define DAC_MIN_12B            512U
#define DAC_MAX_12B            3584U
#define TONE_SAMPLES           32U
#define BEEP_ON_SAMPLES        8000U
#define BEEP_OFF_SAMPLES       4000U

static const uint16_t sine_12b[TONE_SAMPLES] =
{
    2048, 2347, 2637, 2909, 3157, 3374, 3554, 3692,
    3784, 3826, 3817, 3756, 3645, 3487, 3287, 3050,
    2784, 2496, 2195, 1890, 1590, 1304, 1040,  805,
     609,  458,  355,  304,  306,  360,  466,  620
};

static volatile uint16_t tone_index = 0U;
static volatile uint32_t audio_sample_count = 0U;
static volatile uint8_t tone_enabled = 1U;

static void dbg_print(const char *s)
{
    HAL_UART_Transmit(&huart4, (uint8_t *)s, strlen(s), HAL_MAX_DELAY);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2)
    {
        uint16_t dac_value = DAC_MIDPOINT_12B;

        if (tone_enabled)
        {
            dac_value = sine_12b[tone_index];
            tone_index++;

            if (tone_index >= TONE_SAMPLES)
            {
                tone_index = 0U;
            }
        }

        HAL_DAC_SetValue(&hdac, DAC_CHANNEL_1, DAC_ALIGN_12B_R, dac_value);

        audio_sample_count++;

        if (tone_enabled && audio_sample_count >= BEEP_ON_SAMPLES)
        {
            tone_enabled = 0U;
            audio_sample_count = 0U;
            tone_index = 0U;
        }
        else if (!tone_enabled && audio_sample_count >= BEEP_OFF_SAMPLES)
        {
            tone_enabled = 1U;
            audio_sample_count = 0U;
            tone_index = 0U;
        }
    }
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_USART4_UART_Init();   // Debug
    MX_DAC_Init();
    MX_TIM2_Init();

    HAL_Delay(1000);

    dbg_print("\r\nNODE 2 DAC TONE TEST READY\r\n");
    dbg_print("PA4 DAC_OUT1 should output a repeating tone burst.\r\n");

    if (HAL_DAC_Start(&hdac, DAC_CHANNEL_1) != HAL_OK)
    {
        Error_Handler();
    }

    HAL_DAC_SetValue(&hdac, DAC_CHANNEL_1, DAC_ALIGN_12B_R, DAC_MIDPOINT_12B);

    if (HAL_TIM_Base_Start_IT(&htim2) != HAL_OK)
    {
        Error_Handler();
    }

    while (1)
    {
        HAL_Delay(1000);
        dbg_print("DAC tone test running\r\n");
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

