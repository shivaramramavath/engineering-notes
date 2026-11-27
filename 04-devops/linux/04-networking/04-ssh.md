# Secure Shell (SSH): Architecture, Configuration, and Hardening

| Metadata | Details |
| :--- | :--- |
| **Topic** | OpenSSH Server and Client, Cryptography, Tunnels, ProxyJump, and Hardening |
| **Module** | `04-networking` |
| **Track** | `04-devops/linux` |
| **Prerequisites** | `01-fundamentals/05-permissions.md`, `04-networking/01-networking-basics.md` |
| **Target Audience** | SREs, Systems Administrators, Security Engineers, DevOps Engineers |
| **Target Distribution** | Ubuntu 24.04 LTS (OpenSSH 9.6+), Rocky Linux / RHEL 9.4 |
| **Key Tools / Files** | `ssh`, `sshd`, `ssh-keygen`, `ssh-copy-id`, `ssh-agent`, `ssh-add`, `~/.ssh/config`, `/etc/ssh/sshd_config` |

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

Secure Shell (SSH / RFC 4251–4254) is the ubiquitous protocol for remote Linux system administration, automated continuous deployment (CD) runners, and encrypted network tunneling.

SSH replaces insecure legacy remote protocols (Telnet, rlogin, rsh) by encrypting traffic, authenticating remote servers against man-in-the-middle attacks, and validating client identity via public-key cryptography. In modern cloud architecture:
- Bastion hosts and jump boxes mediate access to private VPC networks via **ProxyJump**.
- Encrypted **Local, Remote, and Dynamic (SOCKS5) port-forwarding tunnels** bypass restrictive corporate firewalls to access internal telemetry, monitoring UIs, and databases securely.
- Strict **`sshd_config` hardening** (prohibiting root logins, disabling password authentication, enforcing modern elliptic-curve cryptography) forms the primary line of defense against continuous internet botnet brute-force scans.

---

## Core Concepts

### Cryptographic Algorithms: Ed25519 vs RSA
Modern OpenSSH supports multiple public-key algorithms:
1. **Ed25519 (Recommended Standard)**:
   - Based on Edwards-curve Digital Signature Algorithm (EdDSA) using Curve25519.
   - Fixed 256-bit key length, equivalent to ~3000-bit RSA in cryptographic strength.
   - Extremely fast key generation, signature computation, and resistance to side-channel attacks.
2. **RSA (Legacy / Deprecated for < 3072-bit)**:
   - Factorization-based public-key algorithm. Minimum acceptable size is **3072 or 4096 bits**.
   - SHA-1 signatures for RSA (`ssh-rsa`) are disabled by default in modern OpenSSH (8.8+) due to collision vulnerabilities; modern RSA must use `rsa-sha2-256` or `rsa-sha2-512`.
3. **ECDSA**:
   - Elliptic Curve DSA (NIST curves P-256, P-384). Suspected of theoretical NIST backdoors; widely avoided in favor of Ed25519.

### Key Management Files
- `~/.ssh/id_ed25519`: Private key (Permissions **MUST** be `600` / `-rw-------`). Never leaves the client machine.
- `~/.ssh/id_ed25519.pub`: Public key (Permissions `644`). Uploaded to target hosts.
- `~/.ssh/authorized_keys`: File on the remote server containing public keys allowed to log in as that user.
- `~/.ssh/known_hosts`: Client file storing remote server public host keys to prevent Man-in-the-Middle (MitM) attacks.
- `~/.ssh/config`: Client-side configuration file for aliasing hosts, setting port numbers, specifying identities, and chaining jump hosts.

### Port Forwarding Paradigms
1. **Local Port Forwarding (`-L local_port:target_host:target_port`)**:  
   Opens a port on the client machine and tunnels connections through the SSH server to a destination reachable by the server (e.g., accessing an internal database).
2. **Remote / Reverse Port Forwarding (`-R remote_port:target_host:target_port`)**:  
   Opens a port on the remote SSH server and tunnels inbound connections back to a service running on the local client machine.
3. **Dynamic Port Forwarding (`-D local_port`)**:  
   Turns the SSH client into a local **SOCKS5 proxy**. Any browser or tool configured to use the proxy routes all its traffic through the encrypted SSH tunnel.

---

## Internal Working

```
+-----------------------------------------------------------------------------------------+
|                                  SSH Protocol Connection Flow                           |
+-----------------------------------------------------------------------------------------+
       Client                                                             Server
         |                                                                   |
         |------------------- 1. TCP 3-Way Handshake (Port 22) -------------->|
         |                                                                   |
         |<------------------ 2. Protocol Version Exchange ----------------->|
         |                    "SSH-2.0-OpenSSH_9.6p1"                        |
         |                                                                   |
         |<--- 3. Key Exchange (Diffie-Hellman / Curve25519-SHA256) -------->|
         |     Server presents Host Public Key.                              |
         |     Both sides derive Shared Secret & Session Keys (AES-GCM).     |
         |     (Client verifies Host Key against ~/.ssh/known_hosts)         |
         |                                                                   |
         |==================== Encrypted Tunnel Established =================|
         |                                                                   |
         |------------------- 4. Authentication Request -------------------->|
         |     Client proves possession of Private Key by signing a nonce.   |
         |     Server validates signature against ~/.ssh/authorized_keys.    |
         |<------------------ Authentication Successful --------------------|
         |                                                                   |
         |------------------- 5. Session Multiplexing ---------------------->|
         |     Requests PTY (Interactive Shell), SFTP, or Port Forwarding.   |
         |                                                                   |
```

### Bastion / ProxyJump Architecture
In cloud environments, backend database servers reside in private subnets with no public IP. The client reaches them through a Bastion Host:

```
+-------------------+             +-----------------------+             +-----------------------+
|   Local Client    | === SSH ===>|  Bastion Host (DMZ)   | === SSH ===>|  Private DB (10.0.1.5)|
| (Desktop / CI-CD) |             | (Public Elastic IP)   |             | (Private Subnet Only) |
+-------------------+             +-----------------------+             +-----------------------+
         |                                                                          ^
         +=========================== End-to-End Encrypted Tunnel ==================+
```
With `ProxyJump` (`-J`), the client establishes an SSH tunnel to the Bastion, and then opens an encrypted channel directly to the private target. The Bastion host **cannot decrypt** the inner payload.

---

## Syntax & CLI Tools

### Key Generation and Installation
```bash
# Generate modern Ed25519 keypair with an optional descriptive comment
ssh-keygen -t ed25519 -C "admin@company.com"

# Generate 4096-bit RSA keypair (legacy compatibility)
ssh-keygen -t rsa -b 4096 -C "legacy@company.com"

# Copy public key to remote host securely
ssh-copy-id -i ~/.ssh/id_ed25519.pub user@remote-host.example.com

# Change or add passphrase to an existing private key
ssh-keygen -p -f ~/.ssh/id_ed25519
```

### Essential SSH Client Flags
```bash
# Basic connection
ssh user@remote-host

# Specify custom port (-p) and identity file (-i)
ssh -p 2222 -i ~/.ssh/custom_key user@remote-host

# Jump host (-J)
ssh -J bastion.corp.com:22 user@10.0.2.50

# Verbose debugging (-v, -vv, -vvv)
ssh -vvv user@remote-host

# Execute remote command without opening an interactive shell
ssh user@remote-host "uptime && df -h"

# Allocate pseudo-terminal forcibly (useful when chaining sudo)
ssh -t user@remote-host "sudo systemctl restart nginx"
```

### Port Forwarding Syntax
```bash
# Local Forwarding: Forward local port 5432 to remote database 10.0.1.5:5432
ssh -L 5432:10.0.1.5:5432 -N -f user@bastion.corp.com
# (-N = do not execute remote command, -f = go to background)

# Remote Forwarding: Expose local port 8080 on remote server as port 9090
ssh -R 9090:localhost:8080 -N -f user@public-vps.com

# Dynamic SOCKS5 Proxy on local port 1080
ssh -D 1080 -N -f user@bastion.corp.com
```

---

## Progressive Practical Examples

### Example 1: Crafting an Advanced `~/.ssh/config` File
Eliminate complex command-line flags by organizing hosts cleanly in `~/.ssh/config`.

```text
# Global defaults applied to all hosts
Host *
    ServerAliveInterval 30
    ServerAliveCountMax 3
    IdentitiesOnly yes
    AddKeysToAgent yes
    Compression yes

# Bastion / Jump Host
Host bastion
    HostName bastion.corp.internal
    User ubuntu
    Port 2222
    IdentityFile ~/.ssh/id_ed25519

# Private Production Database (automatically routed via Bastion)
Host db-prod
    HostName 10.0.2.100
    User postgres
    IdentityFile ~/.ssh/id_prod_db
    ProxyJump bastion

# Wildcard pattern for all staging VMs
Host staging-*
    User devops
    IdentityFile ~/.ssh/id_staging
    ProxyJump bastion
```
With this configuration, accessing the private database is as simple as:
```bash
ssh db-prod
```

### Example 2: Managing Passphrase-Protected Keys with `ssh-agent`
Typing passphrases repeatedly breaks automation. `ssh-agent` caches decrypted private keys in memory.

```bash
# 1. Start agent in the current shell session
eval "$(ssh-agent -s)"

# 2. Add key to agent (prompts for passphrase once)
ssh-add ~/.ssh/id_ed25519

# 3. Add key with a 4-hour automatic expiration time
ssh-add -t 4h ~/.ssh/id_ed25519

# 4. List keys currently loaded in agent
ssh-add -l

# 5. Delete all keys from agent on screen lock or logout
ssh-add -D
```

### Example 3: Production Server Hardening (`/etc/ssh/sshd_config.d/`)
Modern distributions prefer modular drop-in configurations under `/etc/ssh/sshd_config.d/`.

```bash
sudo tee /etc/ssh/sshd_config.d/99-hardened.conf << 'EOF'
# Enforce modern protocol
Port 22
Protocol 2

# Disable password authentication completely (keys only)
PasswordAuthentication no
KbdInteractiveAuthentication no
PermitEmptyPasswords no

# Disable root login
PermitRootLogin no

# Cryptographic Algorithm Hardening
KexAlgorithms curve25519-sha256,curve25519-sha256@libssh.org,diffie-hellman-group16-sha512
Ciphers chacha20-poly1305@openssh.com,aes256-gcm@openssh.com
MACs hmac-sha2-512-etm@openssh.com

# Connection Limits and Session Management
MaxAuthTries 3
MaxSessions 5
LoginGraceTime 30
ClientAliveInterval 300
ClientAliveCountMax 2

# Disable insecure legacy features
X11Forwarding no
AllowAgentForwarding no
PermitUserEnvironment no

# Restrict allowed users
AllowGroups ssh-users sudo
EOF

# Test syntax before restarting daemon!
sudo sshd -t

# Reload ssh service safely
sudo systemctl reload ssh
```

### Example 4: Creating a Reverse Tunnel to Expose a Local Webhook / Dev Server
Expose a local development server running on `localhost:3000` to a public VPS for webhook testing.

```bash
# On local developer laptop:
ssh -R 0.0.0.0:8080:localhost:3000 user@my-vps.com -N

# (Requires `GatewayPorts yes` in /etc/ssh/sshd_config on my-vps.com if binding to 0.0.0.0)
```

### Example 5: Non-Interactive Automation via SSH Keys in CI/CD Runners
Best-practice snippet for using SSH inside GitHub Actions or GitLab CI runners without interactive prompts.

```bash
#!/usr/bin/env bash
set -euo pipefail

# Setup temporary SSH runtime environment
mkdir -p ~/.ssh
chmod 700 ~/.ssh

# Write private key from secret environment variable
echo "${DEPLOY_PRIVATE_KEY}" > ~/.ssh/deploy_key
chmod 600 ~/.ssh/deploy_key

# Scan and append remote host fingerprint to avoid interactive prompt
ssh-keyscan -H -p 22 prod.company.com >> ~/.ssh/known_hosts
chmod 644 ~/.ssh/known_hosts

# Execute deployment command non-interactively
ssh -i ~/.ssh/deploy_key \
    -o BatchMode=yes \
    -o StrictHostKeyChecking=yes \
    -o ConnectTimeout=10 \
    deploy@prod.company.com "docker compose pull && docker compose up -d"
```

---

## Best Practices

1. **Enforce `ed25519` Everywhere**: Generate all new keys with `ssh-keygen -t ed25519`. Deprecate RSA keys under 3072-bit and eradicate DSA/ECDSA legacy keys.
2. **Never Disable `StrictHostKeyChecking` Globally**: Setting `StrictHostKeyChecking no` in `Host *` eliminates Man-in-the-Middle protection entirely. If automating dynamic cloud nodes, limit `StrictHostKeyChecking accept-new` to explicit IP CIDRs.
3. **Always Run `sshd -t` Before Restarting SSHD**: `sshd -t` tests configuration syntax. Restarting `sshd` with a syntax error will kill the daemon and lock you out of a remote server permanently.
4. **Always Keep an Active Backup Session When Modifying `sshd_config`**: When editing `/etc/ssh/sshd_config`, never close your current working terminal. Open a secondary terminal window and test logging in before disconnecting the primary session.
5. **Set Exact Permissions on `~/.ssh` Directories**:
   - `~/.ssh/`: `700` (`drwx------`)
   - `~/.ssh/authorized_keys`: `600` (`-rw-------`)
   - `~/.ssh/id_*` (private keys): `600` (`-rw-------`)
   - `~/.ssh/*.pub` (public keys): `644` (`-rw-r--r--`)
   - `~/.ssh/config`: `600` (`-rw-------`)
6. **Use `ProxyJump` Rather than Agent Forwarding (`-A`)**: SSH Agent Forwarding (`ssh -A`) exposes your local agent socket to the intermediate jump host. If the jump host is compromised, a root attacker can hijack your socket to authenticate to other servers. `ProxyJump` (`-J`) establishes a direct TCP proxy without exposing key credentials to the intermediary.

---

## Common Mistakes & Gotchas

| Mistake | Why It Fails | Correct Pattern |
| :--- | :--- | :--- |
| Relaxed permissions on `~/.ssh` or `authorized_keys` (e.g. `chmod 777`) | OpenSSH `StrictModes` rejects keys if files or home directory are writable by others. | Run `chmod 700 ~/.ssh && chmod 600 ~/.ssh/* && chmod 644 ~/.ssh/*.pub`. |
| Editing `/etc/ssh/sshd_config` and restarting without `sshd -t` | Syntax errors prevent `sshd` from starting, locking out all administrators. | Always validate with `sudo sshd -t` before `sudo systemctl reload ssh`. |
| Using `ssh -A` (Agent Forwarding) routinely through shared jump boxes | Malicious root users on the jump box can access `/tmp/ssh-*/agent.*` and hijack your identity. | Use `ProxyJump` (`ssh -J bastion target`) instead. |
| Forgetting `GatewayPorts yes` when using `ssh -R` | By default, remote forwarded ports bind strictly to `127.0.0.1` on the remote server, blocking external access. | Enable `GatewayPorts yes` or `clientspecified` in `sshd_config`. |
| Leaving `PasswordAuthentication yes` active on internet-exposed servers | Leaves port 22 vulnerable to brute-force credential stuffing botnets. | Set `PasswordAuthentication no` and enforce SSH key authentication. |
| Trying to use legacy RSA keys with SHA-1 on Ubuntu 24.04 | OpenSSH 8.8+ disables `ssh-rsa` by default; connection fails with `no mutual signature algorithm`. | Upgrade keys to `ed25519` or force `PubkeyAcceptedKeyTypes +ssh-rsa` temporarily. |

---

## Performance, Security & Cost Implications

### Performance
- **Multiplexing with `ControlMaster`**: In deployment scripts that execute 20 sequential commands over SSH, initiating 20 individual handshakes causes significant delay. Enabling `ControlMaster` in `~/.ssh/config` reuses a single persistent TCP connection:
  ```text
  Host *
      ControlMaster auto
      ControlPath ~/.ssh/control-%r@%h:%p
      ControlPersist 10m
  ```
- **Cipher Benchmarks**: `chacha20-poly1305` and `aes128-gcm` leverage hardware CPU acceleration (AES-NI), delivering gigabit-speed throughput with negligible CPU overhead.

### Security
- **Defense-in-Depth with `fail2ban`**: Pair hardened `sshd_config` with `fail2ban` to automatically drop IP addresses exhibiting repeated failed connection attempts via `nftables`.
- **SSH Certificates**: In enterprise fleets (1000+ nodes), managing static `authorized_keys` becomes unmanageable. SSH Certificates (issued by an internal CA, e.g. HashiCorp Vault or Smallstep) grant short-lived (e.g., 8-hour) cryptographically signed access tokens.

### Cost
- Implementing SSH Bastions with ProxyJump allows private cloud instances to run without costly Public Elastic IPs or NAT Gateways for inbound administration, reducing monthly cloud IP reservation fees.

---

## Comparison Table / Alternatives

| Feature | Direct SSH (`ssh host`) | SSH with ProxyJump (`-J`) | SSH Agent Forwarding (`-A`) | VPN (WireGuard / OpenVPN) |
| :--- | :--- | :--- | :--- | :--- |
| **Network Reach** | Requires Public IP or Direct Route | Accesses private nodes via DMZ | Accesses private nodes via DMZ | Full Layer 3 subnet access |
| **Credential Safety** | High (keys stay on client) | High (end-to-end encrypted) | **Low** (socket exposed on intermediary) | High (kernel cryptographic auth) |
| **Setup Complexity**| None | Minimal (1 line in `~/.ssh/config`) | Flag only (`-A`) | High (requires IP routing & tunnels) |
| **Target Audience** | Standalone Public Servers | Cloud VPC Bastion Architecture | Legacy workflows (discouraged) | Enterprise network interconnect |

---

## Real-World Troubleshooting & Interview Q&A

### Q1: Why does `ssh user@server` fail with `Permission denied (publickey)` despite adding the key to `authorized_keys`?
<details>
<summary>View Solution</summary>

**Troubleshooting Checklist**:
1. **Permissions (`StrictModes`)**: OpenSSH refuses keys if permissions are too permissive:
   - Remote home directory (`~`): Must **not** be writable by group/others (`chmod 755 ~` or `700 ~`).
   - `~/.ssh`: Must be `700` (`chmod 700 ~/.ssh`).
   - `~/.ssh/authorized_keys`: Must be `600` (`chmod 600 ~/.ssh/authorized_keys`).
2. **SELinux Contexts (RHEL/Rocky)**: If the `.ssh` folder was copied manually, the SELinux label might be incorrect. Restore with: `restorecon -R -v ~/.ssh`.
3. **SSHD Configuration**: Verify `/etc/ssh/sshd_config` has `PubkeyAuthentication yes` enabled.
4. **Debug**: Run `ssh -vvv user@server` on the client and inspect `/var/log/auth.log` or `journalctl -u ssh` on the server.
</details>

### Q2: What causes `WARNING: REMOTE HOST IDENTIFICATION HAS CHANGED!` and how should it be handled?
<details>
<summary>View Solution</summary>

**Meaning**: The public key presented by the remote server does not match the key stored in the client's `~/.ssh/known_hosts` for that IP/hostname.

**Causes**:
1. The server was legitimately reinstalled or replaced (new host keys generated).
2. A cloud instance was terminated and a new instance acquired the recycled IP address.
3. **A Man-in-the-Middle (MitM) attack is actively occurring.**

**Resolution**:
- If verified legitimate: Remove the stale entry using `ssh-keygen -R <hostname_or_ip>` and reconnect to accept the new fingerprint.
- If unverified: Halt immediately and verify the host key fingerprint out-of-band via cloud console (`cat /etc/ssh/ssh_host_ed25519_key.pub`).
</details>

### Q3: What is the difference between Local Port Forwarding (`-L`) and Remote Port Forwarding (`-R`)?
<details>
<summary>View Solution</summary>

- **Local Forwarding (`-L local_port:remote_ip:remote_port`)**:  
  Binds a port on the **client**. Traffic sent to `localhost:local_port` on your local laptop travels through the SSH tunnel and is routed by the SSH server to `remote_ip:remote_port`. Used to access remote private databases or web panels from your laptop.
- **Remote Forwarding (`-R remote_port:local_ip:local_port`)**:  
  Binds a port on the **remote SSH server**. Traffic sent to that port on the server travels backwards through the SSH tunnel to your local machine. Used to expose a local service (e.g., local webhook development) to the remote server or public internet.
</details>

### Q4: Why is `ProxyJump` (`-J`) preferred over SSH Agent Forwarding (`-A`)?
<details>
<summary>View Solution</summary>

With Agent Forwarding (`ssh -A`), your local `ssh-agent` Unix domain socket is forwarded and mounted in `/tmp/ssh-*/agent.*` on the intermediate jump host. If an attacker has root privileges on that jump host, they can access your forwarded socket and authenticate as you to any other server in your infrastructure without knowing your private key passphrase.

`ProxyJump` (`ssh -J`) uses `nc` or the built-in `-W` stdio forwarder to establish an encrypted Layer 4 TCP proxy tunnel through the jump host. Authentication to the destination host occurs directly between your client and the target; the jump host never sees your agent socket or private credentials.
</details>

### Q5: How do you gracefully reload OpenSSH daemon without dropping active administrator sessions?
<details>
<summary>View Solution</summary>

Run:
```bash
sudo sshd -t && sudo systemctl reload ssh
```
(On RHEL/CentOS: `sudo systemctl reload sshd`).
`reload` sends `SIGHUP` to the master `sshd` process, causing it to re-read configuration files for new incoming connections while leaving all currently connected child worker processes and active sessions completely uninterrupted.
</details>

### Q6: What does `IdentitiesOnly yes` do in `~/.ssh/config`?
<details>
<summary>View Solution</summary>

By default, the SSH client attempts authentication using all keys loaded in `ssh-agent` plus all standard default files (`~/.ssh/id_rsa`, `~/.ssh/id_ed25519`, etc.) before trying the key specified for the host. 

If you have more than 5 or 6 keys loaded in your agent, remote servers with `MaxAuthTries 6` will disconnect you with `Too many authentication failures` before you can present the correct key. `IdentitiesOnly yes` forces SSH to offer **only** the key specified via `-i` or `IdentityFile` in `~/.ssh/config`.
</details>

### Q7: How does SSH keep-alive prevent connections from dropping during idle periods?
<details>
<summary>View Solution</summary>

Stateful NAT firewalls and cloud routers drop idle TCP connection state tracking entries after 60–300 seconds of inactivity. To prevent drops, configure keep-alive packets:
- **Client-Side**:
  ```text
  ServerAliveInterval 30
  ServerAliveCountMax 3
  ```
  Client sends an encrypted null packet every 30 seconds. If 3 consecutive probes fail, it closes the dead socket cleanly.
- **Server-Side**:
  ```text
  ClientAliveInterval 60
  ClientAliveCountMax 3
  ```
</details>

### Q8: What are SSH Certificates and why are they superior to static `authorized_keys`?
<details>
<summary>View Solution</summary>

Static `authorized_keys` require pushing public keys to every target server and revoking them manually when an employee departs.

**SSH Certificates**:
- A centralized Certificate Authority (CA) signs a user's public key, embedding metadata: username, permitted principals (`ubuntu`, `root`), valid IP ranges, and an **expiration timestamp** (e.g., valid for 8 hours).
- Servers are configured once with `TrustedUserCAKeys /etc/ssh/ca.pub`.
- The server validates the cryptographic signature of the certificate against the CA public key; no user public keys ever need to be uploaded to individual servers, and expired certificates automatically stop working.
</details>

---

## Hands-On Practice Exercises

### Exercise 1: Generating and Inspecting an Ed25519 Keypair
**Task**: Generate a test Ed25519 keypair named `test_ed25519`. Inspect its cryptographic fingerprint and public key comments.
<details>
<summary>View Solution</summary>

```bash
# 1. Generate keypair without passphrase for testing
ssh-keygen -t ed25519 -f /tmp/test_ed25519 -C "test-key-2026" -N ""

# 2. Inspect private and public key files
ls -la /tmp/test_ed25519*

# 3. View SHA256 fingerprint
ssh-keygen -lf /tmp/test_ed25519.pub

# 4. Clean up
rm -f /tmp/test_ed25519 /tmp/test_ed25519.pub
```
</details>

### Exercise 2: Setting Up Local Port Forwarding Tunnel
**Task**: Forward local port `8888` through SSH loopback to connect to a local Python HTTP server on port `8000`.
<details>
<summary>View Solution</summary>

```bash
# 1. Start dummy HTTP server on port 8000
python3 -m http.server 8000 &
HTTP_PID=$!

# 2. Establish local port forwarding tunnel via SSH
ssh -L 8888:127.0.0.1:8000 -N -f 127.0.0.1

# 3. Test reaching port 8888
curl -I http://127.0.0.1:8888

# 4. Kill tunnel and dummy server
kill "${HTTP_PID}"
pkill -f "ssh -L 8888"
```
</details>

### Exercise 3: Auditing Remote Host Key Fingerprints
**Task**: Use `ssh-keyscan` to fetch the public host keys of `github.com`. Format and inspect their cryptographic fingerprints.
<details>
<summary>View Solution</summary>

```bash
# Fetch and inspect fingerprints
ssh-keyscan -t ed25519 github.com 2>/dev/null | ssh-keygen -lf -
# Output displays: 256 SHA256:+DiY3wvvV6TuJJhbpZisF/zLDA0zPMSvHdkr4UvCOqU github.com (ED25519)
```
</details>

### Exercise 4: Enforcing Strict File Permissions on `~/.ssh`
**Task**: Write an idempotent Bash script that inspects and enforces correct security permissions across a user's `~/.ssh` directory.
<details>
<summary>View Solution</summary>

```bash
#!/usr/bin/env bash
set -euo pipefail

SSH_DIR="${HOME}/.ssh"

echo "==> Enforcing SSH permission hardening on ${SSH_DIR}..."
mkdir -p "${SSH_DIR}"
chmod 700 "${SSH_DIR}"

# Hardening files
[ -f "${SSH_DIR}/authorized_keys" ] && chmod 600 "${SSH_DIR}/authorized_keys"
[ -f "${SSH_DIR}/config" ] && chmod 600 "${SSH_DIR}/config"
[ -f "${SSH_DIR}/known_hosts" ] && chmod 644 "${SSH_DIR}/known_hosts"

# Private keys: 600, Public keys: 644
find "${SSH_DIR}" -type f -name "id_*" ! -name "*.pub" -exec chmod 600 {} +
find "${SSH_DIR}" -type f -name "*.pub" -exec chmod 644 {} +

echo "Permissions hardened successfully."
ls -la "${SSH_DIR}"
```
</details>

### Exercise 5: Validating SSH Daemon Configuration
**Task**: Test the syntax of your local `/etc/ssh/sshd_config`. Run `sshd -T` to view the active evaluated configuration values.
<details>
<summary>View Solution</summary>

```bash
# 1. Syntax test
sudo sshd -t && echo "Configuration syntax OK"

# 2. View evaluated configuration parameters
sudo sshd -T | grep -E "passwordauthentication|permitrootlogin|port|pubkeyauthentication"
```
</details>

### Exercise 6: Testing SSH Connectivity with Verbose Debugging
**Task**: Connect to localhost or a remote server using `-vvv`. Identify where the key exchange occurs and which cipher is negotiated.
<details>
<summary>View Solution</summary>

```bash
ssh -vvv -o BatchMode=yes -o ConnectTimeout=5 localhost exit 2>&1 | grep -E "kex|cipher|Authenticated"
```
</details>

---

## Summary Cheat Sheet & Further Reading

### Command Reference
| Operation | Command Example |
| :--- | :--- |
| **Generate Ed25519 Key** | `ssh-keygen -t ed25519 -C "name@work"` |
| **Copy Key to Server** | `ssh-copy-id -i ~/.ssh/id_ed25519.pub user@server` |
| **Connect via Jump Host** | `ssh -J bastion.corp.com user@10.0.1.5` |
| **Local Port Forwarding** | `ssh -L 5432:db.internal:5432 -N -f user@bastion` |
| **Dynamic SOCKS5 Proxy** | `ssh -D 1080 -N -f user@bastion` |
| **Start Agent & Add Key** | `eval "$(ssh-agent)" && ssh-add ~/.ssh/id_ed25519` |
| **Test SSHD Config Syntax**| `sudo sshd -t` |
| **Reload SSHD Service** | `sudo systemctl reload ssh` |
| **Remove Known Host Key** | `ssh-keygen -R <hostname_or_ip>` |

### Further Reading
- `man 1 ssh` and `man 5 ssh_config`
- `man 8 sshd` and `man 5 sshd_config`
- `man 1 ssh-keygen` and `man 1 ssh-agent`
- [OpenSSH Official Specifications & Security](https://www.openssh.com/)
- [Mozilla OpenSSH Server Security Guidelines](https://infosec.mozilla.org/guidelines/openssh)
- [RFC 4253: The Secure Shell (SSH) Transport Layer Protocol](https://datatracker.ietf.org/doc/html/rfc4253)
