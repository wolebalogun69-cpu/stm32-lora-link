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

### Audio Clip V1 Bad Length Fix

The first audio transfer test produced `BAD LENGTH`.

Root cause:

- The transport sends in 2-byte chunks.
- `AUDIO_START` and `AUDIO_END` payloads were 5 bytes.
- The sender padded the last pair with an extra zero byte, but the frame length still said 5.
- This shifted checksum bytes and caused parser misalignment.

Fix:

- `AUDIO_START` changed to 6 bytes by adding a reserved byte.
- `AUDIO_END` changed to 6 bytes by adding a reserved byte.
- `AUDIO_DATA` remains 24 bytes and was already even length.

### Audio V2 Blocking Receiver Prepared

Audio V1 still produced `BAD LENGTH - RESYNC`.

Engineering decision:

- Revert Node 2 audio receive path to the known-good blocking polling parser style.
- Keep the DAC/TIM2 playback path.
- Keep the fixed even-length audio payload protocol.
- Keep Node 1 audio sender unchanged.

Reason:

- The project target is stored audio clips, not live audio.
- Reliable first playback is more important than receive-speed optimization.
- The interrupt/ring-buffer receiver was useful for investigation but still produced occasional parser misalignment.

Prepared output:

- `NODE2_AUDIO_CLIP_RX_PLAYBACK_V2_BLOCKING_BASELINE_main.c`

### Audio V3 Timer-Off-During-RX Prepared

Audio V2 still failed with `BAD LENGTH - RESYNC`.

Likely contributor:

- TIM2 DAC playback interrupt was running from boot, even while receiving LoRa audio packets.
- This added frequent interrupt load during blocking UART receive.

Decision:

- Keep TIM2 stopped during LoRa transfer.
- Start TIM2 only after the full audio clip validates.
- Stop TIM2 after playback completes.

Prepared output:

- `NODE2_AUDIO_CLIP_RX_PLAYBACK_V3_TIMER_OFF_DURING_RX_main.c`

### Audio V4 ACK-First Receiver Prepared

Audio V3 removed `BAD LENGTH`, but Node 1 still reported:

- `ACK TIMEOUT`
- `AUDIO CLIP TRANSFER FAILED`

Interpretation:

- Frame alignment is fixed.
- ACK timing is now the main failure.

Decision:

- Send ACK immediately after frame checksum passes.
- Process/store audio payload after ACK.
- Keep TIM2 stopped during reception and start playback only after full clip validation.

Prepared output:

- `NODE2_AUDIO_CLIP_RX_PLAYBACK_V4_ACK_FIRST_main.c`

### Audio V5 Frame Timeout Receiver Prepared

Audio V4 showed checksum failures where the received checksum looked like it included the next frame header byte:

- Example: `rx=42254`, which is `0xA50E`.

Interpretation:

- A byte was likely missed near the frame checksum.
- The receiver stayed in a partial-frame state until the next retry began.
- The next frame's `0xA5` header was consumed as checksum data.

Decision:

- Add partial-frame timeout reset.
- If no byte arrives within `UART_BYTE_TIMEOUT_MS = 2000` while parsing a frame, reset to header search.
- Keep ACK before debug output after payload acceptance.

Prepared output:

- `NODE2_AUDIO_CLIP_RX_PLAYBACK_V5_FRAME_TIMEOUT_CLEAN_ACK_main.c`

### Audio V5 Stored Clip Transfer Passed

Audio V5 completed the first successful stored audio transfer and playback.

Confirmed:

- Node 1 sends generated audio data in reliable packets.
- Node 2 receives and stores 256 audio bytes.
- Retry/resync recovers from damaged or incomplete frame attempts.
- Node 2 validates the full clip.
- Node 2 plays the stored audio through DAC/TIM2.

Observed:

- Several `FRAME TIMEOUT - RESYNC` events occurred during transfer.
- Despite those events, all audio data offsets were eventually received.
- Node 2 reached `AUDIO COMPLETE - PLAYING`.

Current audio baseline:

- Node 1: `NODE1_AUDIO_CLIP_TX_V1_main.c`
- Node 2: `NODE2_AUDIO_CLIP_RX_PLAYBACK_V5_FRAME_TIMEOUT_CLEAN_ACK_main.c`

Next objective:

- Preserve V5 as the successful audio demo baseline.
- Improve transfer cleanliness by reducing resync events and collecting retry statistics.
