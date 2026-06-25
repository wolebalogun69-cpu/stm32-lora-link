# Audio DAC Baseline

Date: 2026-06-25

## Goal Clarification

The project is not targeting live audio.

The target is:

```text
Send a small recorded/generated audio clip over LoRa,
store it on Node 2,
then play it through DAC_OUT1 on PA4.
```

## Node 2 DAC State

Provided `dac.c` confirms:

- DAC peripheral is enabled.
- `DAC_CHANNEL_1` is configured.
- Output pin is `PA4 -> DAC_OUT1`.
- DAC output buffer is enabled.
- DAC trigger is currently `DAC_TRIGGER_NONE`.

Current DAC config:

```c
sConfig.DAC_Trigger = DAC_TRIGGER_NONE;
sConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
```

## Speaker Hardware

The output device is a passive internal dynamic loudspeaker.

Important engineering note:

- STM32 DAC output should not directly drive a passive dynamic speaker.
- A passive speaker is low impedance and needs a driver/amplifier stage.
- Direct connection may produce very weak audio, distorted audio, or overload the DAC output.

Recommended output chain:

```text
PA4 DAC_OUT1 -> coupling capacitor / amplifier input -> speaker amplifier -> passive speaker
```

For testing, an oscilloscope, powered speaker input, or small amplifier input is preferred.

## Next Step

Prove local Node 2 audio playback before sending audio over LoRa.

Needed next:

- TIM2 initialization, or whichever timer will generate the sample rate.
- Confirm the speaker drive circuit between PA4 and the passive speaker.

