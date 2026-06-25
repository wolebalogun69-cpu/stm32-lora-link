# V8.0 Runtime Result

Date: 2026-06-25

## Result

V8.0 Node 2 interrupt RX ring buffer is significantly better than the previous blocking polling receiver.

Observed run:

- Around 60 cycles completed so far.
- One `BAD LENGTH` occurred after about 20 cycles.
- One `CHECKSUM FAIL` occurred during the run.
- The system recovered and continued operating.

## Interpretation

This is a good recovery result, but not yet a clean reliability baseline.

Positive signs:

- Interrupt RX is working.
- Ring buffer parser is working.
- Parser resynchronization is working.
- ACK/retry behavior lets the system move past bad frames.

Remaining concern:

- A reliable transport layer, especially for future audio, should not regularly produce malformed lengths or checksum failures.

## Next Engineering Step

Prepare V8.1 receiver hardening:

1. Increase RX ring size.
2. Add parser timeout reset for partial frames.
3. Add counters for:
   - frames OK
   - checksum failures
   - bad lengths
   - duplicates
   - ring overflows
   - UART errors
4. Keep protocol unchanged.
5. Keep Node 1 unchanged during the first V8.1 test.

