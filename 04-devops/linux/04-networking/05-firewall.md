# Firewall

A host firewall decides which network packets are allowed to reach (or leave) your server. The principle is simple: **deny everything inbound by default, then open only the ports you intend to serve.** The details that cause incidents are rule ordering, locking yourself out of SSH, and Docker quietly bypassing your rules.

Prerequisites: [Networking Basics](./01-networking-basics.md) (ports, `ss`) and [SSH](./02-ssh.md).

## How Linux firewalling works

Packet filtering happens inside the kernel in **netfilter**. Several tools configure it:

```text
 ufw / firewalld      friendly front-ends (Ubuntu / Red Hat family)
        │
 nftables  ◄── modern rule framework (`nft`)
 iptables  ◄── older interface; on current distros usually `iptables-nft`, a compatibility layer
        │
 netfilter (kernel)
```

Check which iptables you have: `iptables --version` prints `(nf_tables)` or `(legacy)`.

**Use one manager.** Running ufw, raw iptables, and firewalld together produces rules nobody can reason about. On Ubuntu, use `ufw` for simple host rules, or write `nftables` directly if you want full control.

Packets traverse **chains**: `INPUT` (to this machine), `OUTPUT` (from it), and `FORWARD` (routed through it). Each chain has a **policy** (the default when no rule matches), and rules are evaluated **top to bottom; the first match wins**.

### Stateful filtering

Modern firewalls track connections. You allow *new* inbound connections only to specific ports, and automatically allow replies to connections you started (`established,related`). Without that, outbound requests from your server (apt, DNS, API calls) would have their responses dropped.

### Two firewalls, not one

On cloud providers there's usually a **network-level firewall** (security groups, firewall rules) in front of the VM, and the **host firewall** on the VM. Traffic must pass both. "I opened the port in ufw but it still times out" is very often the cloud layer, and vice versa.

## ufw (Ubuntu)

ufw ("uncomplicated firewall") sets sane defaults and a short command syntax.

### A safe baseline

Order matters: **allow SSH before you enable the firewall**, or you cut your own connection.

```bash
sudo ufw default deny incoming
sudo ufw default allow outgoing

sudo ufw allow OpenSSH            # or: sudo ufw allow 22/tcp
sudo ufw allow 80,443/tcp         # web server

sudo ufw enable                   # asks for confirmation
sudo ufw status verbose
```

### Everyday commands

```bash
sudo ufw status numbered                 # rules with index numbers
sudo ufw allow 5432/tcp                  # open a port (all sources)
sudo ufw allow from 203.0.113.5 to any port 22 proto tcp     # only this IP may SSH
sudo ufw allow from 10.0.0.0/24 to any port 5432 proto tcp   # private subnet only
sudo ufw limit OpenSSH                   # allow but rate-limit repeated connection attempts
sudo ufw deny 23/tcp
sudo ufw delete allow 80/tcp             # delete by rule
sudo ufw delete 3                        # delete by number (re-check numbers after each delete)
sudo ufw reload
sudo ufw disable
sudo ufw reset                           # wipe all rules and disable (careful)
```

Specify the **source** whenever you can. "Postgres open to the world" is a very different statement from "Postgres open to the app subnet". A database port should almost never be open to `Anywhere`.

Also confirm IPv6 is covered: `IPV6=yes` in `/etc/default/ufw` (the default), so rules apply to both families.

Logging:

```bash
sudo ufw logging on
sudo journalctl -k | grep UFW             # or: sudo tail -f /var/log/ufw.log
```

`[UFW BLOCK]` lines show source IP, destination port, and protocol of dropped packets, which are very useful for confirming a rule is the cause.

### Not locking yourself out

- Keep your existing SSH session open and test a **new** connection after any change.
- Know your provider's **web/serial console** as a fallback.
- Restrict SSH by source IP only if that IP is stable; a changing home IP is a lockout waiting to happen.
- Optional safety net while experimenting: schedule a disable, then cancel it once you've verified. For example `echo "ufw disable" | sudo at now + 5 minutes` (needs the `at` package), then remove the job with `atq` / `atrm` when everything works.

## nftables

nftables is the current kernel framework, and the better choice when you need more than "open these ports".

Concepts: a **table** (with a family such as `inet` for IPv4+IPv6) contains **chains**, which contain **rules**. A *base chain* hooks into the packet path (`input`, `forward`, `output`) and sets a policy.

`/etc/nftables.conf`:

```text
#!/usr/sbin/nft -f

flush ruleset

table inet filter {
  chain input {
    type filter hook input priority 0; policy drop;

    ct state established,related accept
    ct state invalid drop
    iif "lo" accept

    ip protocol icmp accept
    ip6 nexthdr ipv6-icmp accept

    tcp dport 22 accept
    tcp dport { 80, 443 } accept
  }

  chain forward {
    type filter hook forward priority 0; policy drop;
  }

  chain output {
    type filter hook output priority 0; policy accept;
  }
}
```

Reading the input chain: default drop; allow replies to existing connections; allow loopback (`iif "lo"`); allow ICMP (needed for IPv6 to work and for path MTU discovery); allow SSH and web.

```bash
sudo nft -c -f /etc/nftables.conf      # check syntax only
sudo nft -f /etc/nftables.conf         # load it
sudo nft list ruleset                  # show active rules
sudo systemctl enable --now nftables   # load the file at boot (see systemd note)
```

`flush ruleset` deletes **every** table, including rules added by other tools (Docker, for example). If Docker is installed, scope your changes to your own table instead of flushing everything.

## iptables (you'll still meet it)

Many existing guides and servers use iptables:

```bash
sudo iptables -L -n -v --line-numbers          # list rules with counters
sudo iptables -A INPUT -m conntrack --ctstate ESTABLISHED,RELATED -j ACCEPT
sudo iptables -A INPUT -i lo -j ACCEPT
sudo iptables -A INPUT -p tcp --dport 22 -j ACCEPT
sudo iptables -A INPUT -p tcp -m multiport --dports 80,443 -j ACCEPT
sudo iptables -P INPUT DROP                    # default policy LAST, after allowing SSH
```

- `-A` appends (so order is the order you type); `-I INPUT 1` inserts at the top.
- Rules are **not persistent** across reboots by default; use `iptables-persistent`/`netfilter-persistent` or convert to nftables (`iptables-save | iptables-restore-translate`).
- Setting `-P INPUT DROP` before the SSH rule is the classic self-lockout.

## DROP vs REJECT

- **DROP** silently discards: the client sees a **timeout**. Better for the public internet (slows scanners, reveals less).
- **REJECT** sends back an error: the client immediately sees **connection refused**. Friendlier on internal networks.

This is why "timed out" suggests a firewall while "refused" suggests no listener ([networking basics](./01-networking-basics.md)).

## Docker and the firewall

Docker writes its own iptables/nftables rules for published ports (`-p 8080:80`), and those are evaluated **before** ufw's rules. A container published as `-p 8080:80` is reachable from the internet **even if ufw says deny 8080**.

Safer patterns:

```bash
docker run -p 127.0.0.1:8080:80 myapp    # publish only on loopback, put nginx in front
```

```yaml
# docker-compose.yml
ports:
  - "127.0.0.1:5432:5432"
```

For rules that must apply to container traffic, Docker provides the `DOCKER-USER` chain, which is processed before Docker's own rules. Keep databases and internal services unpublished or bound to loopback, and verify exposure from **outside** the host rather than trusting `ufw status`.

## Verifying and debugging

A firewall problem has three possible places: the service isn't listening, the host firewall blocks it, or the network/cloud firewall blocks it.

```bash
# On the server: is the service listening, on which address?
sudo ss -tlnp | grep ':3000'

# On the server: what do the rules say?
sudo ufw status verbose            # or: sudo nft list ruleset

# From ANOTHER machine: is the port reachable?
nc -zv server.example.com 3000
curl -v --max-time 5 http://server.example.com:3000/
# (nmap -Pn -p 22,80,443 server.example.com gives a quick view of open ports; scan only systems you own.)

# Are packets arriving at all?
sudo tcpdump -ni any port 3000
```

Interpretation:

| From outside | Likely cause |
|---|---|
| `Connection refused` | host reachable, nothing listening (or REJECT rule) |
| Timeout | DROP rule on host, cloud security group, or wrong IP |
| Works locally, fails remotely | service bound to `127.0.0.1`, or firewall |
| Works with ufw disabled | a rule or its order; check `ufw status numbered` and logs |

`tcpdump` is the tie-breaker: if the SYN packet never shows up on the server, the block is upstream (cloud firewall/network). If it arrives and no reply goes out, it's the host (firewall or app). More in [advanced debugging](../06-advanced/02-advanced-debugging.md).

## Common mistakes

- **Enabling the firewall without allowing SSH first** (or setting `iptables -P INPUT DROP` early).
- **Opening ports to everyone** that should be limited to a subnet or IP (databases, admin panels, Redis).
- **Trusting `ufw status` for Docker-published ports.**
- **Forgetting the cloud firewall** (or forgetting the host firewall when the cloud one is open).
- **Mixing ufw, iptables, firewalld, and Docker rules** and losing track of what's active.
- **Blocking ICMP entirely**, breaking IPv6 and path MTU discovery.
- **Rules that don't persist** (raw iptables) and vanish at reboot, or nftables not enabled at boot.
- **Changing rules over SSH with no way back** (no console access, no second session).
- **Relying on the firewall alone**: it complements, not replaces, keeping services bound correctly and software patched. See [server hardening](../05-production/01-server-hardening.md).

## Quick Summary

- Default-deny inbound, allow only needed ports, and restrict sources where possible.
- Rules are evaluated in order, first match wins; stateful tracking lets replies through.
- ufw for simple Ubuntu hosts (allow SSH **before** `enable`); nftables for full control; iptables is legacy but common.
- Pick one firewall manager; remember the cloud firewall is a separate layer.
- Docker-published ports bypass ufw: bind to `127.0.0.1` or use `DOCKER-USER`.
- Timeout means dropped, refused means no listener; confirm from outside the host and use `tcpdump` to find where packets stop.

**Next:** [Server Hardening](../05-production/01-server-hardening.md)
