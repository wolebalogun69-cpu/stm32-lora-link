# Audio V3 Timer Off During RX

Date: 2026-06-26

## Problem

Audio transfer still failed with `BAD LENGTH - RESYNC`.

## Likely Cause

The audio receiver started TIM2 immediately at boot. TIM2 generates the DAC sample interrupt at roughly 8 kHz.

That means the MCU was servicing frequent DAC timer interrupts while also trying to receive LoRa bytes with blocking UART receive.

For stored audio transfer, playback timing is not needed during reception.

## Decision

Create Audio V3:

- Keep blocking UART receive for reliability.
- Keep DAC initialized.
- Keep TIM2 stopped during LoRa transfer.
- Start TIM2 only after `AUDIO_END` validates successfully.
- Stop TIM2 after playback repeats are done.

## Prepared Output

- `NODE2_AUDIO_CLIP_RX_PLAYBACK_V3_TIMER_OFF_DURING_RX_main.c`

## Node 1

Keep using:

- `NODE1_AUDIO_CLIP_TX_V1_main.c`

