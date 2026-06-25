# Engineering Log

## 2026-06-19

### Baseline Captured

The project state was captured for the existing STM32 LoRa UART communication system.

Known current baseline:

- Node 1 uses USART2 on PD5/PD6 for LoRa.
- Node 2 uses USART2 on PA2/PA3 for LoRa.
- Node 2 DAC_OUT1 is PA4 for future audio playback.
- LoRa modules are configured in TRNS mode at 115200 baud, 8N1, 868000 KHz.
- Module-to-module communication works outside STM32 firmware.
- 2-byte chunks with delay are reliable.
- Large UART bursts are unreliable.
- Both nodes currently use blocking polling UART receive.

### Current Protocol

Data frame:

```text
A5 5A
SEQ_L SEQ_H
LEN_L LEN_H
PAYLOAD
CHECKSUM_L CHECKSUM_H
```

ACK frame:

```text
B6 6B
SEQ_L SEQ_H
```

### Engineering Decision

Preserve the current 2-byte paced chunking path as the known-working baseline while improving ACK decode reliability.

Retry logic should be added only after ACK parsing is deterministic.

Audio packet transfer should wait until packet framing, ACK, retry, and receive buffering are reliable.

### Next Review Target

Review Node 2 data-frame receive/decode code, especially:

- Header synchronization
- Sequence decode
- Length bounds
- Payload buffering
- Checksum validation
- ACK send timing

## 2026-06-23

### V7.0 Reliable Baseline Passed

V7.0 was tested successfully on both STM32 nodes.

Confirmed behavior:

- Node 1 transmits framed payloads.
- Node 1 waits for a matching ACK sequence.
- Node 1 retry logic is present with `MAX_RETRIES = 3`.
- Node 1 advances sequence only after `ACK OK`.
- Node 2 validates data-frame checksum.
- Node 2 sends ACK immediately after checksum success.
- Node 2 duplicate sequence handling is present so repeated frames can be ACKed again without being processed as new messages.

V7.0 is now the current known-good reliability baseline.

### Current Engineering Decision

Keep the V7.0 frame format, ACK format, retry count, and duplicate handling unchanged while performing timing reduction tests.

Next objective:

- Reduce `CHUNK_DELAY_MS` in controlled steps.
- Record the fastest reliable chunk delay.
- Do not begin audio packet transport until the selected timing is reliable over repeated packet tests.

### V7.1 Timing Reduction Result

`CHUNK_DELAY_MS = 250` was tested and failed reliability validation.

Observed result:

- Could barely reach 10 packets without checksum failure.
- Stable operation was only around 5 packets at most.

Conclusion:

- 250 ms is not a reliable baseline.
- The failure mode is checksum failure on Node 2, which means the forward data frame is being corrupted, truncated, or misaligned before ACK handling matters.
- The project should revert to the last known reliable timing and avoid further speed tuning until UART receive robustness is improved.

Decision:

- Keep V7.0 as the known-good reliability baseline.
- Do not promote 250 ms to a baseline.
- Next engineering focus should be UART RX robustness: interrupt-driven receive or DMA/ring-buffer receive.

## 2026-06-25

### Node 2 USART2 Interrupt Plumbing Confirmed

Node 2 USART2 initialization was reviewed.

Confirmed:

- USART2 is configured at 115200 baud, 8N1.
- USART2 mode is TX/RX.
- USART2 hardware flow control is disabled.
- USART2 interrupt is enabled through NVIC.
- `USART2_IRQHandler()` calls `HAL_UART_IRQHandler(&huart2)`.

The provided CubeMX pin mapping shows Node 2 USART2 on:

- `PD5 -> USART2_TX`
- `PD6 -> USART2_RX`

This corrects the earlier project assumption that Node 2 USART2 used `PA2/PA3`.

### V8.0 Node 2 IRQ Ring RX Prepared

Prepared Node 2 V8.0 receiver architecture:

- USART2 interrupt receives one byte at a time.
- RX callback pushes bytes into a 128-byte ring buffer.
- Main loop parses frames from the ring buffer.
- ACK-first behavior is preserved.
- Duplicate sequence handling is preserved.
- RX ring overflow is counted and reported.

Node 1 remains unchanged on V7.0 for the first V8.0 test.

### V8.0 Runtime Observation

V8.0 is a major improvement over blocking polling RX.

Observed during a roughly 60-cycle run:

- System continued operating and recovered after errors.
- One `BAD LENGTH` occurred after about 20 cycles.
- One `CHECKSUM FAIL` occurred during the run.
- The system corrected itself and moved on instead of locking up.

Interpretation:

- Ring-buffer interrupt RX is working and improves robustness.
- Parser resynchronization is working.
- Retry/ACK architecture can recover from occasional invalid frames.
- Reliability is not yet clean enough for audio transport.

Next hardening direction:

- Add better receiver diagnostics counters.
- Increase RX ring buffer size.
- Add frame timeout/reset protection while parsing partial frames.
- Consider DMA circular RX if byte interrupt RX still produces occasional corrupt frames.

### V8.1 Node 2 Diagnostic Hardening Prepared

V8.1 was prepared to classify the remaining V8.0 errors.

Changes:

- RX ring buffer increased from 128 bytes to 256 bytes.
- Parser timeout added with `PARSER_TIMEOUT_MS = 3000`.
- Periodic diagnostics report added every 10 seconds.
- Counters added for frames OK, checksum failures, bad lengths, duplicates, parser timeouts, ring overflows, and UART errors.

Protocol behavior is unchanged.

Node 1 remains unchanged on the V7.0 sender/retry baseline for the first V8.1 test.

## 2026-06-25

### Audio Goal Clarified

The project is not targeting live audio.

Target behavior:

- Send a small audio clip over LoRa.
- Store the clip on Node 2.
- Play it after transfer through Node 2 DAC.

### Node 2 DAC/TIM2 Setup Reviewed

Provided DAC setup confirms:

- `PA4 -> DAC_OUT1`
- DAC channel 1 enabled
- DAC output buffer enabled
- DAC trigger is none

Provided TIM2 setup confirms:

- TIM2 base interrupt is enabled.
- Prescaler is 0.
- Period is 261.
- `TIM2_IRQHandler()` is already present in the interrupt file.

### DAC Tone Test Prepared

Prepared a local Node 2 DAC tone test before LoRa audio transfer.

Output:

- `NODE2_AUDIO_DAC_TONE_TEST_main.c`
- `AUDIO_DAC_TONE_TEST_PLAN.md`

Engineering note:

- A passive dynamic speaker should be driven through an amplifier/driver, not directly from PA4.

### DAC Tone Test Passed

Node 2 local DAC tone playback test succeeded.

Confirmed:

- DAC channel 1 works on PA4.
- TIM2 interrupt-driven sample output works.
- The audio output chain produces audible sound.

This clears the way for the first stored audio clip transfer demo.

Next objective:

- Send a small generated 8-bit PCM clip over LoRa.
- Store it on Node 2.
- Play it through the proven DAC/TIM2 output path after transfer completes.

### Audio Clip Transfer V1 Prepared

Prepared the first stored audio clip transfer demo.

Design:

- Preserve existing frame wrapper and ACK.
- Add typed payloads for audio start, data, and end.
- Send a 256-byte generated unsigned 8-bit PCM tone clip.
- Node 2 stores the clip in RAM.
- Node 2 validates the clip checksum at end.
- Node 2 plays the stored clip through the already-proven TIM2/DAC output path.

Prepared outputs:

- `NODE1_AUDIO_CLIP_TX_V1_main.c`
- `NODE2_AUDIO_CLIP_RX_PLAYBACK_V1_main.c`
- `AUDIO_CLIP_TRANSFER_V1_TEST_PLAN.md`
