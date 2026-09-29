# Firmware and CubeIDE project

## Software architecture

`Core/Src/main.c` initializes HAL, the existing HSE/PLL system clock, GPIO, `USART3`, and `OCTOSPI1`, then calls `w6300_app_init()` and polls `w6300_app_poll()`. If QSPI identity checks fail, the application retries initialization once per second. Application code and the W6300 HAL adapter are under App/; CubeMX peripheral setup stays in Core/.

`App/Src/w6300_port.c` holds active-low `RSTn` low for 100 ms and waits 100 ms after release. It registers blocking `HAL OSPI` read/write callbacks. `OCTOSPI1` controls hardware NCS on `PG6`; ioLibrary CS callbacks are no-ops. `HAL OSPI` failures are counted and reported over `USART3`.

## CubeMX and OCTOSPI configuration

The checked-in `.ioc` is the configuration source of truth. It targets `CubeMX 6.12.0` and `STM32CubeH7 FW 1.11.2`. To keep the repository smaller, the generated `Drivers/` tree, `Core/Inc/`, and the generated MSP/interrupt source files are ignored. On a fresh clone, open the `.ioc` with the matching CubeMX and firmware package and run **Generate Code** before building. `Core/Src/main.c` stays versioned because it contains the application startup and polling integration.

- MCU: `STM32H723ZGTx`; preserve the `8 MHz` HSE bypass and 550 MHz system clock.
- `OCTOSPI1`: `Port 1`, `STR`, `Clock Mode 0`, no sample shift, delay block bypassed, `DTR`/`DQS`/free-running clock disabled, polling transfers, no `DMA/MDMA` or OCTOSPI interrupt.
- Kernel clock: `PLL2R` at `76 MHz`, derived from the `8 MHz` HSE (`M=1`, `N=19`, `R=2`). HAL uses `ClockPrescaler` - 1 as the register value; configured prescaler `79` gives an SCK divider of `79` and approximately `0.962 MHz` SCK.
- Current SCK-related configuration: `DeviceSize=17`, `ChipSelectHighTime=2`. The latter is required by the validated `Quad` buffer and TCP tests.
- Pins: `PB2` CLK, `PG6` hardware NCS, `PD11` `IO0`, `PD12` `IO1`, `PF7` `IO2`, `PD13` `IO3`. Current QD2 wiring is WIZ630io J3-3 to `PF7` / `CN9-26` / `D62`. `PF7` uses `OCTOSPIM Port 1 LOW` (`IO0–IO3`). `SB67` is outside this external `PF7` path; its chip resistor state is unverified.
- `USART3`: `PD8/PD9`, `115200 baud`, `8-N-1`, ST-LINK VCP. `PF4` is active-low `W6300_RSTn`, idle high.
- STM32 internal Ethernet/RMII, USB OTG, LwIP, and RTOS are disabled.

Each ioLibrary transfer uses a one-line `8-bit` instruction and a `16-bit` address. `Quad` mode uses four-line address and data phases with `two HAL dummy cycles` (`8 dummy bits`). `DTR` and `DQS` are disabled. The callback preserves the register bits in the ioLibrary opcode and applies the active bus-mode bits (`0x00` for `Single`, `0x80` for `Quad`). HAL failures are counted and reported over `USART3`.

## Startup and fallback

1. Reset W6300 and wait for it to settle.
2. Check CIDR, VER, and HAL status in `Quad` mode.
3. If `Quad` identity fails, reset the chip and try `Single` 1-1-1. If both modes fail, retry initialization after one second.
4. Initialize socket memory: `socket 0` receives `32 KiB` TX and RX; `sockets 1–7` receive zero.
5. Configure static `IPv4` and select `IPv4` loopback mode.
6. Poll PHY state and run the vendor `loopback_tcps()` service on `socket 0`, `port 5000`.

Expected identity values are raw CIDR major `0x61`, normalized CIDR `0x6100`, API `getCIDR()` value `0x6300` (with `RTL 0x11`), VER `0x4661`, and `SYSR 0x01`. The raw major and VER are checked by firmware.

The application buffer is `2048 bytes`. TCP is a byte stream: received bytes are echoed without assumptions about text or packet boundaries. A recoverable socket error closes `socket 0` so the vendor loop can reopen and listen. A QSPI HAL error is logged and latches the red LED; PHY link state controls the green LED.

## Configuration and source tree

Change MAC, `IPv4` address, subnet, gateway, DNS, TCP port, socket number, buffer size, reset delay, clock divider, and dummy cycles in `App/Inc/app_config.h`.

- MAC: 02:00:00:00:00:10
- `IPv4`: 192.168.0.10/24
- Gateway and DNS: 0.0.0.0
- TCP port: 5000; socket: 0; application buffer: `2048 bytes`

App/ contains the application and HAL port; Core/ contains generated startup and peripheral code; Drivers/ contains STM32CubeH7 HAL/CMSIS; Middlewares/Third_Party/ioLibrary_Driver/ is the pinned WIZnet submodule. Vendor source files are not modified. `Debug` and `Release` use `_WIZCHIP_=W6300` and `_WIZCHIP_QSPI_MODE_=QSPI_QUAD_MODE`.

## Build, flash, and UART

Import this repository as an existing `STM32CubeIDE` project and build the `Debug` configuration. On Windows, a headless build can use:

```powershell
stm32cubeidec.exe --launcher.suppressErrors -nosplash -application org.eclipse.cdt.managedbuilder.core.headlessbuild -data <temporary-workspace> -import <repository-path> -cleanBuild W6300-TCP-Loopback/Debug
```

On Ubuntu, the measured clean-build command using `CubeIDE`'s bundled toolchain is documented in UBUNTU_TEST.md. It completed with zero build errors and warnings in the unmodified vendor loopback source.

Program, verify, and reset the generated ELF with `STM32CubeProgrammer CLI` (adjust the executable and ELF paths for the host):

```text
STM32_Programmer_CLI -c port=SWD -w Debug/W6300-TCP-Loopback.elf -v -rst
```

Capture `USART3` through the ST-LINK VCP at `115200 baud`, `8-N-1`, no flow control. Startup output reports QSPI mode, identity, HAL errors, PHY state, and TCP socket state. Network settings are configured in `app_config.h`. See the platform test guides for device discovery and host network setup.

## Validation summary

- Ubuntu 22.04 bring-up: `Quad` identity, `Single`/`Dual`/`Quad` TX-buffer diagnostics, `Quad` read/write direction checks, ping, and TCP echo passed at `0.962 MHz` with `ChipSelectHighTime=2`. Diagnostic code was removed after validation; these results are retained as test history.
- TCP payload sizes 1 through 65536 bytes passed; 64-byte fresh connections passed 1000 iterations.
- Windows: `Single` fallback and TCP echo passed. Windows `Quad` operation is not claimed by that run.

The detailed Golden Reference comparison, diagnostic results, and limits on attributing the change are in UBUNTU_TEST.md. Windows network steps are in WINDOWS_TEST.md.

## CubeMX regeneration and dependency updates

Use the checked-in `.ioc` with `CubeMX 6.12.0` and `STM32CubeH7 1.11.2`. Review regenerated main.c, `stm32h7xx_hal_msp.c`, main.h, and `.cproject`; preserve the application call, `Debug`/`Release` include paths and defines, and source exclusions. Keep hand-written code in App/ and generated-file additions in `USER CODE` sections.

Initialize the pinned dependency after cloning:

```bash
git submodule update --init --recursive
```

To update ioLibrary intentionally, review its source and license, then update the submodule gitlink and `THIRD_PARTY_NOTICES.md` together.
