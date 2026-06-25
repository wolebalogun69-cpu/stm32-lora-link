# V7.0 Success Update

Date: 2026-06-23

## Result

V7.0 was tested successfully on both nodes.

Confirmed full round trip:

```text
Node 1 data frame
-> LoRa
-> Node 2 checksum OK
-> Node 2 ACK-first response
-> LoRa
-> Node 1 matching ACK decode
-> Node 1 sequence advances
```

## Current Known-Good Baseline

- Node 1: `NODE1_V7_0_RELIABLE_TX_ACK_RETRY_main.c`
- Node 2: `NODE2_V7_0_DATA_RX_ACK_FIRST_DUPLICATE_SAFE_main.c`

## Confirmed Features

- Data frame format works.
- ACK frame format works.
- Retry mechanism is present.
- Sequence increments only after ACK success.
- Node 2 sends ACK immediately after checksum success.
- Node 2 duplicate sequence handling is present.

## Next Objective

Begin controlled timing reduction.

Recommended delay test sequence:

1. `CHUNK_DELAY_MS = 500`
2. `CHUNK_DELAY_MS = 250`
3. `CHUNK_DELAY_MS = 100`
4. `CHUNK_DELAY_MS = 50`
5. `CHUNK_DELAY_MS = 20`
6. `CHUNK_DELAY_MS = 10`

Keep frame format, ACK format, retry count, checksum, and duplicate handling unchanged during these tests.

