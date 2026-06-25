# Audio Clip Transfer V1 Prepared

Date: 2026-06-25

## Goal

First non-live audio demo:

```text
Node 1 sends generated audio bytes.
Node 2 stores them.
Node 2 plays the stored buffer after transfer completes.
```

## Design

The existing frame and ACK protocol are preserved.

Audio is added as typed payloads:

- `0x30` audio start
- `0x31` audio data
- `0x32` audio end

The first clip is intentionally small:

- 256 bytes
- unsigned 8-bit PCM
- generated tone
- repeated during playback so it is audible

## Prepared Firmware

- `NODE1_AUDIO_CLIP_TX_V1_main.c`
- `NODE2_AUDIO_CLIP_RX_PLAYBACK_V1_main.c`
- `AUDIO_CLIP_TRANSFER_V1_TEST_PLAN.md`

## Reasoning

This proves the complete stored-audio path without requiring live streaming:

- reliable packet transfer
- ACK/retry
- audio storage
- DAC playback

