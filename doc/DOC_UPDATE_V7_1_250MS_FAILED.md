# V7.1 250 ms Timing Test Failed

Date: 2026-06-23

## Result

`CHUNK_DELAY_MS = 250` is not reliable.

Observed behavior:

- The system can barely complete 10 packets without checksum failure.
- It can do around 5 packets at most before instability appears.

## Interpretation

Checksum failure on Node 2 means the forward data frame is being corrupted, truncated, or misaligned.

This is not primarily an ACK parser problem because Node 2 is failing validation before a valid ACK should be sent.

Likely causes:

- Blocking polling UART RX is missing bytes as LoRa module UART output gets tighter.
- UART overrun is possible when bytes arrive faster than firmware re-enters `HAL_UART_Receive()`.
- Transparent-mode LoRa module buffering may be releasing bytes in bursts.
- Parser has no continuous ring buffer and therefore has little tolerance for bursty serial delivery.

## Decision

Do not promote 250 ms as a baseline.

Keep V7.0 as the current known-good baseline.

## Next Engineering Step

Move from blocking polling receive to continuous receive buffering:

1. Start with interrupt-driven 1-byte UART RX into a ring buffer.
2. Run the existing parser from the ring buffer.
3. Keep the packet format and ACK format unchanged.
4. Re-test timing only after the receiver no longer depends on blocking polling calls.

