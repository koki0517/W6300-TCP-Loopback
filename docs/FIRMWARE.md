# Firmware and CubeIDE project

## Software architecture

`Core/Src/main.c` initializes HAL, the existing HSE/PLL system clock, GPIO, USART3, and OCTOSPI1, then calls `w6300_app_init()` and polls `w6300_app_poll()`. If both QSPI identity checks fail at startup, it retries initialization once per second. The application and W6300 HAL adapter live under `App/`; CubeMX-generated peripheral setup stays in `Core/`.

`App/Src/w6300_port.c` holds active-low RSTn low for 100 ms and waits 100 ms after release. It registers blocking HAL OSPI read/write callbacks. OCTOSPI1 owns hardware NCS on PG6; ioLibrary CS callbacks are no-ops and never bit-bang the pin. QSPI transfer errors are counted and logged over USART3.

## CubeMX and OCTOSPI configuration

The checked-in `.ioc` is the configuration source of truth. It targets CubeMX 6.12.0 and STM32CubeH7 FW 1.11.2. Keep this package version when regenerating the project.

- MCU: STM32H723ZGTx; preserve the existing 8 MHz HSE bypass and 550 MHz system clock.
- OCTOSPI1: Port 1 Quad, Clock Mode 0, STR, DTR/DQS/free-running clock disabled, blocking polling, no DMA/MDMA or OCTOSPI interrupt. Current HAL `DeviceSize=17` and `ChipSelectHighTime=2`.
- OCTOSPI kernel: PLL2R at 76 MHz, derived from the existing 8 MHz HSE / PLL2 plan (`M=1`, `N=19`, `R=2`). HAL stores `ClockPrescaler - 1` in DCR2, so the divider is the configured HAL value; prescaler 79 gives about 0.962 MHz SCLK (`76 MHz / 79`) to match the reference branch's roughly 0.96 MHz clock.
- Pins: PB2 CLK, PG6 hardware NCS, PD11 IO0, PD12 IO1, PF7 IO2, PD13 IO3. The current physical QD2 wire is WIZ630io J3-3 to PF7 / CN9-26 / D62. PF7 uses OCTOSPIM Port 1 IO2 in the LOW group. SB67 is outside this external PF7 path; the actual state of its chip resistor has not been verified. Earlier PE2-configured tests are recorded as history below and in [UBUNTU_TEST.md](UBUNTU_TEST.md), without inferring their physical wiring.
- Sampling: no extra half-cycle shift. This matches the Golden Reference's sample timing. With J3 ground connected and `ChipSelectHighTime=2`, Quad identity and payload reads passed the full diagnostic matrix at 0.962025 MHz; normal Quad TCP echo also passed. The ground and CS-high-time changes were applied together for the successful matrix, so their individual contributions are not isolated.
- USART3: PD8/PD9, 115200 baud, 8-N-1, ST-LINK VCP.
- PF4: active-low `W6300_RSTn`, GPIO output, idle high.
- STM32 internal ETH/RMII, USB_OTG_HS and its GPIO, LwIP, and RTOS are disabled.

Each ioLibrary transfer uses a one-line 8-bit instruction and a 16-bit address. Quad mode uses four-line address/data phases and two dummy cycles. DTR and DQS are disabled. The callback preserves the opcode's register bits and applies the selected mode bits, matching the reference branch; Quad's `0x80` opcode remains `0x80`. HAL failures are counted and logged over USART3. When `APP_QSPI_DIAGNOSTIC_ONLY=1`, no network or socket initialization runs, and the diagnostic does not fall back to Single. `APP_QSPI_BUFFER_DIAGNOSTIC` enables TX-buffer payload checks; `APP_QSPI_REPEAT_READ_DIAGNOSTIC` repeatedly reads a fixed TX-buffer pattern in Quad mode for logic-analyzer capture. Both options default to off.

## W6300 startup and fallback

Startup proceeds as follows:

1. Reset W6300 and wait for it to settle.
2. Register the ioLibrary QSPI callbacks and check the raw CIDR major byte, VER, SYSR, and HAL OSPI status.
3. Try Quad first. If its identity check fails, log the values, reset the chip, and try Single 1-1-1. If both fail, keep the MCU running and retry initialization after one second.
4. Initialize socket memory: socket 0 receives 32 KiB TX and RX; sockets 1–7 receive zero.
5. Configure static IPv4 (`NETINFO_STATIC_V4`, `NETINFO_STATIC`) and select `AS_IPV4` loopback mode.
6. Poll PHY state and run the vendor `loopback_tcps()` service on socket 0, port 5000.

The expected raw CIDR major register is `0x61`, normalized as `0x6100`; VER is `0x4661`. The ioLibrary `getCIDR()` helper also folds RTL bits into its result, so raw CIDR `0x61` and RTL `0x11` produce API CIDR `0x6300`. Firmware checks the raw major byte and VER. The referenced `sucess-udp-callback` branch tries Quad and then falls back to Single 1-1-1; UDP success by itself therefore does not prove Quad operation.

The application buffer is 2048 bytes. TCP remains a byte stream: vendor loopback code echoes received bytes without assuming text, terminators, or packet boundaries. A recoverable socket error closes socket 0 so the vendor loop can reopen and listen. A QSPI HAL error is logged and latches the red LED; PHY link state controls the green LED.

## Configuration and source tree

Change MAC, IPv4 address, subnet, gateway, DNS, TCP port, socket number, buffer size, reset delays, clock divider, dummy cycles, and diagnostic callback selection in `App/Inc/app_config.h`. `W6300_GOLDEN_CRITICAL_CALLBACKS=1U` matches the Golden Reference IRQ-masking callbacks for comparisons; the normal firmware leaves it at `0U` so HAL polling timeouts keep using SysTick. Defaults are:

- MAC `02:00:00:00:00:10`
- IPv4 `192.168.0.10/24`
- gateway/DNS `0.0.0.0`
- TCP port 5000, socket 0, buffer 2048 bytes

```text
App/Inc, App/Src/                  Application and W6300 HAL port
Core/                              CubeMX startup and peripheral code
Drivers/                           STM32CubeH7 HAL and CMSIS
Middlewares/Third_Party/ioLibrary_Driver/  Pinned WIZnet submodule
tools/tcp_loopback_test.py         Windows standard-library test client
docs/                              Hardware, firmware, Windows, and Ubuntu setup
W6300-TCP-Loopback.ioc              CubeMX source of truth
```

The Debug and Release managed builds use `_WIZCHIP_=W6300` and `_WIZCHIP_QSPI_MODE_=QSPI_QUAD_MODE`, and include the required vendor sources `Ethernet/socket.c`, `Ethernet/wizchip_conf.c`, `Ethernet/W6300/w6300.c`, and `Application/loopback/loopback.c`. Other chip implementations are excluded from the managed build. Vendor source files are not modified.

## Build, flash, and UART

Import the repository as an existing STM32CubeIDE project and build the `Debug` configuration. The verified headless command form is:

```powershell
stm32cubeidec.exe --launcher.suppressErrors -nosplash -application org.eclipse.cdt.managedbuilder.core.headlessbuild -data <temporary-workspace> -import <repository-path> -cleanBuild W6300-TCP-Loopback/Debug
```

The Debug clean builds with CubeIDE 1.16.0 completed with zero errors and zero linker errors. They emitted 14 warnings in the unmodified vendor `Application/loopback/loopback.c`; application and HAL-port sources emitted no warnings.

Flash and verify the generated ELF with STM32CubeProgrammer CLI, then reset the target:

```powershell
STM32_Programmer_CLI.exe -c port=SWD -w .\Debug\W6300-TCP-Loopback.elf -v -rst
```

Open the ST-LINK VCP at 115200 baud, 8 data bits, no parity, one stop bit, no flow control. Startup logs report the QSPI attempt/fallback, raw CIDR and VER, HAL status, static network settings, PHY link, and TCP LISTEN state. COM port numbers vary by PC.

## Hardware validation on 2026-09-27 through 2026-09-29

- CubeIDE 1.16.0 Debug clean builds completed with 0 compiler/linker errors. The latest build reports 14 warnings from the unmodified vendor `Application/loopback/loopback.c`; the application and HAL port have no warnings.
- CubeProgrammer 2.17.0 detected the connected NUCLEO-H723ZG (STM32H72x, Device ID `0x483`, ST-LINK SN `002E00343532511131333430`), programmed and verified the ELF, and reset the MCU. USART3 ST-LINK VCP is COM4 at 115200 8-N-1.
- Historical PE2-configured test: an older project revision and SWD snapshot show PE2 configured AF10 for IO2 (`GPIOE_MODER=0xFFFFFFE7`, `OSPEEDR=0x00000030`, `PUPDR=0`, `AFRL=0x00000A00`) while PF7 was unassigned. P1CR=`0x02010101` selected Port 1 LOW. The physical QD2 connection and SB67 resistor state during that test were not independently verified. This is not the current pin assignment.
- Earlier Windows Quad attempts varied Mode 0/3 and sample shifting, including a 0.962 MHz run. The physical wiring at those test times is not independently verified, so do not classify those runs as either a confirmed wiring mismatch or proof about today's PF7 route. See the separated historical and current results in [UBUNTU_TEST.md](UBUNTU_TEST.md).
- In the earlier Mode 0 / 0.962 MHz Windows startup, Quad identity failed and the application's Single 1-1-1 fallback read raw CIDR `0x61` (normalized `0x6100`), API CIDR `0x6300`, RTL `0x11`, VER `0x4661`, and SYSR `0x01`; HAL errors stayed zero. This Single result remains valid because Single does not use IO2. It configured MAC `02:00:00:00:00:10`, IPv4 `192.168.0.10/24`, PHY link UP, and TCP LISTEN on port 5000.
- Windows showed the ASIX adapter Up at 100 Mbps with `192.168.0.20/24`; ARP resolved `192.168.0.10` to `02-00-00-00-00-10`. Ping succeeded 3/3 and `Test-NetConnection` returned `TcpTestSucceeded=True` when run outside the sandbox.
- With the final Mode 3 firmware, the repository Python client passed sizes 1, 64, 512, 1460, 2048, 4096, 16384, and 65536 bytes (90,101 payload bytes total) through the Single fallback. Under the branch-matched Mode 0 setup, it also passed 200 separate 64-byte connections with 0 mismatches and 0 disconnects. The Windows machine had no system Python 3 installed, so the standard-library script was invoked with the bundled Python runtime.
- The Windows results demonstrate working Single-mode QSPI, W6300 networking, Windows link, and TCP echo. Historical Ubuntu diagnostics with PE2 selected in firmware passed Single and Dual 100/100 while Quad remained 0/100 with HAL errors 0; physical wiring for those runs was not independently verified. On 2026-09-29, the current PF7 setup passed Single/Dual/Quad buffer diagnostics and the normal Quad TCP test after J3 GND was connected and `ChipSelectHighTime` was changed from 1 to 2. Earlier failures, intermediate order-sensitive reads, and the successful follow-up are recorded in [UBUNTU_TEST.md](UBUNTU_TEST.md).

## Ubuntu 22.04 and Golden Reference follow-up

The Ubuntu run compares this firmware against the known-working `Boards_2026/Firmware/UDP2CANFD` Golden Reference. Its transaction format and OCTOSPI settings are documented in [UBUNTU_TEST.md](UBUNTU_TEST.md). The current physical QD2 route is PF7; live GPIO/OCTOSPIM readback confirmed PF7 AF10 and Port 1 LOW routing. On 2026-09-29, with J3 GND connected and CS high time set to 2 cycles, Single/Dual/Quad identity and TX-buffer diagnostics passed, as did normal Quad TCP echo. The experiment history and limits on attributing the fix to grounding versus CS timing are in [UBUNTU_TEST.md](UBUNTU_TEST.md). The repeated fixed-pattern capture diagnostic remains available and defaults off.

## CubeMX regeneration and submodule updates

Use the existing `.ioc` with CubeMX 6.12.0 and STM32CubeH7 1.11.2. Review regenerated `main.c`, `stm32h7xx_hal_msp.c`, `main.h`, and `.cproject`; preserve the application call and both Debug/Release include paths, defines, and source exclusions. Keep hand-written code in `App/` and generated-file additions in `USER CODE` regions.

Initialize the pinned dependency after cloning:

```powershell
git submodule update --init --recursive
```

To intentionally move to a newer upstream ioLibrary revision, review its source and license, update the submodule, then update the gitlink and `THIRD_PARTY_NOTICES.md` together.
