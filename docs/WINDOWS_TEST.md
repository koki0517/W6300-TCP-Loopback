# Windows Ethernet and TCP test

## Set the direct-link IPv4 address

In Windows **Settings → Network & Internet → Ethernet → IP assignment**, edit the adapter connected to the WIZ630io and choose Manual IPv4:

- IP address: `192.168.0.20`
- Subnet mask / prefix: `255.255.255.0` / `24`
- Gateway and DNS: leave blank

Do not change unrelated adapters. The direct WIZ630io connection may appear as **Unidentified network** or **No Internet**; that is normal.

## Check link and reachability

Confirm the correct Ethernet adapter reports `Up` and has `192.168.0.20/24`:

```powershell
Get-NetAdapter
Get-NetIPAddress -AddressFamily IPv4
```

After the firmware logs `[PHY] link UP`, test IPv4 and ARP:

```powershell
ping 192.168.0.10
arp -a
```

Check TCP port 5000:

```powershell
Test-NetConnection 192.168.0.10 -Port 5000
```

The TCP result should contain `TcpTestSucceeded : True`.

## Run the binary echo test

Python 3 and its standard library are sufficient; no `pip install` is needed.

```powershell
py -3 tools\tcp_loopback_test.py
```

The defaults test payload sizes 1, 64, 512, 1460, 2048, 4096, 16384, and 65536 bytes. The pattern is binary and includes `0x00`; each test connects, sends all bytes, receives the exact byte count in a loop, and compares every byte.

```powershell
py -3 tools\tcp_loopback_test.py --host 192.168.0.10 --port 5000
py -3 tools\tcp_loopback_test.py --size 64 --count 1000
py -3 tools\tcp_loopback_test.py --size 1 --size 64 --size 2048 --timeout 10
```

If Windows has multiple active adapters and chooses the wrong route, bind to the direct Ethernet address:

```powershell
py -3 tools\tcp_loopback_test.py --source 192.168.0.20
py -3 tools\tcp_loopback_test.py --source 192.168.0.20 --size 64 --count 1000
```

Each count iteration opens a new TCP connection to exercise socket close/relisten recovery. Any connection, timeout, EOF, or payload mismatch exits non-zero. Ctrl+C exits with status 130.

## Troubleshooting

| Symptom | Check |
| --- | --- |
| WIZ630io link LED is off | Verify 3V3/GND, RJ45 cable, PC Ethernet adapter link, RSTn release, and the WIZ630io PHY LED. |
| PHY remains DOWN | Check the RJ45 connection goes to the PC adapter, not the NUCLEO onboard Ethernet connector. Confirm the PC adapter is enabled and `Up`. |
| Ping fails | Check PC `192.168.0.20/24`, W6300 `192.168.0.10/24`, PHY link, ARP, and the UART CIDR sanity check before investigating TCP. |
| TCP connect fails | Confirm UART reports `[TCP] listening on port 5000`, then inspect `Test-NetConnection`, adapter selection, and IP settings. |
| UART reports an invalid CIDR or QSPI HAL error | Check PB2 clock, PG6 hardware NCS, PD11/PD12/PE2/PD13 data wiring, Mode 3, Quad phases, and the 16-bit address/two dummy cycles. Check that SB67 is OFF so PE2 is available for IO2. |
| NUCLEO QSPI IO2 is unreliable | Recheck **SB67 OFF**. PE2 must not be loaded by the NUCLEO circuitry. |

Use the ST-LINK VCP at 115200 baud, 8 data bits, no parity, one stop bit, and no flow control to read startup and QSPI diagnostics.
