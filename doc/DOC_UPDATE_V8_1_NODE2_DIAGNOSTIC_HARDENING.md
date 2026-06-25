# V8.1 Node 2 Diagnostic Hardening

Date: 2026-06-25

## Reason

V8.0 improved reliability but still produced occasional parser errors:

- One `BAD LENGTH`
- One `CHECKSUM FAIL`
- The system recovered and continued

V8.1 is designed to classify the remaining failure source.

## Changes

- RX ring buffer increased to 256 bytes.
- Parser timeout added.
- Periodic diagnostic counters added.
- UART error callback now counts UART errors.
- Protocol, checksum, ACK format, ACK-first behavior, and duplicate handling remain unchanged.

## Prepared Output

- `NODE2_V8_1_IRQ_RING_RX_DIAGNOSTIC_HARDENED_main.c`
- `V8_1_NODE2_DIAGNOSTIC_HARDENING_TEST_PLAN.md`

## Next Objective

Run 100 or more packets and capture diagnostic counters.

The most important counters are:

- `checksum_fail`
- `bad_len`
- `parser_timeout`
- `ring_overflow`
- `uart_error`

