# Dual-Stack IPv6 Networking and Corporate Proxy Configuration

```
Track: DevOps & Containerization
Module: 04-volumes-and-networking
Prerequisites: 04-volumes-and-networking/04-networking.md, 04-volumes-and-networking/06-ports-publishing-and-firewalls.md
Difficulty: Advanced
Estimated Duration: 45 minutes
Target Docker Version: 27.x+
Last Validated: February 2025
```

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [IPv6 Architecture in Docker](#3-ipv6-architecture-in-docker)
4. [Corporate Proxies in Container Environments](#4-corporate-proxies-in-container-environments)
5. [Syntax & Configuration Reference](#5-syntax--configuration-reference)
6. [Practical Implementation & Examples](#6-practical-implementation--examples)
7. [Production Best Practices & Antipatterns](#7-production-best-practices--antipatterns)
8. [Common Pitfalls & Troubleshooting](#8-common-pitfalls--troubleshooting)
9. [Performance Tuning & Resource Optimization](#9-performance-tuning--resource-optimization)
10. [Security Hardening & Threat Model](#10-security-hardening--threat-model)
11. [Architectural Comparison Table](#11-architectural-comparison-table)
12. [Real-World Scenario Walkthrough](#12-real-world-scenario-walkthrough)
13. [Self-Assessment & Interview Questions](#13-self-assessment--interview-questions)
14. [Hands-on Practice Exercises](#14-hands-on-practice-exercises)
15. [Summary & Reference Cheat Sheet](#summary--reference-cheat-sheet)

---

## 1. What & Why

Enterprise container deployments must operate within complex infrastructure constraints:
- **IPv4 Exhaustion & Modern Cloud Standards**: Hyperscalers (AWS, Google Cloud) now charge hourly fees for public IPv4 addresses, accelerating the transition to **IPv6-only or dual-stack (IPv4/IPv6)** container networks.
- **Corporate Firewalls & Mandatory Outbound Proxies**: Enterprise developers and CI/CD runners frequently reside behind strict forward HTTP/HTTPS corporate proxies (e.g., Zscaler, BlueCoat, Squid) that perform Deep Packet Inspection (DPI) and block unauthenticated outbound traffic.

Configuring Docker for dual-stack IPv6 and corporate proxies requires careful coordination across three layers:
1. The Docker daemon (`/etc/docker/daemon.json` and systemd).
2. The Docker CLI client (`~/.docker/config.json`).
3. In-container build and runtime environments (`HTTP_PROXY` and custom corporate CA certificates).

---

## 2. Core Concepts

### Dual-Stack IPv6
By default, Docker operates exclusively in IPv4 mode. Enabling IPv6 creates a dual-stack configuration where containers receive:
- An IPv4 private address (e.g., `172.20.0.2`).
- An IPv6 globally routable or unique local address (ULA) (e.g., `2001:db8:1::2` or `fd00::2`).
- Seamless connectivity to both IPv4-only and IPv6-only public endpoints without NAT64.

### The Three Tiers of Proxy Configuration
A proxy configured in one location does **not** automatically apply to the others:
1. **Docker Daemon Systemd Proxy**: Used exclusively by `dockerd` to pull base images from Docker Hub or external registries (`docker pull`).
2. **Docker CLI Client Proxy (`~/.docker/config.json`)**: Injects `HTTP_PROXY`, `HTTPS_PROXY`, and `NO_PROXY` automatically into `docker build` and `docker run` environments.
3. **In-Container Runtime Proxy**: Passed via environment variables to application code.

---

## 3. IPv6 Architecture in Docker

### Routing Models: NAT66 vs Routed IPv6

```
+-------------------------------------------------------------+
|                     INTERNET ROUTER                         |
|                     (IPv6 Subnet)                           |
+------------------------------+------------------------------+
                               |
              +----------------+----------------+
              |                                 |
              v                                 v
+-----------------------------+   +---------------------------+
| Approach A: NAT66 (Default) |   | Approach B: Routed IPv6   |
| - Uses Unique Local (fd00::)|   | - Uses Public Subnet (/64)|
| - Docker masquerades IPv6   |   | - No NAT! Direct end-to-  |
|   packets via ip6tables     |   |   end routability         |
| - Standard in Docker 27+    |   | - Requires host route     |
|                             |   |   advertisement / NDP     |
+-----------------------------+   +---------------------------+
```

---

## 4. Corporate Proxies in Container Environments

### Corporate CA Inspection Problem
Enterprise proxies intercept outbound TLS connections, terminate the SSL session, inspect the payload, and re-sign the connection using an internal corporate Certificate Authority (CA).
- Inside a standard minimal container (Alpine, Debian Slim), this corporate CA is **untrusted**.
- `curl`, `pip`, `npm`, and `apt` fail immediately with:
  `SSL: CERTIFICATE_VERIFY_FAILED: self-signed certificate in certificate chain`.
- The internal corporate root CA certificate must be injected into the container's trust store.

---

## 5. Syntax & Configuration Reference

### Enabling IPv6 in `/etc/docker/daemon.json`

```json
{
  "ipv6": true,
  "fixed-cidr-v6": "2001:db8:1::/64",
  "experimental": true,
  "ip6tables": true
}
```

### Docker Daemon Systemd Proxy Configuration
In `/etc/systemd/system/docker.service.d/http-proxy.conf`:

```ini
[Service]
Environment="HTTP_PROXY=http://proxy.corp.internal:8080"
Environment="HTTPS_PROXY=http://proxy.corp.internal:8080"
Environment="NO_PROXY=localhost,127.0.0.1,docker-registry.corp.internal,.corp.internal"
```

### Docker Client Proxy (`~/.docker/config.json`)

```json
{
  "proxies": {
    "default": {
      "httpProxy": "http://proxy.corp.internal:8080",
      "httpsProxy": "http://proxy.corp.internal:8080",
      "noProxy": "localhost,127.0.0.1,.corp.internal,10.0.0.0/8"
    }
  }
}
```

---

## 6. Practical Implementation & Examples

### Example 1: Creating a Dual-Stack IPv4/IPv6 User-Defined Network

```bash
# 1. Create dual-stack bridge network
docker network create \
  --driver bridge \
  --ipv6 \
  --subnet 172.25.0.0/16 \
  --subnet fd00:dead:beef::/64 \
  dualstack-net

# 2. Run container and inspect assigned IP addresses
docker run -d --name dual-app --network dualstack-net alpine:3.21 sleep 3600

# 3. Query both IPv4 and IPv6 addresses
docker inspect dual-app --format 'IPv4: {{.NetworkSettings.Networks.dualstack_net.IPAddress}} | IPv6: {{.NetworkSettings.Networks.dualstack_net.GlobalIPv6Address}}'
# Displays: IPv4: 172.25.0.2 | IPv6: fd00:dead:beef::2

# 4. Ping external IPv6 address (Google Public DNS IPv6)
docker exec dual-app ping -c 2 2001:4860:4860::8888
```

### Example 2: In-Container Corporate CA Certificate Injection

```dockerfile
# syntax=docker/dockerfile:1.7
FROM debian:bookworm-slim

# Copy corporate root CA into system certificates
COPY corporate-root-ca.crt /usr/local/share/ca-certificates/corporate-root-ca.crt

# Update system certificate trust store
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
 && update-ca-certificates \
 && rm -rf /var/lib/apt/lists/*

# Now curl and package managers trust the corporate proxy inspection
RUN curl -fsSL https://api.github.com
```

### Example 3: BuildKit Secret Mount for Corporate Proxy Credentials
Never hardcode proxy passwords into `Dockerfile` or build arguments!

```dockerfile
# syntax=docker/dockerfile:1.7
FROM python:3.12-slim

# Mount proxy authentication credentials securely during build
RUN --mount=type=secret,id=proxy_creds \
    export HTTPS_PROXY="http://$(cat /run/secrets/proxy_creds)@proxy.corp.internal:8080" && \
    pip install --no-cache-dir requests
```
Build command:
```bash
docker buildx build --secret id=proxy_creds,src=proxy_auth.txt .
```

### Example 4: Configuring Build-Time vs Runtime Proxies

```bash
# Build-time proxy:
docker build \
  --build-arg HTTP_PROXY="http://proxy.corp.internal:8080" \
  --build-arg HTTPS_PROXY="http://proxy.corp.internal:8080" \
  --build-arg NO_PROXY="localhost,127.0.0.1" \
  -t my-app .

# Runtime proxy:
docker run -d \
  -e HTTP_PROXY="http://proxy.corp.internal:8080" \
  -e HTTPS_PROXY="http://proxy.corp.internal:8080" \
  -e NO_PROXY="localhost,127.0.0.1,database" \
  my-app
```

---

## 7. Production Best Practices & Antipatterns

### Best Practices
- **Enable `ip6tables: true`**: In Docker 27+, always set `"ip6tables": true` in `daemon.json` so Docker manages IPv6 firewalling and security rules with the same isolation guarantees as IPv4.
- **Configure `NO_PROXY` Comprehensively**: Always include `localhost`, `127.0.0.1`, internal Docker service names, and local private subnets (`10.0.0.0/8`, `172.16.0.0/12`) in `NO_PROXY`. If omitted, internal container-to-container calls will route to the corporate proxy and fail.
- **Use `~/.docker/config.json` for Developer Machines**: Avoid cluttering individual Dockerfiles with proxy logic. Use client-level proxy injection.
- **Mount Corporate Certificates at Build Time**: Use multi-stage builds to ensure internal corporate CA certificates do not unnecessarily bloat external customer-facing images.

### Anti-Patterns
- **Hardcoding Corporate Proxy URLs in Dockerfile `ENV`**: Bakes internal enterprise proxy hostnames and credentials directly into image metadata, breaking the image when run outside the corporate network.
- **Setting `insecure_skip_verify` to Bypass Proxy SSL Errors**: Disables cryptographic verification, exposing packages to Man-in-the-Middle malware injection.
- **Ignoring IPv6 Firewalls**: Assuming IPv6 containers are private without `ip6tables` enabled exposes containers to direct unsolicited internet probes.

---

## 8. Common Pitfalls & Troubleshooting

| Symptom | Root Cause | Diagnosis Command | Permanent Fix |
|:---|:---|:---|:---|
| `docker pull` fails with `proxyconnect tcp: connection refused` behind corporate firewall | Docker daemon lacks systemd HTTP_PROXY environment variables | `systemctl show --property=Environment docker` | Configure `/etc/systemd/system/docker.service.d/http-proxy.conf`. |
| `SSL: CERTIFICATE_VERIFY_FAILED` during `pip install` inside container | Corporate proxy re-signing SSL traffic with private enterprise CA | `openssl s_client -connect pypi.org:443 -showcerts` | Install corporate root CA into `/usr/local/share/ca-certificates` and run `update-ca-certificates`. |
| Inter-container communication fails with `HTTP 502 Bad Gateway` | Container routing internal container DNS names to the corporate proxy | Check container `echo $NO_PROXY` | Add internal container names and domain suffixes to `NO_PROXY`. |
| IPv6 ping fails with `Network is unreachable` | Host kernel does not have IPv6 forwarding enabled | `sysctl net.ipv6.conf.all.forwarding` | Set `sysctl -w net.ipv6.conf.all.forwarding=1` in `/etc/sysctl.conf`. |
| `docker build` ignores proxy in `~/.docker/config.json` | Older Docker client version or BuildKit disabled | `docker version` | Enable BuildKit (`DOCKER_BUILDKIT=1`) or pass `--build-arg HTTP_PROXY=...`. |
| IPv6 addresses change randomly on every container reboot | Missing static IPv6 configuration on user-defined network | `docker inspect <container>` | Use `--ip6 <fixed_ipv6>` on network with declared `--subnet`. |

---

## 9. Performance Tuning & Resource Optimization

1. **DNS Lookup Optimization for Proxies**:
   If `HTTP_PROXY="http://proxy.corp.internal:8080"` uses a hostname, ensure local DNS caches the resolution. Repeated proxy hostname queries can introduce 10-20ms of latency per connection.
2. **IPv6 Direct Routing**:
   Unlike IPv4 which relies heavily on CPU-intensive NAT connection tracking, routed public IPv6 networks allow direct kernel routing without NAT translation, saving CPU cycles on multi-gigabit gateways.

---

## 10. Security Hardening & Threat Model

```
+--------------------------------------------------------------+
| ATTACK: Credential Leakage via Proxy Environment Variables   |
+--------------------------------------------------------------+
| Developer authors Dockerfile:                                |
| `ENV HTTP_PROXY="http://alice:Password123@proxy.corp:8080"`  |
| Image pushed to public registry (Docker Hub).                |
| Attacker inspects image history: `docker history <image>`    |
| Attacker extracts corporate proxy credentials!               |
|                                                              |
| PERMANENT FIX: USE BUILDKIT SECRETS OR ~/.docker/config.json |
+--------------------------------------------------------------+
```

---

## 11. Architectural Comparison Table

| Attribute | IPv4-Only Network | Dual-Stack IPv4 / IPv6 |
|:---|:---|:---|
| **Address Space** | Limited (RFC 1918 Private) | Virtually Infinite |
| **NAT Overhead** | Mandatory (`MASQUERADE`) | Optional (Direct routing or NAT66) |
| **Public Cloud Cost** | Increasing ($0.005/hr per IPv4) | **Zero Additional IP Cost** |
| **Docker Configuration** | Default out of the box | Requires `daemon.json` setup |
| **Enterprise Adoption** | Universal legacy standard | High-growth modern standard |

---

## 12. Real-World Scenario Walkthrough

### Scenario: Unblocking Docker Builds on Developer Laptops Behind Zscaler

#### The Challenge
A software engineering team at a Fortune 500 company deployed Zscaler Internet Security. Overnight, all developer Docker builds broke across 400 engineers:
`curl: (60) SSL certificate problem: self-signed certificate in certificate chain`.
Engineers were unable to build Dockerfiles or download npm/pip packages.

#### The Resolution
1. **Extracted Corporate Root CA**:
   Exported `ZscalerRootCertificate-2025.crt` from host system keychain.
2. **Automated Base Image Standard**:
   Created a certified internal base image (`internal-registry.corp/base/debian:latest`) containing the pre-installed CA certificate.
3. **Automated Client Config**:
   Distributed a managed `~/.docker/config.json` via MDM (Jamf/Intune):
   ```json
   {
     "proxies": {
       "default": {
         "httpProxy": "http://127.0.0.1:9000",
         "httpsProxy": "http://127.0.0.1:9000",
         "noProxy": "localhost,127.0.0.1,.corp.internal"
       }
     }
   }
   ```
4. Build pipelines returned to 100% operational status without editing individual repository Dockerfiles.

---

## 13. Self-Assessment & Interview Questions

<details>
<summary><strong>1. Why doesn't setting `HTTP_PROXY` inside `~/.docker/config.json` affect `docker pull` commands?</strong></summary>

**Answer:**
`~/.docker/config.json` configures the Docker **client** (CLI). It automatically injects proxy environment variables into containers created via `docker run` and `docker build`. However, `docker pull` is executed directly by the background **Docker daemon** (`dockerd`), not the client. The daemon requires proxy environment variables to be configured in its host systemd unit file (`/etc/systemd/system/docker.service.d/http-proxy.conf`).
</details>

<details>
<summary><strong>2. What is the danger of forgetting to configure `NO_PROXY` when running containers behind a corporate proxy?</strong></summary>

**Answer:**
If `NO_PROXY` is empty or missing, all outgoing network traffic from the container will be forwarded to the corporate proxy. When a web container attempts to communicate with a database container using its Docker internal DNS name (e.g. `http://database:5432`), the request is sent to the external corporate proxy. The corporate proxy cannot resolve internal Docker DNS names, resulting in `502 Bad Gateway` or connection timeouts.
</details>

<details>
<summary><strong>3. How does Docker implement IPv6 firewalling and isolation when `"ip6tables": true` is enabled?</strong></summary>

**Answer:**
When `ip6tables: true` is enabled in `daemon.json`, Docker manipulates the Linux kernel's `ip6tables` subsystem in the same way it handles IPv4 iptables. It creates `DOCKER`, `DOCKER-USER`, and isolation chains for IPv6, preventing unauthorized external inbound IPv6 connections to unpublished ports and ensuring container network isolation.
</details>

<details>
<summary><strong>4. What is the difference between NAT66 and Native IPv6 Routing in Docker?</strong></summary>

**Answer:**
- **NAT66 (Network Address Translation for IPv6)**: Containers receive private Unique Local Addresses (ULA, `fd00::/8`), and the host kernel masquerades outgoing packets using the host's public IPv6 address.
- **Native IPv6 Routing**: Containers receive globally routable public IPv6 addresses directly from an allocated `/64` subnet. Packets are routed directly without address translation, preserving end-to-end IP transparency.
</details>

<details>
<summary><strong>5. How do you inject a corporate SSL certificate into an Alpine Linux container?</strong></summary>

**Answer:**
Copy the `.crt` file into `/usr/local/share/ca-certificates/` and run `update-ca-certificates`:
```dockerfile
COPY corp-ca.crt /usr/local/share/ca-certificates/corp-ca.crt
RUN apk add --no-cache ca-certificates && update-ca-certificates
```
</details>

<details>
<summary><strong>6. Why should you never bake corporate proxy authentication passwords into Dockerfile `ENV` instructions?</strong></summary>

**Answer:**
`ENV` instructions are permanently recorded in the image manifest and layer history. Anyone who pulls the image or runs `docker history <image>` can inspect the environment variables and extract the plaintext corporate credentials.
</details>

<details>
<summary><strong>7. What sysctl setting is mandatory on the Linux host to enable IPv6 container routing?</strong></summary>

**Answer:**
`net.ipv6.conf.all.forwarding=1`. If IPv6 forwarding is disabled in the Linux kernel, the host will drop incoming and outgoing IPv6 packets routed to and from container bridge interfaces.
</details>

<details>
<summary><strong>8. How can an administrator verify that a container has been assigned a valid global IPv6 address?</strong></summary>

**Answer:**
Run:
`docker inspect <container> --format '{{.NetworkSettings.GlobalIPv6Address}}'`
Or inside the container:
`ip -6 addr show dev eth0`
</details>

---

## 14. Hands-on Practice Exercises

<details>
<summary><strong>Exercise 1: Creating a Dual-Stack IPv4/IPv6 Network</strong></summary>

**Goal:** Author an IPv6-enabled network and verify dual-stack address assignment.

**Step-by-step Solution:**
1. Create network:
   ```bash
   docker network create --ipv6 --subnet 172.31.0.0/16 --subnet fd00:cafe:face::/64 dual-test-net
   ```
2. Run container:
   ```bash
   docker run -d --name dual-tester --network dual-test-net alpine:3.21 sleep 3600
   ```
3. Inspect addresses:
   ```bash
   docker inspect dual-tester --format '{{json .NetworkSettings.Networks.dual_test_net}}' | jq '{IPv4: .IPAddress, IPv6: .GlobalIPv6Address}'
   ```
</details>

<details>
<summary><strong>Exercise 2: Simulating Corporate Proxy Injection via CLI</strong></summary>

**Goal:** Launch a container that routes outbound traffic through a mock local proxy.

**Step-by-step Solution:**
1. Run container passing proxy arguments:
   ```bash
   docker run --rm -it \
     -e HTTP_PROXY="http://10.0.0.1:8080" \
     -e NO_PROXY="localhost,127.0.0.1" \
     alpine:3.21 sh -c "echo Proxy configured: \$HTTP_PROXY"
   ```
</details>

<details>
<summary><strong>Exercise 3: Installing Custom CA Certificates in Debian Slim</strong></summary>

**Goal:** Test certificate installation commands in a disposable container.

**Step-by-step Solution:**
1. Run interactive container:
   ```bash
   docker run --rm -it debian:bookworm-slim bash
   ```
2. Inside container:
   ```bash
   apt-get update && apt-get install -y ca-certificates
   touch /usr/local/share/ca-certificates/mock-corp.crt
   update-ca-certificates
   # Outputs: 1 added, 0 removed; done.
   ```
</details>

<details>
<summary><strong>Exercise 4: Verifying IPv6 Local Loopback Inside Container</strong></summary>

**Goal:** Verify container processes can bind and listen on `::1`.

**Step-by-step Solution:**
1. Run Alpine container:
   ```bash
   docker run --rm -it alpine:3.21 ping6 -c 2 ::1
   # Success!
   ```
</details>

<details>
<summary><strong>Exercise 5: Inspecting Systemd Docker Daemon Proxy Config</strong></summary>

**Goal:** Verify whether your host Docker daemon currently has systemd proxy settings.

**Step-by-step Solution:**
1. Query systemd:
   ```bash
   systemctl show --property=Environment docker
   ```
</details>

<details>
<summary><strong>Exercise 6: Configuring Client Proxy in `~/.docker/config.json`</strong></summary>

**Goal:** Validate client configuration schema.

**Step-by-step Solution:**
1. Inspect or create `~/.docker/config.json`:
   ```bash
   cat ~/.docker/config.json
   ```
</details>

---

## 15. Summary & Reference Cheat Sheet

### Configuration Locations
- **Docker Daemon Proxy**: `/etc/systemd/system/docker.service.d/http-proxy.conf`
- **Docker Client Proxy**: `~/.docker/config.json`
- **IPv6 Daemon Config**: `/etc/docker/daemon.json` (`"ipv6": true, "ip6tables": true`)

### Golden Rules
1. **Always configure `NO_PROXY`** with `localhost,127.0.0.1` and internal network subnets.
2. **Never commit proxy passwords** to Dockerfiles or Git repositories.
3. **Always set `"ip6tables": true`** when enabling IPv6 to avoid firewall bypass.
4. **Inject corporate root CAs** cleanly into `/usr/local/share/ca-certificates`.

---

[← Previous: 07-container-to-host-and-container-to-container.md](07-container-to-host-and-container-to-container.md) | [Next: 09-network-troubleshooting.md →](09-network-troubleshooting.md)
