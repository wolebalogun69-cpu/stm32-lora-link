# V7.0 Test Plan

## Goal

Confirm a clean reliable ACK/retry baseline without raw diagnostic byte spam.

## Firmware Under Test

- Node 1: `NODE1_V7_0_RELIABLE_TX_ACK_RETRY_main.c`
- Node 2: `NODE2_V7_0_DATA_RX_ACK_FIRST_DUPLICATE_SAFE_main.c`

Node 2 file now includes the STM32L073 `SystemClock_Config()` using MSI range 5, no PLL, and voltage scale 1.

## Expected Normal Output

### Node 1

```text
NODE 1 V7.0 RELIABLE TX + ACK RETRY READY

TX SEQ=1 ATTEMPT=1
ACK OK SEQ=1

TX SEQ=2 ATTEMPT=1
ACK OK SEQ=2
```

### Node 2

```text
NODE 2 V7.0 DATA RX + ACK FIRST + DUPLICATE SAFE READY

FRAME OK SEQ=1
MESSAGE: HELLO123ABCD
ACK SENT
```

## Duplicate Test

Temporarily block or disrupt Node 1 ACK reception after Node 2 receives a valid frame. Node 1 should retry the same sequence.

Expected Node 2 behavior on repeated sequence:

```text
DUPLICATE SEQ=1 - ACK RESENT
```

Node 2 should resend ACK, but should not process the same payload as a new message.

## Pass Criteria

- Node 1 advances sequence only after `ACK OK`.
- Node 2 sends ACK immediately after checksum success.
- Duplicate sequence is ACKed again without normal message processing.
- No raw `RX:xx` debug appears in Node 1 normal output.

## Next Step After Pass

Start timing reduction tests by lowering `CHUNK_DELAY_MS` in controlled steps:

1. 500 ms
2. 250 ms
3. 100 ms
4. 50 ms
5. 20 ms
6. 10 ms

Do not start audio transport until the selected chunk delay is reliable over repeated packet tests.
