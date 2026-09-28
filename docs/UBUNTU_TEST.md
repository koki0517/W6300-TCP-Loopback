# Ubuntu 22.04 build and hardware validation

This document records the Linux handoff from the Windows-tested firmware and the Ubuntu 22.04 measurements made on 2026-09-28. The board was already wired to the WIZ630io; the host network changes below are limited to that direct-link adapter.

## Handoff baseline

- The repository was fetched from `origin/main` before testing. Start HEAD and current `origin/main` are both `6a4a292149fe129a4f9c9abfc8c360f48faf92d5` (`Implement W6300 TCP loopback and QSPI diagnostics`). The required Windows handoff commit is included.
- The pinned ioLibrary submodule is `3e01f80f82773c10cdead6be4332e7cd116cb323`.
- Initial `git status --short` was empty. No commit or push was made.
- Host: Ubuntu 22.04.5 LTS, kernel `6.8.0-138-generic`.
- STM32CubeIDE: `/opt/st/stm32cubeide_1.16.1_2`, version 1.16.1.
- STM32CubeProgrammer: `/home/koki/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI`, version 2.17.0.
- Python 3 and Git are provided by the Ubuntu host. No Python package was installed for the loopback client.

## Find the tools and attached devices

Do not assume an IDE, Programmer, VCP, or Ethernet interface path. These commands show what is installed and connected:

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

The attached ST-LINK V3 appeared as USB `0483:374e`, serial `002E00343532511131333430`, firmware `V3J6M2`. CubeProgrammer identified a NUCLEO-H723ZG, Device ID `0x483`, Rev Z, target voltage 3.25 V. Probe it with:

```bash
/home/koki/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI -c port=SWD
```

If USB permission is denied, inspect the device node named by `lsusb`, `groups`, and the installed udev rules before changing permissions. Here the device node was accessible and the ST-LINK probe worked. No ST-LINK firmware update was run.

The VCP appeared as `/dev/ttyACM0`, with stable symlink `/dev/serial/by-id/usb-STMicroelectronics_STLINK-V3_002E00343532511131333430-if02`. Discover it on each host rather than assuming the number. The USART3 settings are 115200 baud, 8-N-1, no flow control. `picocom` was not installed; the built-in `stty` and `cat` were sufficient to save logs without installing pyserial:

```bash
stty -F /dev/ttyACM0 115200 cs8 -cstopb -parenb -ixon -ixoff -crtscts raw
timeout 60s cat /dev/ttyACM0 > /tmp/w6300-startup-uart.log
```

The Ethernet adapter connected to WIZ630io was `enx04ab18c5869b`, USB `0b95:1790` (ASIX AX88179), using driver `ax88179_178a`. `ethtool` was not installed; the driver was identified from `/sys/class/net/<iface>/device/driver`. No packages were added. Wi-Fi and unrelated interfaces were left alone.

## Build and flash

CubeIDE GUI/headless launcher was not usable in this session because it had no display and its default Eclipse configuration path was not writable. The generated CubeIDE `Debug` Makefile was built directly using CubeIDE's bundled GCC 12.3.0. This is the measured clean-build command:

```bash
CUBE_GCC_BIN=/opt/st/stm32cubeide_1.16.1_2/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.12.3.rel1.linux64_1.0.200.202406132123/tools/bin
env PATH="$CUBE_GCC_BIN:/usr/bin:/bin" make -C Debug clean all -j4
```

The clean Debug build completed with 0 errors and 14 warnings, all from the unmodified ioLibrary `Application/loopback/loopback.c`. The generated ELF is `Debug/W6300-TCP-Loopback.elf`. The system GCC was older and did not support a CubeIDE compiler flag, so the bundled compiler path is required on this host.

Program, verify, and reset the connected board with:

```bash
/home/koki/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI \
  -c port=SWD -w Debug/W6300-TCP-Loopback.elf -v -rst
```

CubeProgrammer reported `Download verified successfully` on each flash.

## Ethernet and TCP baseline

The direct-link adapter already had `192.168.0.20/24`; it was not changed. If that address is absent, first confirm the correct WIZ630io adapter from `lsusb`, `ip -br link`, and the driver path. Then only on that adapter:

```bash
sudo ip link set <iface> up
sudo ip addr add 192.168.0.20/24 dev <iface>
```

Do not flush addresses, disable NetworkManager, or change Wi-Fi. Once UART reports PHY link UP, run:

```bash
ping -I <iface> -c 3 192.168.0.10
ip neigh show 192.168.0.10 dev <iface>
python3 tools/tcp_loopback_test.py --host 192.168.0.10 --source 192.168.0.20
```

Before moving QD2 to PF7, normal firmware used Quad identity followed by Single fallback. Across the recorded pre-PF7 stages, ping passed 3/3, ARP resolved to `02:00:00:00:00:10`, and the default Python matrix passed all eight sizes (90,101 bytes). These are valid historical Single-fallback results, not current PF7 Quad payload results. Current PF7 results are recorded in the final section below.

On this W6300 server, socket 0 briefly leaves LISTEN while a connection closes and reopens. The client therefore retries only an initial `ECONNREFUSED` for up to 250 ms; the retry count is printed. The retry is platform-neutral and needed for a repeatable fresh-connection matrix. The Windows test procedure remains available in [WINDOWS_TEST.md](WINDOWS_TEST.md).

## Boards_2026 Golden Reference comparison

The known-working UDP2CANFD design is treated as a Golden Reference. Its KiCad/BOM physical MCU is STM32H730VBTx, while its `UDP2CANFD.ioc` target is STM32H723VET6. This project targets STM32H723ZGT6. The Cube/HAL target in the Golden `.ioc` is therefore the same H723 family as this NUCLEO; the different physical package and pinout do not make its OCTOSPI configuration irrelevant.

| Item | Boards_2026 UDP2CANFD | NUCLEO-H723ZG project | Difference classification |
| --- | --- | --- | --- |
| MCU | KiCad/BOM STM32H730VBTx; `.ioc` STM32H723VET6 | `.ioc` STM32H723ZGT6 | Physical MCU/package differs; Cube target family is H723 in both. |
| CubeMX / STM32CubeH7 | CubeMX 6.16.1 / FW H7 1.12.1 | CubeMX 6.12.0 / FW H7 1.11.2 | Software package version differs. The HAL meanings below were checked in both source trees. |
| OCTOSPI kernel | D1HCLK, 275 MHz | PLL2R, 76 MHz | Meaningful clock-source difference. |
| `ClockPrescaler` | `7-1` evaluates to 6; HAL writes 5 to DCR2, so divider is 6 | 79; HAL writes 78 to DCR2, so divider is 79 | Meaningful clock-divider difference. |
| Actual SCK | 275 MHz / 6 = 45.833 MHz (derived from `.ioc`, RCC, and HAL behavior) | 76 MHz / 79 = 0.962025 MHz | Meaningful timing difference. |
| `ClockMode` | Mode 0 | Mode 0 | Same. |
| `SampleShifting` | None | None | Same. |
| `DelayBlockBypass` | Bypassed | Bypassed | Same. |
| `DelayHoldQuarterCycle` | Disabled | Disabled | Same. |
| `FifoThreshold` | 1 | 1 | Same. |
| `DeviceSize` | 32 | 17 | Configured difference; both issue explicit 16-bit indirect addresses, so this is low-priority for register identity. |
| `ChipSelectHighTime` | 1 | 1 | Same. |
| `MemoryType` | MICRON | MICRON | Same. |
| OSPIM `ClkPort` / `NCSPort` | Port 1 / Port 1 | Port 1 / Port 1 | Same. |
| OSPIM `IOLowPort` | `HAL_OSPIM_IOPORT_1_HIGH`, physical P1 IO4–IO7 | `HAL_OSPIM_IOPORT_1_LOW`, physical P1 IO0–IO3 | Pin-group difference only; each selects the physical group actually wired on that board. Live NUCLEO PCR confirmed LOW is enabled. |
| GPIO pins / AF | PA3 AF12 CLK; PB10 AF9 NCS; PC1 AF10 IO4; PC2/PC3 AF4 IO5/IO6; PE10 AF10 IO7 | PB2 AF9 CLK; PG6 AF10 NCS; PD11/PD12/PD13 AF9 IO0/IO1/IO3; PF7 AF10 IO2 (current) | Package/pin assignment difference. Do not copy Golden pin names to the NUCLEO. |
| IO2 routing / SB67 | Port 1 HIGH group, IO4–IO7 | Port 1 LOW group, IO0–IO3; QD2 is externally wired to PF7 | Pin-group and external-pin difference only. SB67 controls the alternate PE2 board route and is outside the current PF7 path; its chip resistor is removed. |
| GPIO speed / pull | Very high / no pull | Very high / no pull | Same electrical GPIO configuration. |
| Instruction phase | 1 line, 8 bits | 1 line, 8 bits | Same. |
| Address phase | 4 lines | 4 lines | Same for Quad. |
| Data phase | 4 lines | 4 lines | Same for Quad. |
| Address size | 16 bits | 16 bits | Same. |
| Dummy phase | Fixed 8 bits | Fixed 8 bits | Same per W6300 datasheet. |
| HAL dummy cycles | 2 for Quad | 2 for Quad; 4 for Dual; 8 for Single | Same Quad encoding; Dual uses 8 bits / 2 lines = 4 cycles. |
| Opcode reconstruction | `(opcode & 0x3F) | 0x80` | `(opcode & 0x3F) | active_mode_bits` | Same in Quad; the NUCLEO callback applies `0x00`, `0x40`, or `0x80` for runtime Single/Dual/Quad. |
| DQS / DTR | Disabled / disabled | Disabled / disabled | Same. |
| SIOO | Send instruction every command | Send instruction every command | Same. |
| Transfer API | `HAL_OSPI_Command` then `HAL_OSPI_Receive` / `HAL_OSPI_Transmit` | Same | Same. |
| NCS control | OCTOSPI hardware NCS | OCTOSPI hardware NCS | Same; ioLibrary CS callbacks are no-ops on both. |
| Reset assert / post-reset delay | 10 ms / 100 ms | 100 ms / 100 ms | Reset-low duration differs; NUCLEO holds reset longer. |
| Critical callback | `__disable_irq()` / `__enable_irq()` | Normally no-op so SysTick advances HAL polling timeout | Both callback styles were measured; the current PF7 baseline uses no-op callbacks. Results are separated by wiring stage below. |

The STM32H7 HAL defines `HAL_OSPIM_IOPORT_1_LOW` as Port 1 IO[3:0] and `_HIGH` as Port 1 IO[7:4]. `HAL_OSPIM_Config` enables the corresponding `IOLEN` or `IOHEN` field in P1CR and selects the OCTOSPI instance. These definitions and the register layout are in the [STM32H7 HAL user manual](https://www.st.com/resource/en/user_manual/um2217-description-of-stm32h7-hal-and-lowlayer-drivers-stmicroelectronics.pdf) and [RM0468](https://www.st.com/resource/en/reference_manual/rm0468-stm32h723733-stm32h725735-and-stm32h730-value-line-advanced-armbased-32bit-mcus-stmicroelectronics.pdf). The checked-in HAL source in both FW 1.11.2 and FW 1.12.1 uses the same `ClockPrescaler - 1` register write and LOW/HIGH mapping.

W6300 defines Single, Dual, and Quad modes in the instruction and supports them in its official datasheet. The datasheet fixes the dummy phase at 8 bits in every mode: 8 Single cycles, 4 Dual cycles, and 2 Quad cycles. The pinned ioLibrary defines `QSPI_DUAL_MODE` as `0x40` and `QSPI_QUAD_MODE` as `0x80`. See the [W6300 datasheet](https://docs.wiznet.io/assets/files/20251204_W6300_DS_V101E-4f4cd2e75de8d76f51a741f6a492ea01.pdf) and [WIZnet W6300 documentation](https://docs.wiznet.io/Product/Chip/Ethernet/W6300). No transaction-phase setting was copied from an unofficial source.

### Live NUCLEO register check

Read registers with CubeProgrammer in HOTPLUG mode so the currently running firmware is not reset first. For example:

```bash
STM32_Programmer_CLI -c port=SWD mode=HOTPLUG \
  -r32 0x5200B404 4 -r32 0x52005008 4 -r32 0x5200500C 4 \
  -r32 0x52005100 4 -r32 0x52005108 4 -r32 0x52005110 4
```

On the low-speed Mode 0 diagnostic, SWD readback was:

| Register | Value | Interpretation |
| --- | --- | --- |
| `RCC_D1CCIPR` (`0x5802444C`) | `0x00000020` | OCTOSPI kernel source is PLL2R. |
| `OCTOSPIM_P1CR` (`0x5200B404`) | `0x02010101` | CLK, NCS, and IO[3:0] are enabled from OCTOSPI1; IO[7:4] enable is clear. |
| GPIO AF current PF7 configuration | GPIOF_MODER=`0xFFFFBDFF`; OSPEEDR=`0x0000C000`; PUPDR=`0`; AFRL=`0xA0000000` | PF7 AF10, very-high speed, no pull; P1CR=`0x02010101` routes Port 1 LOW (IO0–IO3). |
| GPIO AF historical PE2 configuration | GPIOE_MODER=`0xFFFFFFE7`; OSPEEDR=`0x00000030`; PUPDR=`0`; AFRL=`0x00000A00`; PF7 AFRL=`0` | Earlier PE2 AF10 configuration; later superseded by the physical PF7 reroute. |
| `OCTOSPI1_CR` | `0x10000001` | OCTOSPI enabled; FIFO threshold 1. |
| `OCTOSPI1_DCR1` | `0x00100008` | DeviceSize 17, Mode 0, delay block bypassed. |
| `OCTOSPI1_DCR2` | `0x0000004E` | Prescaler register 78, divisor 79. |
| `OCTOSPI1_DCR3` / `DCR4` | `0x00000000` / `0x00000000` | Chip-select boundary/max transfer and refresh remain zero. |
| `OCTOSPI1_CCR` | `0x03001301` | 1-line instruction, 4-line address, 16-bit address size, 4-line data. |
| `OCTOSPI1_TCR` | `0x00000002` | Two dummy cycles. |
| `OCTOSPI1_IR` / `AR` | `0x00000080` / `0x00002000` | Quad opcode and last diagnostic address (SYSR). |

The pre-PF7 and PE2-era results below preserve the chronological test record. At that time, some firmware builds selected PF7 while QD2 was physically on PE2; later PE2-matched tests also failed Quad identity. The user subsequently moved QD2 to PF7, after which all three identity modes passed. Do not use the earlier zero-CIDR observations as the current PF7 result.

## Historical Quad and Dual identity results before PF7 reroute

The first all-mode diagnostic and the Golden-clock retry below were run before the physical wiring correction was known; firmware used PF7 for IO2 while the wire was on PE2. They are retained as historical results, but their Quad failures do not test a complete IO2 route. The PE2-corrected rerun is recorded separately below. The all-mode diagnostic is isolated from network initialization. To reproduce the low-speed comparison, set `APP_QSPI_DIAGNOSTIC_ONLY` to `1U` and `W6300_OSPI_CLOCK_MODE` to `HAL_OSPI_CLOCK_MODE_0` in `App/Inc/app_config.h`; keep the normal PLL2R 76 MHz and prescaler 79. The firmware hardware-resets W6300 before each mode and reads raw CIDR 100 times, API CIDR, RTL, VER 100 times, SYSR, HAL status/state/error code, and error count.

| Mode | Opcode / phases / HAL dummy | CIDR raw | API CIDR / RTL | VER | SYSR | HAL status / errors |
| --- | --- | --- | --- | --- | --- | --- |
| Single 1-1-1 | `0x00`; 1-1-1; 8 cycles | `0x61`, 100/100 | `0x6300` / `0x11` | `0x4661`, 100/100 | `0x01` | 0 / 0 |
| Dual 1-2-2 | `0x40`; 1-2-2; 4 cycles | `0x61`, 100/100 | `0x6300` / `0x11` | `0x4661`, 100/100 | `0x01` | 0 / 0 |
| Quad 1-4-4 | `0x80`; 1-4-4; 2 cycles | `0x00`, 0/100 | `0x0000` / `0x00` | `0x0000`, 0/100 | `0x00` | 0 / 0 |

All three modes ran at 0.962025 MHz with Mode 0, no sample shift, DQS/DTR disabled, SIOO every command, and hardware NCS. A second comparison using the Golden `__disable_irq()`/`__enable_irq()` critical callbacks produced the same results. The diagnostic logs were saved during the run under `/tmp/w6300-ubuntu-single-dual-quad-uart.log` and `/tmp/w6300-ubuntu-golden-critical-quad-uart.log`.

An exact Golden-clock Quad retry also ran at Mode 0 with D1HCLK 275 MHz, `ClockPrescaler=6`, and 45.833 MHz SCK. The startup identity read returned raw CIDR `0x00`, API CIDR `0x0000`, RTL/VER/SYSR zero, and HAL errors zero. Single fallback at the same SCK read raw CIDR `0x61`, API CIDR `0x6300`, RTL `0x11`, VER `0x4661`, and SYSR `0x01`. Networking was not tested at this temporary 45.833 MHz setting. Its UART log is `/tmp/w6300-ubuntu-golden-quad-uart.log`.

### Historical PE2-matched diagnostic (2026-09-28)

The user confirmed the physical wire is WIZ630io J3-3/QD2 to PE2. The earlier application had PF7 configured for IO2, so those Quad runs had a pin mismatch. Changed `W6300-TCP-Loopback.ioc` and `HAL_OSPI_MspInit` to PE2 AF10, clean-built the diagnostic firmware (0 errors, 14 warnings from unchanged vendor code), flashed and verified it, then ran the all-mode diagnostic at Mode 0 and 0.962025 MHz.

| Mode | CIDR | API CIDR / RTL | VER | SYSR | HAL errors |
| --- | --- | --- | --- | --- | --- |
| Single 1-1-1 | `0x61`, 100/100 | `0x6300` / `0x11` | `0x4661`, 100/100 | `0x01` | 0 |
| Dual 1-2-2 | `0x61`, 100/100 | `0x6300` / `0x11` | `0x4661`, 100/100 | `0x01` | 0 |
| Quad 1-4-4 | `0x00`, 0/100 | `0x0000` / `0x00` | `0x0000`, 0/100 | `0x00` | 0 |

SWD readback confirmed PE2 AF10, no pull, very-high speed; PF7 no longer had AF configured. P1CR=`0x02010101` still selects Port 1 LOW. The UART capture is `/tmp/w6300-ubuntu-pe2-single-dual-quad-uart.log`. The user clarified that the resistor on SB67 had already been removed, so SB67 was OFF during this diagnostic and PE2 was selected for QSPI_BK1_IO2. The normal Mode 3 operational firmware was restored and verified afterward; its startup log is `/tmp/w6300-ubuntu-pe2-final-startup-uart.log`.

### Historical Golden Reference timing retest with PE2 (2026-09-28)

After the PE2 pin correction, reran the diagnostic using the Golden Reference timing and critical callback: OCTOSPI kernel source D1HCLK at 275 MHz, Mode 0, no sample shift, `ClockPrescaler=6` (divider 6, 45.833333 MHz SCK), delay block bypassed, DHQC disabled, FIFO threshold 1, hardware NCS, and Golden `__disable_irq()` / `__enable_irq()` critical callbacks. SB67's resistor was already removed, so PE2 was selected for QSPI IO2. The diagnostic reset W6300 before each bus mode and did no network initialization.

| Mode | CIDR raw | API CIDR / RTL | VER | SYSR | HAL errors |
| --- | --- | --- | --- | --- | --- |
| Single 1-1-1 | `0x61`, 100/100 | `0x6300` / `0x11` | `0x4661`, 100/100 | `0x01` | 0 |
| Dual 1-2-2 | `0x61`, 100/100 | `0x6300` / `0x11` | `0x4661`, 100/100 | `0x01` | 0 |
| Quad 1-4-4 | `0x00`, 0/100 | `0x0000` / `0x00` | `0x0000`, 0/100 | `0x00` | 0 |

The live register readback matched the selected timing and routing: `RCC_D1CCIPR=0x00000000` (D1HCLK source), `OCTOSPIM_P1CR=0x02010101` (Port 1 LOW), GPIOE AFRL=`0x00000A00` (PE2 AF10), GPIOD AFRH=`0x00999077` (PD11/12/13 AF9), GPIOB AFRL=`0x00000900` (PB2 AF9), and GPIOG AFRL=`0x0A000000` (PG6 AF10). OCTOSPI readback was CR=`0x10000001`, DCR1=`0x00100008`, DCR2=`0x00000005` (divider 6), DCR3/DCR4=`0`, CCR=`0x03001301` (1-4-4, 16-bit address), TCR=`0x00000002`, IR=`0x00000080`. Build completed with 0 errors and 14 unchanged vendor warnings; CubeProgrammer flash and verify succeeded. UART: `/tmp/w6300-ubuntu-golden-pe2-quad-uart.log`.

The exact Golden timing and IRQ callback therefore did not make Quad identity pass. Single and Dual remained stable at the same SCK, while Quad returned zero with HAL errors zero. A separate 0.962025 MHz Mode 0 diagnostic changed only the critical callback from no-op to the Golden IRQ-masking callback; both builds produced the same Single/Dual pass and Quad failure. Its UART capture is `/tmp/w6300-ubuntu-pe2-quad-golden-critical-0962-uart.log`. Normal Mode 3 / 0.962025 MHz operational firmware was then rebuilt, flashed, verified, and reset; its final startup capture is `/tmp/w6300-ubuntu-final-operational-uart.log`.

### What the PE2-era intermediate result established

Single and Dual success confirmed the QD0/QD1 paths and 1-2-2 transaction during that PE2 wiring stage. The user confirmed SB67 was OFF. The PE2-matched Quad identity diagnostic failed at both 0.962025 MHz and the exact Golden Reference clock of 45.833 MHz, with HAL errors zero. The high-speed rerun also used the Golden `__disable_irq()` / `__enable_irq()` critical callbacks. These conclusions describe the earlier PE2 wiring and were superseded when QD2 was moved to PF7.

Those PE2-era Quad-only results pointed to QD2/PE2 or QD3/PD13 continuity, or behavior unique to the four-line phase. Current PF7 signal capture guidance follows the newer results below.

### PF7 reroute: current Quad read and TCP results (2026-09-28)

The user moved WIZ630io J3-3/QD2 from PE2 to PF7 at NUCLEO CN9-26/D62. The project now configures PF7 AF10 as OCTOSPIM Port 1 IO2. The three data pins are routed through Port 1 LOW (IO0–IO3); live `OCTOSPIM_P1CR=0x02010101` confirms that selection. SB67 is not in this external PF7 signal path. The checked-in `.ioc` and HAL MSP match this wiring.

The diagnostic used Golden transaction semantics at low speed: Mode 0, no sample shift, PLL2R 76 MHz / prescaler 79 = 0.962025 MHz SCK, hardware NCS, STR, DQS/DTR off, instruction width 1, address/data width 4 in Quad, 16-bit address, opcode mode bits `0x80`, and two HAL dummy cycles (8 dummy bits). Firmware reset W6300 before each mode and performed no network setup in the identity comparison.

| Mode | CIDR raw / count | API CIDR / RTL | VER / count | SYSR | HAL status / errors |
| --- | --- | --- | --- | --- | --- |
| Single 1-1-1 | `0x61`, 100/100 | `0x6300` / `0x11` | `0x4661`, 100/100 | `0x01` | 0 / 0 |
| Dual 1-2-2 | `0x61`, 100/100 | `0x6300` / `0x11` | `0x4661`, 100/100 | `0x01` | 0 / 0 |
| Quad 1-4-4 | `0x61`, 100/100 | `0x6300` / `0x11` | `0x4661`, 100/100 | `0x01` | 0 / 0 |

Thus PF7 resolved the identity-read failure: Quad command, address, and register-data phases return stable identity values. This does not establish reliable longer Quad data transfers.

With `APP_QSPI_BUFFER_DIAGNOSTIC=1`, Single and Dual TX-buffer read/write comparisons passed at lengths 1, 2, 3, 4, 7, 8, 15, 16, 31, 32, 63, 64, 127, and 256 bytes. Quad passed lengths 1, 2, 3, 4, and 7; length 8 failed at offset 4 (`expected 0x0F`, `actual 0x03`), with HAL error count 0. A fixed 16-byte test using `00 FF 55 AA 0F F0 33 CC 11 22 44 88 7F FE A5 5A` further isolated the direction:

- Single write followed by Quad read failed. The Quad-read test still failed when split into chunks of at most 7 bytes or one byte at a time; a bytewise sample differed at index 1 (`expected 0xFF`, `actual 0xF5`).
- Quad write followed by Single read passed all 16 bytes in the baseline no-op critical-callback build.
- Matching the Golden IRQ-masking callbacks did not fix Quad reads; in that run the Quad-write/Single-read comparison instead mismatched at indices 5–7. Half-cycle sample shift did not improve the Quad read. Mode 3 preserved identity reads but also left the Quad read failure; its write-direction comparison mismatched at indices 5–7. Each comparison changed only the named callback, sample shift, or clock mode.

The normal firmware was restored with `APP_QSPI_DIAGNOSTIC_ONLY=0`, `APP_QSPI_BUFFER_DIAGNOSTIC=0`, `W6300_GOLDEN_CRITICAL_CALLBACKS=0`, Mode 0/no sample shift, and 0.962025 MHz SCK. It was clean-built, flashed, verified, and reset. UART reported Quad identity passed, communication sanity passed, W6300 initialization, PHY link UP, and TCP LISTEN on port 5000. `getnetinfo()` readback printed subnet `255.255.240.12` although the configured value is `255.255.255.0`; this is consistent with the observed Quad read-data corruption and needs confirmation after the receive path is fixed. Ping passed 3/3 and ARP resolved the W6300 MAC as REACHABLE. Python TCP test passed the 1-byte payload, then failed at 64 bytes, offset 15 (`sent 0xB0`, `received 0x44`). The default matrix stopped there, and the 64-byte x 1000-connection test was not run. The final startup capture is `/tmp/w6300-ubuntu-pf7-final-normal-uart.log` and the TCP result came from `tools/tcp_loopback_test.py --host 192.168.0.10 --source 192.168.0.20`.

### Current fault boundary and next physical measurement

Known working on the current PF7 setup: W6300 reset, Single and Dual identity, Quad CIDR/VER/SYSR identity, the OCTOSPIM Port 1 LOW mapping, HAL transaction completion (zero errors), Ethernet PHY link, ARP/ping, and TCP connection establishment. Historical Windows Single-fallback TCP echo remains fully successful. The unresolved boundary is Quad receive payload data from W6300 to STM32; the data corruption can still come from QD0–QD3 electrical signals or OCTOSPI sampling/input handling, so software evidence alone cannot identify the exact line.

For the next hardware measurement, use a 3.3 V-compatible logic analyzer or oscilloscope with the grounds connected. Capture both the WIZ630io J3 end and the NUCLEO end during a low-speed Quad receive. Probe SCK at J3-6/PB2 (CN10-15/D27), NCS at J3-7/PG6 (CN10-13/D26), and QD0–QD3 at J3-1..4 and their MCU endpoints PD11 (CN10-23/D30), PD12 (CN10-21/D29), PF7 (CN9-26/D62), PD13 (CN10-19/D28). For a CIDR read, expect NCS low, about 0.962 MHz SCK, 1-line opcode `0x80`, four-line address, two dummy clocks, then data `0x61` as nibbles `0x6` followed by `0x1` on QD3..QD0. For the buffer pattern read, the first bytes are `0x00`, then `0xFF`: the four data lines should be low for both nibbles of `0x00`, then high for both nibbles of `0xFF`. Compare the waveform at J3 and at the MCU connector. If they differ, inspect that wire/connection; if they match through the MCU pin while HAL returns different bytes, the evidence shifts to STM32 input sampling/routing. With power off, also check J3-3 to PF7 continuity, J3-4 to PD13, and shorts between data lines or to GND/3V3.

## Troubleshooting

| Symptom | Check |
| --- | --- |
| CubeProgrammer cannot see ST-LINK | Confirm `lsusb`, inspect the actual `/dev/bus/usb/<bus>/<device>` permissions, `groups`, and udev rules. Do not treat permission denial as a board or firmware fault. |
| `/dev/ttyACM*` is missing | Check `lsusb`, `udevadm info --query=all --name=<device>`, `/dev/serial/by-id/`, and the latest kernel messages if permitted. Device numbers can change. |
| Ping fails | Confirm only the WIZ630io adapter has `192.168.0.20/24`, UART reports PHY link UP, and `ip neigh` resolves the W6300 MAC. |
| TCP connection is refused | Check UART for TCP LISTEN on port 5000. The client retries the brief socket close/reopen gap; persistent refusal means the firmware is not listening or the wrong interface/address was selected. |
| Quad identity works but payload bytes mismatch | Current PF7 firmware reads CIDR/VER correctly, but TX-buffer and TCP payload reads mismatch. Capture W6300-driven QD0–QD3 at both J3 and the MCU endpoints as described above; SB67 is outside the PF7 path. |

The Windows record remains in [FIRMWARE.md](FIRMWARE.md) and [WINDOWS_TEST.md](WINDOWS_TEST.md). This Ubuntu report adds the Dual result and does not replace the Windows measurements.
