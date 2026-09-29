# W6300 TCP Loopback

`NUCLEO-H723ZG` と `WIZ630io`（`W6300`）を `OCTOSPI1` で接続し、PC からの `TCP/IPv4` データをそのまま echo するサンプルです。STM32 内蔵 Ethernet、`LwIP`、`FreeRTOS`、`DMA` は使用しません。

## Validation status

Ubuntu 22.04 で `Quad 1-4-4` の identity、buffer read/write、TCP echo を実機確認しています。検証条件は `SCK 0.962 MHz`、`ChipSelectHighTime=2`、`WIZ630io` の `J3-5 GND` と `J2 GND` の接続です。`1 byte`〜`64 KiB` の payload と、64 byte の新規接続 1,000 回が成功しました。詳細と条件は [Ubuntu validation](docs/UBUNTU_TEST.md) を参照してください。Windows で記録した結果は Single fallback による通信確認です。

## Hardware

```text
PC 192.168.0.20/24
    │ Ethernet
WIZ630io / W6300 192.168.0.10/24
    │ OCTOSPI1
NUCLEO-H723ZG
```

必要なもの、現在の配線、PC 側 IP 設定は [Hardware and wiring](docs/HARDWARE.md) を参照してください。

## Build and run

1. Clone the repository with its pinned submodule:

   ```bash
   git clone --recurse-submodules https://github.com/koki0517/W6300-TCP-Loopback.git
   ```

2. Open `W6300-TCP-Loopback.ioc` in `CubeMX 6.12.0` with `STM32CubeH7 1.11.2` installed and run **Generate Code** to restore the ignored HAL/CMSIS and generated support files. Then import the repository as an existing `STM32CubeIDE` project, build `Debug`, flash the ELF with `STM32CubeProgrammer`, and reset the board. See [Firmware and build](docs/FIRMWARE.md).

3. Connect the PC Ethernet adapter to `WIZ630io` and set it to `192.168.0.20/24`. Follow the platform guide: [Windows](docs/WINDOWS_TEST.md) or [Ubuntu 22.04](docs/UBUNTU_TEST.md).

4. Run the standard-library `Python` client:

   ```bash
   python3 tools/tcp_loopback_test.py --host 192.168.0.10 --source 192.168.0.20
   ```

   On Windows, use py -3 tools\tcp_loopback_test.py.

The default test matrix sends binary payloads from `1 byte` through `64 KiB` and checks every returned byte. `Python` packages are not required.

## Configuration and license

Network settings, TCP port, socket, and buffer size are in [app_config.h](App/Inc/app_config.h). See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for dependency and license information.
