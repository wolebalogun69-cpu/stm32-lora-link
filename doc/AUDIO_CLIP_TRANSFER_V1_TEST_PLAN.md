# Audio Clip Transfer V1 Test Plan

Date: 2026-06-25

## Goal

Send a small generated audio clip over LoRa, store it on Node 2, then play it through DAC.

This is not live audio.

## Firmware Under Test

- Node 1: `NODE1_AUDIO_CLIP_TX_V1_main.c`
- Node 2: `NODE2_AUDIO_CLIP_RX_PLAYBACK_V1_main.c`

## Audio Clip

- Format: unsigned 8-bit PCM
- Clip size: 256 bytes
- Playback sample rate: TIM2 rate, approximately 8 kHz
- Playback repeats: 32
- Audible output: repeated tone segment

## Packet Payload Types

Existing frame wrapper is preserved:

```text
A5 5A
SEQ_L SEQ_H
LEN_L LEN_H
PAYLOAD
CHECKSUM_L CHECKSUM_H
```

ACK is preserved:

```text
B6 6B
SEQ_L SEQ_H
```

Audio payload types:

```text
0x30 AUDIO_START: type, clip_len_l, clip_len_h, sample_rate_l, sample_rate_h
0x31 AUDIO_DATA:  type, offset_l, offset_h, count, sample_bytes...
0x32 AUDIO_END:   type, clip_len_l, clip_len_h, checksum_l, checksum_h
```

## Expected Node 1 Output

```text
NODE 1 AUDIO CLIP TX V1 READY
TX AUDIO_START SEQ=1 ATTEMPT=1
ACK OK SEQ=1
TX AUDIO_DATA SEQ=2 ATTEMPT=1
ACK OK SEQ=2
...
TX AUDIO_END SEQ=<n> ATTEMPT=1
ACK OK SEQ=<n>
AUDIO CLIP TRANSFER COMPLETE
```

## Expected Node 2 Output

```text
NODE 2 AUDIO CLIP RX + PLAYBACK V1 READY
AUDIO START len=256 rate=8000
ACK SENT
AUDIO DATA offset=0 count=20 received=20
ACK SENT
...
AUDIO COMPLETE - PLAYING
ACK SENT
```

Then the speaker should play the received tone.

## First Pass Criteria

- Node 1 reaches `AUDIO CLIP TRANSFER COMPLETE`.
- Node 2 reaches `AUDIO COMPLETE - PLAYING`.
- Speaker produces audible tone playback.
- No `PAYLOAD REJECTED`.
- No repeated `CHECKSUM FAIL`.
- No `RX RING OVERFLOW`.

## Notes

Transfer is intentionally slow because `CHUNK_DELAY_MS` remains conservative at 500 ms.

This test proves stored clip transfer and playback before any optimization.

