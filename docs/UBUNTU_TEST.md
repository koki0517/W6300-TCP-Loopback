# Ubuntu 22.04 build and hardware validation

This guide describes how to build, flash, and test the project on Ubuntu. On a fresh clone, first follow [the CubeMX regeneration steps](FIRMWARE.md#cubemx-regeneration-and-dependency-updates); the HAL/CMSIS and generated support files are ignored. The validation summary records both the earlier corruption and the later passing Quad setup; machine-specific USB paths and temporary UART log names are omitted.

## Tools and attached devices

Discover tool paths and interfaces on the current host instead of assuming fixed names:

```bash
uname -a
lsb_release -a
git --version
python3 --version
find /opt /usr/local "$HOME" -iname 'stm32cubeide*' 2>/dev/null
find /opt /usr/local "$HOME" -iname 'STM32_Programmer_CLI' 2>/dev/null
lsusb
ls -l /dev/ttyACM* /dev/serial/by-id/ 2>/dev/null
ip -br link
ip -br addr
ip route
```

Probe `ST-LINK` with the discovered `CubeProgrammer` executable:

```bash
/path/to/STM32_Programmer_CLI -c port=SWD
```

If access is denied, inspect the USB device permissions, groups, and udev rules before changing permissions. A permission error is not a firmware failure.

The `ST-LINK` `VCP` is typically `/dev/ttyACM*`. Identify it from `/dev/serial/by-id/` and current kernel messages. USART3 uses `115200 baud`, `8-N-1`, no flow control. A basic capture uses standard tools:

```bash
VCP=/dev/ttyACM0   # replace with the device found on this host
stty -F "$VCP" 115200 cs8 -cstopb -parenb -ixon -ixoff -crtscts raw
timeout 60s cat "$VCP" > startup-uart.log
```

Identify the USB Ethernet adapter connected to `WIZ630io` using lsusb, ip -br link, and, if available, ethtool -i <iface> or /sys/class/net/<iface>/device/driver. Do not assume its interface name.

## Build and flash

Use the generated CubeIDE `Debug Makefile` with CubeIDE's bundled `Arm GCC`. Locate `arm-none-eabi-gcc` under the CubeIDE installation and set `CUBE_GCC_BIN` to its containing tools/bin directory:

```bash
CUBE_GCC_BIN=/path/to/STM32CubeIDE/plugins/.../tools/bin
env PATH="$CUBE_GCC_BIN:/usr/bin:/bin" make -C Debug clean all -j4
```

On the measured Ubuntu host this used `CubeIDE 1.16.1` and its bundled `GCC 12.3.0`. The clean build completed with zero errors and `14 warnings` from the unmodified ioLibrary `Application/loopback/loopback.c`.

Program, verify, and reset the target:

```bash
/path/to/STM32_Programmer_CLI \
  -c port=SWD -w Debug/W6300-TCP-Loopback.elf -v -rst
```

The recorded test host used `CubeProgrammer` 2.17.0. After reset, save the startup UART output and confirm the selected QSPI mode, `CIDR`/VER, HAL status, PHY link, and `TCP LISTEN` state.

## Ethernet and TCP test

Confirm the correct `WIZ630io` adapter before changing its address. If `192.168.0.20/24` is already present, leave it unchanged. Otherwise configure only that interface:

```bash
sudo ip link set <iface> up
sudo ip addr add 192.168.0.20/24 dev <iface>
```

Do not flush addresses or alter other interfaces. When UART reports `PHY link UP`, run:

```bash
ping -I <iface> -c 3 192.168.0.10
ip neigh show 192.168.0.10 dev <iface>
python3 tools/tcp_loopback_test.py --host 192.168.0.10 --source 192.168.0.20
```

The default Python matrix tests 1, 64, 512, 1460, 2048, 4096, 16384, and 65536 bytes. --count opens a fresh connection for each iteration:

```bash
python3 tools/tcp_loopback_test.py --size 64 --count 1000 --source 192.168.0.20
```

The W6300 service briefly closes and reopens its listening socket between connections. The client retries initial `ECONNREFUSED` for up to 250 ms; retries do not count as payload failures.

## Golden Reference comparison

The known-working Golden Reference is Robomech-NHK/Boards_2026/Firmware/UDP2CANFD. The board KiCad/BOM lists `STM32H730VBTx`, while UDP2CANFD.ioc targets `STM32H723VET6`. This project targets `STM32H723ZGTx`. The H723 Cube/HAL target makes the OCTOSPI setup a meaningful comparison; the physical pin assignments remain board-specific.

| Item | UDP2CANFD Golden Reference | This NUCLEO project | Difference |
| --- | --- | --- | --- |
| MCU | Board: `STM32H730VBTx`; .ioc: `STM32H723VET6` | `STM32H723ZGTx` | Physical MCU/package differs; Cube target is H723 in both. |
| `CubeMX` / `STM32CubeH7` | 6.16.1 / 1.12.1 | 6.12.0 / 1.11.2 | Software versions differ. |
| OCTOSPI kernel clock | `D1HCLK`, `275 MHz` | `PLL2R`, `76 MHz` | Clock source differs. |
| `ClockPrescaler` | 6 (HAL register divider 6) | 79 (HAL register divider 79) | Divider differs. |
| Actual `SCK` | 45.833 MHz | `0.962025 MHz` | Meaningful timing difference. |
| `ClockMode` | `Mode 0` | `Mode 0` | Same. |
| `SampleShifting` | None | None | Same. |
| `DelayBlockBypass` | Enabled | Enabled | Same. |
| `DelayHoldQuarterCycle` | Disabled | Disabled | Same. |
| `FifoThreshold` | 1 | 1 | Same. |
| `DeviceSize` | 32 | 17 | Configured difference; explicit indirect addresses are 16-bit. |
| `ChipSelectHighTime` | 1 cycle | 2 cycles | Configured difference. |
| `MemoryType` | MICRON | MICRON | Same. |
| OSPIM CLK/NCS ports | Port 1 / Port 1 | Port 1 / Port 1 | Same. |
| OSPIM IO group | Port 1 HIGH, `IO4–IO7` | Port 1 LOW, `IO0–IO3` | Board pin groups differ; each selects the wired group. |
| GPIO pins / AF | PA3 `AF12` CLK; PB10 `AF9` NCS; PC1 `AF10` IO4; PC2/PC3 AF4 IO5/IO6; PE10 `AF10` IO7 | PB2 `AF9` CLK; PG6 `AF10` NCS; PD11/PD12/PD13 `AF9` IO0/IO1/IO3; `PF7` `AF10` IO2 | Pin assignment differs by board/package. |
| GPIO speed / pull | `Very high` / `no pull` | `Very high` / `no pull` | Same. |
| Instruction phase | `1 line`, `8 bits` | `1 line`, `8 bits` | Same. |
| Address phase | `4 lines` in Quad | `4 lines` in Quad | Same. |
| Data phase | `4 lines` in Quad | `4 lines` in Quad | Same. |
| Address size | `16 bits` | `16 bits` | Same. |
| Dummy bits | 8 | 8 | Same per W6300 specification. |
| HAL dummy cycles | 2 in Quad | 2 Quad / 4 Dual / 8 Single | Same for Quad; phase width changes cycles in other modes. |
| Opcode reconstruction | Mask the opcode to six bits, then OR 0x80 | Mask the opcode to six bits, then OR the active mode bits | Same Quad opcode; this firmware also selects Single/Dual bits. |
| DQS / DTR | Disabled / disabled | Disabled / disabled | Same. |
| SIOO | Instruction every command | Instruction every command | Same. |
| Transfer API | `HAL_OSPI_Command` + `HAL_OSPI_Receive`/Transmit | Same | Same. |
| NCS control | OCTOSPI `hardware NCS` | OCTOSPI `hardware NCS` | Same. |
| Reset low / post-reset delay | `10 ms` / `100 ms` | `100 ms` / `100 ms` | Reset-low time differs. |
| Critical callbacks | IRQ disable / enable | Normally no-op for HAL timeout progress | Callback policy differs. |

The Golden setup uses `D1HCLK` and 45.833 MHz `SCK`. The Quad transaction format (1-line instruction, 4-line address/data, 16-bit address, `8 dummy bits`, `Mode 0`, `hardware NCS`, DQS/DTR off) matches this project. The two boards use different `OCTOSPIM` pin groups: HAL `IOPORT_1_LOW` selects P1 IO[3:0], while `IOPORT_1_HIGH` selects P1 IO[7:4]. This project uses `IO0–IO3`. The selected STM32CubeH7 HAL package implements this selection in `HAL_OSPIM_Config`; after code generation, its source is available at `Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_ospi.c`. Register definitions are documented in [`RM0468`](https://www.st.com/resource/en/reference_manual/rm0468-stm32h723733-stm32h725735-and-stm32h730-value-line-advanced-armbased-32bit-mcus-stmicroelectronics.pdf). W6300 mode bits and the fixed 8-bit dummy phase are defined by the [W6300 datasheet](https://docs.wiznet.io/assets/files/20251204_W6300_DS_V101E-4f4cd2e75de8d76f51a741f6a492ea01.pdf) and [WIZnet documentation](https://docs.wiznet.io/Product/Chip/Ethernet/W6300).

## Validation results

### Current validated setup

The current test used `Mode 0`, no sample shift, `DeviceSize=17`, `PLL2R` at `76 MHz` with prescaler `79`, `ChipSelectHighTime=2`, and `SCK` at `0.962025 MHz`. Hardware NCS and the Golden 1-4-4 transaction format were retained. `J3-5 GND` was connected to NUCLEO GND in addition to `J2 GND`. A later retest after changing the 3.3 V pickup point repeated the diagnostic and TCP checks; the exact NUCLEO connector used for that pickup was not recorded.

| Check | Result |
| --- | --- |
| `Single 1-1-1` identity | `CIDR` `0x61` 100/100; API `CIDR` `0x6300`; RTL `0x11`; VER `0x4661` 100/100; `SYSR` `0x01`; `HAL errors 0` |
| `Dual 1-2-2` identity | Same values; 100/100; `HAL errors 0` |
| `Quad 1-4-4` identity | Same values; 100/100; `HAL errors 0` |
| TX-buffer read/write, Single and Dual | All lengths 1, 2, 3, 4, 7, 8, 15, 16, 31, 32, 63, 64, 127, 256 passed |
| TX-buffer read/write, Quad | Same lengths passed |
| Quad direction checks | Single write to Quad read, chunked reads, bytewise reads, and Quad write to Single read passed |
| ICMP | Ping passed 3/3 |
| TCP default matrix | 1, 64, 512, 1460, 2048, 4096, 16384, 65536 bytes passed (90,101 bytes total) |
| TCP reconnect matrix | 64 bytes × `1,000 fresh connections` passed (64,000 matching bytes) |

The final TCP firmware contains no bring-up diagnostic modes. UART reported Quad identity passed, `HAL errors 0`, `PHY link UP`, and `TCP LISTEN` on `port 5000`. The default client matrix and reconnect matrix passed on the same Quad firmware. Connection retries may occur while the single W6300 socket returns to LISTEN after a close; no payload mismatch or failed completed connection was recorded. The buffer diagnostics above are retained as historical validation results.

A regression retest after removing the bring-up code was performed on 2026-09-29 using the generated files retained in the local working copy. CubeIDE 1.16.1 clean Debug build completed with `0 errors` and `14 warnings`; CubeProgrammer 2.17.0 programmed, verified, and reset the target. UART confirmed Quad identity (`CIDR 0x61`, `VER 0x4661`, `HAL errors 0`), `PHY link UP`, and `TCP LISTEN`. Ping passed `3/3`, the eight-size matrix passed (`90,101 bytes`), and `64 bytes × 1,000` fresh connections passed (`64,000 bytes`). A fresh clone still needs CubeMX Code Generate before build because the generated HAL/CMSIS tree is ignored.

### Diagnostic history

The historical `PE2` firmware setting did not establish the physical QD2 route. Single and Dual identity passed while Quad identity failed (0/100, `HAL errors 0`), including a 45.833 MHz Golden-clock run. Those measurements describe the selected firmware settings only; historical physical wiring and `SB67` state were not independently verified.

With the current `PF7` route, an earlier low-speed run passed identity in all three modes but had Quad payload corruption: lengths through 7 bytes passed and length 8 failed at offset 4 (`0x0F` read as `0x03`). A 1-byte TCP echo passed, while a 64-byte echo mismatched. `HAL errors` remained zero. The earlier network subnet readback was corrupted as well.

Two single-variable A/B tests did not change that payload result:
- `DeviceSize=32` with `PLL2R` `76 MHz` and `0.962025 MHz` `SCK`.
- `D1HCLK` `275 MHz` with the prescaler set for approximately 1 MHz `SCK` (measured `1.074218 MHz`).

A two-channel scope capture at the `WIZ630io` showed two `NCS-low` intervals, each containing `16 SCK pulses` for one-byte Quad reads. Connecting `J3-5 GND` in addition to `J2 GND` made the observed waveform cleaner. This alone did not establish the root cause. With both grounds connected, reversing the two-read order or inserting a `1 ms gap` made the repeated read pass; the original order without a gap still failed. Changing only `ChipSelectHighTime` from 1 to 2 cycles then made the original no-gap order pass for 11,000 pairs with zero mismatches and zero `HAL errors`.

After this setup change, the full buffer matrix and Quad TCP checks passed. Since the final successful setup included both the additional ground connection and `ChipSelectHighTime`=2, their individual contributions were not isolated. `DeviceSize=17` and `PLL2R` were retained; the unsuccessful A/B values are not the current defaults.

## Troubleshooting

| Symptom | Check |
| --- | --- |
| `ST-LINK` probe is denied | Check USB permissions, groups, and udev rules. |
| `VCP` is missing | Recheck lsusb, `/dev/serial/by-id/`, and `/dev/ttyACM*`; device names vary. |
| Ping fails | Confirm the selected interface, its `192.168.0.20/24` address, PHY link, and neighbor entry. |
| TCP connect fails | Confirm UART shows `TCP LISTEN` on `port 5000` and that the test uses source `192.168.0.20`. |
| Quad payload corruption returns | Check the two ground connections, current QD2 route to `PF7`, and `ChipSelectHighTime`=2. The verified test conditions are listed above. |

The Windows Single-fallback test is described in [WINDOWS_TEST.md](WINDOWS_TEST.md).
