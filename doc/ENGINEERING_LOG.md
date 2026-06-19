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

