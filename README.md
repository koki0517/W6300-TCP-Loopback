# W6300 TCP Loopback

NUCLEO-H723ZGとWIZ630io（W6300）をOCTOSPI1/QSPIで接続し、Windows PCから送信した任意のバイト列をそのまま返すTCP/IPv4サーバーです。STM32内蔵Ethernet、LwIP、FreeRTOS、DMAは使いません。

実機では、参照ブランチのQuad設定（Mode 0、約0.96 MHz）でCIDR読出しが失敗し、Single 1-1-1へfallbackするとTCP echoが動作しました。1〜65536 byteのbinary echoと64 byte×200接続は成功しています。要求仕様のQuad 1-4-4は未成立で、原因をソフト設定だけでは説明できず、IO0〜IO3の信号・配線確認が残っています。詳細は[ハードウェア](docs/HARDWARE.md)と[firmware検証記録](docs/FIRMWARE.md)を参照してください。

## 構成

```text
Windows PC                             NUCLEO-H723ZG
192.168.0.20/24                       STM32H723ZGTx
Python TCP client   ── Ethernet ──>   OCTOSPI1/QSPI ── WIZ630io / W6300
192.168.0.10:5000  <── echo bytes ──   hardwired TCP/IPv4 server
```

## 必要なもの

- NUCLEO-H723ZG、WIZ630io、配線（[ハードウェア手順](docs/HARDWARE.md)）
- STM32CubeIDE **1.16.0** とSTM32CubeH7 FW **1.11.2**
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

このテストは0x00を含むbinary payloadを使い、1 byteから64 KiBまでを送受信して完全一致を確認します。追加手順とトラブルシューティングは[Windowsテスト手順](docs/WINDOWS_TEST.md)にあります。

## 設定とライセンス

IP、MAC、TCP port、socket番号、loopback bufferは[`App/Inc/app_config.h`](App/Inc/app_config.h)で変更できます。配線（QD2=PF7、SB67変更なし）は[docs/HARDWARE.md](docs/HARDWARE.md)、設計とCubeMX再生成時の注意点は[docs/FIRMWARE.md](docs/FIRMWARE.md)を参照してください。

このリポジトリはCubeIDE projectとSTM32CubeH7 HAL/CMSISの既存ライセンスを保持します。WIZnet ioLibrary_Driverはpinned Git submoduleとして配布します。ライセンス概要は[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)を確認してください。
