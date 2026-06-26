# Audio V5 Stored Clip Transfer Success

Date: 2026-06-26

## Result

Audio V5 successfully completed the first stored audio transfer and playback.

Observed Node 2 output:

```text
AUDIO START len=256
FRAME TIMEOUT - RESYNC
AUDIO DATA offset=0 count=20 received=20
AUDIO DATA offset=20 count=20 received=40
AUDIO DATA offset=40 count=20 received=60
FRAME TIMEOUT - RESYNC
FRAME TIMEOUT - RESYNC
AUDIO DATA offset=60 count=20 received=80
AUDIO DATA offset=80 count=20 received=100
AUDIO DATA offset=100 count=20 received=120
AUDIO DATA offset=120 count=20 received=140
FRAME TIMEOUT - RESYNC
AUDIO DATA offset=140 count=20 received=160
AUDIO DATA offset=160 count=20 received=180
FRAME TIMEOUT - RESYNC
AUDIO DATA offset=180 count=20 received=200
AUDIO DATA offset=200 count=20 received=220
AUDIO DATA offset=220 count=20 received=240
AUDIO DATA offset=240 count=16 received=256
AUDIO COMPLETE - PLAYING
```

## Interpretation

The first complete stored-audio path works:

```text
Node 1 generated audio bytes
-> LoRa packet transfer with ACK/retry
-> Node 2 stores audio buffer
-> Node 2 validates final clip
-> Node 2 plays through DAC/TIM2
```

The `FRAME TIMEOUT - RESYNC` messages show that some frame attempts are still damaged or incomplete, but the retry mechanism recovers and the complete clip is eventually received.

## Current Baseline

- Node 1: `NODE1_AUDIO_CLIP_TX_V1_main.c`
- Node 2: `NODE2_AUDIO_CLIP_RX_PLAYBACK_V5_FRAME_TIMEOUT_CLEAN_ACK_main.c`

## Next Engineering Objective

Improve transfer cleanliness and reduce timeout/resync events.

Recommended next step:

- Keep the successful V5 behavior unchanged as the baseline.
- Add sender-side inter-frame settling delay after ACK success.
- Track retries/timeouts per audio frame on Node 1.
- Consider reducing payload size from 20 audio bytes to 12 or 16 if timeout/resync remains frequent.

