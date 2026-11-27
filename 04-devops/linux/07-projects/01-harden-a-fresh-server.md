# Project 01: Harden a Fresh Cloud Server to CIS Standards

| Metadata | Details |
| :--- | :--- |
| **Project** | Production Server Baseline Hardening |
| **Module** | `07-projects` — Production Reference Projects & Labs |
| **Track** | `04-devops/linux` |
| **Target OS** | Ubuntu 24.04 LTS, RHEL 9.4 / Rocky Linux 9.4 |
| **Complexity** | Intermediate |
| **Estimated Duration**| 45–60 minutes |

---

## 1. Architecture & Design

### Scenario Context
Cloud compute instances provisioned on AWS EC2, GCP Compute Engine, or bare-metal providers often start with overly permissive defaults: standard SSH on port 22 with password authentication permitted, unconfigured firewalls, permissive kernel network parameters (accepting ICMP redirects or IP source routing), and absent intrusion detection.

This project delivers an idempotent, automated bash hardening suite aligned with CIS (Center for Internet Security) Linux Benchmark Level 1 recommendations.

### Operational & Threat Model
```
+-------------------------------------------------------------------------+
| Threat Vectors: Internet Attacker vs. Hardened Host                     |
|                                                                         |
|  [ Internet Threats ]                                                   |
|  - SSH Brute-Force & Botnet Scans                                       |
|  - IP Spoofing, ICMP Redirect Attacks, SYN Flood DoS                    |
|  - Unprivileged Privilege Escalation via SUID Binaries                  |
|                                                                         |
|                                  |                                      |
|                                  v                                      |
|  +-------------------------------------------------------------------+  |
|  | HARDENED LINUX HOST                                               |  |
|  |                                                                   |  |
|  | 1. Network Perimeter: UFW / Nftables Firewall                     |  |
|  |    Default DROP Incoming; Allow Only Port 2222 (Rate-Limited)     |  |
|  |                                                                   |  |
|  | 2. Remote Access: OpenSSH Daemon Lockdown                         |  |
|  |    Port 2222 | Ed25519 Keys Only | PasswordAuthentication NO      |  |
|  |    PermitRootLogin NO | MaxAuthTries 3 | ClientAliveInterval 300  |  |
|  |                                                                   |  |
|  | 3. Intrusion Prevention: Fail2ban (Nftables Multiport)            |  |
|  |    Bans abusive IPs for 1 hour after 5 failed attempts           |  |
|  |                                                                   |  |
|  | 4. Kernel Protection: Hardened /etc/sysctl.d/99-security.conf      |  |
|  |    Disable IP Forwarding, Ignore ICMP Redirects, SYN Cookies      |  |
|  |                                                                   |  |
|  | 5. System Integrity: Unattended Upgrades & SUID Audit             |  |
|  |    Automatic security CVE patching + auditd tracking              |  |
|  +-------------------------------------------------------------------+  |
+-------------------------------------------------------------------------+
```

---

## 2. Implementation Artifacts

### Artifact 1: Comprehensive Hardening Script (`harden-server.sh`)

```bash
#!/usr/bin/env bash
# ==============================================================================
# Script: harden-server.sh
# Purpose: Idempotent CIS Level-1 Server Hardening Automation
# Supported: Ubuntu 24.04 LTS / Debian 12
# ==============================================================================

set -euo pipefail

NEW_SSH_PORT=2222
ADMIN_USER="deploy"

log() { echo -e "\033[1;32m[+] $*\033[0m"; }
warn() { echo -e "\033[1;33m[!] $*\033[0m"; }
error() { echo -e "\033[1;31m[-] $*\033[0m" >&2; exit 1; }

# 1. Root & OS Validation
[[ "${EUID}" -eq 0 ]] || error "This script must be executed as root (or with sudo)."
source /etc/os-release
[[ "${ID}" =~ ^(ubuntu|debian)$ ]] || error "Unsupported OS: ${ID}. Script designed for Debian/Ubuntu."

log "Phase 1: Updating System Packages..."
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get upgrade -y -qq
apt-get install -y -qq ufw fail2ban unattended-upgrades auditd libpam-pwquality curl sudo

log "Phase 2: Creating Dedicated Deploy / Admin User..."
if ! id "${ADMIN_USER}" >/dev/null 2>&1; then
    useradd -m -s /bin/bash -G sudo "${ADMIN_USER}"
    log "User '${ADMIN_USER}' created. Please set an authorized SSH key in ~${ADMIN_USER}/.ssh/authorized_keys."
fi

# Ensure sudoers drop-in is safe
echo "${ADMIN_USER} ALL=(ALL) NOPASSWD:ALL" > "/etc/sudoers.d/90-${ADMIN_USER}-init"
chmod 0440 "/etc/sudoers.d/90-${ADMIN_USER}-init"
visudo -c -f "/etc/sudoers.d/90-${ADMIN_USER}-init"

log "Phase 3: Hardening OpenSSH Server..."
SSH_CONF_DROPIN="/etc/ssh/sshd_config.d/99-hardened.conf"
cat << EOF > "${SSH_CONF_DROPIN}"
# CIS Level 1 SSH Configuration
Port ${NEW_SSH_PORT}
PermitRootLogin no
PasswordAuthentication no
PubkeyAuthentication yes
X11Forwarding no
MaxAuthTries 3
MaxSessions 4
ClientAliveInterval 300
ClientAliveCountMax 2
AllowAgentForwarding no
KexAlgorithms curve25519-sha256,curve25519-sha256@libssh.org,diffie-hellman-group16-sha512
Ciphers chacha20-poly1305@openssh.com,aes256-gcm@openssh.com
MACs hmac-sha2-512-etm@openssh.com
EOF
chmod 0600 "${SSH_CONF_DROPIN}"

# Validate sshd configuration before reloading
sshd -t || error "sshd_config syntax check failed! Aborting reload."
systemctl restart ssh || systemctl restart sshd

log "Phase 4: Configuring Kernel Hardening Parameters (sysctl)..."
SYSCTL_CONF="/etc/sysctl.d/99-cis-hardening.conf"
cat << 'EOF' > "${SYSCTL_CONF}"
# IP Spoofing & Redirection Protection
net.ipv4.ip_forward = 0
net.ipv4.conf.all.send_redirects = 0
net.ipv4.conf.default.send_redirects = 0
net.ipv4.conf.all.accept_redirects = 0
net.ipv4.conf.default.accept_redirects = 0
net.ipv4.conf.all.secure_redirects = 0
net.ipv4.conf.default.secure_redirects = 0
net.ipv6.conf.all.accept_redirects = 0
net.ipv6.conf.default.accept_redirects = 0

# Reverse Path Filtering (Spoof Prevention)
net.ipv4.conf.all.rp_filter = 1
net.ipv4.conf.default.rp_filter = 1

# Ignore ICMP Broadcasts & Bogus Errors
net.ipv4.icmp_echo_ignore_broadcasts = 1
net.ipv4.icmp_ignore_bogus_error_responses = 1

# SYN Flood Mitigation
net.ipv4.tcp_syncookies = 1
net.ipv4.tcp_max_syn_backlog = 4096
net.ipv4.tcp_synack_retries = 2

# Memory & Kernel Security
kernel.randomize_va_space = 2
kernel.kptr_restrict = 2
kernel.dmesg_restrict = 1
fs.protected_hardlinks = 1
fs.protected_symlinks = 1
fs.protected_fifos = 2
fs.protected_regular = 2
EOF
sysctl --system >/dev/null

log "Phase 5: Configuring Host Firewall (UFW)..."
ufw default deny incoming
ufw default allow outgoing
ufw allow "${NEW_SSH_PORT}/tcp" comment "Hardened SSH Port"
ufw --force enable

log "Phase 6: Configuring Fail2ban..."
cat << EOF > /etc/fail2ban/jail.d/sshd.local
[sshd]
enabled = true
port = ${NEW_SSH_PORT}
filter = sshd
backend = systemd
maxretry = 5
findtime = 600
bantime = 3600
banaction = nftables-multiport
EOF
systemctl enable --now fail2ban
systemctl restart fail2ban

log "Phase 7: Configuring Automatic Security Updates..."
cat << 'EOF' > /etc/apt/apt.conf.d/20auto-upgrades
APT::Periodic::Update-Package-Lists "1";
APT::Periodic::Unattended-Upgrade "1";
APT::Periodic::AutocleanInterval "7";
EOF
systemctl enable --now unattended-upgrades

log "Phase 8: Disabling Legacy & Unused Filesystems..."
cat << 'EOF' > /etc/modprobe.d/cis-disable-fs.conf
install cramfs /bin/true
install freevxfs /bin/true
install jffs2 /bin/true
install hfs /bin/true
install hfsplus /bin/true
install squashfs /bin/true
install udf /bin/true
EOF

log "======================================================================"
log "HARDENING COMPLETE!"
log "CRITICAL: Do NOT close this active shell yet."
log "Open a separate terminal window and verify SSH connection:"
log "  ssh -p ${NEW_SSH_PORT} ${ADMIN_USER}@<SERVER_IP>"
log "======================================================================"
```

---

## 3. Execution & Verification

### Step-by-Step Test Procedure

1. **Staging the Script**:
   ```bash
   sudo nano /usr/local/sbin/harden-server.sh
   sudo chmod 700 /usr/local/sbin/harden-server.sh
   sudo /usr/local/sbin/harden-server.sh
   ```

2. **Simulated Verification Output**:
   ```text
   [+] Phase 1: Updating System Packages...
   [+] Phase 2: Creating Dedicated Deploy / Admin User...
   [+] Phase 3: Hardening OpenSSH Server...
   [+] Phase 4: Configuring Kernel Hardening Parameters (sysctl)...
   [+] Phase 5: Configuring Host Firewall (UFW)...
   Firewall is active and enabled on system startup
   [+] Phase 6: Configuring Fail2ban...
   [+] Phase 7: Configuring Automatic Security Updates...
   [+] Phase 8: Disabling Legacy & Unused Filesystems...
   ======================================================================
   HARDENING COMPLETE!
   ```

3. **Verify Firewall Rules**:
   ```bash
   sudo ufw status verbose
   # Status: active
   # Logging: on (low)
   # Default: deny (incoming), allow (outgoing), disabled (routed)
   # To                         Action      From
   # --                         ------      ----
   # 2222/tcp                   ALLOW IN    Anywhere (Hardened SSH Port)
   ```

4. **Verify Fail2ban Status**:
   ```bash
   sudo fail2ban-client status sshd
   # Status for the jail: sshd
   # |- Filter
   # |  |- Currently failed: 0
   # |  `- Total failed:     0
   # `- Actions
   #    |- Currently banned: 0
   #    `- Total banned:     0
   ```

---

## 4. Failure Modes & Edge Cases

| Failure Mode / Incident | Root Cause | Remediation Runbook |
| :--- | :--- | :--- |
| **Locked Out of SSH Session** | UFW firewall active before port 2222 opened, or sshd port changed without reload. | Access via Cloud Provider Virtual Web Console (AWS EC2 Serial Console / GCP Serial Port). Run `sudo ufw allow 2222/tcp` and `sudo systemctl restart ssh`. |
| **Cloud-Init Overrides Sshd Config** | Cloud-init template at `/etc/ssh/sshd_config.d/50-cloud-init.conf` overwrote password authentication. | Name hardening file `99-hardened.conf` (alphabetically last) so drop-in overrides earlier files. |
| **Unattended Upgrades Fails silently** | Disk space full in `/var/cache/apt/archives` or broken dependency lock. | Check `/var/log/unattended-upgrades/unattended-upgrades.log`. Clean via `apt-get clean` and run `unattended-upgrades -d --dry-run`. |
| **Fail2ban Fails to Hook Nftables** | Legacy iptables selected instead of nftables backend. | Ensure `banaction = nftables-multiport` in `/etc/fail2ban/jail.d/sshd.local`. Verify with `nft list ruleset`. |

---

## 5. Production Considerations

- **SSH Port 22 vs Non-Standard Ports**: Moving SSH to 2222 eliminates 99% of automated Internet script-kiddie scan noise in auth logs. It does not replace key-based authentication.
- **Bastion / Jump Hosts**: In enterprise VPCs, never expose SSH (even on port 2222) directly to `0.0.0.0/0`. Restrict `ufw allow from <VPN_BASTION_IP> to any port 2222 proto tcp`.
- **Auditd Rule Set**: Install standard CIS auditd rules in `/etc/audit/rules.d/cis.rules` to monitor file modifications to `/etc/passwd`, `/etc/sudoers`, and `/var/log/audit/`.
- **Automated Compliance Verification**: Periodically audit hardened instances using OpenSCAP (`oscap-chroot`) or Lynis (`lynis audit system`).

---
*Next Project: [02-bash-backup-script-with-rotation.md](file:///04-devops/linux/07-projects/02-bash-backup-script-with-rotation.md)*
