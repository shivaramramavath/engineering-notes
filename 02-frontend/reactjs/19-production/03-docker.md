# Docker

**Docker** packages an application and everything it needs to run (OS libraries, web server, config) into an **image** that runs the same way anywhere. For a frontend SPA that's just static files, you often don't need it: a static host or CDN is simpler and cheaper ([deployment](./02-deployment.md)). But Docker is the right tool when:

- Your organization **deploys everything as containers** (ECS, Cloud Run, Kubernetes, Fly, a VM running Docker).
- You want **identical** build and runtime environments across machines.
- You need a **custom server** (specific headers, a proxy to the API, auth in front of the SPA).
- You're running the whole stack locally with Docker Compose.

If none of those apply, skip it. Containers add operational weight.

## The idea: multi-stage builds

Building the app needs Node and hundreds of MB of `node_modules`. *Serving* it needs only a web server and the built files. A **multi-stage Dockerfile** uses one stage to build and a second, tiny stage to serve, leaving all build tooling out of the final image:

```text
Stage 1 "build":  node image → npm ci → npm run build  → /app/dist
                                                            │ copy only dist/
Stage 2 "serve":  nginx image ←──────────────────────────────┘     (final image: ~50 MB, no Node, no source)
```

## A production Dockerfile

```dockerfile
# ---- Stage 1: build ----
FROM node:24-alpine AS build
WORKDIR /app

# Install dependencies first: this layer is cached until package files change
COPY package.json package-lock.json ./
RUN npm ci

# Then copy source and build
COPY . .
ARG VITE_API_URL=/api
ENV VITE_API_URL=$VITE_API_URL
RUN npm run build

# ---- Stage 2: serve ----
FROM nginx:alpine AS serve
COPY nginx.conf /etc/nginx/conf.d/default.conf
COPY --from=build /app/dist /usr/share/nginx/html
EXPOSE 80
CMD ["nginx", "-g", "daemon off;"]
```

Use a **current LTS** Node version, matching your `.nvmrc` ([build reproducibility](./01-build.md#reproducible-builds)). Pin it as specifically as you're comfortable with.

Why it's structured this way:

- **Dependency layer first.** Docker caches each instruction as a layer. Copying only `package*.json` and running `npm ci` *before* copying the source means dependency installation is cached until the lockfile changes, which makes rebuilds fast.
- **`npm ci`**, not `npm install`: exact, reproducible installs.
- **Final image contains only nginx and `dist/`**: no source code, no Node, no `node_modules`, a small attack surface.
- **`ARG`/`ENV` for build-time values** like `VITE_API_URL` (they're baked into the bundle, [00](./00-environment-variables.md#build-time-vs-runtime-configuration)).

### `.dockerignore`

Keep junk out of the build context (faster, safer):

```text
node_modules
dist
.git
.env
.env.*
!.env.example
Dockerfile
docker-compose*.yml
coverage
playwright-report
test-results
```

Excluding `node_modules` matters: the image should install its own for the container's OS. Excluding `.env*` prevents local secrets from being copied into image layers.

## The nginx config

```nginx
# nginx.conf
server {
  listen 80;
  server_name _;
  root /usr/share/nginx/html;
  index index.html;

  gzip on;
  gzip_types text/css application/javascript application/json image/svg+xml;
  gzip_min_length 1024;

  # Hashed assets: cache forever; a missing file is a real 404 (no HTML fallback)
  location /assets/ {
    add_header Cache-Control "public, max-age=31536000, immutable";
    try_files $uri =404;
  }

  # Runtime config: never cache
  location = /config.js {
    add_header Cache-Control "no-cache";
  }

  # Everything else: SPA fallback; never cache index.html
  location / {
    add_header Cache-Control "no-cache";
    try_files $uri /index.html;
  }

  # Optional: proxy the API (same-origin, no CORS)
  # location /api/ {
  #   proxy_pass http://backend:3000/;
  #   proxy_set_header Host $host;
  #   proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
  # }
}
```

An nginx gotcha that bites security headers: **`add_header` directives in a `location` replace (not add to) those defined at the `server` level.** If you set security headers at server level and then `add_header Cache-Control` in a location, the security headers vanish for that location. Either repeat them in every location, or put shared headers in an `include` file used by each location. Verify with `curl -I`. See [security headers](./05-security.md#security-headers).

Brotli requires an additional nginx module and isn't in the stock image. Often the CDN or load balancer in front handles compression.

## Build and run

```bash
docker build -t acme-web:1.0.0 .
docker run --rm -p 8080:80 acme-web:1.0.0
# open http://localhost:8080 and test a deep link + refresh
```

Pass build-time config with `--build-arg`:

```bash
docker build --build-arg VITE_API_URL=https://api.example.com -t acme-web:1.0.0 .
```

### Tagging

Tag images with something **traceable**: the **git commit SHA** and/or a semantic version. Avoid relying on `latest` in production, because you can't tell what it is or roll back to it.

```bash
docker build -t registry.example.com/acme-web:$(git rev-parse --short HEAD) .
```

## Runtime configuration

A `VITE_*` variable passed via `ARG` is **baked in at build time**, so one image per environment. To build **once** and configure at **container start** ([runtime config](./00-environment-variables.md#runtime-configuration)), generate `config.js` from container environment variables when the container boots. The official nginx image runs any executable scripts found in `/docker-entrypoint.d/` before starting:

```sh
# docker/40-config.sh
#!/bin/sh
set -eu
cat > /usr/share/nginx/html/config.js <<EOF
window.__CONFIG__ = {
  API_URL: "${API_URL:-/api}",
  ENV: "${APP_ENV:-production}",
  SENTRY_DSN: "${SENTRY_DSN:-}"
};
