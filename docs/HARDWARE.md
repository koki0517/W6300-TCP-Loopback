# Hardware and wiring

## Hardware

- `NUCLEO-H723ZG` (`STM32H723ZG`)
- WIZnet `WIZ630io` (`W6300`)
- Connect `WIZ630io` RJ45 directly to the PC Ethernet adapter.
- Connect the NUCLEO ST-LINK USB to the PC.
- The NUCLEO onboard Ethernet connector and `W6300` `INTn` are unused.

## `WIZ630io` connections

| `WIZ630io` | Signal | `NUCLEO-H723ZG` | OCTOSPI / GPIO |
| --- | --- | --- | --- |
| `J3-1` | `QD0` | `CN10-23` / D30 | `PD11` / `P1_IO0` |
| `J3-2` | `QD1` | `CN10-21` / D29 | `PD12` / `P1_IO1` |
| `J3-3` | `QD2` | `CN9-26` / D62 | `PF7` / `P1_IO2` |
| `J3-4` | `QD3` | `CN10-19` / D28 | `PD13` / `P1_IO3` |
| `J3-5` | GND | NUCLEO GND | Ground return |
| `J3-6` | QSPI_CLK | `CN10-15` / D27 | `PB2` / `P1_CLK` |
| `J3-7` | QSPI_NCS | `CN10-13` / D26 | `PG6` / `P1_NCS` |
| `J3-8` | 3V3_IN | NUCLEO 3.3 V rail | Supply |
| `RSTn` | Active-low reset | `CN10-7` | `PF4` / `W6300_RSTn` |
| `INTn` | Interrupt, unused | Not connected | — |

For the validated Quad setup, connect `J3-5` GND to NUCLEO GND in addition to the `WIZ630io` J2 GND connection. The latest validation followed a change to the 3.3 V pickup point, but the exact NUCLEO connector location was not recorded. An older note listed `CN8-7`; treat that specific pin as unconfirmed.

`QD2` currently runs from `J3-3` to `PF7`. Firmware configures `PF7` `AF10` as OCTOSPIM Port 1 IO2; IO0–IO3 use Port 1 LOW. `PE2` is not used. `SB67` is outside the external `PF7` signal path; the state of its chip resistor has not been verified. For board pin and alternate-route details, see [ST UM2407](https://www.st.com/resource/en/user_manual/um2407-stm32h7-nucleo144-board-stmicroelectronics.pdf) and the [`STM32H723ZG` datasheet](https://www.st.com/resource/en/datasheet/stm32h723zg.pdf).

## PC Ethernet settings

Configure only the adapter connected to `WIZ630io`:

| Setting | Value |
| --- | --- |
| IPv4 address | `192.168.0.20` |
| Subnet mask | `255.255.255.0` (/24) |
| Gateway | None |
| DNS | None |

The `W6300` uses `192.168.0.10/24`; gateway and DNS are `0.0.0.0`. Platform-specific setup is in [WINDOWS_TEST.md](WINDOWS_TEST.md) and [UBUNTU_TEST.md](UBUNTU_TEST.md).
