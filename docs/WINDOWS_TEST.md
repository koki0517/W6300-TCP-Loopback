# Windows Ethernet and TCP test

## Configure the direct-link adapter

In Settings → Network & Internet → Ethernet → IP assignment, set the adapter connected to WIZ630io to Manual IPv4:

- Address: `192.168.0.20`
- Subnet mask: `255.255.255.0` (/24)
- Gateway and DNS: blank

Do not change unrelated adapters. This direct link does not provide Internet access.

## Check the connection

After the firmware reports `PHY link UP`, check the W6300 address:

```powershell
ping 192.168.0.10
arp -a
Test-NetConnection 192.168.0.10 -Port 5000
```

`TcpTestSucceeded` should be True, using source address `192.168.0.20`.

## Run the binary echo test

`Python 3` and its standard library are sufficient:

```powershell
py -3 tools\tcp_loopback_test.py
```

The default matrix checks payload sizes 1, 64, 512, 1460, 2048, 4096, 16384, and 65536 bytes. Payloads are binary and include zero bytes.

Useful options:

```powershell
py -3 tools\tcp_loopback_test.py --source 192.168.0.20
py -3 tools\tcp_loopback_test.py --size 64 --count 200
py -3 tools\tcp_loopback_test.py --size 1 --size 64 --size 2048 --timeout 10
```

`--count` opens a fresh TCP connection for each iteration. `--source` binds the client to the direct-link adapter. The client exits non-zero on connection errors, timeout, early EOF, or payload mismatch.

## Recorded Windows result

The 2026-09-28 Windows run verified ping, ARP, TCP port 5000, the default payload matrix, and 200 fresh 64-byte connections. That firmware selected the Single 1-1-1 fallback; this record does not establish Quad operation on Windows. The latest Quad TCP validation is documented in [UBUNTU_TEST.md](UBUNTU_TEST.md).

## Troubleshooting

| Symptom | Check |
| --- | --- |
| PHY remains DOWN | Confirm the PC adapter is connected to WIZ630io RJ45 and is up. |
| Ping fails | Check both /24 addresses, PHY link, and ARP before investigating TCP. |
| TCP connection fails | Confirm UART reports `TCP LISTEN` on port 5000 and `Test-NetConnection` uses source `192.168.0.20`. |
| QSPI identity fails | Check the wiring in [HARDWARE.md](HARDWARE.md) and inspect the startup UART log. |
| Quad identity passes but data is wrong | Use the validated wiring and OCTOSPI conditions recorded in [UBUNTU_TEST.md](UBUNTU_TEST.md). |

The ST-LINK VCP uses `115200 baud`, 8 data bits, no parity, one stop bit, and no flow control. Its `COM port` depends on the PC.
