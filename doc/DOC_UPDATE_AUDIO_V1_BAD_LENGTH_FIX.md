# Audio V1 Bad Length Fix

Date: 2026-06-25

## Problem

The first audio transfer test produced `BAD LENGTH` on Node 2.

## Cause

The transport sends UART data in 2-byte chunks.

`AUDIO_START` and `AUDIO_END` originally used 5-byte payloads:

```text
type + 4 metadata bytes = 5 bytes
```

The sender padded the final pair with an extra zero byte, but the frame length still said 5. This shifted the receiver's checksum bytes and caused parser misalignment.

## Fix

Make audio control payloads even-length by adding one reserved byte:

```text
AUDIO_START = 6 bytes
AUDIO_END   = 6 bytes
```

`AUDIO_DATA` was already even length:

```text
4 byte header + 20 audio bytes = 24 bytes
```

## Updated Files

- `NODE1_AUDIO_CLIP_TX_V1_main.c`
- `NODE2_AUDIO_CLIP_RX_PLAYBACK_V1_main.c`
- `AUDIO_CLIP_TRANSFER_V1_TEST_PLAN.md`

