# WireGuard VPN: Modern Linux Virtual Private Networks

| Metadata | Details |
| :--- | :--- |
| **Topic** | WireGuard Kernel VPN, Cryptokey Routing, Configuration, and Production Tunnels |
| **Module** | `04-networking` |
| **Track** | `04-devops/linux` |
| **Prerequisites** | `04-networking/01-networking-basics.md`, `04-networking/02-ip-routing-and-interfaces.md`, `04-networking/05-firewall.md` |
| **Target Audience** | Network Engineers, SREs, Systems Architects, Security Engineers |
| **Target Distribution** | Ubuntu 24.04 LTS (Kernel 6.8+), Rocky Linux / RHEL 9.4 (Kernel 5.14+) |
| **Key Tools / Files** | `wg`, `wg-quick`, `wireguard-tools`, `/etc/wireguard/wg0.conf`, `systemctl` |

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

For decades, establishing secure encrypted site-to-site and remote-access tunnels across Linux servers meant wrestling with legacy VPN protocols:
- **IPsec (StrongSwan / Libreswan)**: Notoriously complex configuration (IKEv1/IKEv2, phase 1/2 proposals, thousands of cryptographic combinations) and fragile interoperability.
- **OpenVPN**: Operates in userspace using TUN/TAP virtual network drivers, incurring heavy context-switching overhead, slow throughput, and large codebases (~100,000+ lines of C).

**WireGuard** revolutionized Linux VPN architecture. Merged directly into the mainline Linux kernel (Linux 5.6+), WireGuard features:
- **Minimal Codebase**: Fewer than 4,000 lines of code (auditable in a single sitting).
- **Modern Fixed Cryptography**: Eliminates cipher negotiation flaws by standardizing strictly on state-of-the-art primitives: Curve25519 (ECDH), ChaCha20-Poly1305 (AEAD encryption), BLAKE2s (hashing), and SipHash.
- **Cryptokey Routing**: Binds public encryption keys directly to allowed IP address subnets, eliminating complex routing table scripts.
- **Wire-Speed Performance**: Operates entirely in kernel space, delivering line-rate multi-gigabit throughput with sub-millisecond connection handshake latency.
- **Stealth Silence**: Responds only to authenticated packets; port scanners report the port as completely closed or silent (UDP drop).

---

## Core Concepts

### Cryptokey Routing Semantics
The central design breakthrough of WireGuard is **Cryptokey Routing**:
- Every peer is associated with its **Public Key** and a list of **`AllowedIPs`**.
- **When sending an outbound packet**: The kernel inspects the destination IP, matches it against the peer whose `AllowedIPs` encompasses that address, encrypts the payload using that peer's public key, and transmits it via UDP.
- **When receiving an inbound packet**: The kernel decrypts the payload with the private key, identifies the sending peer by its public key, and verifies that the packet's source IP matches that peer's configured `AllowedIPs`. If the source IP does not match, **the kernel drops the packet immediately**.

### Network Interface Model (`wg0`)
WireGuard treats tunnels as standard Linux network interfaces (`wg0`, `wg1`):
- Created via `ip link add dev wg0 type wireguard`.
- Can be assigned IP addresses (`ip addr add 10.0.0.1/24 dev wg0`).
- Can have custom MTUs, routing policies, and firewall rules applied just like physical Ethernet devices (`eth0`).

### Full-Tunnel vs Split-Tunnel Routing
- **Split-Tunneling (`AllowedIPs = 10.0.0.0/16`)**: Only traffic destined for the specified internal corporate subnet travels through the encrypted WireGuard tunnel. All general public internet traffic (YouTube, GitHub, cloud APIs) exits directly via the user's local ISP gateway.
- **Full-Tunneling (`AllowedIPs = 0.0.0.0/0, ::/0`)**: 100% of all outbound network traffic from the client is forced through the WireGuard tunnel and exits through the VPN gateway node.

### MTU Constraints (1420 Bytes)
WireGuard encapsulates IP packets inside UDP datagrams. 
- Standard Ethernet MTU is **1500 bytes**.
- Overhead: IPv4 Header (20B) + UDP Header (8B) + WireGuard Header/Tag (32B) = **60 bytes** overhead (80 bytes for IPv6).
- Therefore, the maximum safe MTU for a WireGuard interface is typically **1420 bytes** ($1500 - 80$). Setting MTU too high triggers packet drops on links with "Don't Fragment" (DF) enabled.

---

## Internal Working

```
+-----------------------------------------------------------------------------------------+
|                                    Cryptokey Routing Engine                             |
+-----------------------------------------------------------------------------------------+
                             Outbound Packet (Dest: 10.50.0.5)
                                            |
                                            v
+-----------------------------------------------------------------------------------------+
|                          WireGuard Routing Lookup Table (FIB)                           |
|   Peer A (Pubkey A): AllowedIPs = 10.50.0.1/32, Endpoint = 203.0.113.10:51820          |
|   Peer B (Pubkey B): AllowedIPs = 10.50.0.0/24, Endpoint = 198.51.100.25:51820  <-- HIT|
+-------------------------------------------+---------------------------------------------+
                                            |
                                            v
+-----------------------------------------------------------------------------------------+
|                                Kernel Cryptographic Engine                              |
|   1. Ephemeral Noise Protocol Handshake (1-RTT session key exchange)                    |
|   2. Encrypts payload with ChaCha20-Poly1305 using derived session key                  |
|   3. Encapsulates encrypted payload into Outer UDP Packet (Port 51820)                  |
+-------------------------------------------+---------------------------------------------+
                                            |
                                            v
+-----------------------------------------------------------------------------------------+
|                                Outer Kernel Routing & Wire                              |
|            Sends UDP datagram to Endpoint: 198.51.100.25:51820 via eth0                 |
+-----------------------------------------------------------------------------------------+
```

### The 1-RTT Noise Protocol Handshake
Unlike OpenVPN (which negotiates ciphers over multi-step TLS handshakes lasting seconds), WireGuard uses the **Noise Protocol Framework**:
1. Handshake requires exactly **one round trip (1-RTT)**.
2. If no data has been transmitted for 10 seconds, sessions automatically tear down.
3. When new packets arrive, the handshake completes in milliseconds without user intervention.
4. If a client changes IP addresses (e.g., roaming from corporate Wi-Fi to mobile 5G), the server detects the updated source endpoint in the authenticated packet and transparently updates its endpoint table without dropping the tunnel (**seamless mobility**).

---

## Syntax & CLI Tools

### Generating Cryptographic Keys
```bash
# Generate private key with restricted permissions
wg genkey > privatekey
chmod 600 privatekey

# Derive corresponding public key from the private key
wg pubkey < privatekey > publickey

# Generate an optional pre-shared symmetric key (for post-quantum resistance)
wg genpsk > preshared.key
```

### Low-Level `wg` CLI Inspection
```bash
# Display status of all active WireGuard interfaces and connected peers
sudo wg show

# Display specific interface transfer stats
sudo wg show wg0 transfer

# Display public key and active listening port
sudo wg show wg0 endpoints

# Dynamically add a peer without restarting the interface
sudo wg set wg0 peer <PEER_PUBLIC_KEY> allowed-ips 10.0.0.2/32 endpoint 203.0.113.40:51820
```

### High-Level Management with `wg-quick`
`wg-quick` is a bash wrapper script that automates interface creation, IP address assignment, routing table rules, and firewall hooks defined in `/etc/wireguard/<interface>.conf`.

```bash
# Bring up WireGuard interface
sudo wg-quick up wg0

# Bring down WireGuard interface
sudo wg-quick down wg0

# Enable WireGuard interface at system boot via systemd
sudo systemctl enable --now wg-quick@wg0
```

---

## Progressive Practical Examples

### Example 1: Point-to-Point Site-to-Site Tunnel Between Two Cloud Servers
Establish an encrypted tunnel between Server A (`10.100.0.1`) and Server B (`10.100.0.2`).

**Server A Configuration (`/etc/wireguard/wg0.conf`)**:
```ini
[Interface]
# Server A private IP inside VPN
Address = 10.100.0.1/24
ListenPort = 51820
PrivateKey = aAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=
MTU = 1420

[Peer]
# Server B
PublicKey = bBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB=
Endpoint = 198.51.100.20:51820
AllowedIPs = 10.100.0.2/32
PersistentKeepalive = 25
```

**Server B Configuration (`/etc/wireguard/wg0.conf`)**:
```ini
[Interface]
# Server B private IP inside VPN
Address = 10.100.0.2/24
ListenPort = 51820
PrivateKey = cCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC=
MTU = 1420

[Peer]
# Server A
PublicKey = dDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD=
Endpoint = 203.0.113.10:51820
AllowedIPs = 10.100.0.1/32
PersistentKeepalive = 25
```

```bash
# On both servers:
sudo chmod 600 /etc/wireguard/wg0.conf
sudo systemctl enable --now wg-quick@wg0

# Test connectivity:
ping 10.100.0.2
```

### Example 2: Road-Warrior VPN Gateway (Server Hub Routing Client Internet)
A central cloud VPS acts as a secure VPN gateway for remote laptops.

**Server Hub Configuration (`/etc/wireguard/wg0.conf`)**:
```ini
[Interface]
Address = 10.8.0.1/24
ListenPort = 51820
PrivateKey = <SERVER_PRIVATE_KEY>

# Automatically enable IP forwarding and NAT masquerade upon interface start
PostUp = sysctl -w net.ipv4.ip_forward=1
PostUp = iptables -A FORWARD -i wg0 -j ACCEPT; iptables -t nat -A POSTROUTING -o eth0 -j MASQUERADE
PostDown = iptables -D FORWARD -i wg0 -j ACCEPT; iptables -t nat -D POSTROUTING -o eth0 -j MASQUERADE

# Laptop Client 1
[Peer]
PublicKey = <CLIENT1_PUBLIC_KEY>
AllowedIPs = 10.8.0.2/32

# Phone Client 2
[Peer]
PublicKey = <CLIENT2_PUBLIC_KEY>
AllowedIPs = 10.8.0.3/32
```

**Client Configuration (`wg-client.conf`)**:
```ini
[Interface]
Address = 10.8.0.2/24
PrivateKey = <CLIENT1_PRIVATE_KEY>
DNS = 1.1.1.1

[Peer]
PublicKey = <SERVER_PUBLIC_KEY>
Endpoint = vpn.company.com:51820
# Route 100% of all internet traffic through the VPN
AllowedIPs = 0.0.0.0/0, ::/0
PersistentKeepalive = 25
```

### Example 3: Generating WireGuard QR Codes for Mobile Devices
Provision mobile clients effortlessly by converting configuration files into terminal QR codes.

```bash
# Install qrencode utility
sudo apt-get install -y qrencode

# Display WireGuard client config as an ASCII QR code in the terminal
qrencode -t ansiutf8 < /etc/wireguard/mobile-client.conf

# Scan with the official WireGuard iOS/Android mobile app!
```

### Example 4: Maintaining NAT Traversal with `PersistentKeepalive`
When a client sits behind a restrictive home router or stateful cloud NAT, the NAT gateway drops idle UDP state tracking after 30–60 seconds, preventing the server from initiating communication to the client.

```ini
[Peer]
PublicKey = <SERVER_PUBLIC_KEY>
Endpoint = vpn.corp.com:51820
AllowedIPs = 10.0.0.0/16
# Send an authenticated empty keepalive packet every 25 seconds
PersistentKeepalive = 25
```
This forces the NAT gateway to keep its mapping table entry open permanently.

### Example 5: Production Peer Health Check and Watchdog Script
Automated watchdog script checking handshake freshness and restarting dead peers.

```bash
#!/usr/bin/env bash
set -euo pipefail

INTERFACE="wg0"
MAX_HANDSHAKE_AGE_SEC=180
NOW=$(date +%s)

echo "=== WireGuard Interface Health Audit (${INTERFACE}) ==="

# Extract peer pubkey and latest handshake epoch
sudo wg show "${INTERFACE}" latest-handshakes | while read -r PEER HANDSHAKE; do
    if [ "${HANDSHAKE}" -eq 0 ]; then
        echo "WARNING: Peer ${PEER} has NEVER completed a handshake!"
        continue
    fi

    AGE=$(( NOW - HANDSHAKE ))
    if [ "${AGE}" -gt "${MAX_HANDSHAKE_AGE_SEC}" ]; then
        echo "ALERT: Peer ${PEER} last handshake was ${AGE}s ago (stale > ${MAX_HANDSHAKE_AGE_SEC}s)!"
    else
        echo "OK:    Peer ${PEER} handshake fresh (${AGE}s ago)."
    fi
done
```

---

## Best Practices

1. **Always Set `PersistentKeepalive = 25` on Clients Behind NAT**: Servers with static public IPs do not need keepalives, but mobile clients, laptops, or cloud nodes behind NAT gateways **must** configure `PersistentKeepalive = 25` to prevent NAT timeout drops.
2. **Standardize on `MTU = 1420` in Configurations**: Avoid leaving MTU unconfigured on links that traverse cloud VPCs, PPPoE, or nested tunnels. Setting `MTU = 1420` explicitly in the `[Interface]` section avoids MTU fragmentation and packet blackholes.
3. **Restrict File Permissions on `/etc/wireguard` to `700`**: Configuration files contain raw, unencrypted private keys. Ensure directory permissions are strictly root-only:
   ```bash
   sudo chmod 700 /etc/wireguard
   sudo chmod 600 /etc/wireguard/*.conf
   ```
4. **Use Pre-Shared Keys (PSK) for Defense-in-Depth**: Add `PresharedKey = <psk>` to the `[Peer]` section. This introduces post-quantum symmetric encryption (256-bit symmetric key) alongside Curve25519, future-proofing against quantum computing breakthroughs.
5. **Enforce Systemd Service Integration**: Always manage tunnels via `systemctl enable --now wg-quick@wg0`. This guarantees automatic recovery on system reboot and graceful interface teardown on shutdown.
6. **Assign Unique Non-Overlapping `/32` Subnets per Peer**: In star-topology VPNs, never assign overlapping subnets to different peers in `AllowedIPs`. WireGuard routes based on longest prefix match; overlapping `/32` entries cause the latter peer to hijack the former peer's traffic.

---

## Common Mistakes & Gotchas

| Mistake | Why It Fails | Correct Pattern |
| :--- | :--- | :--- |
| Forgetting `net.ipv4.ip_forward = 1` on the VPN gateway server | Linux drops forwarded packets; clients can ping the VPN server IP but cannot reach internal subnets or the internet. | Enable IP forwarding permanently in `/etc/sysctl.d/99-wireguard.conf`. |
| Overlapping `AllowedIPs` across multiple peers on the same server | WireGuard cryptokey routing cannot map the same IP to two different public keys; traffic routes unpredictably. | Ensure every peer has an exclusive, distinct IP in `AllowedIPs` (e.g. `10.8.0.2/32`, `10.8.0.3/32`). |
| Forgetting to open UDP port 51820 on server firewall | Intermediate firewall drops incoming UDP handshakes; `wg show` displays 0 handshakes. | Allow UDP port: `nft add rule inet filter input udp dport 51820 accept` or `ufw allow 51820/udp`. |
| Misunderstanding `AllowedIPs` as an access control list only | `AllowedIPs` does double-duty: it configures both the kernel routing table **and** cryptographic ingress packet filtering. | Specify all subnets the client is allowed to receive AND send through the tunnel. |
| Neglecting `PersistentKeepalive` on nodes behind NAT | Client can initiate connections, but after 30 seconds of inactivity, server cannot send packets back to client. | Add `PersistentKeepalive = 25` in client `[Peer]` section. |
| Setting MTU to 1500 | Outer UDP + IP headers exceed 1500 bytes; packets with DF bit set are dropped silently. | Set `MTU = 1420` in interface configuration. |

---

## Performance, Security & Cost Implications

### Performance
- **In-Kernel Execution**: Unlike OpenVPN (which copies packets back and forth between kernel TUN drivers and userspace daemons), WireGuard processes packets entirely inside the kernel network subsystem (`wireguard.ko`). 
- **Benchmarks**: On modern multi-core servers, WireGuard delivers **4x–6x higher throughput** and **3x lower latency** than OpenVPN, saturating 10Gbps links with minimal CPU overhead.

### Security
- **Stealth / Zero Attack Surface**: WireGuard does not respond to unauthenticated packets. If an attacker port-scans UDP port 51820, WireGuard silently drops the packet without emitting ICMP port unreachable or handshake rejection packets. The port appears completely dead/filtered to scanners (e.g., Nmap).
- **No Dynamic Cipher Negotiation**: Security flaws in TLS often stem from downgrade attacks (forcing clients into weak 3DES or RC4 ciphers). WireGuard has no cipher negotiation; modern primitives are hardcoded.

### Cost
- Implementing WireGuard site-to-site tunnels between AWS, GCP, and on-premises datacenters replaces expensive proprietary hardware VPN appliances and managed cloud VPN services (e.g. AWS Site-to-Site VPN charges $0.05/hr per tunnel plus data transfer), saving thousands of dollars annually.

---

## Comparison Table / Alternatives

| Feature / Dimension | WireGuard | OpenVPN | IPsec (IKEv2) |
| :--- | :--- | :--- | :--- |
| **Kernel / Userspace** | Kernel space (`wireguard.ko`) | Userspace (TUN/TAP) | Kernel space (XFRM) |
| **Codebase Size** | ~4,000 lines | ~100,000+ lines | ~400,000+ lines |
| **Cryptography** | Fixed Modern (ChaCha20, Curve25519) | Negotiable (OpenSSL stack) | Negotiable (Complex suites) |
| **Handshake Latency** | **1-RTT** (~5ms) | Multi-RTT (100ms–2000ms) | Multi-step IKE exchange |
| **NAT Traversal** | Automatic (Roaming + Keepalive) | Requires configuration | NAT-T (UDP port 4500) |
| **Configuration Model**| Public keys & AllowedIPs | Certificates, CAs, and keys | Complex proposals, policies |
| **Primary Use Cases** | Modern cloud mesh, dev tunnels, mobile VPN | Legacy enterprise corporate VPNs | Inter-datacenter router peering |

---

## Real-World Troubleshooting & Interview Q&A

### Q1: `wg show` displays sent bytes increasing, but received bytes remain at 0. What is wrong?
<details>
<summary>View Solution</summary>

**Meaning**: The local client is transmitting handshake initiation packets, but has received zero responses from the remote peer.

**Root Causes**:
1. **Firewall Drop**: The remote server's firewall (e.g. `nftables`, `ufw`, AWS Security Group) is blocking UDP port 51820.
2. **Incorrect Public Key**: The server has the wrong public key configured for this peer, or the client configured the wrong public key for the server. WireGuard silently discards packets with invalid cryptographic signatures.
3. **Mismatched Endpoint**: The client is targeting an invalid IP address or port for the server.
4. **NAT Drop**: The server is attempting to reply, but an intermediate NAT router dropped the UDP return packet.
</details>

### Q2: How does WireGuard handle dynamic IP roaming (e.g., switching from Wi-Fi to cellular data)?
<details>
<summary>View Solution</summary>

WireGuard handles network roaming seamlessly without reconnecting:
1. When a client roams from Wi-Fi to mobile cellular, its public IP changes.
2. The client transmits its next authenticated data packet to the server from the new IP.
3. The server kernel decrypts and validates the packet using the client's public key.
4. Upon successful cryptographic validation, the server's kernel **automatically updates the peer's endpoint IP and port** to the newly observed source address.
5. All subsequent return packets are routed to the new IP immediately with zero packet loss or renegotiation delay.
</details>

### Q3: What is the exact purpose of `AllowedIPs` on the client vs on the server?
<details>
<summary>View Solution</summary>

`AllowedIPs` serves two distinct purposes:
- **On the Client**:
  - Acts as a **Routing Table Filter**: Any destination IP matching `AllowedIPs` is routed into the `wg0` interface. If `AllowedIPs = 0.0.0.0/0`, all traffic routes into the tunnel.
  - Acts as an **Ingress Filter**: The client only accepts incoming packets from the server whose internal source IP falls within `AllowedIPs`.
- **On the Server**:
  - Acts as an **Ingress Firewall**: The server only allows packets from that peer if the packet's source IP matches that peer's `AllowedIPs`.
  - Acts as an **Egress Demultiplexer**: When routing packets into `wg0`, the server checks `AllowedIPs` across all peers to determine which peer's public key to use for encryption.
</details>

### Q4: Why does WireGuard use UDP instead of TCP?
<details>
<summary>View Solution</summary>

Running a VPN over TCP creates the notorious **"TCP-over-TCP Meltdown"** problem.
- When packet loss occurs on the physical link, both the inner TCP stream (the application) and the outer TCP stream (the VPN tunnel) attempt independent retransmissions and exponential backoffs.
- The two congestion control algorithms compete against each other, causing bufferbloat, stalling, and complete connection collapse.
- WireGuard runs over UDP, allowing the underlying applications to manage their own congestion control and retransmissions cleanly.
</details>

### Q5: How do you verify that WireGuard traffic is not leaking out through the physical interface?
<details>
<summary>View Solution</summary>

1. **Verify Default Route**:
   ```bash
   ip route get 8.8.8.8
   # Expected output: 8.8.8.8 dev wg0 src 10.8.0.2
   ```
2. **Inspect External IP via Curl**:
   ```bash
   curl https://ifconfig.me
   # Must return the public IP of the VPN gateway server, not the local ISP IP!
   ```
3. **Packet Capture Physical Link**:
   ```bash
   sudo tcpdump -i eth0 -nn "not udp port 51820"
   # Must show zero plaintext application packets exiting the physical interface.
   ```
</details>

### Q6: What is the purpose of `wg-quick`'s `Table = off` directive?
<details>
<summary>View Solution</summary>

By default, `wg-quick up` automatically adds routes for all subnets listed in `AllowedIPs` into the main routing table. 
In complex enterprise setups with existing BGP routing daemons (e.g. FRRouting, Bird) or custom policy routing tables (`ip rule`), automatic route injection causes conflicts. Setting `Table = off` instructs `wg-quick` to configure the WireGuard interface and cryptokey bindings **without touching the Linux kernel routing tables**, leaving routing control entirely to the administrator.
</details>

### Q7: Why does `wg` not have a command to "disconnect" a client?
<details>
<summary>View Solution</summary>

WireGuard is inherently **connectionless and stateless**, functioning like an Ethernet switch. There is no active "session" to disconnect. 
- If no traffic flows, zero packets are transmitted.
- To revoke access for a peer, simply remove the peer's public key from the server configuration:
  ```bash
  sudo wg set wg0 peer <PEER_PUBLIC_KEY> remove
  ```
- The server will immediately stop accepting packets encrypted with that key.
</details>

### Q8: How can you pass WireGuard traffic through environments that block UDP port 51820?
<details>
<summary>View Solution</summary>

Some restrictive hotel or corporate firewalls block all non-standard UDP ports.
Workarounds:
1. Change the listening port to standard UDP port `53` (DNS) or `123` (NTP): `ListenPort = 53`.
2. Encapsulate WireGuard UDP packets inside standard TLS/HTTPS streams using tools like **`udp2raw`** or **`shadowsocks`**.
</details>

---

## Hands-On Practice Exercises

### Exercise 1: Generating WireGuard Keypairs and Pre-Shared Keys
**Task**: Use `wg` to generate a private key, derive its public key, and generate a pre-shared key. Inspect their formatting.
<details>
<summary>View Solution</summary>

```bash
# 1. Generate keys
PRIV_KEY=$(wg genkey)
PUB_KEY=$(echo "${PRIV_KEY}" | wg pubkey)
PSK=$(wg genpsk)

# 2. Inspect
echo "Private Key: ${PRIV_KEY}"
echo "Public Key:  ${PUB_KEY}"
echo "PSK:         ${PSK}"

# Notice all keys are standard 44-character Base64 encoded strings!
```
</details>

### Exercise 2: Creating a Local Mock WireGuard Interface
**Task**: Create a virtual WireGuard interface `wg-test` using `ip link`. Assign an IP address and private key. Verify with `wg show`. Tear it down.
<details>
<summary>View Solution</summary>

```bash
# 1. Create interface
sudo ip link add dev wg-test type wireguard

# 2. Assign IP and bring UP
sudo ip addr add 10.99.0.1/24 dev wg-test
sudo ip link set dev wg-test up

# 3. Assign generated private key
PRIV_KEY=$(wg genkey)
echo "${PRIV_KEY}" | sudo wg set wg-test private-key /dev/stdin

# 4. View status
sudo wg show wg-test

# 5. Clean up
sudo ip link delete dev wg-test
```
</details>

### Exercise 3: Simulating Cryptokey Ingress Dropping
**Task**: Explain how WireGuard protects against IP spoofing. If a rogue host sends packets with source IP `10.8.0.5` through a peer configured with `AllowedIPs = 10.8.0.2/32`, what does the kernel do?
<details>
<summary>View Solution</summary>

The Linux kernel decrypts the packet using the peer's public key. Next, the cryptokey routing engine checks whether the decrypted source IP (`10.8.0.5`) falls within that peer's registered `AllowedIPs` (`10.8.0.2/32`). 
Because `10.8.0.5` is not in `AllowedIPs`, the kernel **silently drops the packet immediately** and increments the interface's rx error counter. The packet never reaches the networking stack or local applications.
</details>

### Exercise 4: Building a Split-Tunnel Configuration
**Task**: Draft a client configuration file that accesses a private corporate VPC subnet `172.20.0.0/16` through the VPN, while letting all general internet traffic exit locally.
<details>
<summary>View Solution</summary>

```ini
[Interface]
PrivateKey = <CLIENT_PRIVATE_KEY>
Address = 10.8.0.50/24

[Peer]
PublicKey = <SERVER_PUBLIC_KEY>
Endpoint = vpn.company.com:51820
# Split-tunneling: only route corporate subnet!
AllowedIPs = 172.20.0.0/16, 10.8.0.1/32
PersistentKeepalive = 25
```
</details>

### Exercise 5: Auditing Interface Transfer Statistics
**Task**: Use `wg show <interface> transfer` to extract the exact byte volume transferred and received across all peers.
<details>
<summary>View Solution</summary>

```bash
# Display raw transfer stats
sudo wg show all transfer 2>/dev/null || echo "No active WireGuard interfaces currently running"
```
</details>

### Exercise 6: Verifying WireGuard Kernel Module Status
**Task**: Check if the `wireguard` kernel module is loaded in your running Linux kernel. Inspect its version and module dependencies.
<details>
<summary>View Solution</summary>

```bash
# Check if module is loaded or built-in
lsmod | grep -i wireguard || modinfo wireguard 2>/dev/null || true
```
</details>

---

## Summary Cheat Sheet & Further Reading

### Command Reference
| Operation | Command Example |
| :--- | :--- |
| **Generate Private Key** | `wg genkey > private.key` |
| **Derive Public Key** | `wg pubkey < private.key > public.key` |
| **Generate Pre-Shared Key**| `wg genpsk > preshared.key` |
| **Show All VPN Tunnels** | `sudo wg show` |
| **Start Tunnel via Quick** | `sudo wg-quick up wg0` |
| **Stop Tunnel via Quick** | `sudo wg-quick down wg0` |
| **Systemd Auto-Start** | `sudo systemctl enable --now wg-quick@wg0` |
| **Dynamically Remove Peer**| `sudo wg set wg0 peer <PUBKEY> remove` |
| **Generate QR Code** | `qrencode -t ansiutf8 < /etc/wireguard/client.conf` |

### Further Reading
- `man 8 wg` and `man 8 wg-quick`
- [WireGuard Official Technical Whitepaper (Jason A. Donenfeld)](https://www.wireguard.com/papers/wireguard.pdf)
- [WireGuard Formal Verification & Cryptography](https://www.wireguard.com/formal-verification/)
- [The Noise Protocol Framework](http://noiseprotocol.org/)
- [Linux Kernel In-Tree WireGuard Documentation](https://docs.kernel.org/networking/device_drivers/cellular/wireguard.html)
