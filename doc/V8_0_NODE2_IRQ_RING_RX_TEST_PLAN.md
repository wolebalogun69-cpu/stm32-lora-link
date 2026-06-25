# V8.0 Node 2 IRQ Ring RX Test Plan

## Goal

Replace Node 2 blocking polling UART receive with interrupt-driven byte capture into a ring buffer.

Node 1 remains on the V7.0 reliable sender/retry baseline.

## Firmware Under Test

- Node 1: `NODE1_V7_0_RELIABLE_TX_ACK_RETRY_main.c`
- Node 2: `NODE2_V8_0_IRQ_RING_RX_DATA_RX_ACK_FIRST_DUPLICATE_SAFE_main.c`

## What Changed On Node 2

- `HAL_UART_Receive(&huart2, &b, 1, 5000)` removed from the main parser path.
- `HAL_UART_Receive_IT(&huart2, &lora_rx_byte, 1)` starts continuous receive.
- `HAL_UART_RxCpltCallback()` pushes bytes into a ring buffer.
- Main loop parses bytes from the ring buffer.
- ACK-first and duplicate sequence handling are preserved.

## Expected Startup

Node 2:

```text
NODE 2 V8.0 IRQ RING RX + ACK FIRST READY
```

Node 1:

```text
NODE 1 V7.0 RELIABLE TX + ACK RETRY READY
```

## Pass Criteria At 500 ms

- Node 2 prints `FRAME OK`.
- Node 1 prints `ACK OK`.
- No `CHECKSUM FAIL`.
- No `RX RING OVERFLOW`.
- Sequence numbers continue increasing.

## Timing Retest After 500 ms Passes

Only after 500 ms passes, retest:

1. `CHUNK_DELAY_MS = 250`
2. `CHUNK_DELAY_MS = 100`
3. `CHUNK_DELAY_MS = 50`

Stop at the first timing that produces:

- `CHECKSUM FAIL`
- `FRAME FAILED`
- repeated `ACK TIMEOUT`
- `RX RING OVERFLOW`

## Notes

USART2 interrupt plumbing has been confirmed:

- `HAL_NVIC_SetPriority(USART2_IRQn, 0, 0)`
- `HAL_NVIC_EnableIRQ(USART2_IRQn)`
- `USART2_IRQHandler()` calls `HAL_UART_IRQHandler(&huart2)`

Current CubeMX USART2 pin mapping shown by the provided Node 2 project:

- `PD5 -> USART2_TX`
- `PD6 -> USART2_RX`

