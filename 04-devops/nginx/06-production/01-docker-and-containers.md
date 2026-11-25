# Docker and Kubernetes

## Concept
In containers, nginx behaves the same, but how you supply config, handle logs, resolve backends, and reload changes. This file covers running nginx in Docker and Docker Compose, and the main patterns in Kubernetes, including what changed with the Kubernetes **ingress-nginx** project.

**Prerequisites:** [installation.md](../01-fundamentals/installation.md), [reverse-proxy.md](../02-core-features/reverse-proxy.md), [operations.md](operations.md)

---

# Part 1: Docker

## The Official Image
```bash
docker run -d --name web -p 8080:80 nginx:stable
```
| Tag family | Use |
|---|---|
| `nginx:stable`, `nginx:stable-alpine` | Production default (bug fixes only). Alpine is smaller |
| `nginx:mainline` | Newest features |
| `nginxinc/nginx-unprivileged` | Runs as non-root, listens on **8080** by default |

Pin to a specific version tag in production (e.g. `nginx:1.28`), and rebuild regularly for security fixes.

## Container-Specific Behavior
- nginx runs in the **foreground** (`daemon off;`). The container lives as long as the master process.
- Logs go to **stdout/stderr** (the log files are symlinks to `/dev/stdout` and `/dev/stderr`), so use `docker logs web`. Don't rely on file-based log rotation.
- Config lives in `/etc/nginx/nginx.conf` and `/etc/nginx/conf.d/*.conf`. The default site is `conf.d/default.conf`.
- Static files default to `/usr/share/nginx/html`.

## Supplying Config

### Bind-mount (development, simple setups)
```bash
docker run -d -p 8080:80 \
  -v "$PWD/nginx.conf":/etc/nginx/nginx.conf:ro \
  -v "$PWD/site":/usr/share/nginx/html:ro \
  nginx:stable
```
Mounting a **single config file** and editing it on the host can leave the container with a stale inode on some editors/filesystems. If edits do not appear, mount the directory instead.

### Bake into an image (production)
```dockerfile
# Build the app, then serve it with nginx
FROM node:22-alpine AS build
WORKDIR /app
COPY . .
RUN npm ci && npm run build

FROM nginx:stable-alpine
COPY nginx/default.conf /etc/nginx/conf.d/default.conf
COPY --from=build /app/dist /usr/share/nginx/html
```
An immutable image gives the same config in every environment. Run `nginx -t` in CI:
```bash
docker run --rm -v "$PWD/nginx":/etc/nginx/conf.d:ro nginx:stable nginx -t
```

### Environment variables via templates
The official image processes `*.template` files in `/etc/nginx/templates/` at start, substituting **environment variables** and writing results to `/etc/nginx/conf.d/`.
```nginx
# /etc/nginx/templates/default.conf.template
server {
    listen 80;
    location / { proxy_pass http://${BACKEND_HOST}:${BACKEND_PORT}; }
}
```
```bash
docker run -d -e BACKEND_HOST=app -e BACKEND_PORT=3000 \
  -v "$PWD/templates":/etc/nginx/templates:ro nginx:stable
```
nginx's own variables (`$host`, `$uri`) are left untouched, because only defined environment variables are substituted.

## Docker Compose Example
```yaml
services:
  app:
    image: myorg/app:1.4
    expose: ["3000"]

  nginx:
    image: nginx:stable-alpine
    ports: ["80:80", "443:443"]
    volumes:
      - ./nginx/conf.d:/etc/nginx/conf.d:ro
      - ./certs:/etc/nginx/certs:ro
    depends_on: [app]
```
```nginx
# nginx/conf.d/default.conf
server {
    listen 80;
    location / {
        proxy_pass http://app:3000;          # service name resolves via Docker DNS
        proxy_set_header Host $host;
        proxy_set_header X-Forwarded-Proto $scheme;
    }
}
```
Only nginx publishes ports. The app is reachable only on the internal network, as recommended in [security-hardening.md](../05-security-performance/security-hardening.md).

## The DNS Gotcha in Containers
nginx resolves hostnames in `proxy_pass` and `upstream` **once, at startup or reload**.
- If `app` is not running when nginx starts, nginx fails with `host not found in upstream` and exits.
- If `app` is recreated with a new IP, nginx keeps sending to the old address (502 errors) until reloaded.

Fixes:
```nginx
# Re-resolve at runtime using Docker's embedded DNS (127.0.0.11)
resolver 127.0.0.11 valid=10s;
location / {
    set $backend http://app:3000;
    proxy_pass $backend;               # variable forces runtime resolution
}
```
Or start nginx only after backends are healthy (`depends_on` with `condition: service_healthy`), and reload nginx after redeploying backends. Note that using a variable in `proxy_pass` changes URI handling (no automatic prefix replacement) and bypasses `upstream` groups.

## Reload, Test, and Debug
```bash
docker exec web nginx -t                 # validate the running container's config
docker exec web nginx -s reload          # graceful reload, no restart
docker logs -f web                       # access and error logs
docker exec web nginx -T | less          # effective config
docker exec -it web sh                   # shell (Alpine: sh, Debian: bash)
```
A restart (`docker restart`) drops connections, so prefer `reload` for config changes. With Compose: `docker compose exec nginx nginx -s reload`.

## TLS in Docker
- Mount certificates read-only from the host, a volume, or a secret manager.
- Certbot can run as a separate container sharing a volume with nginx. Reload nginx after renewal (`docker exec` in the deploy hook, or a scheduled reload).
- Or terminate TLS at a cloud load balancer or an ingress, and keep nginx on plain HTTP internally.

## Client IP in Containers
With Docker's default userland proxy or when nginx sits behind a load balancer, `$remote_addr` may show the gateway or load balancer IP. Use the `realip` module with only trusted ranges ([rate-limiting.md](../03-traffic-management/rate-limiting.md)), or the load balancer's proxy-protocol / forwarded-header feature.

## Running as Non-Root
Use `nginxinc/nginx-unprivileged`, or ensure the image config:
- Listens on a port above 1024 (e.g. 8080).
- Writes its PID and temp files to writable paths:
```nginx
pid /tmp/nginx.pid;
http {
    client_body_temp_path /tmp/client_temp;
    proxy_temp_path       /tmp/proxy_temp;
    fastcgi_temp_path     /tmp/fastcgi_temp;
}
```
Combine with `read_only: true` and a writable `tmpfs` for `/tmp` and cache directories, plus dropped capabilities.

## Health Checks
```yaml
healthcheck:
  test: ["CMD", "wget", "-qO-", "http://127.0.0.1/healthz"]
  interval: 10s
  timeout: 3s
  retries: 3
```
(Alpine images include `wget`. Debian-based images may not include `curl` by default.)

---

# Part 2: Kubernetes

## Choosing a Pattern

| Pattern | Use when |
|---|---|
| **nginx as a Deployment + Service + ConfigMap** | You want a normal nginx (static site, custom reverse proxy, API gateway) with config you control |
| **Sidecar nginx** | One pod needs its own proxy, TLS terminator, or auth layer in front of the app container |
| **Ingress controller or Gateway API implementation** | You want cluster-wide HTTP routing from Kubernetes resources (Ingress, HTTPRoute) |

Writing your own nginx config in a Deployment (first pattern) is entirely supported and unaffected by the ingress changes below.

## Pattern 1: nginx Deployment with a ConfigMap
```yaml
apiVersion: v1
kind: ConfigMap
metadata: { name: nginx-conf }
data:
  default.conf: |
    server {
      listen 8080;
      location /healthz { return 200 "ok\n"; }
      location / { proxy_pass http://my-app.default.svc.cluster.local:3000; }
    }
---
apiVersion: apps/v1
kind: Deployment
metadata: { name: nginx }
spec:
  replicas: 2
  selector: { matchLabels: { app: nginx } }
  template:
    metadata:
      labels: { app: nginx }
      annotations:
        checksum/config: "<hash of the ConfigMap>"   # change forces a rollout
    spec:
      containers:
        - name: nginx
          image: nginxinc/nginx-unprivileged:stable
          ports: [{ containerPort: 8080 }]
          volumeMounts:
            - { name: conf, mountPath: /etc/nginx/conf.d, readOnly: true }
          readinessProbe:
            httpGet: { path: /healthz, port: 8080 }
          livenessProbe:
            httpGet: { path: /healthz, port: 8080 }
          resources:
            requests: { cpu: 100m, memory: 64Mi }
            limits:   { memory: 128Mi }
      volumes:
        - name: conf
          configMap: { name: nginx-conf }
---
apiVersion: v1
kind: Service
metadata: { name: nginx }
spec:
  selector: { app: nginx }
  ports: [{ port: 80, targetPort: 8080 }]
```

### Config changes
- Editing a ConfigMap updates mounted files after a delay, but **nginx does not reload by itself**.
- Simplest and safest: change a pod-template annotation (like a config checksum, which Helm and Kustomize can generate) so the Deployment performs a **rolling update**.
- Alternative: a sidecar or controller that watches the file and runs `nginx -s reload`.
- Validate config in CI (`nginx -t` in a container) before applying it.

### DNS in Kubernetes
- Use Service DNS names (`my-app.default.svc.cluster.local`). nginx still resolves at startup, but Service IPs are stable, so this is usually fine.
- For headless services or pod IPs that change, use `resolver kube-dns.kube-system.svc.cluster.local valid=10s;` (check your cluster's DNS service name) with a variable in `proxy_pass`.
- Pods that start before the Service exists fail with `host not found in upstream` and restart, so use readiness checks and sensible startup ordering.

### Client IP and scaling
- Set `externalTrafficPolicy: Local` on a LoadBalancer Service to preserve source IPs, or use proxy protocol / `X-Forwarded-For` from the cloud load balancer with `real_ip` configured.
- Rate-limit state is **per pod**, so with N replicas, the effective limit is roughly N times the configured one unless you account for it.
- Cache zones are per pod as well. Use `emptyDir` for cache directories and set `max_size` below the volume limit.

### Graceful shutdown
```yaml
lifecycle:
  preStop:
    exec: { command: ["/bin/sh", "-c", "sleep 5; nginx -s quit"] }
terminationGracePeriodSeconds: 30
```
The short sleep gives the endpoint removal time to propagate before nginx begins draining.

## Pattern 2: Ingress Controllers and Gateway API

### Important: ingress-nginx has been retired
The Kubernetes community **ingress-nginx** controller (the project at `kubernetes/ingress-nginx`) was announced for retirement by Kubernetes SIG Network and the Security Response Committee, with best-effort maintenance only until **March 2026**. After that date it receives no new releases, bug fixes, or security patches. Existing installations keep running, but they are unsupported and will not get fixes for new vulnerabilities.

What to do:
- **New clusters:** do not choose community ingress-nginx.
- **Existing clusters:** plan migration. Kubernetes recommends the **Gateway API**, or another maintained Ingress controller listed in the Kubernetes documentation.
- Check the current status in the Kubernetes announcement before deciding: <https://kubernetes.io/blog/2025/11/11/ingress-nginx-retirement/>.

Be careful not to confuse these separate projects:

| Project | What it is |
|---|---|
| `kubernetes/ingress-nginx` | Community controller, the one that was retired |
| F5 NGINX Ingress Controller | Separate controller maintained by F5/NGINX. The retirement notice does not describe it, so check its own documentation for support status |
| NGINX Gateway Fabric | F5/NGINX implementation of the Gateway API |
| Your own nginx Deployment (Pattern 1) | Ordinary nginx you configure, which is unaffected |

### Gateway API in brief
Gateway API replaces annotations-heavy Ingress with typed resources: `GatewayClass` (which implementation), `Gateway` (listeners, ports, TLS), and `HTTPRoute` (host/path routing to Services). Capabilities that ingress-nginx exposed through annotations (rate limits, timeouts, header rules) map to route filters or implementation-specific policies, so review each annotation you use during migration.

### Migration checklist
1. Inventory Ingress resources and the annotations they depend on (rewrites, auth, rate limits, timeouts, snippets).
2. Choose a Gateway API implementation (or alternative Ingress controller) that supports those features.
3. Deploy it alongside the old controller with a separate class and address.
4. Recreate routes, test with `curl --resolve` against the new address, then shift DNS or load-balancer traffic gradually.
5. Remove the old controller after traffic has moved.

## Troubleshooting in Containers
```bash
kubectl logs deploy/nginx -f
kubectl exec deploy/nginx -- nginx -t
kubectl exec deploy/nginx -- nginx -T | less
kubectl describe pod <pod>                       # probe failures, OOMKilled, mount errors
kubectl get endpoints my-app                     # does the Service have backends?
kubectl run tmp --rm -it --image=curlimages/curl -- sh   # test from inside the cluster
```

| Symptom | Likely cause |
|---|---|
| CrashLoopBackOff, `host not found in upstream` | Backend Service name wrong or not yet created |
| 502 from nginx | Service has no ready endpoints, wrong `targetPort`, backend not listening on the pod IP (bound to 127.0.0.1 only) |
| Config change not applied | ConfigMap updated but pods not restarted or reloaded |
| `Permission denied` on start | Non-root container writing to a read-only path (use `/tmp` paths) |
| OOMKilled | Memory limit too low for buffers and cache zones ([performance-tuning.md](../05-security-performance/performance-tuning.md)) |
| Wrong client IP in logs | Missing `externalTrafficPolicy: Local` or `real_ip` configuration |

## Common Mistakes
- Using `latest` tags, so deployments change unexpectedly.
- Expecting a ConfigMap edit to reload nginx automatically.
- Letting nginx start before its backend exists, or never re-resolving after backend IPs change.
- Running as root with a writable root filesystem.
- Per-pod rate limits and caches treated as global.
- Logging to files inside a container instead of stdout/stderr.
- Starting a new project on community ingress-nginx after its retirement.
- Binding backends to `127.0.0.1` inside their own container, so nginx in another container or pod cannot reach them.

## Related / Next
- [operations.md](operations.md): reload, upgrade, and monitoring principles that still apply
- [troubleshooting.md](troubleshooting.md): error messages and diagnosis
- [spa-with-api-proxy](../07-projects/spa-with-api-proxy.md): a complete frontend + API example
