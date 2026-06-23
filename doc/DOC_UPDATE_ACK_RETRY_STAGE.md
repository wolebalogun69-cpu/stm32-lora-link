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

### First Runtime Result

Node 1 V6.8 transmits sequence 1 and retries three times, but does not receive a matching ACK:

```text
TX SEQ=1 ATTEMPT=1
ACK TIMEOUT SEQ=1 ATTEMPT=1
TX SEQ=1 ATTEMPT=2
ACK TIMEOUT SEQ=1 ATTEMPT=2
TX SEQ=1 ATTEMPT=3
ACK TIMEOUT SEQ=1 ATTEMPT=3
FRAME FAILED SEQ=1
```

Node 2 prints `FRAME OK`, which proves:

- Node 1 to Node 2 LoRa path works.
- Data frame header, length, payload, and checksum are valid.
- Failure is now isolated to the ACK return path, ACK timing, or Node 1 ACK reception/parser.

### Next Diagnostic

1. Move Node 2 `send_ack(sequence)` before debug prints after checksum success.
2. Add temporary raw-byte logging inside Node 1 `wait_for_ack()`.
3. Check whether Node 1 receives raw bytes `B6 6B 01 00`.

### V6.9 Diagnostic Drop-Ins

Prepared diagnostic firmware outputs:

- `NODE1_V6_9_RELIABLE_TX_ACK_RAW_DEBUG_main.c`
- `NODE2_V6_9_DATA_RX_ACK_FIRST_main.c`

Expected Node 1 ACK debug if the return path works:

```text
RX:B6
RX:6B
RX:01
RX:00
ACK OK SEQ=1
```

If Node 2 prints `FRAME OK` and `ACK SENT`, but Node 1 prints no `RX:` bytes, the issue is likely the return LoRa/UART path rather than packet framing or checksum.

### V6.9 Runtime Result

V6.9 produced the expected results on both nodes.

Confirmed behavior:

- Node 1 sends data frame sequence 1.
- Node 2 receives the frame and reports `FRAME OK`.
- Node 2 sends ACK immediately after checksum success.
- Node 1 receives raw ACK bytes.
- Node 1 decodes the ACK sequence correctly.
- Node 1 reports `ACK OK SEQ=1`.

This validates the full round trip:

```text
Node 1 data frame -> LoRa -> Node 2 decode -> Node 2 ACK -> LoRa -> Node 1 ACK decode
```

Current project status:

- Stage 1 reliable packet framing: passed for the current test frame.
- Stage 2 bidirectional path: basic bidirectional path confirmed.
- Stage 3 ACK and retry: basic ACK/retry mechanism confirmed with diagnostic firmware.

Next engineering step:

- Create a clean V7.0 baseline by removing raw ACK byte debug from Node 1.
- Keep ACK-first behavior on Node 2.
- Add duplicate sequence handling on Node 2 so retries caused by lost ACKs do not process the same payload twice.
- Then begin reducing `CHUNK_DELAY_MS` gradually while measuring reliability.
