# DAC Tone Test Prepared

Date: 2026-06-25

## Context

The project goal has been clarified:

- Not live audio
- Send small audio clips over LoRa
- Store on Node 2
- Play through DAC on Node 2

## Provided Peripheral Setup

DAC:

- DAC channel 1 enabled
- `PA4 -> DAC_OUT1`
- Output buffer enabled
- Trigger set to none

TIM2:

- Prescaler: 0
- Period: 261
- Interrupt enabled
- `TIM2_IRQHandler()` already calls `HAL_TIM_IRQHandler(&htim2)`

## Prepared Test

Prepared `NODE2_AUDIO_DAC_TONE_TEST_main.c`.

The test:

- Starts DAC channel 1
- Starts TIM2 interrupt
- Outputs a small sine wavetable
- Produces a repeating audible tone burst if the analog output chain is valid

## Engineering Note

The passive dynamic speaker needs a driver/amplifier. The STM32 DAC pin should be treated as a signal source, not a power output.

