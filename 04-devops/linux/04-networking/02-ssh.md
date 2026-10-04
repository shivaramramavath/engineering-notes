# SSH

SSH (Secure Shell) gives you an encrypted channel to a remote machine: a shell, file transfer, and the ability to tunnel other network traffic through it. It is how you manage nearly every Linux server, so a few hours spent understanding keys, the client config file, and tunnels pays off daily.

Prerequisites: [Networking Basics](./01-networking-basics.md) (ports, DNS) and [Permissions](../01-fundamentals/05-permissions.md) (SSH is strict about file modes).

## How it works

Two authentications happen, in this order:

```text
client                                   server (sshd, port 22)
  │ ── connect ───────────────────────────► │
  │ ◄── server's HOST key ───────────────── │  1. Is this really the server I meant?
  │     (checked against ~/.ssh/known_hosts)│
  │ ── prove identity with your KEY ──────► │  2. Is this user allowed in?
  │     (server checks authorized_keys)     │
  │ ◄══════ encrypted session ══════════════│
```

1. **Host authentication**: the server proves its identity with its *host key*. On first connection you're shown a fingerprint and asked to trust it; it's then saved in `~/.ssh/known_hosts`. This protects you from connecting to an impostor.
2. **User authentication**: you prove who you are, ideally with a **key pair** rather than a password.

A key pair has a **private key** (stays on your machine, never shared) and a **public key** (placed on servers). The server challenges you to prove you hold the private key without ever sending it.

## Connecting

```bash
ssh alice@203.0.113.10
ssh -p 2222 alice@host              # non-default port
ssh -i ~/.ssh/deploy_key alice@host # a specific key
ssh alice@host 'uptime; df -h /'    # run a command, then return
ssh -v alice@host                   # verbose; -vv / -vvv for more detail
```

## Keys

Generate a modern key (Ed25519 is small, fast, and the sensible default):

```bash
ssh-keygen -t ed25519 -C "alice@laptop"
```

Set a **passphrase** when prompted; it encrypts the private key on disk so a stolen laptop or copied file isn't an instant compromise. This creates:

```text
~/.ssh/id_ed25519        private key   (mode 600)
~/.ssh/id_ed25519.pub    public key    (safe to share)
```

Install the public key on a server:

```bash
ssh-copy-id -i ~/.ssh/id_ed25519.pub alice@host     # needs an existing way to log in
```

Or append it manually to `~/.ssh/authorized_keys` on the server. On cloud VMs, the provider usually injects a key you choose at creation time.

### Permissions SSH insists on

sshd (with the default `StrictModes yes`) refuses keys when files are too open:

```bash
chmod 700 ~/.ssh
chmod 600 ~/.ssh/authorized_keys
chmod 600 ~/.ssh/id_ed25519
# and your home directory must not be group- or world-writable
```

The client likewise refuses a private key that others can read (`UNPROTECTED PRIVATE KEY FILE!`) and a config file with bad ownership.

### ssh-agent: type the passphrase once

```bash
eval "$(ssh-agent -s)"       # start an agent (many desktops/shells already run one)
ssh-add ~/.ssh/id_ed25519    # unlock the key into the agent
ssh-add -l                   # list loaded keys
```

### Host key changes

If you see `WARNING: REMOTE HOST IDENTIFICATION HAS CHANGED!`, either the server was legitimately rebuilt/re-imaged, or someone is intercepting the connection. **Verify first** (ask the provider, or check the fingerprint via a console), then remove the stale entry:

```bash
ssh-keygen -R host.example.com
```

Don't make a habit of disabling host key checking; it removes the protection against impersonation.

## The client config: `~/.ssh/config`

Stop typing long commands. Define hosts once:

```text
Host web
    HostName 203.0.113.10
    User alice
    Port 22
    IdentityFile ~/.ssh/id_ed25519
    IdentitiesOnly yes

Host db-internal
    HostName 10.0.1.5
    User alice
    ProxyJump bastion

Host bastion
    HostName 198.51.100.7
    User jump

Host *
    ServerAliveInterval 60
    ServerAliveCountMax 3
    AddKeysToAgent yes
```

Now `ssh web`, `scp file web:/tmp/`, and `rsync ... web:/path` all work with short names.

Notes:

- For each option, the **first value found wins**, so put specific `Host` blocks before `Host *` defaults.
- `IdentitiesOnly yes` makes ssh use only the listed key instead of offering every key in your agent, which avoids `Too many authentication failures`.
- `ServerAliveInterval` sends keepalives so idle sessions aren't dropped by NAT or firewalls.
- `ProxyJump` (or `ssh -J bastion target`) hops through a **bastion host** to reach private machines, without exposing them to the internet. Prefer it over agent forwarding (`ssh -A`), which lets anyone with root on the intermediate host use your agent.

### Connection reuse

Opening a new SSH connection is slow for repeated commands or many `rsync`/`scp` calls. Multiplexing shares one connection:

```text
Host *
    ControlMaster auto
    ControlPath ~/.ssh/cm-%r@%h:%p
    ControlPersist 10m
```

## Copying files

```bash
scp report.pdf web:/tmp/                  # local to remote
scp web:/var/log/app.log .                # remote to local
scp -r ./site web:/var/www/               # directory
scp -P 2222 file host:/tmp/               # note: scp uses capital -P for port (ssh uses lowercase -p)

rsync -avz -e ssh ./site/ web:/var/www/site/    # only transfers differences; resumable-ish
sftp web                                        # interactive file transfer
```

Prefer **rsync** for anything repeated or large, and remember the trailing-slash rule from [cron and timers](../03-system/04-cron-and-timers.md). Modern OpenSSH (9.0+) makes `scp` use the SFTP protocol internally, but `rsync` still gives you much better control.

## Tunnels (port forwarding)

SSH can carry other traffic through its encrypted connection. This is how you reach a database or admin UI that is deliberately not exposed to the internet.

### Local forward: `-L` (bring a remote service to you)

```bash
ssh -L 5433:localhost:5432 web
#     │    │         │
#     │    └─────────┴── target, as seen FROM the server
#     └── port on YOUR machine
```

```text
 your laptop                       web (server)
 psql -p 5433 ─► :5433 ══ssh══► sshd ──► localhost:5432 (Postgres)
```

Now `psql -h localhost -p 5433` on your laptop talks to Postgres on the server, which can keep listening only on its own loopback. The target host in the middle is resolved by the **server**, so it can also be another machine only the server can reach (`-L 8080:10.0.1.20:80`).

Useful flags: `-N` (don't run a remote command, just forward), `-f` (go to background).

```bash
ssh -fN -L 3000:localhost:3000 web      # forward in the background
```

### Remote forward: `-R` (expose your local service on the server)

```bash
ssh -R 8080:localhost:3000 web
```

Connections to port 8080 **on the server** are carried back to port 3000 on your laptop. By default the server binds it to its loopback only; exposing it more widely needs `GatewayPorts` on the server, which you should think twice about.

### Dynamic forward: `-D` (a SOCKS proxy)

```bash
ssh -D 1080 web
```

Point a browser or tool at SOCKS proxy `localhost:1080` and its traffic exits from the server. Handy for reaching internal web UIs.

Local forwards bind to `127.0.0.1` by default, which is what you want. Binding to all interfaces (`-L 0.0.0.0:...`) shares your tunnel with the whole network.

## Server side

Server config is `/etc/ssh/sshd_config` (not `ssh_config`, which is the client's). Always validate and keep an escape route when changing it:

```bash
sudo sshd -t                       # test the config for errors
sudo systemctl reload ssh          # Ubuntu/Debian service name; "sshd" on Red Hat family
```

Keep your current session open and test a **new** login in a second terminal before closing the first. A broken config plus a closed session equals a locked-out server. Hardening settings (disable password login, restrict users, change defaults) are in [server hardening](../05-production/01-server-hardening.md).

If your session hangs after a network blip, you can kill it from the keyboard: press `Enter`, then `~`, then `.`.

## Debugging

Start with the client:

```bash
ssh -vvv alice@host
```

It shows which keys are tried, which config is applied, and where it fails. Then look at the server:

```bash
sudo journalctl -u ssh -n 50 --no-pager       # or: sudo tail /var/log/auth.log
```

| Error | Usual cause |
|---|---|
| `Permission denied (publickey)` | wrong user or key; public key missing from `authorized_keys`; bad permissions; key not offered (check `-v`) |
| `Connection refused` | sshd not running, wrong port, or nothing listening ([`ss -tlnp`](./01-networking-basics.md)) |
| `Connection timed out` | firewall or cloud security group dropping port 22, wrong IP ([firewall](./03-firewall.md)) |
| `Host key verification failed` | host key changed; verify, then `ssh-keygen -R` |
| `Too many authentication failures` | agent offers many keys; use `IdentitiesOnly yes` and `-i` |
| `Bad owner or permissions on ~/.ssh/config` | config must be owned by you and not writable by others (`chmod 600`) |
| Works with a password, not a key | server-side `~/.ssh/` permissions, SELinux context, or home dir writable by others |

## Common mistakes

- No passphrase on a key that sits on a laptop, or sharing one key across every person and machine. Use one key per person/device.
- Copying the **private** key instead of the `.pub` file to the server.
- Overly open permissions on `~/.ssh` or the home directory.
- Using `ssh -A` through machines you don't fully trust.
- Disabling `StrictHostKeyChecking` globally to silence warnings.
- Editing `sshd_config` and restarting without a second session open.
- Mixing up `scp -P` and `ssh -p`.
- Leaving long-lived tunnels or SOCKS proxies running and forgetting them.

## Quick Summary

- SSH authenticates the server (host key, `known_hosts`) and you (key pair, `authorized_keys`).
- Use `ssh-keygen -t ed25519` with a passphrase; get the public key onto the server with `ssh-copy-id`; mind the 700/600 permissions.
- Put hosts, users, keys, `ProxyJump`, and keepalives in `~/.ssh/config`.
- `-L` pulls a remote service to your machine, `-R` pushes yours out, `-D` is a SOCKS proxy.
- Use `rsync -e ssh` for transfers; reach private hosts through a bastion with `ProxyJump`.
- Debug with `ssh -vvv` and `journalctl -u ssh`; test config with `sshd -t` and keep a second session open.

**Next:** [Firewall](./03-firewall.md)
