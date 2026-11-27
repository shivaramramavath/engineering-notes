# Package Managers Compared: APT, DNF, Pacman & APK Rosetta Stone

| Metadata | Details |
| :--- | :--- |
| **Topic** | Linux Package Management Rosetta Stone: APT, DNF, Pacman, APK |
| **Module** | `09-cheatsheets` — High-Density Linux Production Cheatsheets |
| **Track** | `04-devops/linux` |
| **Target OS** | Ubuntu/Debian, RHEL/Rocky, Arch, Alpine |
| **Last Updated**| 2026 |

---

## 1. Overview & Distribution Families

Linux system administrators frequently operate across heterogeneous environments: Ubuntu servers in AWS, Rocky/RHEL nodes on-premise, Alpine Linux inside lightweight container images, and Arch on developer workstations.

This cheatsheet provides a **Rosetta Stone** mapping standard package lifecycle operations across the four major packaging ecosystems:
1. **APT / dpkg**: Debian, Ubuntu (v24.04 uses modern Deb822 `.sources` format).
2. **DNF / rpm**: RHEL, Rocky Linux, AlmaLinux, Fedora, CentOS Stream.
3. **Pacman**: Arch Linux, Manjaro.
4. **APK**: Alpine Linux (musl-libc container baseline).

---

## 2. Package Management Rosetta Stone

| Operational Task | Ubuntu / Debian (`apt`) | RHEL / Rocky (`dnf`) | Alpine Linux (`apk`) | Arch Linux (`pacman`) |
| :--- | :--- | :--- | :--- | :--- |
| **Refresh Repo Metadata** | `apt update` | `dnf check-update` | `apk update` | `pacman -Sy` |
| **Install Package** | `apt install <pkg>` | `dnf install <pkg>` | `apk add <pkg>` | `pacman -S <pkg>` |
| **Install Without Prompts**| `apt install -y <pkg>` | `dnf install -y <pkg>` | `apk add <pkg>` (default) | `pacman -S --noconfirm <pkg>` |
| **Install No Recommends** | `apt install --no-install-recommends <pkg>` | `dnf install --setopt=install_weak_deps=False <pkg>` | `apk add --no-scripts <pkg>` | `pacman -S --asdeps <pkg>` |
| **Remove Package** | `apt remove <pkg>` | `dnf remove <pkg>` | `apk del <pkg>` | `pacman -R <pkg>` |
| **Purge (Delete Configs)**| `apt purge <pkg>` | `dnf remove <pkg>` | `apk del --purge <pkg>`| `pacman -Rns <pkg>` |
| **Upgrade All Packages** | `apt upgrade` | `dnf upgrade` | `apk upgrade` | `pacman -Su` |
| **Full Distro Upgrade** | `apt full-upgrade` | `dnf distro-sync` | `apk upgrade --available` | `pacman -Syu` |
| **Search by Name** | `apt search <term>` | `dnf search <term>` | `apk search <term>` | `pacman -Ss <term>` |
| **Show Package Details** | `apt show <pkg>` | `dnf info <pkg>` | `apk info <pkg>` | `pacman -Si <pkg>` |
| **List Installed Packages**| `apt list --installed` | `dnf list --installed` | `apk info` | `pacman -Q` |
| **Find File Owner** | `dpkg -S /path/to/file` | `dnf provides /path/to/file` | `apk info -W /path/to/file` | `pacman -Qo /path/to/file` |
| **List Files in Package** | `dpkg -L <pkg>` | `dnf repoquery -l <pkg>`| `apk info -L <pkg>` | `pacman -Ql <pkg>` |
| **Clean Package Cache** | `apt clean` | `dnf clean all` | `apk cache clean` | `pacman -Sc` |
| **Remove Orphaned Deps** | `apt autoremove` | `dnf autoremove` | `apk del` (automatic) | `pacman -Rns $(pacman -Qdtq)` |
| **Hold / Lock Version** | `apt-mark hold <pkg>` | `dnf versionlock add <pkg>` | `apk add <pkg>=1.2.3` | Add to `IgnorePkg` in `/etc/pacman.conf` |
| **Unhold Version** | `apt-mark unhold <pkg>` | `dnf versionlock delete <pkg>`| Remove constraint | Remove from `IgnorePkg` |

---

## 3. Configuration & Repository Locations

```
1. Debian & Ubuntu (Modern Deb822):
   - Repositories: /etc/apt/sources.list.d/*.sources (Modern stanza format)
   - Legacy repos: /etc/apt/sources.list
   - Trusted GPG Keys: /etc/apt/keyrings/*.gpg
   - Cache directory: /var/cache/apt/archives/

2. RHEL / Rocky Linux:
   - Repositories: /etc/yum.repos.d/*.repo
   - Global Config: /etc/dnf/dnf.conf
   - Cache directory: /var/cache/dnf/

3. Alpine Linux:
   - Repositories: /etc/apk/repositories
   - Cache directory: /var/cache/apk/

4. Arch Linux:
   - Global Config: /etc/pacman.conf
   - Mirrorlist: /etc/pacman.d/mirrorlist
   - Cache directory: /var/cache/pacman/pkg/
```

---

## 4. Modern Deb822 Repository Format (Ubuntu 24.04 Standard)

Starting with Ubuntu 24.04 LTS, third-party repositories transition from single-line `sources.list` entries to structured **Deb822 `.sources`** files.

### Example: Adding a Third-Party Repository Cleanly

```bash
#!/usr/bin/env bash
# Adding Docker official repository using Deb822 standard
set -euo pipefail

# 1. Download official GPG key into dedicated keyrings directory
sudo install -m 0755 -d /etc/apt/keyrings
curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo gpg --dearmor -o /etc/apt/keyrings/docker.gpg
sudo chmod 644 /etc/apt/keyrings/docker.gpg

# 2. Create structured Deb822 configuration file
cat << EOF | sudo tee /etc/apt/sources.list.d/docker.sources
Types: deb
URIs: https://download.docker.com/linux/ubuntu
Suites: noble
Components: stable
Architectures: amd64
Signed-By: /etc/apt/keyrings/docker.gpg
EOF

# 3. Update index
sudo apt update
```

---

## 5. Dockerfile Minimal Container Optimization Patterns

### Pattern A: Clean Alpine Minimal Install (`apk`)
```dockerfile
# Best practice: use --no-cache to avoid saving tarball archives to disk
RUN apk update && apk add --no-cache \
    curl \
    ca-certificates \
    jq
```

### Pattern B: Clean Debian/Ubuntu Minimal Install (`apt`)
```dockerfile
# Best practice: avoid recommended bloat and purge lists in the same RUN layer
RUN apt-get update && apt-get install -y --no-install-recommends \
    curl \
    ca-certificates \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*
```

### Pattern C: Clean RHEL/Rocky Minimal Install (`dnf`)
```dockerfile
RUN dnf install -y --setopt=install_weak_deps=False \
    curl \
    && dnf clean all \
    && rm -rf /var/cache/dnf
```

---

## 6. Offline Package Inspection (Raw Package Files)

```bash
# === Inspect Debian .deb files ===
dpkg -I package.deb                         # View package metadata & control information
dpkg -c package.deb                         # List files inside .deb archive
dpkg -i package.deb                         # Install local .deb file (manual)

# === Inspect Red Hat .rpm files ===
rpm -qip package.rpm                        # View package metadata
rpm -qlp package.rpm                        # List files inside .rpm archive
rpm -ivh package.rpm                        # Install local .rpm with progress hashes
rpm -V package_name                         # Verify integrity of installed package files against RPM database!
```

---
*Linux Track Complete! All modules (01 to 09) fully authored and verified.*
