# Engineering Log Update

## 2026-06-19

### Node 1 Reliable TX + ACK Retry Drafted

Node 1 V6.8 was prepared as the first reliable transmit and retry implementation.

Changes from the ACK-listener-only Node 1 V6.7:

- Adds data-frame transmission using the established `A5 5A` frame format.
- Adds Node 1 checksum calculation matching the Node 2 receiver calculation.
- Adds ACK wait logic that only accepts `B6 6B SEQ_L SEQ_H` when the ACK sequence matches the transmitted data sequence.
- Adds bounded retry using `MAX_RETRIES = 3`.
- Preserves the known-working 2-byte chunk pacing with `CHUNK_DELAY_MS = 500`.

### Current Test Objective

Flash Node 1 V6.8 and current Node 2 receiver firmware, then verify:

- Node 1 prints `TX SEQ=<n> ATTEMPT=1`.
- Node 2 prints `FRAME OK SEQ=<n>`.
- Node 2 sends ACK.
- Node 1 prints `ACK OK SEQ=<n>`.
- Sequence increments only after matching ACK.

### Notes

The implementation intentionally keeps blocking polling UART receive and slow chunk pacing for this test stage. Interrupt or DMA RX should be introduced after the ACK/retry behavior is confirmed stable.

### Build Fix

Initial Node 1 V6.8 draft declared `SystemClock_Config()` but did not include its implementation. The STM32F030 clock configuration from the previous known-building Node 1 code was restored to resolve:

```text
undefined reference to `SystemClock_Config'
```
