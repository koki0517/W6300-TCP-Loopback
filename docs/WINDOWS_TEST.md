# Windows Ethernet and TCP test

## Configure the direct-link adapter

In Windows **Settings → Network & Internet → Ethernet → IP assignment**, set the adapter connected to WIZ630io to Manual IPv4:

- IP address: `192.168.0.20`
- Subnet mask / prefix: `255.255.255.0` / `24`
- Gateway and DNS: leave blank

Do not change unrelated adapters. Windows may label the direct link **Unidentified network** or **No Internet**; that is expected because this link does not provide Internet access.

## Check link, IP, and TCP port

Confirm which adapter is connected and has the configured address:

```powershell
Get-NetAdapter
Get-NetIPAddress -AddressFamily IPv4
```

After UART reports `[PHY] link UP`, test ping and ARP:

```powershell
ping 192.168.0.10
arp -a
```

Check TCP port 5000:

```powershell
Test-NetConnection 192.168.0.10 -Port 5000
```

The result should include `TcpTestSucceeded : True` and select the Ethernet adapter with source address `192.168.0.20`.

## Run the binary echo test

Python 3 and its standard library are sufficient; no `pip install` is needed.

```powershell
py -3 tools\tcp_loopback_test.py
```

The default matrix tests 1, 64, 512, 1460, 2048, 4096, 16384, and 65536 bytes. Payloads are deterministic binary data containing `0x00`. The client sends the full payload, loops until it has received exactly the same number of bytes, and compares every byte.

```powershell
py -3 tools\tcp_loopback_test.py --host 192.168.0.10 --port 5000
py -3 tools\tcp_loopback_test.py --size 64 --count 200
py -3 tools\tcp_loopback_test.py --size 1 --size 64 --size 2048 --timeout 10
```

Each count iteration opens a new TCP connection to exercise socket close/relisten recovery. The hardware test passed 200 fresh connections at 64 bytes, plus the full default size matrix including 64 KiB.

If Windows has multiple active adapters and chooses the wrong route, bind to the direct Ethernet address:

```powershell
py -3 tools\tcp_loopback_test.py --source 192.168.0.20
py -3 tools\tcp_loopback_test.py --source 192.168.0.20 --size 64 --count 200
```

Any connection, timeout, EOF, or payload mismatch exits non-zero. Ctrl+C exits with status 130.

## Troubleshooting

| Symptom | Check |
| --- | --- |
| WIZ630io link LED is off | Verify 3V3/GND, RJ45 cable, PC Ethernet adapter link, and RSTn release. |
| PHY remains DOWN | Connect the PC adapter to the WIZ630io RJ45, not the NUCLEO onboard Ethernet connector. Confirm the PC adapter is up. |
| Ping fails | Check PC `192.168.0.20/24`, W6300 `192.168.0.10/24`, PHY link, ARP, and UART CIDR sanity before investigating TCP. |
| TCP connect fails | Confirm UART reports `[TCP] listening on port 5000`, then check `Test-NetConnection`, adapter selection, and IP settings. |
| Quad identity passes but payload data is wrong | Current Ubuntu PF7 wiring passed identity reads but failed Quad TX-buffer reads and TCP payload comparison. Check the Quad receive phase and QD0-QD3 signals; see the measured captures/results in [UBUNTU_TEST.md](UBUNTU_TEST.md). |
| Single identity read fails or HAL reports a QSPI error | Check PB2 clock, PG6 hardware NCS, PD11/PD12/PF7/PD13 data wiring, Clock Mode 0, address/data phases, dummy cycles, and reset. |
| QSPI IO2 is unreliable | The current QD2 path is WIZ630io J3-3 to PF7 at CN9-26/D62. SB67 affects the alternate PE2 board route and is not in this PF7 signal path. |

Use the ST-LINK VCP at 115200 baud, 8 data bits, no parity, one stop bit, and no flow control. Read the startup log to see which QSPI mode was selected and whether the PHY is up.

## PC-side hardware validation record

On 2026-09-28, the ASIX Ethernet adapter was Up at 100 Mbps with `192.168.0.20/24`; ping succeeded 3/3 and `Test-NetConnection` reported `TcpTestSucceeded=True` using that adapter. The final Mode 3 firmware passed every default binary size. Under the branch-matched Mode 0 setup, 200 fresh TCP connections at 64 bytes also passed. The test used Python 3.12.14's standard library. This PC had the Windows `py.exe` launcher but no separately installed Python interpreter, so the local run used the available bundled runtime; install Python 3 on a normal Windows setup to use `py -3` as shown above.
