# Audio V2 Blocking Receiver

Date: 2026-06-26

## Reason

Audio V1 still produced `BAD LENGTH - RESYNC` after the even-length packet fix.

This suggests the audio receiver was still affected by the V8.0 interrupt/ring-buffer receiver's occasional parser misalignment.

Because the project target is stored audio clips, not live streaming, speed is less important than reliable first playback.

## Decision

Create Audio V2 using the known-good blocking receive parser style from the V7 baseline.

## What Changed

Node 2 Audio V2:

- Removes interrupt/ring-buffer receive for the audio demo.
- Uses blocking polling receive:

```c
HAL_UART_Receive(&huart2, &b, 1U, 5000U)
```

- Keeps DAC/TIM2 playback.
- Keeps typed audio payloads.
- Keeps ACK/retry compatibility.
- Keeps duplicate sequence ACK resend behavior.
- Adds bad-length debug with decoded length and sequence.

## Prepared Output

- `NODE2_AUDIO_CLIP_RX_PLAYBACK_V2_BLOCKING_BASELINE_main.c`

## Node 1

Keep using the updated:

- `NODE1_AUDIO_CLIP_TX_V1_main.c`

