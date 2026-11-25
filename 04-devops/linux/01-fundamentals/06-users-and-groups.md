# Users and Groups

Linux is a multi-user system, and every process and file belongs to a user and a group. Almost every "permission denied" and every service-running-as-the-wrong-user bug traces back to this model, so it is worth understanding properly before touching [permissions](./05-permissions.md).

## The model

- A **user** is identified by a numeric **UID**. Names are just labels mapped to UIDs.
- A **group** is identified by a **GID**. A user has one **primary group** and any number of **supplementary groups**.
- **UID 0 is root.** The kernel treats UID 0 as all-powerful; the name `root` is only a convention.
- Processes run *as* a user. A file's access is decided by comparing the process's UID/GIDs to the file's owner/group.

Typical UID ranges on Debian/Ubuntu:

| Range | Used for |
|---|---|
| 0 | root |
| 1 to 999 | system accounts (services like `www-data`, `postgres`) |
| 1000+ | regular human users |

(Red Hat family starts regular users at 1000 too on modern releases, older ones at 500.)

## Inspecting users

```bash
whoami                 # current username
id                     # UID, GID, and all groups
id alice               # same for another user
groups                 # group names for current user
getent passwd alice    # look up a user (works with LDAP etc., not just local files)
who                    # who is logged in
last -n 5              # recent logins
```

Example `id` output:

```text
uid=1000(alice) gid=1000(alice) groups=1000(alice),27(sudo),999(docker)
```

## Where it's stored

| File | Contents | Readable by |
|---|---|---|
| `/etc/passwd` | one line per user | everyone |
| `/etc/shadow` | password hashes, aging info | root only |
| `/etc/group` | groups and their members | everyone |

`/etc/passwd` fields, colon-separated:

```text
alice:x:1000:1000:Alice Dev:/home/alice:/bin/bash
│     │ │    │    │         │           └─ login shell
│     │ │    │    │         └─ home directory
│     │ │    │    └─ comment (full name)
│     │ │    └─ primary GID
│     │ └─ UID
│     └─ "x" = hash is in /etc/shadow
└─ username
```

A service account usually has a shell of `/usr/sbin/nologin` (or `/bin/false`), meaning nobody can log in as it interactively. That is intentional and good practice.

Despite the name, `/etc/passwd` holds no passwords. Don't edit these files by hand; use the tools below (or `vipw` / `vigr` if you must).

## Managing users and groups

On Debian/Ubuntu there are two layers: low-level `useradd` (portable, minimal defaults) and friendly `adduser` (interactive, creates home and prompts for password).

```bash
sudo adduser alice                    # Debian/Ubuntu: friendly
sudo useradd -m -s /bin/bash alice    # portable: -m creates home, -s sets shell
sudo passwd alice                     # set or change password

sudo usermod -aG sudo alice           # add to the sudo group (see warning below)
sudo usermod -s /usr/sbin/nologin svc # change shell
sudo passwd -l alice                  # lock the password

sudo userdel alice                    # delete the user, keep the home dir
sudo userdel -r alice                 # delete the user and home dir

sudo groupadd deploy
sudo gpasswd -d alice deploy          # remove from a group
```

Create a system account for a service:

```bash
sudo useradd --system --no-create-home --shell /usr/sbin/nologin myapp
```

> **Always use `-aG` with `usermod`.** `usermod -G docker alice` (without `-a`) *replaces* all of alice's supplementary groups, which can remove her from `sudo` and lock her out of admin access.

### Group membership doesn't apply until a new login

Group changes affect new sessions only. After `usermod -aG docker alice`, alice must log out and back in (or run `newgrp docker` in the current shell) before `id` shows it and Docker commands work. This is one of the most common "I added the group but it still fails" causes.

## Becoming another user: `su` and `sudo`

```bash
su - alice          # switch to alice with her environment, asks alice's password
su -                # become root (needs root's password; often disabled on Ubuntu)
sudo command        # run one command as root, asks YOUR password
sudo -u postgres psql   # run as a specific user
sudo -i             # interactive root shell with root's environment
```

- `su -` vs `su`: the dash starts a **login shell**, loading the target user's environment and `PATH`. Without it you keep much of your own.
- On Ubuntu, root has no usable password by default; admins use `sudo`. Members of the `sudo` group (on RHEL family: `wheel`) can use it.
- `sudo` logs each command in `/var/log/auth.log` (Debian/Ubuntu) or via the journal, which gives you an audit trail that a shared root login doesn't.

### Configuring sudo

Always edit with `visudo`; it validates syntax before saving, since a broken sudoers file can lock you out.

```bash
sudo visudo -f /etc/sudoers.d/deploy
```

```text
# let the deploy user restart one service without a password
deploy ALL=(root) NOPASSWD: /usr/bin/systemctl restart myapp
```

Prefer files in `/etc/sudoers.d/` over editing `/etc/sudoers` directly, and grant specific commands rather than `ALL`. This is least privilege; see [server hardening](../05-production/01-server-hardening.md).

### The redirection trap

```bash
sudo echo "x" > /etc/some-file      # FAILS: the redirect runs as YOU, not root
echo "x" | sudo tee /etc/some-file  # works: tee runs as root
echo "x" | sudo tee -a /etc/some-file   # append
```

`sudo` only elevates the command, not the shell's `>` redirection. More on this in [shell basics](../02-shell/01-shell-basics.md).

## Practical patterns

- **One account per human, plus service accounts per app.** Never share a login, and never run a web app as root.
- **Shared access to a directory**: put users in a common group and give that group access to the directory (see [permissions](./05-permissions.md)).
- **Why Docker group = root-equivalent.** Membership in `docker` lets a user mount the host filesystem into a container, which is effectively root on the machine. Treat it as a privileged group.

## Common mistakes

- `usermod -G` without `-a`, dropping existing groups.
- Forgetting to re-login after changing group membership.
- Editing `/etc/sudoers` with a plain editor and breaking it.
- Running services as root "to avoid permission problems".
- Assuming usernames are what the system checks. It compares **numeric IDs**, which matters when moving files or mounting volumes between machines or containers (UID 1000 on one host may be a different name on another).

## Quick Summary

- Users have a UID, a primary group, and supplementary groups; root is UID 0.
- `/etc/passwd`, `/etc/shadow`, `/etc/group` are the databases; use `id` and `getent` to inspect.
- `adduser`/`useradd`, `usermod -aG`, `passwd`, `userdel -r` manage accounts; system accounts use `nologin`.
- Use `sudo` (not shared root logins), edit with `visudo`, and grant narrow commands.
- Group changes need a new login; `sudo` doesn't cover `>` redirection, use `tee`.

**Next:** [Permissions](./05-permissions.md)
