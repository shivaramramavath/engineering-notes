# Module 04: Linux Networking & Remote Access

[<- Back to Linux Track Root](../README.md) | [<- Previous Module: 03-system](../03-system/README.md) | [Next Module: 05-production ->](../05-production/README.md)

---

## 1. Overview & Scope

Networking is the connective tissue of modern cloud and container infrastructure. Every ingress packet to a Kubernetes cluster, every microservice gRPC call, and every database replication query traverses the Linux network stack.

Module 04 covers modern Linux networking using the standard `iproute2` suite, `ss`, Netplan, NetworkManager, `systemd-resolved`, OpenSSH hardening, packet filtering via `nftables` and `ufw`, low-level packet capture with `tcpdump`, protocol diagnostics with `curl`/`socat`, TLS certificate inspection with OpenSSL, and modern VPN mesh topologies with WireGuard.

---

## 2. Core Architecture: Linux Network Subsystem & Netfilter

```
+---------------------------------------------------------------------------------------+
| LINUX NETWORK PACKET TRAVERSAL & NETFILTER PIPELINE                                   |
|                                                                                       |
|   Physical / Virtual Network Interface (NIC / veth / tun / tap)                      |
|         │                                                                             |
|         v                                                                             |
|   [ Ingress Hook ] (XDP / TC eBPF / Driver)                                          |
|         │                                                                             |
|         v                                                                             |
|   [ Netfilter: PREROUTING ] (DNAT, raw, mangle)                                       |
|         │                                                                             |
|         ├───> [ Routing Decision ] ──(Forwarded)──> [ FORWARD ] ──> [ POSTROUTING ]   |
|         │                                                                │            |
|         v (Local Destination)                                            v (Outgoing) |
|   [ Netfilter: INPUT ] (Firewall: nftables / ufw)                  Network Interface  |
|         │                                                                ^            |
|         v                                                                │            |
|   [ Kernel Socket Layer: TCP / UDP ]                               [ POSTROUTING ]    |
|   (Queried via `ss -tulpn`)                                              ^            |
|         │                                                                │            |
|         v                                                          [ OUTPUT Chain ]   |
|   [ User Space Application: Nginx / Envoy / Go App ] ──────────────(Local Packet)────┘
+---------------------------------------------------------------------------------------+
```

---

## 3. Module Notes Catalog

| # | Note | Level | Time | Key Concepts & Topics |
| :---: | :--- | :---: | :---: | :--- |
| **01** | [Linux Networking Basics: OSI, TCP/IP & Architecture](./01-networking-basics.md) | Beginner-Int | 55m | OSI vs TCP/IP model, packet headers, MTU, loopback, MAC addresses, sockets, socket states (LISTEN, ESTABLISHED, TIME_WAIT) |
| **02** | [IP Addressing, Routing & Interfaces with iproute2](./02-ip-routing-and-interfaces.md) | Intermediate | 60m | `ip address`, `ip link`, `ip route`, CIDR notation, default gateways, static routes, policy routing, virtual interfaces (veth, bridge, bond) |
| **03** | [DNS Resolution & Name Service in Linux](./03-dns-resolution-and-tools.md) | Intermediate | 55m | `/etc/resolv.conf`, `nsswitch.conf`, `systemd-resolved`, `resolvectl`, DNS lookup tools (`dig`, `nslookup`), DNS caching & TTL |
| **04** | [SSH Mastery: Keys, Configs, Tunnels & Hardening](./04-ssh.md) | Intermediate | 65m | Asymmetric keys (`ed25519`), `~/.ssh/config` multiplexing, port forwarding (local, remote, dynamic SOCKS), `sshd_config` hardening, SSH certificates |
| **05** | [Linux Firewalls: Netfilter, Nftables & UFW](./05-firewall.md) | Intermediate | 60m | Netfilter architecture, `nftables` tables, chains and rules, `ufw` administration, `firewalld` zones, rate limiting and connection tracking (`conntrack`) |
| **06** | [Packet Capture & Network Analysis: Tcpdump & Wireshark](./06-tcpdump-and-wireshark-basics.md) | Advanced | 65m | `tcpdump` syntax, Berkeley Packet Filters (BPF), PCAP recording and rotation, analyzing TCP handshakes, window size, retransmissions in Wireshark |
| **07** | [Network Client Tools: Curl, Wget, Netcat & Socat](./07-curl-wget-netcat-and-socat.md) | Intermediate | 50m | HTTP debugging with `curl` (headers, timing `-w`, SSL flags), `wget` spidering, `nc` port testing & banner grabbing, `socat` bidirectional relaying |
| **08** | [Socket Inspection & Port Troubleshooting with ss](./08-ss-and-port-troubleshooting.md) | Intermediate | 50m | `ss` vs deprecated `netstat`, socket memory buffers (`-m`), filtering by state/port/CIDR, diagnosing port conflicts and TIME_WAIT socket exhaustion |
| **09** | [TLS/SSL Certificates & PKI with OpenSSL](./09-tls-certificates-with-openssl.md) | Intermediate | 55m | X.509 PKI, RSA vs ECC keys, CSR creation, self-signed certs, CA chains, verifying certificates with `openssl s_client`, Let's Encrypt / Certbot |
| **10** | [Network Performance, Bandwidth & Latency Testing](./10-load-and-bandwidth-testing.md) | Advanced | 50m | Bandwidth measurement with `iperf3`, latency and jitter analysis, `mtr` vs `traceroute`, TCP buffer bloat, synthetic HTTP benchmarking with `wrk` |
| **11** | [Modern Linux VPNs: WireGuard Fundamentals](./11-vpn-wireguard-basics.md) | Advanced | 60m | WireGuard kernel module architecture, Noise protocol, public-private key cryptography, interface configuration (`wg0`), site-to-site & point-to-point tunnels |

---

## 4. Key Milestones & Learning Objectives

By completing Module 04, you will be able to:
1. **Configure Production Network Interfaces**: Manipulate IP addresses, default routes, and network bridges using native `iproute2` commands and Netplan YAML definitions.
2. **Harden Remote SSH Access**: Eliminate password-based authentication, enforce `ed25519` cryptographic keys, disable root logins, and configure resilient jump hosts with proxy command tunnels.
3. **Build High-Performance Firewalls**: Author stateful firewall rulesets using `nftables` and `ufw` that isolate sensitive internal services and prevent brute-force abuse.
4. **Capture & Deconstruct Packet Drops**: Trace dropped packets, TCP resets (`RST`), and SSL handshake failures using `tcpdump` and Wireshark filters.
5. **Establish Secure Mesh Tunnels**: Deploy lightweight, kernel-space WireGuard VPN tunnels connecting distributed cloud nodes with sub-millisecond overhead.
