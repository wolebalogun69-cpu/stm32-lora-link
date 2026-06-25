# V8.0 Node 2 IRQ Ring RX Update

Date: 2026-06-25

## Reason

V7.1 timing reduction to `CHUNK_DELAY_MS = 250` failed with checksum errors after only a small number of packets.

Checksum failures occur before ACK logic matters, so the next improvement targets the Node 2 receive path.

## Confirmed USART2 Interrupt Support

The Node 2 project has USART2 interrupt support enabled:

```c
HAL_NVIC_SetPriority(USART2_IRQn, 0, 0);
HAL_NVIC_EnableIRQ(USART2_IRQn);
```

`stm32l0xx_it.c` contains:

```c
void USART2_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart2);
}
```

## V8.0 Change

Node 2 now uses:

```text
USART2 interrupt RX
-> 1-byte HAL receive callback
-> RX ring buffer
-> parser consumes bytes from ring buffer
```

The packet format, checksum, ACK format, ACK-first behavior, and duplicate sequence handling are unchanged.

## Prepared Output

- `NODE2_V8_0_IRQ_RING_RX_DATA_RX_ACK_FIRST_DUPLICATE_SAFE_main.c`
- `V8_0_NODE2_IRQ_RING_RX_TEST_PLAN.md`

## Pin Mapping Correction

The provided Node 2 CubeMX USART2 configuration shows:

```text
USART2_TX -> PD5
USART2_RX -> PD6
```

This corrects the earlier project-memory assumption that Node 2 USART2 used `PA2/PA3`.

