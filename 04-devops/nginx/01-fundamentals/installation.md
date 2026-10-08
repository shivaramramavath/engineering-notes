# Installing nginx

## Concept
Get a working nginx, know where its files live, and confirm it runs.

**Prerequisites:** a Linux/macOS shell and `sudo` access (or Docker).

## Install Options

### Package manager (recommended to start)
```bash
# Debian / Ubuntu
sudo apt update && sudo apt install nginx

# RHEL / Rocky / Fedora
sudo dnf install nginx

# macOS (Homebrew)
brew install nginx
```

### Docker
```bash
docker run -d --name web -p 8080:80 nginx:stable
# Mount your own config:
docker run -d -p 8080:80 -v $PWD/nginx.conf:/etc/nginx/nginx.conf:ro nginx:stable
```

### From source
Only needed for custom modules or specific compile flags.
```bash
./configure --with-http_ssl_module --with-http_v2_module
make && sudo make install
nginx -V   # shows the flags your binary was built with
```

> **Stable vs mainline:** stable receives only bug fixes; mainline gets new features first. Use stable for production unless you need a newer feature.

## Directory Layout (package install)

| Path | Purpose |
|---|---|
| `/etc/nginx/nginx.conf` | Main config file |
| `/etc/nginx/conf.d/*.conf` | Drop-in server configs (RHEL-style, also used in Docker) |
| `/etc/nginx/sites-available/`, `sites-enabled/` | Debian/Ubuntu convention: enable a site by symlinking |
| `/var/log/nginx/` | `access.log` and `error.log` |
| `/usr/share/nginx/html` or `/var/www/html` | Default web root |
| `/run/nginx.pid` | Master process PID |

Exact paths vary by distro. Run `nginx -V` and look at `--conf-path`, `--error-log-path`.

## First Run
```bash
sudo systemctl enable --now nginx   # start now and on boot
systemctl status nginx
curl -I http://localhost            # expect HTTP/1.1 200 OK
```

## Verify and Control
```bash
sudo nginx -t                  # test config syntax
sudo nginx -s reload           # apply config without dropping connections
sudo systemctl restart nginx   # full restart (drops connections)
```

## Common Mistakes
- **Port 80 already in use:** another web server (Apache) is running. Check with `sudo ss -ltnp | grep :80`.
- **Editing config but forgetting to reload:** changes are not applied until reload.
- **Firewall blocking 80/443:** open them (`ufw allow 'Nginx Full'` or firewalld equivalent).
- **SELinux denying proxy or file access (RHEL):** see [troubleshooting](../06-production/troubleshooting.md).

## Related / Next
- [config-structure.md](config-structure.md): how the config file is organized
