# Nginx for Node.js Apps

Nginx sits in front of a Node.js app in almost every real deployment. This folder covers it as its own topic, split by concern rather than as one long file.

## In this folder

| File                                 | Covers                                                                                              |
| ------------------------------------ | --------------------------------------------------------------------------------------------------- |
| `01-reverse-proxy-basics.md`         | Why put Nginx in front of Node at all, `proxy_pass`, essential headers, and Express's `trust proxy` |
| `02-tls-https.md`                    | HTTPS termination at Nginx, and getting a free certificate with Certbot/Let's Encrypt               |
| `03-static-files-and-compression.md` | Serving static assets directly from Nginx, and gzip/Brotli compression                              |
| `04-caching.md`                      | Nginx-level response caching (`proxy_cache`) and browser caching headers                            |
| `05-load-balancing.md`               | Distributing requests across multiple Node instances with `upstream`                                |
| `06-rate-limiting.md`                | Stopping abusive traffic before it reaches Node, with `limit_req`                                   |
| `07-commands-and-troubleshooting.md` | The commands you'll actually run, and how to debug a broken config                                  |

## Why Node needs this at all

Node can serve HTTP directly (`02-core-modules/03-http.md`) — Nginx isn't strictly required. But Nginx does several things far better than Node should have to: terminating TLS, serving static files efficiently, load balancing across instances, buffering slow clients, and providing a single well-audited layer for security headers and rate limiting.

```
Client → Nginx (port 80/443) → Node.js app (port 3000, plain HTTP)
```

## Install

```bash
# Debian/Ubuntu
sudo apt update && sudo apt install nginx

# macOS
brew install nginx
```

```bash
sudo systemctl status nginx
sudo systemctl enable nginx     # start on boot
```

Config lives at `/etc/nginx/` on Linux — `/etc/nginx/nginx.conf` is the main file, with per-site configs typically in `/etc/nginx/sites-available/`, symlinked into `/etc/nginx/sites-enabled/`.

## What you should be able to do after this folder

- Reverse-proxy a Node app through Nginx, with the headers Node needs to see the real client correctly
- Terminate HTTPS at Nginx with a free Let's Encrypt certificate
- Serve static files and compressed responses directly from Nginx, bypassing Node
- Cache responses at the Nginx layer, and set correct browser caching headers
- Load-balance across multiple Node instances
- Rate-limit at the Nginx layer, as a first line of defense before requests reach your app
- Diagnose a broken config and reload safely without dropping connections

## Next

This ties directly into `16-production/03-docker-and-compose.md` and `16-production/04-nginx.md` — a typical production setup runs Nginx either as its own container in front of your app container(s), or as a managed load balancer in front of a container orchestrator.
