# Networking Basics

Most "my app is down" reports are really networking questions: is the machine reachable, is the name resolving, is something listening on the port, is a firewall in the way? This note gives you the minimum model (addresses, routes, ports, DNS) and a set of tools that answer those questions one layer at a time.

Prerequisites: [Shell Basics](../02-shell/01-shell-basics.md) and [Processes and Signals](../03-system/01-processes-and-signals.md) (a "listening" socket belongs to a process).

## What happens when you open a URL

```text
curl https://app.example.com/health
  │
  ├─ 1. DNS      name → IP address            (UDP/TCP 53)
  ├─ 2. Routing  which interface/gateway?     (ip route)
  ├─ 3. TCP      three-way handshake to :443  (SYN, SYN-ACK, ACK)
  ├─ 4. TLS      certificate + encryption
  └─ 5. HTTP     request → response
```

Debugging is just finding which step fails. Every tool below targets one of them.

## Addresses and networks

- An **IPv4 address** is four bytes (`192.168.1.20`). A **CIDR** suffix says how many leading bits identify the network: `192.168.1.0/24` means the first 24 bits are the network, leaving 256 addresses (254 usable hosts).
- **Private ranges** (not routable on the internet): `10.0.0.0/8`, `172.16.0.0/12`, `192.168.0.0/16`. Cloud VMs usually have a private IP plus a mapped public one.
- **Loopback**: `127.0.0.1` (IPv6 `::1`), `localhost`. Traffic never leaves the machine.
- **IPv6** addresses look like `2001:db8::1`. Many systems have both, and a service reachable over one family may not be over the other.

### `0.0.0.0` vs `127.0.0.1`: what a server binds to

When a service listens, it **binds** to an address:

| Bind address | Reachable from |
|---|---|
| `127.0.0.1` | only this machine |
| `0.0.0.0` | every IPv4 interface, so the network too |
| a specific IP | only that interface |

A Node app doing `app.listen(3000, '127.0.0.1')` works from `curl localhost:3000` on the server but not from another machine. Mixing these up is one of the most common causes of "works locally, not remotely". A database should usually bind to `127.0.0.1` or a private IP, not `0.0.0.0`.

## Inspecting your interfaces: `ip`

`ip` (from `iproute2`) replaces the old `ifconfig`, `route`, and `arp`. Those come from the deprecated `net-tools` package and may not even be installed.

```bash
ip -br addr              # brief: interface, state, addresses
ip addr show eth0
ip link                  # link layer: up/down, MAC
ip route                 # routing table
ip route get 8.8.8.8     # which interface/gateway would reach this IP?
ip neigh                 # ARP/neighbour cache (IP to MAC on the local network)
```

```text
$ ip route
default via 10.0.0.1 dev eth0 proto dhcp src 10.0.0.15
10.0.0.0/24 dev eth0 proto kernel scope link src 10.0.0.15
```

Read it as: anything in `10.0.0.0/24` is directly reachable on `eth0`; everything else goes to the **default gateway** `10.0.0.1`. No default route means no internet.

## Ports, TCP, and UDP

A port is a 16-bit number (0 to 65535) that selects a service on an IP. A connection is identified by **source IP:port + destination IP:port**.

- **TCP**: connection-oriented, reliable, ordered (HTTP, SSH, databases).
- **UDP**: connectionless, no delivery guarantee (DNS queries, streaming, QUIC).
- Ports below **1024** are privileged: binding them needs root or the `CAP_NET_BIND_SERVICE` capability. That's why apps typically listen on 3000/8080 behind a reverse proxy on 80/443.

Common ports worth recognizing:

| Port | Service | Port | Service |
|---|---|---|---|
| 22 | SSH | 3306 | MySQL/MariaDB |
| 53 | DNS | 5432 | PostgreSQL |
| 80 / 443 | HTTP / HTTPS | 6379 | Redis |
| 25 / 587 | SMTP | 27017 | MongoDB |

## What is listening, and who is connected: `ss`

`ss` replaces `netstat`.

```bash
ss -tulpn                 # listening TCP+UDP sockets with owning process
ss -tlnp                  # listening TCP only
ss -tnp                   # established TCP connections
ss -s                     # summary counts
ss -tlnp 'sport = :3000'  # who owns port 3000?
```

Flags: `t` TCP, `u` UDP, `l` listening, `n` numeric (no name lookups, faster), `p` process (needs `sudo` for other users' processes).

```text
State   Local Address:Port   Process
LISTEN  0.0.0.0:22           users:(("sshd",pid=812,fd=3))
LISTEN  127.0.0.1:5432       users:(("postgres",pid=1044,fd=6))
LISTEN  *:3000               users:(("node",pid=2210,fd=19))
```

Here SSH is open to the network, Postgres is local-only (good), and Node listens on all interfaces. `*` or `[::]` means all addresses; on Linux an IPv6 wildcard listener usually accepts IPv4 too, depending on the app and `bindv6only`.

**First question for any "can't connect" problem: is anything actually listening on that port, on the address I expect?**

## Reachability tools

```bash
ping -c 4 1.1.1.1            # ICMP echo; -c limits the count
traceroute example.com       # hop-by-hop path (sudo apt install traceroute)
mtr -rwc 20 example.com      # ping + traceroute combined report (sudo apt install mtr-tiny)
```

- `ping` failing doesn't prove a host is down; many networks and cloud firewalls block ICMP. Test the actual port instead.
- In `traceroute`, hops showing `* * *` just mean the router didn't answer probes; look at whether later hops respond.

Test a TCP port without any special tool, using bash:

```bash
timeout 3 bash -c '</dev/tcp/example.com/443' && echo open || echo closed-or-filtered
nc -zv example.com 443        # netcat; flags differ between variants (OpenBSD nc is the Ubuntu default)
```

## DNS

DNS maps names to addresses. The resolver on a Linux box works through, in order (see `/etc/nsswitch.conf`): `/etc/hosts`, then DNS servers.

```bash
cat /etc/hosts                   # static overrides (127.0.0.1 localhost, etc.)
cat /etc/resolv.conf             # on Ubuntu, usually 127.0.0.53: systemd-resolved's local stub
resolvectl status                # the real upstream DNS servers, per interface
resolvectl query example.com
```

Query tools:

```bash
getent hosts example.com         # what applications actually get (honors /etc/hosts, nsswitch)
dig example.com +short           # A records (sudo apt install dnsutils / bind9-dnsutils)
dig AAAA example.com +short
dig MX example.com +short
dig @1.1.1.1 example.com         # ask a specific server, bypassing the local resolver
dig example.com +trace           # walk the delegation from the root servers
```

`dig` talks to DNS directly and **ignores `/etc/hosts`**, so `dig` and your app can legitimately disagree. When they do, trust `getent hosts` as the app's view.

Record types you'll use: `A` (IPv4), `AAAA` (IPv6), `CNAME` (alias), `MX` (mail), `TXT` (verification, SPF), `NS` (delegation).

**"DNS propagation" is really cache expiry.** Every answer carries a **TTL** (seconds). Resolvers keep the old answer until it expires, so lower the TTL *before* a planned change, not after. Failed lookups are cached briefly too.

## HTTP with curl

```bash
curl https://example.com                       # GET, body to stdout
curl -I https://example.com                    # headers only (HEAD)
curl -v https://example.com                    # verbose: DNS, TLS handshake, headers
curl -sS -f https://example.com/health         # quiet, show errors, fail on HTTP >= 400 (good for scripts)
curl -L http://example.com                     # follow redirects
curl -o page.html https://example.com          # save to a file (-O keeps the remote name)
curl --max-time 5 https://example.com          # don't hang forever

curl -X POST https://api.example.com/items \
  -H 'Content-Type: application/json' \
  -d '{"name":"test"}'

curl --json '{"name":"test"}' https://api.example.com/items   # newer curl (7.82+): sets headers for you
```

Get just the status code, or a timing breakdown that tells you which phase is slow:

```bash
curl -s -o /dev/null -w '%{http_code}\n' https://example.com

curl -s -o /dev/null https://example.com -w \
'dns=%{time_namelookup} connect=%{time_connect} tls=%{time_appconnect} ttfb=%{time_starttransfer} total=%{time_total}\n'
```

Test a server **before DNS points at it**, or bypass DNS entirely:

```bash
curl --resolve app.example.com:443:203.0.113.10 https://app.example.com/
curl -H 'Host: app.example.com' http://203.0.113.10/     # for plain HTTP virtual hosts
```

Avoid `-k` (skip TLS verification) except for a quick local test; it removes the protection TLS exists to give you.

## A troubleshooting ladder

Work from the bottom up and stop at the first failure:

| Step | Command | If it fails |
|---|---|---|
| 1. Interface has an IP | `ip -br addr` | link down, DHCP problem |
| 2. Default route exists | `ip route` | no gateway configured |
| 3. Internet by IP | `ping -c3 1.1.1.1` | routing / upstream (or ICMP blocked) |
| 4. DNS works | `getent hosts example.com` | resolver config, upstream DNS |
| 5. Target port reachable | `nc -zv host 443` | firewall, service down |
| 6. Service listening (on the server) | `ss -tlnp` | not running, wrong bind address |
| 7. App answers | `curl -v ...` | app bug, proxy misconfig |

### Reading the error

| Symptom | Usually means |
|---|---|
| `Connection refused` | host reachable, **nothing listening** (or an explicit REJECT) |
| `Connection timed out` | packets **dropped**: firewall/security group, wrong IP, no route |
| `Could not resolve host` / `Name or service not known` | DNS problem |
| TLS/certificate error | wrong name, expired or untrusted certificate, wrong time |
| `502` / `504` | a reverse proxy reached you but the upstream app failed or was slow |

Refused vs timed out is the most useful distinction: refused means "the machine answered, nobody home"; timeout points at a firewall or routing. See [firewall](./03-firewall.md).

## Common mistakes

- **Binding to `127.0.0.1`** and expecting remote access (or binding a database to `0.0.0.0` and exposing it).
- **Forgetting the cloud firewall.** Security groups and host firewalls are separate layers; both must allow the traffic.
- **Concluding "it's down" because ping fails.**
- **Trusting `dig` over `getent`** when `/etc/hosts` or search domains are involved.
- **Editing `/etc/resolv.conf` directly** on a systemd-resolved system; it's managed and your edit gets overwritten. Configure via netplan/NetworkManager/`resolvectl`.
- **Testing IPv4 only** when clients use IPv6 (or vice versa).
- **Reaching for `ifconfig`/`netstat`**, which may not be installed. Use `ip` and `ss`.
- **Changing DNS records without lowering the TTL first.**

## Quick Summary

- A request goes DNS → route → TCP → TLS → HTTP; debug by finding the failing layer.
- `ip -br addr` and `ip route` show addresses and the path out; `ss -tlnp` shows who is listening and on which address.
- `127.0.0.1` is local-only, `0.0.0.0` is everywhere; choose deliberately.
- `getent hosts` is the app's DNS view, `dig` the DNS server's view; TTLs explain "propagation".
- `curl -v` and `-w` timings show where HTTP time goes; `--resolve` tests before DNS changes.
- Refused means no listener; timed out means dropped.

**Next:** [SSH](./02-ssh.md)
