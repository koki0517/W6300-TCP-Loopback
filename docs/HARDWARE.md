# Hardware and wiring

## Parts and network

- NUCLEO-H723ZG（STM32H723ZGTx）
- WIZnet WIZ630io（W6300）
- WIZ630ioのRJ45をテストPCのEthernet adapterへ接続
- NUCLEOのST-LINK USBをPCへ接続
- NUCLEOオンボードEthernet connectorは使いません。STM32内蔵ETHも使用しません。
- W6300の`INTn`は未接続です。firmwareにも割り込み入力を設定しません。

## WIZ630io J3 wiring

| WIZ630io | Signal | NUCLEO-H723ZG connector | MCU pin / function |
| --- | --- | --- | --- |
| J3-1 | QD0 | CN10-23 / D30 | PD11 / OCTOSPIM_P1_IO0 |
| J3-2 | QD1 | CN10-21 / D29 | PD12 / OCTOSPIM_P1_IO1 |
| J3-3 | QD2 | CN9-26 / D62 | PF7 / OCTOSPIM_P1_IO2 |
| J3-4 | QD3 | CN10-19 / D28 | PD13 / OCTOSPIM_P1_IO3 |
| J3-5 | GND | GND | Ground |
| J3-6 | QSPI_CLK | CN10-15 / D27 | PB2 / OCTOSPIM_P1_CLK |
| J3-7 | QSPI_NCS | CN10-13 / D26 | PG6 / OCTOSPIM_P1_NCS |
| J3-8 | 3V3_IN | CN8-7 | 3V3 |
| RSTn | Reset, active low | CN10-7 | PF4 / `W6300_RSTn` |
| INTn | Interrupt, unused | Not connected | Not configured |

QSPI test wiring includes WIZ630io J3-5 GND connected to NUCLEO GND, in addition to the already connected WIZ630io J2 GND. After adding the J3 ground, the user observed a cleaner waveform at the WIZ630io. Keep the J3 ground connected for the current Quad test setup; the paired ground and firmware timing experiments are recorded in [UBUNTU_TEST.md](UBUNTU_TEST.md).

**QD2:** 現在の配線はWIZ630io J3-3からPF7（CN9-26/D62）です。CubeMX/HAL MSPはPF7 AF10、OCTOSPIM Port 1 LOW group（IO0〜IO3）として設定しています。SB67はPE2の基板内routeに関係しますが、PF7への外部配線には入りません。SB67上のチップ抵抗の実装状態は確認していません。過去の物理配線状態は独立に確認できていないため、過去の試験記録から推定しません。PF7配線の導通確認が必要な場合は、WIZ630io J3-3からNUCLEO CN9-26/D62までを測定してください。[ST UM2407](https://www.st.com/resource/en/user_manual/um2407-stm32h7-nucleo144-board-stmicroelectronics.pdf)と[ST STM32H723ZG datasheet](https://www.st.com/resource/en/datasheet/stm32h723zg.pdf)を参照。

## PC IPv4 configuration

Set the Ethernet adapter connected to the WIZ630io to:

| Setting | Value |
| --- | --- |
| IPv4 address | `192.168.0.20` |
| Subnet mask | `255.255.255.0` |
| Default gateway | Blank / none |
| DNS server | Blank / none |

The WIZ630io uses `192.168.0.10/24`, with gateway and DNS set to `0.0.0.0`. Windows may label this direct link **Unidentified network** or **No Internet**. That is expected: the link is only for the local TCP test and does not provide Internet access.
