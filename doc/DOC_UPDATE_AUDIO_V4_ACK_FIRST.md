# Audio V4 ACK-First Receiver

Date: 2026-06-26

## Problem

Audio V3 no longer reports `BAD LENGTH`, but Node 1 reports:

```text
ACK TIMEOUT
AUDIO CLIP TRANSFER FAILED
```

## Interpretation

Frame alignment is fixed, but ACK timing is not reliable enough during the audio transaction.

## Decision

Create Audio V4 with ACK-first behavior after a valid frame checksum:

```text
Receive complete frame
Validate frame checksum
Send ACK immediately
Then process/store audio payload
```

This matches the behavior that made V7.0 reliable.

## Prepared Output

- `NODE2_AUDIO_CLIP_RX_PLAYBACK_V4_ACK_FIRST_main.c`

## Node 1

Keep using:

- `NODE1_AUDIO_CLIP_TX_V1_main.c`

