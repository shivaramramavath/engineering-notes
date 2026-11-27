# Permissions, Users & Access Control: High-Density Cheatsheet

| Metadata | Details |
| :--- | :--- |
| **Topic** | Linux File Permissions, User Accounts, ACLs & Extended Attributes |
| **Module** | `09-cheatsheets` — High-Density Linux Production Cheatsheets |
| **Track** | `04-devops/linux` |
| **Target OS** | Ubuntu 24.04 LTS, RHEL 9.4 / Rocky Linux 9.4 |
| **Last Updated**| 2026 |

---

## 1. Permission Bits & Octal Calculations

```
File Mode Layout: [Special: 3 bits] [User: 3 bits] [Group: 3 bits] [Others: 3 bits]
Example: - r w x r - x r - - (0754)

Binary / Octal Matrix:
  Binary   Octal   Symbolic   Meaning
  -------------------------------------------------------------
  000        0       ---      No permissions
  001        1       --x      Execute only
  010        2       -w-      Write only
  011        3       -wx      Write and Execute
  100        4       r--      Read only
  101        5       r-x      Read and Execute
  110        6       rw-      Read and Write
  111        7       rwx      Full Read, Write, and Execute
```

### Regular Files vs. Directories

| Bit | On a Regular File | On a Directory |
| :--- | :--- | :--- |
| **`r` (4)** | Read file contents (`cat`, `less`) | List entries inside directory (`ls`) |
| **`w` (2)** | Modify/overwrite file contents | Create, rename, delete files inside |
| **`x` (1)** | Execute binary or script | Traverse/enter directory (`cd`), access metadata |

---

## 2. Special Permissions (SUID, SGID, Sticky)

```bash
# === SUID (SetUID - Octal 4000) ===
# Runs binary with privileges of the file owner (e.g., root)
chmod 4755 /usr/local/bin/custom-tool       # Symbolic: -rwsr-xr-x
chmod u+s /usr/local/bin/custom-tool
chmod u-s /usr/local/bin/custom-tool        # Remove SUID
# Note: Capital 'S' (-rwSr-xr-x) means SUID is set, but owner lacks execute (+x)!

# === SGID (SetGID - Octal 2000) ===
# On executable: runs with privileges of file group
# On directory: new files automatically inherit parent directory's group
chmod 2775 /var/www/shared                  # Symbolic: drwxrwsr-x
chmod g+s /var/www/shared
chmod g-s /var/www/shared

# === Sticky Bit (Octal 1000) ===
# On directory: only file owner, directory owner, or root can delete files
chmod 1777 /tmp                             # Symbolic: drwxrwxrwt
chmod +t /tmp
chmod -t /tmp
```

---

## 3. Umask & Default Permissions Reference

```
Default Created Files:       0666 (rw-rw-rw-)  [Execute bit never set by default]
Default Created Directories: 0777 (rwxrwxrwx)
Effective Permission = Default - Umask

Common Umask Presets:
  Umask    Files Created   Dirs Created   Typical Environment
  --------------------------------------------------------------------
  0022     0644 (rw-r--r--) 0755 (rwxr-xr-x) Standard server default
  0002     0664 (rw-rw-r--) 0775 (rwxrwxr-x) Shared developer teams
  0027     0640 (rw-r-----) 0750 (rwxr-x---) Security hardened servers
  0077     0600 (rw-------) 0700 (rwx------) Highly sensitive environments

Commands:
  umask                   # Display current umask in octal (e.g., 0022)
  umask -S                # Display current umask symbolically (u=rwx,g=rx,o=rx)
  umask 027               # Temporarily set umask for current shell session
```

---

## 4. `chmod`, `chown`, and `chgrp` Fast Reference

```bash
# === chmod (Change Mode) ===
chmod 644 file.txt                          # Absolute octal assignment
chmod 755 script.sh
chmod +x script.sh                          # Add execute to all (user, group, others)
chmod u=rw,go=r file.txt                    # Explicit symbolic mapping
chmod -R u=rwX,go=rX /var/www/html          # Capital 'X': execute only on dirs & existing bins

# === Safe Recursive Permission Setting ===
find /var/www -type d -exec chmod 755 {} +  # Directories to 755
find /var/www -type f -exec chmod 644 {} +  # Regular files to 644

# === chown (Change Owner & Group) ===
chown deploy file.txt                       # Change owner only
chown deploy:www-data file.txt              # Change owner and group
chown :www-data file.txt                    # Change group only
chown -R deploy:deploy /opt/myapp           # Recursive change
chown --reference=ref_file.txt target.txt   # Copy ownership from reference file

# === chgrp (Change Group) ===
chgrp -R analytics /var/log/app             # Change group recursively
```

---

## 5. POSIX Access Control Lists (ACLs)

```bash
# === Inspect ACLs ===
getfacl /var/log/app.log                    # View users, groups, and mask

# === Grant User & Group Permissions ===
setfacl -m u:alice:rw /var/log/app.log       # Grant user alice read/write
setfacl -m g:auditors:r /var/log/app.log     # Grant group auditors read-only
setfacl -m m::r /var/log/app.log            # Adjust effective rights mask

# === Default Directory ACLs (Inherited by new files) ===
setfacl -d -m g:developers:rwx /data/proj/   # Apply default ACL to directory
setfacl -R -m u:bob:rwX /data/proj/          # Recursive ACL assignment

# === Remove ACLs ===
setfacl -x u:alice /var/log/app.log          # Remove alice's entry
setfacl -b /var/log/app.log                  # Remove ALL extended ACLs (resets to DAC)
setfacl -k /data/proj/                       # Remove default directory ACLs
```

---

## 6. Linux Extended Attributes (`chattr` / `lsattr`)

```bash
# === Inspect Attributes ===
lsattr /etc/resolv.conf                     # Displays active flags (e.g., ----i---------)

# === Immutable (+i) ===
# Cannot be modified, deleted, overwritten, or renamed, even by root!
chattr +i /etc/resolv.conf                  # Make immutable
chattr -i /etc/resolv.conf                  # Remove immutable flag

# === Append-Only (+a) ===
# Can only be opened in append mode; existing contents cannot be modified
chattr +a /var/log/critical-audit.log       # Protect audit logs
chattr -a /var/log/critical-audit.log

# === Other Common Attributes ===
chattr +d file.txt                          # No dump (excluded from dump backups)
chattr +s file.txt                          # Secure deletion (zeroed out on delete)
```

---

## 7. User & Group Administration One-Liners

```bash
# === User Creation & Modification ===
useradd -m -s /bin/bash -G sudo,docker deploy # Create user with home, shell, groups
useradd -r -s /usr/sbin/nologin -d /var/empty app # Create unprivileged system user
usermod -aG docker alice                     # Append alice to docker group (CRITICAL: -a)
usermod -s /bin/zsh bob                      # Change default shell
userdel -r testuser                          # Delete user AND remove /home/testuser

# === Group Management ===
groupadd engineering                         # Create new group
groupdel engineering                         # Delete group
gpasswd -d alice docker                      # Remove alice from docker group
id alice                                     # Display UID, GID, and all group memberships
groups                                       # Display groups for current shell

# === Password & Account Lockdown ===
passwd alice                                 # Change password
passwd -l alice                              # Lock user account (prepends ! to hash)
passwd -u alice                              # Unlock user account
chage -l alice                               # View password expiration and aging policy
chage -E 2026-12-31 alice                    # Set account expiration date
```

---

## 8. Sudoers Configuration Quick Reference

```text
# Edit ONLY with: sudo visudo (syntax validated before saving)
# Or drop-ins in: /etc/sudoers.d/99-deploy (Permissions MUST be 0440!)

# Syntax: Who Where = (AsWhom) Commands
deploy ALL=(ALL) ALL                         # Full root access with password
deploy ALL=(ALL) NOPASSWD: ALL               # Full root access without password

# Granular Least-Privilege Rules:
deploy ALL=(ALL) NOPASSWD: /usr/bin/systemctl restart nginx, /usr/bin/systemctl reload nginx
%developers ALL=(ALL) /usr/bin/journalctl    # Group developers can run journalctl

# Security Directives:
Defaults env_reset                           # Reset environment variables on sudo
Defaults secure_path="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
```

---
*Next Cheatsheet: [03-shell-and-bash.md](file:///04-devops/linux/09-cheatsheets/03-shell-and-bash.md)*
