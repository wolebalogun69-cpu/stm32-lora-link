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

Current focus: reliable ACK sequence decode, then retry logic, then audio packet transfer.

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
| LoRa TX from MCU | PA2 | USART2_TX | LoRa RX | Active LoRa UART |
| LoRa RX into MCU | PA3 | USART2_RX | LoRa TX | Active LoRa UART |
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
- TX pin: PA2
- RX pin: PA3
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

Node 1 sends a data frame in paced 2-byte chunks. Node 2 sends an ACK frame in paced 2-byte chunks.

Current chunking constants:

```c
#define CHUNK_SIZE_BYTES      2U
#define CHUNK_DELAY_MS        500U
#define ACK_TIMEOUT_MS        12000U
#define MAX_RETRIES           /* not implemented yet */
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

## Known Failed Tests

| Failure | Observed Behavior | Suspected Cause | Priority |
|---|---|---|---|
| Large UART bursts unreliable | Data loss, corruption, or missed decode | Blocking polling receive, UART overrun, LoRa module serial-buffer limits, parser timing | High |
| ACK sequence decode not reliable enough yet | Retry layer blocked | ACK parser depends on polling timing and byte availability | High |
| Audio packet transfer not ready | Not started safely | Packet and ACK reliability must be solved first | High |

## Current Objective

Make ACK sequence decode reliable while preserving the known-working 2-byte paced transmission path.

Immediate success criteria:

- Node 1 sends a framed data packet with sequence number.
- Node 2 decodes the packet and checksum.
- Node 2 replies with `B6 6B SEQ_L SEQ_H`.
- Node 1 reliably detects matching ACK sequence.
- Mismatched or malformed ACK bytes do not falsely pass.
- Retry logic can be added on top without changing the wire format.

## Open Issues

| Issue | Impact | Current Direction |
|---|---|---|
| Blocking polling RX | Can miss bytes during bursts and does not scale to audio | Keep current method for baseline, then migrate to interrupt or DMA RX |
| Very large chunk delay | Reliable but slow | Preserve while debugging; later tune down after parser is stable |
| No implemented retry count | Sender can only fail or pass once | Add bounded `MAX_RETRIES` after ACK parser is stable |
| No RX ring buffer | Parser only sees bytes when actively receiving | Add ring buffer before audio transport |
| Unknown checksum implementation details | Need to verify sender/receiver match | Review `calc_data_checksum()` and receiver validation next |

## Engineering Decisions

| Date | Decision | Reason |
|---|---|---|
| 2026-06-19 | Preserve 2-byte chunking with delay as known-good baseline | It is currently reliable and useful for isolating parser/ACK issues |
| 2026-06-19 | Treat ACK as `B6 6B SEQ_L SEQ_H` | Simple, low-overhead, and sequence-specific |
| 2026-06-19 | Do not start audio transport until ACK/retry is reliable | Audio will amplify packet-loss and buffering problems |

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

