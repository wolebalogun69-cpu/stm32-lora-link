# V8.0 21-Cycle Error Observation

Date: 2026-06-25

## Observation

During V8.0 Node 2 interrupt RX ring-buffer testing, one error appeared after approximately 21 cycles.

## Pending Details Needed

The exact error type is not yet known.

Required next details:

- `CHUNK_DELAY_MS` used during the test
- Node 1 error line
- Node 2 error line
- Whether the error was:
  - `CHECKSUM FAIL`
  - `ACK TIMEOUT`
  - `FRAME FAILED`
  - `RX RING OVERFLOW`
  - `BAD LENGTH - RESYNC`

## Engineering Interpretation

If the test was at `250 ms`, this is an improvement over the previous 250 ms result, but it is not reliable enough to promote.

If the test was at `500 ms`, this is a regression from the V7.0 baseline and V8.0 needs correction before timing tests continue.

## Next Action

Classify the error before changing protocol or timing.

