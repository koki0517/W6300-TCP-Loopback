# W6300 TCP Loopback

NUCLEO-H723ZGとWIZ630io（W6300）をOCTOSPI1/QSPIで接続し、PCから送信した任意のバイト列を返すTCP/IPv4サーバーです。STM32内蔵Ethernet、LwIP、FreeRTOS、DMAは使いません。

Windows側ではSingle fallbackによるTCP echoが完全に成功しています。UbuntuでQD2をPF7へ配線し直した後は、Single/Dual/Quadのidentity readが各100/100で成功しました。一方、QuadのTX-buffer readでは8 byteからデータ不一致が起き、TCPでも接続後の64 byte payloadが不一致でした。現在はQuad read data pathを切り分けています。実配線、過去のPE2試験、UART・register・network結果は[ハードウェア手順](docs/HARDWARE.md)、[Ubuntu 22.04テスト](docs/UBUNTU_TEST.md)、[firmware検証記録](docs/FIRMWARE.md)に記録しています。

## 構成

```text
Windows PC                             NUCLEO-H723ZG
192.168.0.20/24                       STM32H723ZGTx
Python TCP client   ── Ethernet ──>   OCTOSPI1/QSPI ── WIZ630io / W6300
192.168.0.10:5000  <── echo bytes ──   hardwired TCP/IPv4 server
```

## 必要なもの

- NUCLEO-H723ZG、WIZ630io、配線（[ハードウェア手順](docs/HARDWARE.md)）
- STM32CubeIDE 1.16.x（Windows 1.16.0、Ubuntu 1.16.1で検証）とSTM32CubeH7 FW **1.11.2**
- Python 3（標準ライブラリだけを使います）
- Git for Windows

## Quick start

```powershell
git clone --recurse-submodules https://github.com/koki0517/W6300-TCP-Loopback.git
cd W6300-TCP-Loopback
```

既存のcloneを使う場合はsubmoduleを取得します。

```powershell
git submodule update --init --recursive
```

STM32CubeIDEで既存のCubeIDE projectとしてリポジトリをimportし、`Debug` configurationをbuildします。NUCLEOをST-LINK USBで接続してDebug ELFを書き込み、resetします。詳細は[firmware手順](docs/FIRMWARE.md)を参照してください。

Windows Ethernet adapterに`192.168.0.20`、サブネットマスク`255.255.255.0`を設定し、PCをWIZ630ioのRJ45へ接続します。gatewayとDNSは空欄のままで構いません。

```powershell
py -3 tools\tcp_loopback_test.py
```

このテストは0x00を含むbinary payloadを使い、1 byteから64 KiBまでを送受信して完全一致を確認します。Windows手順は[Windowsテスト](docs/WINDOWS_TEST.md)、Ubuntu 22.04でのbuild/flash/UART/Ethernet手順は[Ubuntuテスト](docs/UBUNTU_TEST.md)にあります。

## 設定とライセンス

IP、MAC、TCP port、socket番号、loopback bufferは[`App/Inc/app_config.h`](App/Inc/app_config.h)で変更できます。Windows手順は[docs/WINDOWS_TEST.md](docs/WINDOWS_TEST.md)、Ubuntu 22.04のbuild/flash/UART/Ethernet手順は[docs/UBUNTU_TEST.md](docs/UBUNTU_TEST.md)、現在の配線は[docs/HARDWARE.md](docs/HARDWARE.md)を参照してください。

このリポジトリはCubeIDE projectとSTM32CubeH7 HAL/CMSISの既存ライセンスを保持します。WIZnet ioLibrary_Driverはpinned Git submoduleとして配布します。ライセンス概要は[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)を確認してください。
