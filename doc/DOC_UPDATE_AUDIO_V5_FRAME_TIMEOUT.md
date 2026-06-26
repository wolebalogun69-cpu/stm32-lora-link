# Audio V5 Frame Timeout Receiver

Date: 2026-06-26

## Observation

Audio V4 log showed checksum failures such as:

```text
CHECKSUM FAIL SEQ=2 calc=3823 rx=42254
```

`42254` is `0xA50E`, which suggests the receiver used `0xA5`, the next frame header byte, as part of the checksum after a prior byte was missed.

## Decision

Add a frame timeout reset.

If the receiver is inside a partial frame and no byte arrives within `UART_BYTE_TIMEOUT_MS`, it resets to header-search state.

## V5 Changes

- Adds `UART_BYTE_TIMEOUT_MS = 2000`.
- Resets parser on partial-frame receive timeout.
- Keeps TIM2 off during reception.
- Keeps playback after full audio validation.
- Sends ACK after payload is accepted but before debug output.
- Reduces debug before ACK.

## Prepared Output

- `NODE2_AUDIO_CLIP_RX_PLAYBACK_V5_FRAME_TIMEOUT_CLEAN_ACK_main.c`

## Node 1

Keep using:

- `NODE1_AUDIO_CLIP_TX_V1_main.c`

