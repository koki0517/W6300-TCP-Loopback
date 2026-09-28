# Hardware and wiring

## Parts and network

- NUCLEO-H723ZG（STM32H723ZGTx）
- WIZnet WIZ630io（W6300）
- WIZ630ioのRJ45をWindows PCのEthernet adapterへ接続
- NUCLEOのST-LINK USBをPCへ接続
- NUCLEOオンボードEthernet connectorは使いません。STM32内蔵ETHも使用しません。
- W6300の`INTn`は未接続です。firmwareにも割り込み入力を設定しません。

## WIZ630io J3 wiring

| WIZ630io | Signal | NUCLEO-H723ZG connector | MCU pin / function |
| --- | --- | --- | --- |
| J3-1 | QD0 | CN10-23 | PD11 / QSPI_BK1_IO0 |
| J3-2 | QD1 | CN10-21 | PD12 / QSPI_BK1_IO1 |
| J3-3 | QD2 | CN9-26 | PF7 / OCTOSPI1_IO2 |
| J3-4 | QD3 | CN10-19 | PD13 / QSPI_BK1_IO3 |
| J3-5 | GND | GND | Ground |
| J3-6 | QSPI_CLK | CN10-15 | PB2 / QSPI_CLK |
| J3-7 | QSPI_NCS | CN10-13 | PG6 / QSPI_CS |
| J3-8 | 3V3_IN | CN8-7 | 3V3 |
| RSTn | Reset, active low | CN10-7 | PF4 / `W6300_RSTn` |
| INTn | Interrupt, unused | Not connected | Not configured |

現在の配線ではWIZ630io J3-3/QD2をCN9-26/PF7へ接続し、PE2は使用しません。SB67は変更せず、OFFにする必要もありません。この配線はPE2とSAI_A_MCLKの共有を避けています。

## PC IPv4 configuration

Set the Ethernet adapter connected to the WIZ630io to:

| Setting | Value |
| --- | --- |
| IPv4 address | `192.168.0.20` |
| Subnet mask | `255.255.255.0` |
| Default gateway | Blank / none |
| DNS server | Blank / none |

The WIZ630io uses `192.168.0.10/24`, with gateway and DNS set to `0.0.0.0`. Windows may label this direct link **Unidentified network** or **No Internet**. That is expected: the link is only for the local TCP test and does not provide Internet access.
