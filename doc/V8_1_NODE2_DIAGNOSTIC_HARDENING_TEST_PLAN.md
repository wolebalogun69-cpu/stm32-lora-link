# V8.1 Node 2 Diagnostic Hardening Test Plan

## Goal

Determine why occasional V8.0 parser errors still occur while preserving the working protocol.

## Firmware Under Test

- Node 1: keep V7.0 reliable sender/retry firmware unchanged.
- Node 2: `NODE2_V8_1_IRQ_RING_RX_DIAGNOSTIC_HARDENED_main.c`

## V8.1 Changes

- RX ring buffer increased from 128 bytes to 256 bytes.
- Parser timeout added:
  - `PARSER_TIMEOUT_MS = 3000`
- Diagnostic report added every 10 seconds.
- Counters added:
  - `ok`
  - `checksum_fail`
  - `bad_len`
  - `dup`
  - `parser_timeout`
  - `ring_overflow`
  - `uart_error`

## Expected Diagnostic Line

```text
DIAG ok=10 checksum_fail=0 bad_len=0 dup=0 parser_timeout=0 ring_overflow=0 uart_error=0
```

## Test Procedure

1. Start with the last known safe timing.
2. Run at least 100 packets.
3. Record every diagnostic line.
4. Watch for:
   - `checksum_fail`
   - `bad_len`
   - `parser_timeout`
   - `ring_overflow`
   - `uart_error`

## Interpretation

### If `ring_overflow` increases

The main loop is not draining RX bytes fast enough, or debug/ACK delays are blocking too long.

### If `uart_error` increases

The UART peripheral reports hardware-level RX issues such as overrun, framing, or noise errors.

### If `bad_len` or `checksum_fail` increases but `ring_overflow` and `uart_error` stay zero

The corruption may be coming from the LoRa transparent link or from packet boundary/timing behavior.

### If `parser_timeout` increases

The parser started a frame but did not receive the full frame in time. This suggests partial frame loss or long inter-byte gaps.

## Pass Criteria

For a reliability baseline:

```text
checksum_fail=0
bad_len=0
ring_overflow=0
uart_error=0
FRAME FAILED=0 on Node 1
```

