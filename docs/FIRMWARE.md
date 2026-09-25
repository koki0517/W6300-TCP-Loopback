# Firmware and CubeIDE project

## Software architecture

`Core/Src/main.c` initializes HAL, the existing HSE/PLL system clock, GPIO, OCTOSPI1, and USART3, then calls `w6300_app_init()` once and `w6300_app_poll()` in the main loop. Application logic and the HAL port live under `App/`; CubeMX generated files remain under `Core/`.

`App/Src/w6300_port.c` performs the W6300 reset and registers blocking Quad-SPI callbacks with ioLibrary. It registers no-op CS callbacks; OCTOSPI1 hardware NCS on PG6 controls chip select, with no GPIO CS toggling. `App/Src/w6300_app.c` checks CIDR/version, sets socket memory and static IPv4, reports PHY/link state, and polls the vendor `loopback_tcps()` function on socket 0.

The QSPI command uses one-line, 8-bit instructions; four-line, 16-bit addresses; four-line data; two dummy cycles; STR; and DQS off. The opcode and 16-bit address are passed to the HAL unchanged. The reference WIZnet H723 implementation uses the same phase widths and two Quad dummy cycles.

## CubeMX and clock configuration

The checked-in `.ioc` is the configuration source of truth. It targets CubeMX 6.12.0 and STM32CubeH7 FW 1.11.2; do not migrate the project to a newer package just to build it.

- MCU: STM32H723ZGTx; retain the existing 8 MHz HSE bypass and 550 MHz system clock.
- OCTOSPI1: Port 1 Quad, Clock Mode 3, STR, DTR/DQS/free-running clock disabled, hardware NCS, blocking polling; no DMA/MDMA or OCTOSPI interrupt.
- OCTOSPI kernel clock: D1HCLK at 275 MHz. Prescaler 9 divides by 9, giving about 30.6 MHz on SCK. OCTOSPIM maps CLK, NCS, and IO[3:0] to Port 1.
- USART3 on PD8/PD9: 115200 baud, 8-N-1, ST-LINK VCP.
- PF4 is the active-low `W6300_RSTn` output; it idles high.
- STM32 RMII/ETH, USB_OTG_HS, USB GPIO, LwIP, and RTOS are not enabled.

## W6300 startup

1. Hold RSTn low for 10 ms, release it, then wait at least 100 ms.
2. Register ioLibrary read/write callbacks and read CIDR, VER, and SYSR.
3. Require CIDR `0x6100`; reject version `0x0000`/`0xFFFF` and any HAL OSPI error.
4. Assign socket 0 32 KiB TX and 32 KiB RX; assign zero to sockets 1–7.
5. Set static IPv4 (`NETINFO_STATIC_V4`, `NETINFO_STATIC`) and IPv4 loopback mode.
6. Poll PHY link and run the vendor TCP server loop on socket 0, port 5000.

QSPI HAL failures are counted and reported over USART3. A bus error is latched as fatal and shown by the red LED. Recoverable TCP errors close socket 0 so the vendor loop can reopen and listen again.

## Network and application settings

Change MAC, IPv4, subnet, gateway, DNS, TCP port, socket number, buffer length, and timing constants in `App/Inc/app_config.h`. The default is MAC `02:00:00:00:00:10`, IP `192.168.0.10/24`, gateway/DNS `0.0.0.0`, socket 0, port 5000, and a 2048-byte application buffer.

## Project tree

```text
App/Inc, App/Src/                 Application and W6300 HAL port
Core/                             CubeMX generated startup and peripheral code
Drivers/                          STM32CubeH7 HAL and CMSIS files
Middlewares/Third_Party/ioLibrary_Driver/  Pinned WIZnet submodule
tools/tcp_loopback_test.py        Windows standard-library test client
docs/                             Hardware, firmware, and PC setup
W6300-TCP-Loopback.ioc             CubeMX source of truth
```

The CubeIDE Debug and Release managed builds include the application and the four required vendor sources: `Ethernet/socket.c`, `Ethernet/wizchip_conf.c`, `Ethernet/W6300/w6300.c`, and `Application/loopback/loopback.c`. Other chip drivers and vendor examples are excluded.

## Build, flash, and UART

Import the repository as an existing STM32CubeIDE project. Select the `Debug` configuration and build. The project is also configured for headless build with the CubeIDE 1.16.0 executable, using the Eclipse application `org.eclipse.cdt.managedbuilder.core.headlessbuild` and the `W6300-TCP-Loopback/Debug` build target.

Flash `Debug/W6300-TCP-Loopback.elf` over ST-LINK/SWD with STM32CubeProgrammer CLI, verify the image, and reset the target. Open the ST-LINK Virtual COM Port at 115200, 8-N-1, no flow control. Startup logs include reset completion, CIDR/VER/SYSR, PHY, MAC/IP/subnet, and the TCP listen transition.

## Current hardware validation

On 2026-09-26, STM32CubeIDE 1.16.0 completed a clean Debug build with 0 errors and 14 warnings from the unmodified vendor `Application/loopback/loopback.c`. STM32CubeProgrammer detected the attached NUCLEO-H723ZG, programmed the ELF, verified it, and reset the MCU. USART3 captured firmware startup, but the W6300 read returned CIDR `0x9888`, VER `0x8888`, and SYSR `0x88` with no HAL OSPI errors. The firmware correctly stopped at the CIDR sanity check, before IPv4 or TCP setup; ping and TCP loopback therefore remain unverified.

SWD reads confirmed the configured AF9/AF10 values for PB2, PD11/PD12, PE2, PD13, and PG6; OCTOSPIM Port 1 was enabled and PF4 read high after reset release. Windows Ethernet was `Up` at 100 Mbps with `192.168.0.20/24`, but a source-bound ping to `192.168.0.10` reported destination host unreachable. The [Robomech-NHK UDP example](https://github.com/Robomech-NHK/wiz630io_test) was compared, including its success branch's OCTOSPI pin routing. Confirm that SB67 is OFF and inspect the physical QSPI path before treating the W6300 bus as working.

## CubeMX regeneration and submodule updates

Save changes to the existing `.ioc` using the stated CubeMX and STM32CubeH7 versions. Review regenerated `main.c`, `stm32h7xx_hal_msp.c`, `main.h`, and `.cproject`; preserve application calls and managed-build include paths/source exclusions in both configurations. Keep custom application code in `App/`.

Initialize the pinned dependency after cloning:

```powershell
git submodule update --init --recursive
```

To intentionally move to a newer upstream ioLibrary revision, review its changes and license, update the submodule, then update the gitlink and `THIRD_PARTY_NOTICES.md` together.
