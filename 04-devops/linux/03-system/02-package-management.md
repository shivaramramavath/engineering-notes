# Package Management

Instead of downloading installers, Linux distros ship software as **packages** from signed **repositories**, and a package manager resolves dependencies, installs, upgrades, and removes them. Knowing it well means safer installs, repeatable servers, and automatic security patching, which is one of the highest-value habits in production.

Prerequisites: [Getting Started](../01-fundamentals/01-getting-started.md) (distro families) and [Users and Groups](../01-fundamentals/04-users-and-groups.md) (`sudo`).

## How it works

```text
 repository (signed index + .deb/.rpm files)
        │   apt update / dnf makecache   → refresh the list of what's available
        ▼
 local package index ──► resolver ──► download ──► verify signature ──► install
                         (deps)                                       (dpkg / rpm)
```

Two layers exist:

| Layer | Debian/Ubuntu | Red Hat family |
|---|---|---|
| Low-level (single package files, no dependency resolution) | `dpkg` | `rpm` |
| High-level (repos, dependencies) | `apt` | `dnf` |

You nearly always use the high-level tool.

## apt (Debian, Ubuntu)

Use `apt` interactively and `apt-get` in scripts (its output and behavior are stable; `apt` warns that its CLI is not guaranteed for scripting).

```bash
sudo apt update                    # refresh the package index (does NOT install anything)
sudo apt upgrade                   # install newer versions of installed packages
sudo apt full-upgrade              # also allow installing/removing packages to resolve changes (kernel updates etc.)

apt search nginx                   # search names/descriptions
apt show nginx                     # details: version, deps, size
sudo apt install nginx             # install
sudo apt install -y curl git       # no confirmation prompt (scripts)
sudo apt install nginx=1.24.0-2ubuntu7   # pin a specific version (must exist in the index)

sudo apt remove nginx              # remove program, keep config files
sudo apt purge nginx               # remove program and system config
sudo apt autoremove                # remove dependencies nothing needs now
sudo apt clean                     # clear downloaded .deb cache
```

`update` and `upgrade` are different steps: `update` only refreshes the list; without it you install stale versions or hit "Unable to locate package".

### Inspecting what's installed

```bash
apt list --installed | grep nginx
apt policy nginx               # installed vs candidate version, and which repo it comes from
dpkg -l | grep nginx           # installed packages
dpkg -L nginx                  # files a package installed
dpkg -S /usr/sbin/nginx        # which package owns this file
apt list --upgradable          # pending updates
```

`apt policy` is the first thing to run when you get an unexpected version: it shows candidates and their priorities.

### Holding versions

```bash
sudo apt-mark hold nginx       # don't upgrade this package
apt-mark showhold
sudo apt-mark unhold nginx
```

### Repositories

Where packages come from is defined in `/etc/apt/sources.list` and `/etc/apt/sources.list.d/`. Newer Ubuntu releases use the **deb822** `.sources` format (for example `/etc/apt/sources.list.d/ubuntu.sources`); older ones use one-line `deb ...` entries. Look at what your system uses before editing.

Adding a third-party repository (a vendor's apt repo) has three parts: fetch its signing key, store it in a keyring file, and reference it with `signed-by` so the key only trusts that repo.

```bash
sudo install -d -m 0755 /etc/apt/keyrings
curl -fsSL https://example.com/repo/key.gpg | sudo gpg --dearmor -o /etc/apt/keyrings/example.gpg

echo "deb [signed-by=/etc/apt/keyrings/example.gpg] https://example.com/apt stable main" \
  | sudo tee /etc/apt/sources.list.d/example.list
sudo apt update
```

Take the exact URLs and the repo line from the vendor's own documentation. `apt-key` is deprecated: keys added that way are trusted for *every* repo. PPAs (`sudo add-apt-repository ppa:name/ppa`) are an Ubuntu convenience for community repos; treat them like any third-party source.

### When apt complains

```text
Could not get lock /var/lib/dpkg/lock-frontend
```

Another package process (often unattended-upgrades) is running. Wait and retry; check with `ps aux | grep -E "apt|dpkg|unattended"`. **Don't delete lock files** unless you've confirmed nothing is running.

```bash
sudo dpkg --configure -a     # finish an interrupted install
sudo apt --fix-broken install   # (or: apt -f install) repair broken dependencies
```

## dnf (RHEL, Rocky, Alma, Fedora)

```bash
sudo dnf install nginx
sudo dnf remove nginx
sudo dnf upgrade                  # refreshes metadata and upgrades (no separate "update" step needed)
dnf check-update                  # list available updates
dnf search nginx
dnf info nginx
dnf provides '*/bin/dig'          # which package provides a file/command
dnf list installed
dnf history                       # transactions
sudo dnf history undo 12          # roll back transaction 12
```

Repos are `.repo` files in `/etc/yum.repos.d/`. `dnf repolist` shows what's enabled. On older systems `yum` still exists, usually as an alias to `dnf`.

### Command mapping

| Task | apt | dnf |
|---|---|---|
| Refresh index | `apt update` | automatic (`dnf makecache`) |
| Upgrade all | `apt upgrade` | `dnf upgrade` |
| Install | `apt install x` | `dnf install x` |
| Remove (with config) | `apt purge x` | `dnf remove x` |
| Which package owns file | `dpkg -S file` | `dnf provides file` |
| Files in package | `dpkg -L x` | `rpm -ql x` |
| Package info | `apt show x` | `dnf info x` |

## Automatic security updates

Unpatched servers are the most common real-world compromise. Automate security patches, and plan for reboots.

### Debian/Ubuntu: unattended-upgrades

```bash
sudo apt install unattended-upgrades
sudo dpkg-reconfigure -plow unattended-upgrades    # enable
```

```text
/etc/apt/apt.conf.d/20auto-upgrades      # on/off and frequency
/etc/apt/apt.conf.d/50unattended-upgrades   # which origins to upgrade, reboot, mail
/var/log/unattended-upgrades/            # what it did
```

```text
// 20auto-upgrades
APT::Periodic::Update-Package-Lists "1";
APT::Periodic::Unattended-Upgrade "1";
```

By default it applies **security** updates only, which is the conservative choice. Test it with `sudo unattended-upgrade --dry-run -d`. It's often enabled by default on Ubuntu Server; verify rather than assume.

Some updates (kernel, libc) need a reboot to take effect:

```bash
ls /var/run/reboot-required && cat /var/run/reboot-required.pkgs
sudo apt install needrestart     # tells you which services need a restart after library updates
```

You can have unattended-upgrades reboot automatically at a set time (`Unattended-Upgrade::Automatic-Reboot`), which is fine for stateless nodes and risky for databases unless it's planned.

### Red Hat family

Install `dnf-automatic`, configure `/etc/dnf/automatic.conf` (for example to apply only security updates), and enable its systemd timer. See [timers](./04-cron-and-timers.md) for how timers work.

## Beyond the distro's packages

| Source | Use when | Watch out for |
|---|---|---|
| Distro repo | default choice | versions may lag |
| Vendor repo | you need a newer version (Node, Docker, PostgreSQL) | trust, and you must keep it updated |
| `snap` / Flatpak | sandboxed apps, Ubuntu defaults | different update model, slower startup |
| `npm -g`, `pip` | language tools | system Python is "externally managed"; use `venv`/`pipx` rather than `--break-system-packages` |
| Tarball into `/opt` | no package exists | no dependency tracking or auto-updates |
| Version managers (nvm, pyenv) | multiple versions per user | per-user paths don't exist for system services, so a systemd unit needs the absolute path |

Language runtimes are the classic case: a system service running Node needs a node binary at a stable, absolute path ([systemd](./03-systemd-and-services.md)), so a distro or vendor package is usually easier to run in production than a per-user nvm install.

Be wary of `curl ... | sudo bash` install one-liners. If you use one, download it first, read it, then run it.

## Common mistakes

- Running `apt install` without `apt update` first on a fresh machine ("Unable to locate package").
- Mixing repositories from a different release (a package built for a newer Ubuntu), which can break dependencies on the whole system.
- Deleting dpkg lock files while an upgrade is still running.
- Trusting a third-party key globally (`apt-key`) instead of `signed-by`.
- Leaving a reboot pending for months after kernel updates.
- `sudo pip install` into system Python, which can break system tools. Use a virtual environment.
- Running `autoremove` on a server without reading the list it proposes.
- Upgrading production blindly: pin or hold critical packages (databases) and test updates first.

## Quick Summary

- Packages come from signed repositories; `apt`/`dpkg` on Debian family, `dnf`/`rpm` on Red Hat family.
- `apt update` refreshes the list; `apt upgrade` installs updates; `apt policy` explains versions.
- Add vendor repos with a dedicated keyring and `signed-by`, never `apt-key`.
- Turn on automatic security updates and track pending reboots.
- Prefer distro or vendor packages over per-user installs for services.
- Hold critical package versions and test before upgrading production.

**Next:** [systemd and Services](./03-systemd-and-services.md)
