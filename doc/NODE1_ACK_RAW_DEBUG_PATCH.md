# Node 1 ACK Raw Debug Patch

For the next test, temporarily add this debug print inside `wait_for_ack()` immediately after a byte is successfully received:

```c
if (HAL_UART_Receive(&huart2, &b, 1, 1000) != HAL_OK)
{
    continue;
}

char rx_msg[24];
sprintf(rx_msg, "RX:%02X\r\n", b);
dbg_print(rx_msg);
```

Expected result if ACK reaches Node 1:

```text
RX:B6
RX:6B
RX:01
RX:00
ACK OK SEQ=1
```

If Node 1 prints no `RX:` bytes while Node 2 prints `ACK SENT`, then the issue is not the parser. It is the return radio/UART path.

If Node 1 prints different bytes, the issue is ACK corruption, unexpected framing, or stale bytes in the RX stream.

