# Node 2 ACK-First Patch

Apply this change to the current Node 2 receiver after checksum passes.

Current behavior:

```c
dbg_print("MESSAGE: ");
dbg_print(payload);
dbg_print("\r\nSending ACK...\r\n");

send_ack(sequence);

dbg_print("ACK SENT\r\n\r\n");
```

Recommended diagnostic behavior:

```c
send_ack(sequence);

char msg[120];
sprintf(msg, "\r\nFRAME OK SEQ=%lu\r\n", (unsigned long)sequence);
dbg_print(msg);

dbg_print("MESSAGE: ");
dbg_print(payload);
dbg_print("\r\nACK SENT\r\n\r\n");
```

Reason:

Node 2 already validated the packet. Send the ACK immediately, before debug UART output. This removes debug printing as a possible ACK delay source and matches the future reliable protocol behavior.

If ACK still times out after this patch, the problem is likely one of:

- Node 2 LoRa TX path not reaching Node 1
- Node 1 LoRa RX path not receiving
- LoRa module turnaround delay
- Node 1 ACK parser receiving unexpected bytes

