# Server Hardening

A freshly provisioned server on a public IP starts receiving automated login attempts within minutes. Hardening means shrinking what an attacker can reach and limiting what they can do if something is compromised. It isn't a one-time script; it's a short list of habits: patch, authenticate with keys, expose little, run with least privilege, and keep logs you'll actually read.

Prerequisites: [SSH](../04-networking/02-ssh.md), [Firewall](../04-networking/03-firewall.md), [Users and Groups](../01-fundamentals/04-users-and-groups.md), and [Package Management](../03-system/02-package-management.md).

## The mindset

- **Reduce attack surface.** Software that isn't installed or listening can't be exploited.
- **Least privilege.** Every user, process, and key gets only what it needs.
- **Defense in depth.** Assume any one layer (firewall, password, app) will fail; have another behind it.
- **Patch quickly.** Most real compromises use known, already-fixed vulnerabilities.
- **Detect and recover.** Logs, backups, and a way to rebuild matter as much as prevention.

A reasonable order for a new server:

```text
1. non-root admin user + SSH keys     4. firewall: default deny
2. harden sshd (after keys work!)      5. automatic updates
3. remove/close unneeded services      6. fail2ban, logging, time sync, backups
```

## 1. Know what's running and exposed

```bash
sudo ss -tlnp                                  # what listens, on which address, owned by whom
systemctl list-unit-files --state=enabled      # what starts at boot
```

Anything listening on `0.0.0.0`/`[::]` that you don't recognize or need should be stopped and disabled (`sudo systemctl disable --now name`) or uninstalled. Databases and caches should bind to `127.0.0.1` or a private IP ([networking basics](../04-networking/01-networking-basics.md)).

## 2. Harden SSH

First make sure **key login works** from a second terminal. Only then turn off passwords.

Put your settings in a drop-in file instead of editing the main config. **Name matters:** sshd uses the **first** value it reads for each option, and files in `/etc/ssh/sshd_config.d/` are read in alphabetical order. Many cloud images ship `50-cloud-init.conf` containing `PasswordAuthentication yes`, which would beat a file named `99-...`. Use a low number so yours is read first.

`/etc/ssh/sshd_config.d/01-hardening.conf`:

```text
PermitRootLogin no
PasswordAuthentication no
KbdInteractiveAuthentication no
PubkeyAuthentication yes
MaxAuthTries 3
LoginGraceTime 30
X11Forwarding no
AllowUsers alice deploy
```

(`KbdInteractiveAuthentication` is the current name for what older guides call `ChallengeResponseAuthentication`.)

```bash
sudo sshd -t                                      # syntax check
sudo sshd -T | grep -Ei 'passwordauth|permitroot'   # the EFFECTIVE values, after all includes
sudo systemctl reload ssh                         # service is "sshd" on the Red Hat family
```

Then, **before closing your current session**, open a new terminal and confirm you can still log in. Also try a password login and confirm it's refused.

Notes:

- `AllowUsers`/`AllowGroups` restricts who may log in at all, a cheap and effective control.
- Changing the SSH port reduces log noise from bots but is **not** a security control; keys and disabled passwords are. If you change it, update the firewall first.
- Keep `UsePAM yes` (the default on Debian/Ubuntu); it's needed for account and session handling.

## 3. Firewall

Default deny inbound; allow SSH, 80/443, and nothing else you can't justify. Restrict database or admin ports to specific sources. Remember the cloud firewall is a second layer, and that Docker-published ports can bypass ufw. All covered in [Firewall](../04-networking/03-firewall.md).

## 4. Automatic updates

Turn on unattended security updates and track pending reboots ([package management](../03-system/02-package-management.md)):

```bash
sudo apt install unattended-upgrades
sudo dpkg-reconfigure -plow unattended-upgrades
ls /var/run/reboot-required 2>/dev/null          # a kernel/libc update is waiting for a reboot
```

## 5. fail2ban

fail2ban watches logs for repeated failures (for example failed SSH logins) and temporarily bans the source IP through the firewall.

```bash
sudo apt install fail2ban
```

Never edit `jail.conf`; override in `/etc/fail2ban/jail.local`:

```text
[DEFAULT]
ignoreip = 127.0.0.1/8 ::1 203.0.113.5     # your own stable IP, so you can't ban yourself
bantime  = 1h
findtime = 10m
maxretry = 5

[sshd]
enabled = true
```

```bash
sudo systemctl enable --now fail2ban
sudo fail2ban-client status sshd                    # currently banned IPs, failure counts
sudo fail2ban-client set sshd unbanip 198.51.100.9  # undo a ban
```

Be realistic about its role: with password authentication disabled, brute force can't succeed anyway, so fail2ban mostly **reduces log noise and load**. It's a useful extra layer, not the thing that protects you. Make sure it's reading the right log source on your distro (journald vs `/var/log/auth.log`); `fail2ban-client status sshd` should show a non-zero "Total failed" after some bot traffic.

## 6. Least privilege in practice

- **One unprivileged user per service.** Never run a web app or database as root ([systemd note](../03-system/03-systemd-and-services.md) shows a dedicated `User=`).
- **Narrow `sudo`.** Grant specific commands in `/etc/sudoers.d/` instead of `ALL`, and prefer individual accounts to a shared one.
- **Tight file permissions.** Secrets (`.env`, keys) readable only by the owner/service, e.g. `chmod 640` with the service group; never world-readable, never in git ([permissions](../01-fundamentals/05-permissions.md)).
- **Sandbox services** with `NoNewPrivileges`, `ProtectSystem`, `ProtectHome`, `PrivateTmp` in the unit file. `systemd-analyze security myapp` scores what's missing.
- **Audit accounts and setuid files:**

```bash
awk -F: '$3 == 0 {print $1}' /etc/passwd                    # should print only "root"
sudo awk -F: '$2 == "" {print $1}' /etc/shadow              # accounts with an EMPTY password
sudo lastb | head                                           # recent failed logins
last | head                                                 # recent successful logins
find / -xdev -perm -4000 -type f 2>/dev/null                # setuid binaries; know what's expected
```

- **Containers aren't a free pass.** Don't add users to the `docker` group casually (it is effectively root), and don't run containers `--privileged` without a reason.

## 7. Mandatory access control: AppArmor and SELinux

Normal Unix permissions (DAC) let the *owner* decide access. **MAC** adds a system-wide policy that confines programs even if they run as root or are exploited. It's a second gate **on top of** file permissions, which is why "permissions look right but access is denied" can be MAC.

| | AppArmor | SELinux |
|---|---|---|
| Default on | Ubuntu, Debian, SUSE | RHEL, Fedora, Rocky, Alma |
| Model | per-program profiles keyed on **paths** | every file/process has a **label**; policy says which labels may interact |
| Complexity | lower | higher, more granular |

### AppArmor

```bash
sudo aa-status                                  # loaded profiles, enforce vs complain
sudo journalctl -k | grep -i 'apparmor="DENIED"'   # what it blocked
sudo aa-complain /etc/apparmor.d/usr.sbin.foo   # log violations without blocking (tools: apparmor-utils)
sudo aa-enforce  /etc/apparmor.d/usr.sbin.foo
```

Profiles live in `/etc/apparmor.d/`. If an app can't read a non-standard data directory, the fix is usually adding that path to its profile, not disabling AppArmor.

### SELinux

```bash
getenforce                                  # Enforcing / Permissive / Disabled
sestatus
ls -Z /var/www/html                         # show file labels
ps -eZ | grep nginx                         # show process labels
sudo ausearch -m AVC -ts recent             # recent denials (audit log)
sudo restorecon -Rv /srv/site               # reset labels to policy defaults
sudo semanage fcontext -a -t httpd_sys_content_t '/srv/site(/.*)?'   # define the label for a custom path
sudo setsebool -P httpd_can_network_connect on    # toggle a policy boolean, persistently
```

The typical failure: you move web content to a custom path, permissions are fine, nginx still gets "Permission denied" because the files have the wrong **label**. Fix it with `semanage fcontext` + `restorecon`.

**Do not "fix" problems with `setenforce 0` and leave it.** Use permissive mode briefly to *diagnose* (if the problem disappears, it's SELinux), then add the correct label or boolean and go back to enforcing. Permanently disabling it removes a layer that's saved many people from real exploits.

## 8. Logging, time, and auditing

- **Know where logs are:** `journalctl -u ssh`, `/var/log/auth.log` (Debian/Ubuntu) or `/var/log/secure` (RHEL), and your app and web server logs. Review failed logins and `sudo` use periodically.
- **Keep logs off the box too.** An attacker with root can alter local logs; shipping them to another system preserves evidence.
- **Keep the clock correct** (`timedatectl` should say `System clock synchronized: yes`; `systemd-timesyncd` or `chrony` do it). Wrong time breaks TLS validation and makes logs impossible to correlate.
- For a baseline report, **Lynis** (`sudo apt install lynis`, then `sudo lynis audit system`) lists suggested improvements. Read them critically; not every suggestion fits your server.
- For compliance-grade audit trails, look at `auditd`.

## 9. Backups and recovery

Hardening prevents some incidents; backups let you survive the rest. Keep versioned, off-machine backups and test restoring ([backups with rsync](../03-system/04-cron-and-timers.md)). Prefer being able to rebuild a server from scripts over nursing one that's been compromised.

## Hardening checklist

- [ ] Admin user with `sudo`; root SSH login disabled
- [ ] SSH keys only; `PasswordAuthentication no` confirmed with `sshd -T`
- [ ] Only needed ports open; default-deny firewall; cloud firewall reviewed
- [ ] Unneeded services removed; databases bound to loopback/private IP
- [ ] Unattended security updates on; reboots tracked
- [ ] Services run as dedicated users, with sandboxing where possible
- [ ] Secrets not world-readable and not in git
- [ ] AppArmor/SELinux enforcing
- [ ] fail2ban (optional extra), time sync, log review
- [ ] Off-machine backups and a tested restore

## Common mistakes

- **Locking yourself out** by disabling passwords before keys work, or reloading sshd without a second session open.
- **Hardening file loaded too late** (`99-...conf`), so a cloud-init drop-in keeps passwords enabled. Verify with `sshd -T`.
- **Treating a port change or fail2ban as the security**, instead of keys and patching.
- **Banning your own IP** with fail2ban (set `ignoreip`).
- **Disabling SELinux/AppArmor** to make an error go away.
- **Running services as root**, or adding broad `NOPASSWD: ALL` sudo rules.
- **Copy-pasting giant "hardening scripts"** without understanding them; each change should be one you can explain.
- **Forgetting the layers outside the server:** cloud firewall, exposed admin panels, leaked secrets.

## Quick Summary

- Shrink exposure (`ss -tlnp`), patch automatically, and default-deny the firewall.
- SSH: keys only, no root login, `AllowUsers`; put settings in an early drop-in and check with `sshd -T`; test from a second session.
- fail2ban cuts noise; it doesn't replace key-only auth.
- Least privilege: dedicated service users, narrow `sudo`, strict file modes, systemd sandboxing.
- AppArmor/SELinux add a policy layer; fix labels/profiles, don't disable them.
- Keep logs, accurate time, and tested off-machine backups.

**Next:** [Troubleshooting](./02-troubleshooting.md)
