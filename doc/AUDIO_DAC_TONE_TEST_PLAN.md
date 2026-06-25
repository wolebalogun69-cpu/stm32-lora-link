# Node 2 DAC Tone Test Plan

Date: 2026-06-25

## Goal

Prove that Node 2 can produce audible DAC output on `PA4 / DAC_OUT1` before sending audio data over LoRa.

## Firmware Under Test

- `NODE2_AUDIO_DAC_TONE_TEST_main.c`

## What This Test Does

- Starts DAC channel 1 on `PA4`.
- Starts TIM2 interrupt.
- Outputs a 32-sample sine wavetable to the DAC.
- Produces a repeating tone burst:
  - tone on for about 1 second
  - silence for about 0.5 seconds

## Expected Serial Output

```text
NODE 2 DAC TONE TEST READY
PA4 DAC_OUT1 should output a repeating tone burst.
DAC tone test running
```

## Expected Audio Result

If PA4 is connected through a suitable amplifier or speaker driver:

```text
beep ... silence ... beep ... silence
```

## Hardware Warning

A passive dynamic loudspeaker should not be driven directly from the STM32 DAC pin.

Recommended chain:

```text
PA4 DAC_OUT1 -> amplifier / driver -> passive speaker
```

If the speaker is connected directly to PA4, audio may be extremely weak or distorted, and the DAC output may be overloaded.

## Next Step After Pass

Use the same DAC playback method with a small received audio buffer.

First LoRa audio demo target:

```text
Node 1 sends a small 8-bit PCM clip in reliable packets.
Node 2 stores the clip.
Node 2 plays the clip after transfer completes.
```

