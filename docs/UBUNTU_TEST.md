# Ubuntu 22.04 build and hardware validation

This document records the Linux handoff from the Windows-tested firmware and the Ubuntu 22.04 measurements made on 2026-09-28 and 2026-09-29. The board was already wired to the WIZ630io; the host network changes below are limited to that direct-link adapter.

## Handoff baseline

- The repository was fetched from `origin/main` before testing. Start HEAD and current `origin/main` are both `6a4a292149fe129a4f9c9abfc8c360f48faf92d5` (`Implement W6300 TCP loopback and QSPI diagnostics`). The required Windows handoff commit is included.
- The controlled DeviceSize/kernel-source follow-up started from `8441dc326c576265e3f4c9555444c9565c5797fc` (`Add Dual QSPI diagnostics and Ubuntu validation`); `origin/main` matched that commit at the start of this follow-up.
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

In earlier firmware/configuration runs before the current PF7 setup, Quad identity fell back to Single. Across those recorded runs, ping passed 3/3, ARP resolved to `02:00:00:00:00:10`, and the default Python matrix passed all eight sizes (90,101 bytes). These are valid historical Single-fallback results, not current PF7 Quad payload results. The physical QD2 route during each earlier run was not independently verified. Current PF7 results are recorded in the final section below.

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
| IO2 routing / SB67 | Port 1 HIGH group, IO4–IO7 | Port 1 LOW group, IO0–IO3; current QD2 route is PF7 | Pin-group and external-pin difference only. SB67 concerns the alternate PE2 board route and is outside the current PF7 path; its resistor state has not been verified. |
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
| GPIO AF historical PE2 configuration | GPIOE_MODER=`0xFFFFFFE7`; OSPEEDR=`0x00000030`; PUPDR=`0`; AFRL=`0x00000A00`; PF7 AFRL=`0` | Earlier firmware GPIO setup used PE2 AF10. Physical wiring and SB67 state during that test were not independently verified. |
| `OCTOSPI1_CR` | `0x10000001` | OCTOSPI enabled; FIFO threshold 1. |
| `OCTOSPI1_DCR1` | `0x00100008` | DeviceSize 17, Mode 0, delay block bypassed. |
| `OCTOSPI1_DCR2` | `0x0000004E` | Prescaler register 78, divisor 79. |
| `OCTOSPI1_DCR3` / `DCR4` | `0x00000000` / `0x00000000` | Chip-select boundary/max transfer and refresh remain zero. |
| `OCTOSPI1_CCR` | `0x03001301` | 1-line instruction, 4-line address, 16-bit address size, 4-line data. |
| `OCTOSPI1_TCR` | `0x00000002` | Two dummy cycles. |
| `OCTOSPI1_IR` / `AR` | `0x00000080` / `0x00002000` | Quad opcode and last diagnostic address (SYSR). |

The earlier results below preserve the chronological test record. Their firmware pin configurations are known from source/register records, but the physical QD2 route at each historical test time was not independently verified. The current physical QD2 route is PF7, and all three identity modes pass in that setup. Do not use earlier zero-CIDR observations as the current PF7 result.

## Historical Quad and Dual identity results before current PF7 configuration

The first all-mode diagnostic and the Golden-clock retry below are retained as historical results. Their firmware pin configurations are known, but their physical QD2 wiring was not independently verified. A later build configured PE2 AF10; this identifies a firmware setting, not the physical route. The all-mode diagnostic is isolated from network initialization. To reproduce the low-speed comparison, set `APP_QSPI_DIAGNOSTIC_ONLY` to `1U` and `W6300_OSPI_CLOCK_MODE` to `HAL_OSPI_CLOCK_MODE_0` in `App/Inc/app_config.h`; keep the normal PLL2R 76 MHz and prescaler 79. The firmware hardware-resets W6300 before each mode and reads raw CIDR 100 times, API CIDR, RTL, VER 100 times, SYSR, HAL status/state/error code, and error count.

| Mode | Opcode / phases / HAL dummy | CIDR raw | API CIDR / RTL | VER | SYSR | HAL status / errors |
| --- | --- | --- | --- | --- | --- | --- |
| Single 1-1-1 | `0x00`; 1-1-1; 8 cycles | `0x61`, 100/100 | `0x6300` / `0x11` | `0x4661`, 100/100 | `0x01` | 0 / 0 |
| Dual 1-2-2 | `0x40`; 1-2-2; 4 cycles | `0x61`, 100/100 | `0x6300` / `0x11` | `0x4661`, 100/100 | `0x01` | 0 / 0 |
| Quad 1-4-4 | `0x80`; 1-4-4; 2 cycles | `0x00`, 0/100 | `0x0000` / `0x00` | `0x0000`, 0/100 | `0x00` | 0 / 0 |

All three modes ran at 0.962025 MHz with Mode 0, no sample shift, DQS/DTR disabled, SIOO every command, and hardware NCS. A second comparison using the Golden `__disable_irq()`/`__enable_irq()` critical callbacks produced the same results. The diagnostic logs were saved during the run under `/tmp/w6300-ubuntu-single-dual-quad-uart.log` and `/tmp/w6300-ubuntu-golden-critical-quad-uart.log`.

An exact Golden-clock Quad retry also ran at Mode 0 with D1HCLK 275 MHz, `ClockPrescaler=6`, and 45.833 MHz SCK. The startup identity read returned raw CIDR `0x00`, API CIDR `0x0000`, RTL/VER/SYSR zero, and HAL errors zero. Single fallback at the same SCK read raw CIDR `0x61`, API CIDR `0x6300`, RTL `0x11`, VER `0x4661`, and SYSR `0x01`. Networking was not tested at this temporary 45.833 MHz setting. Its UART log is `/tmp/w6300-ubuntu-golden-quad-uart.log`.

### Historical PE2-configured diagnostic (2026-09-28)

This build configured PE2 AF10 for IO2, whereas an earlier source revision configured PF7. The physical QD2 connection during either set of tests was not independently verified. Changed `W6300-TCP-Loopback.ioc` and HAL MSP to PE2 AF10, clean-built the diagnostic firmware (0 errors, 14 warnings from unchanged vendor code), flashed and verified it, then ran the all-mode diagnostic at Mode 0 and 0.962025 MHz.

| Mode | CIDR | API CIDR / RTL | VER | SYSR | HAL errors |
| --- | --- | --- | --- | --- | --- |
| Single 1-1-1 | `0x61`, 100/100 | `0x6300` / `0x11` | `0x4661`, 100/100 | `0x01` | 0 |
| Dual 1-2-2 | `0x61`, 100/100 | `0x6300` / `0x11` | `0x4661`, 100/100 | `0x01` | 0 |
| Quad 1-4-4 | `0x00`, 0/100 | `0x0000` / `0x00` | `0x0000`, 0/100 | `0x00` | 0 |

SWD readback confirmed the firmware configured PE2 AF10, no pull, very-high speed; PF7 no longer had AF configured. P1CR=`0x02010101` selected Port 1 LOW. The physical PE2 signal route and SB67 resistor state were not independently verified for this run. The UART capture is `/tmp/w6300-ubuntu-pe2-single-dual-quad-uart.log`. The normal Mode 3 operational firmware was restored and verified afterward; its startup log is `/tmp/w6300-ubuntu-pe2-final-startup-uart.log`.

### Historical Golden Reference timing retest with PE2 (2026-09-28)

With firmware PE2 AF10 selected, the diagnostic used Golden Reference timing and critical callbacks: OCTOSPI kernel source D1HCLK at 275 MHz, Mode 0, no sample shift, `ClockPrescaler=6` (divider 6, 45.833333 MHz SCK), delay block bypassed, DHQC disabled, FIFO threshold 1, hardware NCS, and Golden `__disable_irq()` / `__enable_irq()` callbacks. The physical PE2 board route and SB67 resistor state were not independently verified. The diagnostic reset W6300 before each bus mode and did no network initialization.

| Mode | CIDR raw | API CIDR / RTL | VER | SYSR | HAL errors |
| --- | --- | --- | --- | --- | --- |
| Single 1-1-1 | `0x61`, 100/100 | `0x6300` / `0x11` | `0x4661`, 100/100 | `0x01` | 0 |
| Dual 1-2-2 | `0x61`, 100/100 | `0x6300` / `0x11` | `0x4661`, 100/100 | `0x01` | 0 |
| Quad 1-4-4 | `0x00`, 0/100 | `0x0000` / `0x00` | `0x0000`, 0/100 | `0x00` | 0 |

The live register readback matched the selected timing and routing: `RCC_D1CCIPR=0x00000000` (D1HCLK source), `OCTOSPIM_P1CR=0x02010101` (Port 1 LOW), GPIOE AFRL=`0x00000A00` (PE2 AF10), GPIOD AFRH=`0x00999077` (PD11/12/13 AF9), GPIOB AFRL=`0x00000900` (PB2 AF9), and GPIOG AFRL=`0x0A000000` (PG6 AF10). OCTOSPI readback was CR=`0x10000001`, DCR1=`0x00100008`, DCR2=`0x00000005` (divider 6), DCR3/DCR4=`0`, CCR=`0x03001301` (1-4-4, 16-bit address), TCR=`0x00000002`, IR=`0x00000080`. Build completed with 0 errors and 14 unchanged vendor warnings; CubeProgrammer flash and verify succeeded. UART: `/tmp/w6300-ubuntu-golden-pe2-quad-uart.log`.

The exact Golden timing and IRQ callback therefore did not make Quad identity pass. Single and Dual remained stable at the same SCK, while Quad returned zero with HAL errors zero. A separate 0.962025 MHz Mode 0 diagnostic changed only the critical callback from no-op to the Golden IRQ-masking callback; both builds produced the same Single/Dual pass and Quad failure. Its UART capture is `/tmp/w6300-ubuntu-pe2-quad-golden-critical-0962-uart.log`. Normal Mode 3 / 0.962025 MHz operational firmware was then rebuilt, flashed, verified, and reset; its final startup capture is `/tmp/w6300-ubuntu-final-operational-uart.log`.

### What the PE2-era intermediate result established

Single and Dual identity succeeded in those diagnostics. The PE2-configured Quad identity diagnostic failed at both 0.962025 MHz and the exact Golden Reference clock of 45.833 MHz, with HAL errors zero. The high-speed rerun also used the Golden `__disable_irq()` / `__enable_irq()` critical callbacks. Because historical physical wiring was not independently verified, these results describe the firmware and timing settings only, not the external QD2 route.

Because the physical QD2 route during these PE2-configured tests is unknown, they do not localize an external data line. They show only that the Quad identity result differed under those firmware/timing configurations. Current PF7 signal capture guidance follows the newer results below.

### PF7 results before the ground/timing follow-up (2026-09-28)

The current physical wiring is WIZ630io J3-3/QD2 to PF7 at NUCLEO CN9-26/D62. The project configures PF7 AF10 as OCTOSPIM Port 1 IO2. The four data pins use Port 1 LOW (IO0–IO3); live `OCTOSPIM_P1CR=0x02010101` confirms that selection. SB67 is outside this external PF7 signal path; its resistor state is not known. The checked-in `.ioc` and HAL MSP match the current wiring.

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

The normal firmware was restored with `APP_QSPI_DIAGNOSTIC_ONLY=0`, `APP_QSPI_BUFFER_DIAGNOSTIC=0`, `W6300_GOLDEN_CRITICAL_CALLBACKS=0`, Mode 0/no sample shift, and 0.962025 MHz SCK. It was clean-built, flashed, verified, and reset. UART reported Quad identity passed, communication sanity passed, W6300 initialization, PHY link UP, and TCP LISTEN on port 5000. `getnetinfo()` readback printed subnet `255.255.240.12` although the configured value is `255.255.255.0`, consistent with the Quad read-data corruption measured at that time. Ping passed 3/3 and ARP resolved the W6300 MAC as REACHABLE. Python TCP test passed the 1-byte payload, then failed at 64 bytes, offset 15 (`sent 0xB0`, `received 0x44`). The default matrix stopped there, and the 64-byte x 1000-connection test was not run. This is the pre-follow-up result; the 2026-09-29 J3 GND and CS-high-time result appears below. The startup capture is `/tmp/w6300-ubuntu-pf7-final-normal-uart.log` and the TCP result came from `tools/tcp_loopback_test.py --host 192.168.0.10 --source 192.168.0.20`.

### DeviceSize and OCTOSPI kernel A/B tests (2026-09-28)

The current source/default settings were restored to the baseline after these tests. Each A/B build changed only the named setting; all used Mode 0, no sample shift, PF7/Port 1 LOW, the Golden 1-4-4 transaction format, hardware NCS, 8 dummy bits (2 HAL cycles), and approximately 1 MHz SCK. Each diagnostic clean-built, programmed, verified, and reset successfully. Both clean builds finished with zero errors and the same 14 warnings in unchanged vendor `Application/loopback/loopback.c`.

| Test | DeviceSize | Kernel clock | SCK | Single identity | Dual identity | Quad identity | Quad payload |
| --- | ---: | --- | ---: | --- | --- | --- | --- |
| Baseline | 17 | PLL2R 76 MHz | 0.962025 MHz | 100/100 | 100/100 | 100/100 | lengths 1, 2, 3, 4, 7 pass; 8 fails at offset 4 (`0x0F`→`0x03`) |
| A | 32 | PLL2R 76 MHz | 0.962025 MHz | 100/100 | 100/100 | 100/100 | same length-8 failure and offset/value |
| B | 32 | D1HCLK 275 MHz | 1.074218 MHz | 100/100 | 100/100 | 100/100 | same length-8 failure and offset/value |

For every mode in A and B, raw CIDR was `0x61` for 100/100 reads, API CIDR `0x6300`, RTL `0x11`, VER `0x4661` for 100/100 reads, SYSR `0x01`, and HAL OSPI error count 0. Single and Dual TX-buffer read/write tests passed at all lengths in the earlier matrix (1, 2, 3, 4, 7, 8, 15, 16, 31, 32, 63, 64, 127, 256). The A/B Quad matrix again passed 1, 2, 3, 4, and 7 bytes and failed at 8 bytes, offset 4, expected `0x0F`, actual `0x03`.

The direction test remained asymmetric in both A and B: Single write → Quad read failed, including one-byte and at-most-7-byte chunks; Quad write → Single read passed the 16-byte test in these two builds. This is evidence that Quad receive is the more reproducible failure direction, not proof that Quad transmit is always correct; other recorded configurations had partial Quad-write mismatches. TCP was not retested because neither A nor B improved payload reads.

Test B used `ClockPrescaler=256`, the HAL maximum divider. SWD HOTPLUG readback confirmed the selected source and register settings: `RCC_D1CCIPR=0x00000000` (D1HCLK), `OCTOSPI1_DCR1=0x001F0008` (DeviceSize 32), `DCR2=0x000000FF` (divider 256), and `DCR3=0`. UART reported the integer effective SCK as 1,074,218 Hz. The A log is `/tmp/w6300-device-size-32-pll2-76-uart.log`; the B log is `/tmp/w6300-device-size-32-d1hclk-275-uart.log`.

Neither tested difference changed the Quad payload result. DeviceSize and kernel source are therefore not supported as the cause of this corruption under the tested low-speed conditions. After the repeat-read capture build below, the project was restored to `DeviceSize=17`, PLL2R 76 MHz, prescaler 79 (0.962025 MHz), and all diagnostic options off. That normal build was clean-built, flashed, verified, and reset; the clean UART startup capture is `/tmp/w6300-final-normal-uart-clean.log`.

### Earlier 16-byte repeated-read capture

An earlier repeat diagnostic wrote `00 FF 55 AA 0F F0 33 CC 11 22 44 88 7F FE A5 5A` in Single mode and read 16 bytes in Quad every 50 ms. It ran with DeviceSize 32 and D1HCLK 275 MHz divided by 256 (`~1.074 MHz` SCK). The reads failed with HAL errors 0; one captured result was `FFFFFFFFFFFFFFFFFFFF000000000000`, first mismatch at offset 0 (`expected 0x00`, `actual 0xFF`). UART capture: `/tmp/w6300-quad-repeat-read-uart.log`.

### Two-channel scope capture and bytewise replay (2026-09-29)

The user-provided NCS/CLK capture shows two separate NCS-low sections with 16 SCK pulses in each. At the configured 1-4-4 protocol, a one-byte transaction is 8 instruction clocks + 4 address clocks + 2 dummy clocks + 2 data clocks = 16 SCK pulses. The capture therefore shows two one-byte transactions; it does not require looking for a slower, separate waveform.

The current isolated replay writes and verifies this pattern using Single mode, then repeats two one-byte Quad reads per loop: TX offset `0x7000` expects `0x00`, followed by offset `0x7100` expecting `0xFF`. The loop repeats every 2 ms at the restored baseline settings: DeviceSize 17, PLL2R 76 MHz / prescaler 79, effective SCK 0.962025 MHz. Clean build, flash, verify, and reset succeeded. The startup Single-mode readback matched all 16 bytes. The first 3,000 Quad pairs reported byte 0 as `0xFF` instead of `0x00`, byte 1 as `0xFF` as expected, and HAL errors 0. UART capture: `/tmp/w6300-quad-bytewise-pair-2ms-uart.log`.

This paired replay differs from the earlier direction test, which recorded byte 0 correct and byte 1 as `0xF5` instead of `0xFF`. The returned value therefore varies with the tested address/sequence; it does not support assigning the fault to one data pin yet.

The J3-side QD3/CLK capture and a sequential capture at the MCU end (PD13/CN10-19 with CLK at PB2/CN10-15) looked similar. The user's later captures showed that touching the J1 CS contact could change the displayed edge shape. The user then connected WIZ630io J3-5 GND to NUCLEO GND in addition to the existing J2 GND; the waveform became visibly cleaner. This is direct evidence that the measurement/reference return setup affected the observed trace. It did not, by itself, make the original-order bytewise replay pass.

After adding J3 GND, the unchanged replay (read `0x7000` = `0x00`, then `0x7100` = `0xFF`, no inter-read delay) still failed: summaries at 227,000–230,000 pairs showed the first byte correct and the second byte `0x00`, HAL errors 0. Log: `/tmp/w6300-j3-gnd-quad-read-uart.log`. Two controlled sequence checks then showed the failure depended on transaction order and spacing:

- Reversing the reads (`0x7100`=`0xFF` first, then `0x7000`=`0x00`), with no gap, passed through 9,000 pairs, bad=0, HAL errors=0. Log: `/tmp/w6300-j3-gnd-reverse-order-uart.log`.
- Keeping the original order and inserting a 1 ms gap passed through 5,000 pairs, bad=0, HAL errors=0. Log: `/tmp/w6300-j3-gnd-interread-gap-uart-saved.log`.

The decisive no-gap check kept the original order and changed only OCTOSPI `ChipSelectHighTime` from 1 to 2 cycles in `main.c` and the `.ioc`; J3 GND remained connected. At baseline `DeviceSize=17`, PLL2R 76 MHz / prescaler 79, the effective SCK remained 0.962025 MHz. The replay passed through 11,000 pairs, bad=0, HAL errors=0 (`/tmp/w6300-j3-gnd-csht2-pair-uart.log`). This supports insufficient intertransaction CS-high time as a software timing contributor under the tested setup. Because the added J3 return and CS-high-time change are both part of the final successful setup, their separate contributions are not fully quantified.

With `APP_QSPI_DIAGNOSTIC_ONLY=1` and `APP_QSPI_BUFFER_DIAGNOSTIC=1`, Single 1-1-1, Dual 1-2-2, and Quad 1-4-4 each passed identity reads (CIDR raw `0x61` 100/100, normalized `0x6100`, API `0x6300`, RTL `0x11`; VER `0x4661` 100/100; SYSR `0x01`; HAL errors 0). Each mode passed TX-buffer read/write lengths 1, 2, 3, 4, 7, 8, 15, 16, 31, 32, 63, 64, 127, and 256 bytes. Quad also passed Single-write → Quad-read, Quad-read chunks up to 7 bytes, bytewise Quad reads, and Quad-write → Single-read (16 bytes each). Capture: `/tmp/w6300-j3-gnd-csht2-buffer-matrix-uart.log`.

The normal firmware was restored and clean-built, flashed, verified, and reset with `APP_QSPI_DIAGNOSTIC_ONLY=0`, `APP_QSPI_BUFFER_DIAGNOSTIC=0`, `APP_QSPI_REPEAT_READ_DIAGNOSTIC=0`, `DeviceSize=17`, and `ChipSelectHighTime=2`. UART confirmed Quad identity (`CIDR 0x61`, `VER 0x4661`, `SYSR 0x01`, HAL errors 0) and TCP LISTEN on port 5000. The initial UART sample showed PHY link DOWN, but host network checks after startup succeeded: the ASIX interface `enx04ab18c5869b` had `192.168.0.20/24`; ping to `192.168.0.10` passed 3/3; `ip neigh` resolved `02:00:00:00:00:10` (state STALE at final query). The Python echo matrix passed 1, 64, 512, 1460, 2048, 4096, 16384, and 65536 bytes (90,101 bytes total). A 64-byte x 1000 fresh-connection run passed all 64,000 bytes with no mismatch; the first run reported 1,922 connection retries during server close/relisten intervals. SCK remained 0.962025 MHz. Normal startup capture: `/tmp/w6300-j3-gnd-interread-gap-uart.log`.

At the user's request to focus on TCP, both tests were rerun on the same normal Quad firmware. The 8-size echo matrix again passed all 90,101 bytes. The 64-byte x 1000 test again echoed all 64,000 bytes correctly and exited successfully; it counted 1,864 refused-connect retries, with 25.270 ms average per completed connection. The client retries `ECONNREFUSED` for up to 250 ms while the single W6300 server socket returns to LISTEN after the prior client closes. This is a brief reconnect availability gap, not an echo mismatch or disconnect; current evidence does not show TCP payload corruption.

### Revalidation after moving the power connection point (2026-09-29)

After the user changed the power connection point, the QSPI and network checks were repeated. The exact replacement connector/pin was not specified, so this record does not infer it. ST-LINK detected the NUCLEO-H723ZG at 3.25 V. The test retained DeviceSize 17, PLL2R 76 MHz / prescaler 79 (0.962025 MHz SCK), Mode 0, no sample shift, and ChipSelectHighTime 2.

The diagnostic build passed Single, Dual, and Quad identity reads at 100/100 each (raw CIDR `0x61`, API CIDR `0x6300`, RTL `0x11`, VER `0x4661`, SYSR `0x01`, HAL errors 0). All three modes passed TX-buffer read/write lengths 1, 2, 3, 4, 7, 8, 15, 16, 31, 32, 63, 64, 127, and 256 bytes. Quad direction checks (Single write → Quad read, chunked Quad read, bytewise Quad read, and Quad write → Single read) all passed. UART capture: `/tmp/w6300-power-point-qspi-diagnostic-uart.log`.

The normal Quad firmware was then clean-built, flashed, verified, and reset with all diagnostic options off. UART reported Quad identity passed with HAL errors 0, TCP LISTEN on port 5000, and PHY transitioned from DOWN to UP. Ping on `enx04ab18c5869b` passed 3/3. The Python TCP matrix passed 1, 64, 512, 1460, 2048, 4096, 16384, and 65536 bytes (90,101 total); 64-byte x 1000 fresh connections passed all 64,000 bytes with no payload mismatch. The latter run counted 1,797 refused-connect retries while the W6300's single server socket returned to LISTEN; average completed-connection time was 25.418 ms. UART capture: `/tmp/w6300-power-point-normal-uart.log`.

### Current result and fault boundary

At the tested low-speed setup—J3 GND connected in addition to J2 GND, `ChipSelectHighTime=2`, Mode 0/no sample shift, 0.962025 MHz—the Quad receive payload corruption is no longer reproduced. All mode buffer checks and the normal Quad TCP tests passed. The earlier `DeviceSize=32` and D1HCLK A/B tests did not improve reads before the return-path/timing setup was corrected; they are not implicated by the present successful baseline and were not retained. The strongest current inference is that the combination of local signal return grounding and minimum CS-high time resolved the observed corruption; available tests do not isolate their independent contributions or prove which margin was most responsible. No new per-data-line fault diagnosis is indicated by the current software results.

## Troubleshooting

| Symptom | Check |
| --- | --- |
| CubeProgrammer cannot see ST-LINK | Confirm `lsusb`, inspect the actual `/dev/bus/usb/<bus>/<device>` permissions, `groups`, and udev rules. Do not treat permission denial as a board or firmware fault. |
| `/dev/ttyACM*` is missing | Check `lsusb`, `udevadm info --query=all --name=<device>`, `/dev/serial/by-id/`, and the latest kernel messages if permitted. Device numbers can change. |
| Ping fails | Confirm only the WIZ630io adapter has `192.168.0.20/24`, UART reports PHY link UP, and `ip neigh` resolves the W6300 MAC. |
| TCP connection is refused | Check UART for TCP LISTEN on port 5000. The client retries the brief socket close/reopen gap; persistent refusal means the firmware is not listening or the wrong interface/address was selected. |
| Quad identity works but payload bytes mismatch | First confirm J3-5 GND is connected to NUCLEO GND in addition to J2 GND and `ChipSelectHighTime=2` is present in both `main.c` and the `.ioc`. Those exact settings passed the buffer matrix and TCP tests. SB67 is outside the current PF7 signal path; its resistor state has not been verified. |

The Windows record remains in [FIRMWARE.md](FIRMWARE.md) and [WINDOWS_TEST.md](WINDOWS_TEST.md). This Ubuntu report adds the Dual result and does not replace the Windows measurements.
