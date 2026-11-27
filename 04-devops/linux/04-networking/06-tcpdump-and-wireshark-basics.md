# Packet Analysis with Tcpdump, Wireshark, and BPF

| Metadata | Details |
| :--- | :--- |
| **Topic** | Linux Packet Capture, `tcpdump`, Berkeley Packet Filters (BPF), PCAP Analysis, Wireshark & Tshark |
| **Module** | `04-networking` |
| **Track** | `04-devops/linux` |
| **Prerequisites** | `04-networking/01-networking-basics.md`, `04-networking/05-firewall.md` |
| **Target Audience** | SREs, Network Engineers, Security Operations, Incident Responders |
| **Target Distribution** | Ubuntu 24.04 LTS, Rocky Linux / RHEL 9.4 |
| **Key Tools** | `tcpdump`, `tshark`, `wireshark`, `libpcap`, `capinfos`, `mergecap` |

---

## Table of Contents
1. [What & Why](#what--why)
2. [Core Concepts](#core-concepts)
3. [Internal Working](#internal-working)
4. [Syntax & CLI Tools](#syntax--cli-tools)
5. [Progressive Practical Examples](#progressive-practical-examples)
6. [Best Practices](#best-practices)
7. [Common Mistakes & Gotchas](#common-mistakes--gotchas)
8. [Performance, Security & Cost Implications](#performance-security--cost-implications)
9. [Comparison Table / Alternatives](#comparison-table--alternatives)
10. [Real-World Troubleshooting & Interview Q&A](#real-world-troubleshooting--interview-qa)
11. [Hands-On Practice Exercises](#hands-on-practice-exercises)
12. [Summary Cheat Sheet & Further Reading](#summary-cheat-sheet--further-reading)

---

## What & Why

When application logs fail to reveal why an API request times out, why a microservice emits `502 Bad Gateway`, or why a TLS handshake aborts prematurely, the ultimate source of truth is the **wire**: the raw network frames transmitted and received by the operating system.

**Packet sniffing** captures frames passing through Linux network interfaces before they reach or after they exit the kernel TCP/IP stack.
- **`tcpdump`**: The lightweight, command-line standard available on virtually every production Linux server. Powered by `libpcap`, it enables non-intrusive capture and filtering.
- **Berkeley Packet Filters (BPF)**: In-kernel filtering instructions that discard uninteresting packets before copying them into userspace, preventing CPU exhaustion during high-throughput packet captures.
- **Wireshark and `tshark`**: The industry-standard graphical and CLI protocol analyzers that decode thousands of application protocols, reconstruct TCP streams, and analyze latency bottlenecks.

Mastering packet capture is an indispensable skill for diagnosing TCP retransmissions, zero-window freezes, MTU fragmentation issues, and security intrusions.

---

## Core Concepts

### Packet Capture Architecture: `libpcap` and `AF_PACKET`
In normal operations, applications interact with Layer 4 sockets (`AF_INET`). Packet capture utilities create raw packet sockets (`socket(AF_PACKET, SOCK_RAW, ...)`).
- When a frame arrives at the NIC driver, the kernel makes a clone of the frame.
- The clone is passed to the **Packet Filter (BPF)** running inside kernel space.
- Only frames matching the BPF filter are copied across the kernel-userspace boundary into the `tcpdump` circular memory buffer.

### Berkeley Packet Filter (BPF) Syntax
BPF filters use a natural language expression syntax composed of **primitives**:
- **Type**: `host`, `net`, `port`, `portrange` (e.g., `host 10.0.1.5`, `net 192.168.0.0/16`, `port 443`).
- **Direction**: `src`, `dst`, `src or dst`, `src and dst`.
- **Protocol**: `ip`, `ip6`, `tcp`, `udp`, `icmp`, `arp`, `ether`.
- **Logical Operators**: `and` (`&&`), `or` (`||`), `not` (`!`).
- **Bitwise / Header Offsets**: Inspect specific bits or bytes (e.g., `tcp[tcpflags] & tcp-syn != 0`).

### TCP Control Flags in Dissections
Decoded frames reveal the 6 classical TCP control flags:
- `SYN` (`S` in tcpdump): Synchronize sequence numbers; initiates connection.
- `ACK` (`.` in tcpdump): Acknowledge received data.
- `FIN` (`F` in tcpdump): Finished; graceful sender shutdown.
- `RST` (`R` in tcpdump): Reset; abrupt connection termination / refusal.
- `PSH` (`P` in tcpdump): Push data directly to application without buffer delay.
- `URG` (`U` in tcpdump): Urgent pointer field significant.

---

## Internal Working

```
+-----------------------------------------------------------------------------------------+
|                                    Physical Wire / NIC                                  |
|                                       Ethernet Frame                                    |
+---------------------------------------------+-------------------------------------------+
                                              |
                                              v
+-----------------------------------------------------------------------------------------+
|                                 Linux Kernel NIC Driver                                 |
|                                    Allocates sk_buff                                    |
+---------------------------------------------+-------------------------------------------+
                                              |
                      +-----------------------+-----------------------+
                      |                                               |
                      v                                               v
+-------------------------------------------+   +-----------------------------------------+
|        Kernel TCP/IP Network Stack        |   |       AF_PACKET Raw Socket Tap          |
|    Netfilter -> Routing -> Transport      |   |                                         |
|                                           |   |   +---------------------------------+   |
|                                           |   |   |   In-Kernel BPF Filter Engine   |   |
|                                           |   |   |   Evaluates expression bytecode |   |
|                                           |   |   +----------------+----------------+   |
+---------------------+---------------------+   +--------------------|--------------------+
                      |                                              | Match?
                      v                                              v
+-------------------------------------------+   +-----------------------------------------+
|             Application Socket            |   |       Circular Ring Buffer (mmap)       |
|            Nginx, Postgres, etc.          |   +--------------------+--------------------+
+-------------------------------------------+                        |
                                                                     | Copy to Userspace
                                                                     v
                                                +-----------------------------------------+
                                                |            tcpdump / tshark             |
                                                |       Displays text or writes .pcap     |
                                                +-----------------------------------------+
```

### The In-Kernel BPF Virtual Machine
When you specify `tcpdump -i eth0 'tcp port 80'`:
1. `tcpdump` compiles the string into raw pseudo-machine assembly instructions (cBPF bytecode).
2. It passes this bytecode into the kernel via `setsockopt(..., SO_ATTACH_FILTER, ...)`.
3. The kernel executes these instructions directly against the incoming `sk_buff` in memory.
4. Packets that fail the test (e.g., UDP packets or packets on port 22) are discarded immediately without allocating userspace memory or incurring context switches.

---

## Syntax & CLI Tools

### Essential `tcpdump` Command Flags
```bash
# Capture controls
-i <iface>      # Interface to listen on (e.g., eth0, lo, or 'any' for all interfaces)
-n              # Do not resolve IP addresses to hostnames (prevents reverse DNS delays)
-nn             # Do not resolve IP addresses OR port names (shows raw numbers: 443 instead of https)
-s <snaplen>    # Snapshot length: number of bytes per packet to capture (0 = full packet, default)
-c <count>      # Exit after capturing <count> packets
-v, -vv, -vvv   # Increase packet header decoding verbosity

# Output and File I/O
-w <file.pcap>  # Write raw packets to a PCAP file instead of printing to screen
-r <file.pcap>  # Read and dissect packets from a saved PCAP file
-X              # Print packet contents in both Hex and ASCII (great for payload inspection)
-XX             # Same as -X, plus includes Ethernet link-layer headers
-l              # Line-buffered stdout (essential when piping tcpdump into grep or awk)

# Timestamps
-tttt           # Print timestamps in human-readable date format: 2026-11-26 10:15:30.123456
-ttt            # Print delta time (microseconds elapsed since previous packet)
```

### Rotating Packet Captures (Continuous Ring Buffer)
```bash
# Capture continuous traffic into 100MB files, keeping maximum 10 rotating files
sudo tcpdump -i eth0 -nn -s 0 -w /var/log/capture_%Y-%m-%d_%H%M%S.pcap \
  -C 100 -W 10 -G 3600
# (-C 100 = 100MB per file, -W 10 = keep 10 files, -G 3600 = rotate every hour)
```

### Modern CLI Analysis with `tshark` (Wireshark CLI)
```bash
# Analyze a PCAP file, extracting specific protocol fields cleanly
tshark -r trace.pcap -Y "http.request" -T fields -e ip.src -e http.host -e http.request.uri

# Extract TLS Server Name Indication (SNI) from client handshakes
tshark -r trace.pcap -Y "tls.handshake.extension.type == 0" -T fields -e ip.dst -e tls.handshake.extensions_server_name
```

---

## Progressive Practical Examples

### Example 1: Dissecting a TCP 3-Way Handshake in Real-Time
Observe how `tcpdump` formats a clean connection establishment.

```bash
# Terminal 1: Start sniffer focusing on target web server
sudo tcpdump -i any -nn "host 93.184.216.34 and tcp port 80"

# Terminal 2: Trigger single HTTP request
curl -s http://93.184.216.34 > /dev/null
```
**Output Breakdown**:
```text
10:00:01.100 IP 192.168.1.50.45210 > 93.184.216.34.80: Flags [S], seq 1294812, win 64240, options [mss 1460,sackOK], length 0
10:00:01.125 IP 93.184.216.34.80 > 192.168.1.50.45210: Flags [S.], seq 8391204, ack 1294813, win 65535, options [mss 1460,sackOK], length 0
10:00:01.126 IP 192.168.1.50.45210 > 93.184.216.34.80: Flags [.], ack 8391205, win 64240, length 0
```
- Line 1: Client sends `Flags [S]` (`SYN`), sequence `1294812`.
- Line 2: Server returns `Flags [S.]` (`SYN-ACK`), acknowledging client's sequence (`ack 1294813`).
- Line 3: Client returns `Flags [.]` (`ACK`), completing handshake. Connection is now `ESTABLISHED`.

### Example 2: Capturing DNS Queries and Resolving Latency Issues
Capture DNS query/response pairs on UDP port 53.

```bash
sudo tcpdump -i any -nn -vv "udp port 53"
```
**Sample Output**:
```text
10:05:00.100 IP 192.168.1.50.51234 > 1.1.1.1.53: [udp sum ok] 4120+ A? api.github.com. (32)
10:05:00.122 IP 1.1.1.1.53 > 192.168.1.50.51234: [udp sum ok] 4120 1/0/0 api.github.com. A 140.82.114.6 (48)
```
- Query ID `4120` matches between request and reply.
- Time delta `0.122 - 0.100 = 22ms` indicates exact DNS resolver round-trip time.

### Example 3: Isolating TCP Retransmissions and Resets (Connection Failures)
Filter exclusively for connection drops, resets, or unacknowledged retransmissions.

```bash
# Capture all TCP RST packets (Flags [R])
sudo tcpdump -i any -nn "tcp[tcpflags] & tcp-rst != 0"

# Capture TCP SYN retries (retransmissions indicating firewall drops or dead ports)
sudo tcpdump -i eth0 -nn "tcp[tcpflags] & (tcp-syn) != 0 and not tcp[tcpflags] & (tcp-ack) != 0"
```

### Example 4: Capturing and Inspecting Unencrypted HTTP Payloads
Print payload contents in Hex and ASCII to inspect REST API query strings or headers.

```bash
# Capture HTTP GET and POST requests on port 8080
sudo tcpdump -i eth0 -nn -A -s 0 "tcp port 8080 and (((ip[2:2] - ((ip[0]&0xf)<<2)) - ((tcp[12:2]&0xf0)>>2)) != 0)"
```
*(The BPF formula checks that TCP payload length > 0).*

### Example 5: Production Incident Triage Script: Automated PCAP Capture
Safely capture diagnostic dumps during production latency spikes without risking disk exhaustion.

```bash
#!/usr/bin/env bash
set -euo pipefail

CAPTURE_DIR="/var/log/pcap_incident"
INTERFACE="eth0"
DURATION_SEC=30
MAX_MB=200
OUTPUT_FILE="${CAPTURE_DIR}/incident_$(date +%Y%m%d_%H%M%S).pcap"

mkdir -p "${CAPTURE_DIR}"

echo "==> Starting bounded network capture on ${INTERFACE} for ${DURATION_SEC} seconds..."
# Run tcpdump in background with byte limit and timeout
timeout "${DURATION_SEC}" tcpdump -i "${INTERFACE}" \
  -nn -s 0 \
  -w "${OUTPUT_FILE}" \
  -W 1 -C "${MAX_MB}" \
  "not port 22" || true  # Exclude SSH to avoid capturing our own session!

echo "==> Capture complete. Analyzing capture metrics..."
if [ -f "${OUTPUT_FILE}" ]; then
    capinfos -c -d -u "${OUTPUT_FILE}" 2>/dev/null || ls -lh "${OUTPUT_FILE}"
    echo "Capture ready for Wireshark analysis: ${OUTPUT_FILE}"
else
    echo "No packets captured."
fi
```

---

## Best Practices

1. **Always Exclude Your Management SSH Session (`not port 22`)**: When capturing over an SSH connection, printing packet output or capturing port 22 creates a feedback loop: each packet displayed generates new SSH packets, flooding the terminal and freezing the connection.
2. **Always Use `-nn` to Disable Reverse Name Resolution**: If `tcpdump` attempts to resolve IPs to hostnames via DNS for every captured packet, it generates thousands of secondary DNS queries, introducing severe latency and distorting the capture.
3. **Capture with Ring Buffers (`-C` and `-W`) for Long-Term Audits**: Never leave an unbounded `tcpdump -w trace.pcap` running in production. A busy 10Gbps link can fill the root disk in minutes. Always enforce file size limits (`-C 100`) and file rotation counts (`-W 5`).
4. **Use `-s` (Snaplen) to Control Capture Overhead**: If you only need Layer 3/4 headers for flow analysis or latency timing, limit snaplen to 96 bytes (`-s 96`). This avoids capturing sensitive payload data (PII, credentials) and slashes memory/disk requirements.
5. **Analyze Offline with Wireshark / Tshark**: Capture raw packets on production nodes using lightweight `tcpdump -w trace.pcap`, then transfer the file to an engineer's workstation for deep analysis in Wireshark. Avoid running heavy analysis tools directly on loaded production servers.
6. **Watch Kernel Packet Drop Counters**: When tcpdump exits, it prints: `packets captured, packets received by filter, packets dropped by kernel`. If dropped packets > 0, the buffer was overrun; increase socket buffer size using `-B <kilobytes>` (e.g., `-B 4096`).

---

## Common Mistakes & Gotchas

| Mistake | Why It Fails | Correct Pattern |
| :--- | :--- | :--- |
| Capturing without `not port 22` over SSH | Triggers a cascading storm of self-generated SSH packets, hanging the terminal. | Always append `and not port 22` to your BPF filter. |
| Forgetting `-nn` | `tcpdump` hangs attempting reverse DNS lookups on every packet, dropping packets. | Always use `-nn` for high-speed captures. |
| Writing PCAPs to the root partition (`/`) without size bounds | Fills the system disk to 100%, causing application crashes and server lockup. | Use `-C <MB> -W <count>` and write to dedicated scratch storage (`/var/tmp/`). |
| Capturing on `eth0` when services communicate over `localhost` (`127.0.0.1`) | Local IPC traffic never touches physical interfaces; capture shows 0 packets. | Capture on `lo` or use `-i any`. |
| Believing `tcpdump` captures post-NAT firewall state only | `tcpdump` captures at the packet tap before inbound iptables/nftables filters and after outbound filters. | Correlate with `nft monitor` or connection tracking tables. |
| Reading large 10GB PCAPs directly into GUI Wireshark | Saturates desktop RAM and crashes the GUI. | Slice into smaller chunks first using `editcap` or filter with `tshark`. |

---

## Performance, Security & Cost Implications

### Performance
- **CPU & Memory Pressure**: While in-kernel BPF filtering is fast, copying full 1500-byte packets to userspace under 100,000 packets per second can consume multiple CPU cores. Use `-B 8192` to allocate an 8MB kernel buffer and `-s 96` to capture headers only.
- **`AF_PACKET` vs eBPF / XDP**: For wire-rate multi-gigabit packet inspection without packet loss, modern tools (like Cilium and BCC tools) utilize eBPF and XDP (eXpress Data Path) directly inside the NIC driver, bypassing `sk_buff` allocation entirely.

### Security
- **Sensitive Data Exposure in PCAP Files**: Unencrypted HTTP, database queries, and authorization headers are stored in plaintext inside PCAP files. Ensure captured `.pcap` files have strict permissions (`chmod 600`), and scrub credentials before sharing traces across teams.
- **Promiscuous Mode Detection**: By default, `tcpdump` puts the NIC into **promiscuous mode** to capture all packets on the broadcast domain. On modern switched networks, this only captures unicast traffic destined for the host plus broadcast/multicast.

### Cost
- Storing uncompressed long-term PCAP traces in cloud storage generates significant storage expenses. Compress PCAP files with `zstd` (reducing size by 60%–80%) before archiving.

---

## Comparison Table / Alternatives

| Feature | `tcpdump` | `tshark` | Wireshark GUI | `ngrep` |
| :--- | :--- | :--- | :--- | :--- |
| **Interface** | Command Line | Command Line | Graphical Desktop | Command Line |
| **Resource Footprint** | Extremely Low | Moderate | High (RAM intensive) | Very Low |
| **Protocol Decoders** | Basic (Layers 2–4, basic L7) | Thousands of protocols | Thousands of protocols | Regex on raw packet payload |
| **Live Stream Reconstruction**| No | Yes | Yes (Right-click -> Follow TCP) | No |
| **Best Used For** | Fast capture on production servers | CLI automation & field extraction | In-depth protocol & latency analysis | Quick grep for strings on the wire |

---

## Real-World Troubleshooting & Interview Q&A

### Q1: How do you identify TCP retransmissions using `tcpdump`?
<details>
<summary>View Solution</summary>

A TCP retransmission occurs when a sender re-sends a segment because no `ACK` was received before the Retransmission Timeout (RTO) expired.

**In `tcpdump`**:
Look for consecutive packets from the same source IP and port that have the **identical sequence number** (`seq`) and payload length, separated by a time delta (e.g., 200ms, 400ms, 800ms).
```bash
# Filter for packets with matching sequences using tshark:
tshark -r trace.pcap -Y "tcp.analysis.retransmission"
```
Frequent retransmissions indicate packet loss along the network path or severe intermediate bufferbloat.
</details>

### Q2: What does a TCP `RST` (Reset) packet signify in a capture?
<details>
<summary>View Solution</summary>

A `RST` (`Flags [R]`) abruptly aborts a connection.
Common causes:
1. **Port Closed**: Client sent `SYN` to a port where no application is listening; server kernel immediately replies with `RST-ACK`.
2. **Abrupt Application Termination**: Process crashed or closed socket with unread data in receive buffer (`SO_LINGER` set to 0).
3. **Firewall / Middlebox Intervention**: An intermediate stateful firewall (or Cloud NAT) timed out the connection tracking entry and rejected subsequent packets with a spoofed `RST`.
</details>

### Q3: Why does `tcpdump` report `packets dropped by kernel` upon exit?
<details>
<summary>View Solution</summary>

**Cause**: The rate of incoming packets matching the BPF filter exceeded the speed at which `tcpdump` could read them from the kernel's `AF_PACKET` socket buffer. When the buffer becomes full, the kernel drops newly arriving packets.

**Solutions**:
1. Tighten the BPF filter so only strictly necessary packets are captured.
2. Truncate payload size with `-s 96` (headers only).
3. Increase the kernel capture buffer size using `-B`:
   ```bash
   sudo tcpdump -i eth0 -B 16384 -w capture.pcap
   ```
</details>

### Q4: How do you capture only the initial connection setup (`SYN` packets) without data payloads?
<details>
<summary>View Solution</summary>

Use the BPF bitwise expression checking the TCP flags byte:
```bash
sudo tcpdump -i eth0 -nn "tcp[tcpflags] & tcp-syn != 0 and tcp[tcpflags] & tcp-ack == 0"
```
This isolates raw inbound/outbound connection requests, revealing which external clients are attempting connections without capturing ongoing data transfers.
</details>

### Q5: How do you follow and extract a full HTTP or database conversation in `tshark`?
<details>
<summary>View Solution</summary>

In Wireshark, you right-click a packet and choose "Follow TCP Stream".
In `tshark`, use the `-z follow,tcp,ascii,<stream_id>` option:
```bash
# 1. Identify stream index
tshark -r trace.pcap -Y "http" -T fields -e tcp.stream | head -n 1

# 2. Reconstruct entire stream conversation:
tshark -r trace.pcap -q -z follow,tcp,ascii,0
```
This outputs the exact bidirectional ASCII dialogue (client HTTP request + server HTTP response).
</details>

### Q6: What is a TCP Zero-Window notification?
<details>
<summary>View Solution</summary>

In TCP flow control, the receiver advertises how many bytes of buffer space it has available (`win <size>`).
If an application stops reading from its socket buffer (e.g., thread blocked on disk I/O or stuck in a long GC pause), the buffer fills up. The receiving OS sends a packet with `win 0` (**TCP Zero Window**), commanding the sender to halt all further transmissions until a **Window Update** packet is emitted.
</details>

### Q7: Why do checksums in `tcpdump` output often show `[bad udp cksum]` or `[bad tcp cksum]` on the sender host?
<details>
<summary>View Solution</summary>

**Cause**: **Hardware Checksum Offloading (TX Checksum Offload)**.
When a packet is generated locally, the Linux kernel leaves the TCP/UDP checksum field empty (or filled with garbage zeros) and delegates checksum calculation to the physical NIC hardware. Because `tcpdump` captures the packet from the kernel *before* it reaches the physical NIC, it sees the uncalculated checksum and flags it as bad.
The packet actually leaves the wire with a perfectly valid checksum. You can disable this warning with `-K` (`--dont-verify-checksums`).
</details>

### Q8: What does the BPF filter `ip[2:2] > 1000` mean?
<details>
<summary>View Solution</summary>

`ip[offset:length]` accesses raw bytes in the IP header:
- `ip[2:2]` refers to the **Total Length** field in the IPv4 header (starting at byte offset 2, reading 2 bytes).
- The expression filters for IP packets whose total size exceeds 1,000 bytes, ignoring small `ACK`s or heartbeat probes.
</details>

---

## Hands-On Practice Exercises

### Exercise 1: Capturing ICMP Echo Requests and Responses
**Task**: Use `tcpdump` to capture exactly 4 ICMP packets. In a separate shell, ping `127.0.0.1`. Observe sequence numbers and TTLs.
<details>
<summary>View Solution</summary>

```bash
# 1. In terminal 1: capture 4 ICMP packets
sudo tcpdump -i lo -nn -c 4 icmp &

# 2. Send 2 pings (generates 2 requests + 2 replies = 4 packets)
ping -c 2 127.0.0.1
```
</details>

### Exercise 2: Exporting and Reading a PCAP File
**Task**: Capture 10 seconds of DNS traffic to a file named `/tmp/dns_test.pcap`. Read and parse the file using `tcpdump -r`.
<details>
<summary>View Solution</summary>

```bash
# 1. Start capture
sudo tcpdump -i any -nn -w /tmp/dns_test.pcap "udp port 53" -c 5 &
TCPDUMP_PID=$!

# 2. Trigger DNS lookups
dig google.com +short > /dev/null
dig cloudflare.com +short > /dev/null

# 3. Read back from PCAP
tcpdump -nn -r /tmp/dns_test.pcap
rm -f /tmp/dns_test.pcap
```
</details>

### Exercise 3: Inspecting Packet Hex and ASCII Dumps
**Task**: Use `tcpdump -X` to inspect the raw payload of an HTTP request sent to a local test listener.
<details>
<summary>View Solution</summary>

```bash
# 1. Start netcat listener
nc -l -p 8999 > /dev/null &

# 2. Start sniffer in background
sudo tcpdump -i lo -nn -X "tcp port 8999" -c 3 &

# 3. Send payload
echo "DevOps-Packet-Test" | nc 127.0.0.1 8999

# Notice ASCII dump displays "DevOps-Packet-Test" in right-hand column!
```
</details>

### Exercise 4: Filtering for Specific TCP Flags via BPF
**Task**: Craft a BPF filter to capture packets that have both the `FIN` and `ACK` flags set (connection termination).
<details>
<summary>View Solution</summary>

```bash
# tcp[tcpflags] & (tcp-fin|tcp-ack) == (tcp-fin|tcp-ack)
sudo tcpdump -i any -nn "tcp[tcpflags] & 17 == 17" -c 2
```
*(Flag values: FIN=1, SYN=2, RST=4, PSH=8, ACK=16. $1 + 16 = 17$).*
</details>

### Exercise 5: Analyzing Top Talkers with `tshark`
**Task**: Given a PCAP file, use `tshark` to aggregate and count packet volumes grouped by source IP address.
<details>
<summary>View Solution</summary>

```bash
# If tshark is installed (sudo apt install -y tshark):
tshark -r /tmp/sample.pcap -T fields -e ip.src | sort | uniq -c | sort -rn | head -n 10
```
</details>

### Exercise 6: Rotating Capture with Size Limits
**Task**: Configure `tcpdump` to capture on interface `lo`, rotating files every 10MB, retaining a maximum of 3 files.
<details>
<summary>View Solution</summary>

```bash
sudo tcpdump -i lo -nn -w /tmp/rot_%Y%m%d_%H%M%S.pcap -C 10 -W 3 &
PID=$!

# Generate some traffic
for i in {1..5}; do curl -s http://127.0.0.1:80 2>/dev/null || true; done

# Clean up
sudo kill "${PID}"
rm -f /tmp/rot_*.pcap
```
</details>

---

## Summary Cheat Sheet & Further Reading

### Command Reference
| Goal | Command |
| :--- | :--- |
| **Capture All on Interface** | `sudo tcpdump -i eth0 -nn` |
| **Filter by Host and Port** | `sudo tcpdump -i eth0 -nn "host 10.0.0.5 and port 443"` |
| **Exclude SSH Traffic** | `sudo tcpdump -i eth0 -nn "not port 22"` |
| **Write Full Packets to PCAP**| `sudo tcpdump -i eth0 -s 0 -w out.pcap` |
| **Read PCAP File** | `tcpdump -nn -r in.pcap` |
| **Inspect Hex + ASCII** | `sudo tcpdump -i eth0 -nn -X "tcp port 80"` |
| **Capture SYN Packets Only**| `sudo tcpdump -i eth0 -nn "tcp[tcpflags] & tcp-syn != 0"` |
| **Rotating Buffer (100MBx5)**| `sudo tcpdump -i eth0 -w /var/log/cap.pcap -C 100 -W 5` |
| **Extract Fields with Tshark**| `tshark -r trace.pcap -T fields -e ip.src -e ip.dst` |

### Further Reading
- `man 1 tcpdump` and `man 7 pcap-filter`
- `man 1 tshark`
- [Wireshark Official User's Guide](https://www.wireshark.org/docs/wsug_html_chunked/)
- [Tcpdump & Libpcap Official Repository](https://www.tcpdump.org/)
- [Brendan Gregg: BPF and Tracing Documentation](https://www.brendangregg.com/bpf-performance-tools-book.html)
