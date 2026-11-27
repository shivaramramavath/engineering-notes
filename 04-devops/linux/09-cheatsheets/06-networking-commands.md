# Linux Networking & Port Troubleshooting: High-Density Cheatsheet

| Metadata | Details |
| :--- | :--- |
| **Topic** | Linux Network Stack, iproute2, ss, tcpdump, DNS & Firewall Operations |
| **Module** | `09-cheatsheets` — High-Density Linux Production Cheatsheets |
| **Track** | `04-devops/linux` |
| **Target OS** | Ubuntu 24.04 LTS, RHEL 9.4 / Rocky Linux 9.4 |
| **Last Updated**| 2026 |

---

## 1. iproute2: Modern Interface, IP & Routing Commands

```bash
# Deprecated Commands vs Modern iproute2 Replacement:
#   ifconfig       ->  ip address (ip a)
#   route -n       ->  ip route (ip r)
#   arp -n         ->  ip neighbor (ip neigh)
#   netstat -i     ->  ip -s link

# === IP Addresses (ip a) ===
ip a                                        # Show all interfaces and IP addresses
ip -br a                                    # Brief, tabular output (Interface, State, IP)
ip -4 a show eth0                           # Show only IPv4 addresses on eth0
sudo ip addr add 192.168.1.50/24 dev eth0   # Temporarily assign secondary IP
sudo ip addr del 192.168.1.50/24 dev eth0   # Remove IP address

# === Link & Interface Control (ip link) ===
ip link show                                # List all physical and virtual interfaces
ip -s link show eth0                        # Show packet drop, error, and collision stats
sudo ip link set eth0 up                    # Bring interface UP
sudo ip link set eth0 down                  # Bring interface DOWN
sudo ip link set eth0 mtu 9000              # Set Jumbo Frames (MTU 9000)
sudo ip link set eth0 promisc on            # Enable promiscuous mode for packet sniffing

# === Routing Table (ip r) ===
ip r                                        # Display active routing table
ip r show default                           # Display default gateway route
ip route get 8.8.8.8                        # Test which interface & gateway will route to 8.8.8.8
sudo ip route add 10.0.0.0/8 via 192.168.1.1 dev eth0 # Add static route
sudo ip route del 10.0.0.0/8                # Delete static route
sudo ip route replace default via 192.168.1.254 dev eth0 # Change default gateway

# === ARP / Neighbor Table (ip neigh) ===
ip neigh show                               # Display ARP table (IP to MAC mappings)
sudo ip neigh flush dev eth0                # Flush ARP cache
```

---

## 2. ss (Socket Statistics): Port & Connection Inspection

```bash
# === Essential Inspection Flags ===
# -t (TCP), -u (UDP), -l (Listening), -p (Process/PID), -n (Numeric - NO DNS delays)
sudo ss -tulpn                              # List ALL listening TCP & UDP ports with PIDs
ss -tuna                                    # List all TCP & UDP sockets (listening + established)
ss -s                                       # Summary table of socket allocations (TCP, UDP, RAW)

# === Filtering by Port & Service ===
sudo ss -tulpn '( sport = :80 or sport = :443 )'     # Sockets listening on 80 or 443
ss -tn 'sport = :22'                        # Outbound or inbound SSH connections
ss -tn 'dport = :5432'                      # Outbound connections to PostgreSQL

# === Filtering by TCP State ===
ss -t state established                     # Active established connections
ss -t state time-wait                       # Sockets in TIME_WAIT
ss -t state close-wait                      # Sockets in CLOSE_WAIT (Leaked by application!)
ss -t state syn-recv                        # Half-open connections (Possible SYN flood)

# === Detailed Socket Internals (-i) ===
ss -ti 'dport = :443'                       # Displays RTT, cwnd, MSS, and retransmission stats
```

---

## 3. Tcpdump: Wire-Level Packet Sniffing

```bash
# Basic Syntax: tcpdump -nn -i <interface> [options] [filter_expression]

# Recommended Default Flags:
#   -nn: Do NOT resolve hostnames or port names (fast, no DNS lag)
#   -v / -vv: Verbose packet decode (TTL, ID, TCP options)
#   -s0: Capture FULL packet payload (snaplen 0 = 262144 bytes)
#   -w file.pcap: Write raw packets to file for Wireshark analysis
#   -r file.pcap: Read and decode saved PCAP file

# === Common Target Captures ===
sudo tcpdump -nn -i eth0 port 80            # All traffic on port 80
sudo tcpdump -nn -i any host 10.0.1.25      # Traffic to/from IP across ALL interfaces
sudo tcpdump -nn -i eth0 icmp               # ICMP (Ping / Unreachable) packets only
sudo tcpdump -nn -i eth0 port 53            # DNS queries and responses

# === Advanced Logical Expressions ===
sudo tcpdump -nn -i eth0 'src 192.168.1.10 and (dst port 443 or dst port 80)'
sudo tcpdump -nn -i eth0 'not port 22'      # Exclude SSH traffic from capture!

# === Capture Specific TCP Flags ===
# Capture only SYN packets (new connection attempts):
sudo tcpdump -nn -i eth0 'tcp[tcpflags] & tcp-syn != 0 and tcp[tcpflags] & tcp-ack == 0'

# Capture RST (Reset) packets (Connection drops):
sudo tcpdump -nn -i eth0 'tcp[tcpflags] & tcp-rst != 0'
```

---

## 4. Curl & Network Endpoint Diagnostics

```bash
# === Probing HTTP Endpoints ===
curl -I https://api.example.com             # HEAD request only (Inspect headers)
curl -v https://api.example.com             # Verbose TLS handshake & headers
curl -k -I https://localhost:8443           # Insecure (-k ignores self-signed TLS certs)
curl -s -o /dev/null -w "%{http_code}\n" http://localhost # Print ONLY HTTP status code

# === Precise Millisecond Latency Breakdown ===
curl -w "\
  DNS Lookup:        %{time_namelookup}s\n\
  TCP Connect:       %{time_connect}s\n\
  TLS Handshake:     %{time_appconnect}s\n\
  Pre-Transfer:      %{time_pretransfer}s\n\
  Time to First Byte: %{time_starttransfer}s\n\
  Total Duration:    %{time_total}s\n" \
  -o /dev/null -s https://api.example.com

# === Netcat (nc) Port Probing ===
nc -zv 10.0.1.50 22                         # Test TCP port reachability (-z zero-I/O, -v verbose)
nc -zvu 10.0.1.50 53                        # Test UDP port
nc -l -p 9999                               # Listen on port 9999 (echo server)
```

---

## 5. DNS Resolution & Diagnostic Queries

```bash
# === dig (Domain Information Groper) ===
dig api.example.com                         # Full DNS query with authority & timings
dig api.example.com +short                  # Return ONLY the resolved IP address
dig @1.1.1.1 api.example.com                # Query specific nameserver directly (Cloudflare)
dig -x 8.8.8.8 +short                       # Reverse DNS lookup (PTR record)
dig api.example.com MX +noall +answer       # Query MX (Mail) records only
dig +trace api.example.com                  # Trace resolution path from Root (.) servers down

# === systemd-resolved (Ubuntu 24.04) ===
resolvectl status                           # Show active DNS servers per interface
resolvectl query api.example.com            # Query through systemd-resolved
sudo resolvectl flush-caches                # Flush local system DNS cache
```

---

## 6. Host Firewalls: UFW & Nftables

```bash
# === UFW (Uncomplicated Firewall - Ubuntu/Debian) ===
sudo ufw status verbose                     # View active rules and status
sudo ufw default deny incoming              # Default DROP incoming
sudo ufw default allow outgoing             # Default ALLOW outgoing
sudo ufw allow 22/tcp                       # Allow SSH port
sudo ufw allow 443/tcp comment "HTTPS"      # Allow HTTPS with description
sudo ufw allow from 10.0.0.0/8 to any port 5432 proto tcp # Allow subnet to PostgreSQL
sudo ufw delete allow 22/tcp                # Delete specific rule
sudo ufw --force enable                     # Activate firewall
sudo ufw disable                            # Deactivate firewall

# === Nftables (Modern Kernel Standard) ===
sudo nft list ruleset                       # Dump complete active firewall ruleset
sudo nft list table inet filter             # List specific table
sudo nft flush ruleset                      # EMERGENCY: Flush ALL firewall rules
```

---

## 7. NIC Hardware & Bandwidth Tuning

```bash
# === ethtool (NIC Hardware Tuning) ===
ethtool eth0                                # Show speed (1000/10000Mb/s), duplex, link state
ethtool -g eth0                             # Show current & max ring buffer sizes
sudo ethtool -G eth0 rx 4096 tx 4096        # Enlarge ring buffers to maximum
ethtool -k eth0                             # Show hardware offloads (TSO, GRO, GSO)
sudo ethtool -K eth0 tso on gro on          # Enable hardware offload acceleration
ethtool -S eth0 | grep -E "drop|miss"       # Inspect hardware silicon packet drop counters

# === iperf3 (Network Throughput Benchmarking) ===
# On Server:
iperf3 -s                                   # Start server daemon on port 5201
# On Client:
iperf3 -c 10.0.1.50 -t 10 -P 4              # Test bandwidth over 10s with 4 parallel streams
```

---
*Next Cheatsheet: [07-troubleshooting-flowcharts.md](file:///04-devops/linux/09-cheatsheets/07-troubleshooting-flowcharts.md)*
