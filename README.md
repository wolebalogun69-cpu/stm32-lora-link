# STM32 LoRa Link

Reliable STM32-to-STM32 wireless communication using Four-Faith F8L10A-N-H LoRa modules over UART.

## Hardware

- Node 1: STM32F0308-DISCO
- Node 2: STM32L073VZT6
- Radio: Four-Faith F8L10A-N-H LoRa modules
- Interface: UART

## Current Focus

The current engineering objective is reliable ACK sequence decode, followed by bounded retry logic and then audio packet transport.

Project memory and engineering decisions are tracked in:

- [Project State](docs/PROJECT_STATE.md)
- [Engineering Log](docs/ENGINEERING_LOG.md)

