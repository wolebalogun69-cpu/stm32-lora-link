# LoRa STM32 Project State

Last updated: 2026-06-19

## Project Memory

This project is an existing STM32-to-STM32 wireless communication system using Four-Faith F8L10A-N-H LoRa modules over UART.

The engineering path is:

1. Reliable packet communication
2. Reliable bidirectional communication
3. ACK and retry mechanisms
4. Robust packet protocol
5. Audio packet transport
6. Audio playback on Node 2 DAC

Current focus: first stored audio clip transfer over LoRa, then playback on Node 2 DAC.

## Hardware

### Node 1

- Board: STM32F0308-DISCO
- LoRa interface: USART2
- LoRa module: Four-Faith F8L10A-N-H
- Role: packet sender during current tests

### Node 2

- MCU: STM32L073VZT6
- LoRa interface: USART2
- LoRa module: Four-Faith F8L10A-N-H
- DAC output for future audio playback: DAC_OUT1 on PA4
- Role: packet receiver and ACK sender during current tests

## Pin Mappings

### Node 1: STM32F0308-DISCO

| Signal | STM32 Pin | Peripheral | Connected To | Notes |
|---|---|---|---|---|
| LoRa TX from MCU | PD5 | USART2_TX | LoRa RX | Active LoRa UART |
| LoRa RX into MCU | PD6 | USART2_RX | LoRa TX | Active LoRa UART |
| Alternate TX | PE8 | USART4_TX | Unknown | Not active for LoRa |
| Alternate RX | PE9 | USART4_RX | Unknown | Not active for LoRa |

### Node 2: STM32L073VZT6

| Signal | STM32 Pin | Peripheral | Connected To | Notes |
|---|---|---|---|---|
| LoRa TX from MCU | PD5 | USART2_TX | LoRa RX | Active LoRa UART |
| LoRa RX into MCU | PD6 | USART2_RX | LoRa TX | Active LoRa UART |
| Alternate TX | PA9 | USART1_TX | Unknown | Not active for LoRa |
| Alternate RX | PA10 | USART1_RX | Unknown | Not active for LoRa |
| Audio DAC | PA4 | DAC_OUT1 | Audio output stage | Future stage |

## UART Mappings

### Node 1 UART

- Active peripheral: USART2
- TX pin: PD5
- RX pin: PD6
- Baud rate: 115200
- Format: 8N1
- RX method: blocking polling receive
- TX method: blocking transmit

### Node 2 UART

- Active peripheral: USART2
- TX pin: PD5
- RX pin: PD6
- Baud rate: 115200
- Format: 8N1
- RX method: blocking polling receive
- TX method: blocking transmit

## LoRa Settings

| Setting | Node 1 | Node 2 |
|---|---|---|
| Module | Four-Faith F8L10A-N-H | Four-Faith F8L10A-N-H |
| Work mode | TRNS | TRNS |
| UART baud | 115200 | 115200 |
| UART format | 8N1 | 8N1 |
| Frequency RX/TX | 868000 KHz | 868000 KHz |
| Radio speed | Same on both modules | Same on both modules |
| Node ID | Different from Node 2 | Different from Node 1 |
| Transmit address | Points to Node 2 | Points to Node 1 |
| Sleep mode | None | None |

## Current Firmware State

Both nodes currently use blocking polling UART receive. There is no interrupt-driven RX, DMA RX, or continuous ring buffer yet.

Node 1 sends a data frame in paced 2-byte chunks. Node 2 validates the frame, sends ACK immediately after checksum success, and handles duplicate sequence numbers by resending ACK without processing the payload as a new message.

Current chunking constants:

```c
#define CHUNK_SIZE_BYTES      2U
#define CHUNK_DELAY_MS        500U
#define ACK_TIMEOUT_MS        12000U
#define MAX_RETRIES           3U
```

## Packet Protocol

### Data Frame

```text
A5 5A
SEQ_L SEQ_H
LEN_L LEN_H
PAYLOAD
CHECKSUM_L CHECKSUM_H
```

Current test payload:

```text
HELLO123ABCD
```

Current payload length:

```text
12 bytes
```

### ACK Frame

```text
B6 6B
SEQ_L SEQ_H
```

ACK sequence is 16-bit little-endian.

## Known Working Tests

| Test | Result | Notes |
|---|---|---|
| LoRa module-to-module communication outside STM32 firmware | Working | Confirms basic module pairing and RF path |
| Transparent UART mode | Working | TRNS mode is active |
| 2-byte chunks with delay | Reliable | Current stable baseline |
| ACK frame structure | Defined | `B6 6B SEQ_L SEQ_H` |
| V7.0 reliable ACK/retry baseline | Working | Full round trip confirmed on both nodes |
| 250 ms chunk delay timing test | Failed | Could barely reach 10 packets without checksum failure |
| V8.0 interrupt RX ring buffer | Improved | About 60 cycles with one bad length and one checksum fail, then recovered |
| Node 2 DAC tone playback | Working | Local TIM2-driven DAC tone is audible |
| Audio clip transfer V1 | Prepared | 256-byte generated tone clip, store then play |
| Audio V1 bad length fix | Applied | START/END payloads changed from 5 to 6 bytes to preserve 2-byte chunk alignment |
| Audio V2 blocking receiver | Prepared | Reverts audio RX to known-good blocking parser style for first playback demo |
| Audio V3 timer-off receiver | Prepared | Keeps TIM2 stopped during LoRa transfer, starts playback timer only after full clip validates |
| Audio V4 ACK-first receiver | Prepared | Sends ACK immediately after frame checksum before processing audio payload |
| Audio V5 frame timeout receiver | Prepared | Resets parser if partial frame stalls before retry starts |
| Audio V5 stored clip transfer | Working | 256-byte clip received, validated, and played after retry/resync recovery |

## Known Failed Tests

| Failure | Observed Behavior | Suspected Cause | Priority |
|---|---|---|---|
| Large UART bursts unreliable | Data loss, corruption, or missed decode | Blocking polling receive, UART overrun, LoRa module serial-buffer limits, parser timing | High |
| Large UART bursts unreliable | Still not validated after V7.0 | Requires timing reduction tests, then interrupt/DMA RX before audio | High |
| 250 ms chunk delay unreliable | Node 2 checksum failures after a small number of packets | Forward data path cannot yet tolerate faster pacing | High |
| Occasional V8.0 parser errors | One bad length and one checksum fail seen in about 60 cycles | Needs V8.1 receiver hardening before audio | High |
| Audio packet transfer not ready | Not started safely | Packet and ACK reliability must be solved first | High |
| Passive dynamic speaker drive | DAC pin cannot directly power speaker reliably | Use amplifier/driver between PA4 and speaker |

## Current Objective

Send a small generated audio clip over LoRa, store it on Node 2, then play it through DAC_OUT1 PA4.

Immediate success criteria:

- Node 1 continues to send framed data packets with sequence numbers.
- Node 2 continues to validate checksum and ACK immediately.
- Node 1 continues to advance sequence only after matching ACK.
- Duplicate sequence handling continues to prevent repeated payload processing.
- Timing reduction is paused until the UART receive path is more robust.

## Open Issues

| Issue | Impact | Current Direction |
|---|---|---|
| Blocking polling RX | Can miss bytes during bursts and does not scale to audio | Keep current method for baseline, then migrate to interrupt or DMA RX |
| Very large chunk delay | Reliable but slow | Preserve while debugging; later tune down after parser is stable |
| Timing still conservative | `500 ms` chunk delay is reliable but slow | Reduce in measured steps |
| No RX ring buffer | Parser only sees bytes when actively receiving | Add ring buffer before audio transport |
| Unknown checksum implementation details | Need to verify sender/receiver match | Review `calc_data_checksum()` and receiver validation next |

## Engineering Decisions

| Date | Decision | Reason |
|---|---|---|
| 2026-06-19 | Preserve 2-byte chunking with delay as known-good baseline | It is currently reliable and useful for isolating parser/ACK issues |
| 2026-06-19 | Treat ACK as `B6 6B SEQ_L SEQ_H` | Simple, low-overhead, and sequence-specific |
| 2026-06-19 | Do not start audio transport until ACK/retry is reliable | Audio will amplify packet-loss and buffering problems |
| 2026-06-23 | V7.0 is the known-good ACK/retry baseline | Full data plus ACK round trip passed on both nodes |
| 2026-06-23 | Keep protocol stable during timing reduction | Isolates timing reliability from protocol changes |
| 2026-06-23 | Do not promote 250 ms chunk delay | It fails within roughly 5-10 packets with checksum errors |
| 2026-06-23 | Move next to interrupt or DMA RX | Faster pacing likely requires continuous UART receive buffering |
| 2026-06-25 | Correct Node 2 USART2 pins to PD5/PD6 | Provided CubeMX USART2 MSP config shows PD5/PD6 |
| 2026-06-25 | Prepare Node 2 V8.0 interrupt RX ring buffer | USART2 IRQ setup and handler are confirmed |
| 2026-06-25 | Treat V8.0 as improved but not final | It recovers from occasional errors but is not yet clean enough for audio |
| 2026-06-25 | Prepare V8.1 diagnostic hardening | Larger ring, parser timeout, and counters will classify remaining errors |
| 2026-06-25 | Do not target live audio | Project target is clip transfer, store, then playback |
| 2026-06-25 | Prove DAC playback before LoRa audio | Separates audio output validation from radio transfer issues |
| 2026-06-25 | DAC tone test passed | Node 2 DAC/TIM2 playback path is confirmed audible |
| 2026-06-25 | Prepare first stored audio transfer | Use typed payloads inside existing reliable frame/ACK wrapper |
| 2026-06-25 | Keep transmitted frame payload lengths even | Current 2-byte chunk sender pads odd payloads, which can misalign checksum parsing |
| 2026-06-26 | Use blocking RX for first stored-audio demo | Reliability matters more than speed because audio is stored then played, not live |
| 2026-06-26 | Keep DAC timer off during audio reception | Stored audio does not need playback interrupts during LoRa transfer |
| 2026-06-26 | ACK first after frame checksum for audio transfer | Audio V3 aligned frames but Node 1 still timed out waiting for ACK |
| 2026-06-26 | Add partial-frame timeout for audio receiver | Prevents next retry header from being consumed as stale checksum/payload data |
| 2026-06-26 | Promote Audio V5 to first audio demo baseline | Stored audio transfer and playback completed successfully |

## Future Roadmap

### Stage 1: Reliable Packet Communication

- Verify data-frame parser
- Verify checksum calculation on both nodes
- Confirm no false-positive frame detection

### Stage 2: Reliable Bidirectional Communication

- Allow both nodes to send and receive
- Avoid blocking transmit/receive deadlocks

### Stage 3: ACK and Retry Mechanisms

- Harden ACK parser
- Add `MAX_RETRIES`
- Add retry timeout handling
- Track sequence number rollover

### Stage 4: Robust Packet Protocol

- Add message type
- Add protocol version
- Add payload length limits
- Add CRC instead of simple checksum if needed
- Add parser state machine with resynchronization

### Stage 5: Audio Packet Transport

- Define audio frame size
- Define sample format and sample rate
- Add TX pacing based on LoRa throughput
- Add receiver jitter buffer

### Stage 6: Audio Playback on Node 2 DAC

- Configure DAC on PA4
- Use timer-triggered playback
- Add circular audio buffer
- Handle underrun and late packet behavior
