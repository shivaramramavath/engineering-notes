<!--
================================================================================
Metadata:
  Title: Reference Project 08 - Private Container Registry with Pull-Through Cache and Web UI
  Track: Docker / DevOps
  Difficulty: Intermediate to Advanced
  Estimated Time: 55 minutes
  Prerequisites: 06-production/07-registries.md, 04-volumes-and-networking/01-volumes-and-bind-mounts.md
  Key Terminology: Docker Distribution, Pull-Through Cache, htpasswd Authentication, TLS Termination, Docker Registry UI, Harbor Alternative
================================================================================
-->

# Reference Project 08: Private Container Registry with Pull-Through Cache and Web UI

[<- 07-secure-hardened-image-pipeline.md](./07-secure-hardened-image-pipeline.md) | [Back to Projects Overview](./README.md) | [Next Module: 08-interview ->](../08-interview/README.md)

---

## 1. Overview & Architecture

Relying solely on external public registries (such as Docker Hub or Quay) introduces operational risks: rate limiting (Docker Hub 100/200 pulls per 6 hours), WAN latency on large image pulls across CI worker fleets, and security vulnerabilities from unauthenticated public storage.

This reference project provides a self-hosted, air-gapped capable container registry stack featuring:
1. **Authenticated Primary Registry**: TLS-encrypted private registry with `htpasswd` basic authentication.
2. **Pull-Through Cache Mirror**: Automatically caches pulled images from Docker Hub, accelerating downstream builds and mitigating rate limits.
3. **Registry Web UI**: Visual web interface for browsing repositories, tags, and manifests.

### Architectural Diagram

```
+-----------------------------------------------------------------------------------+
|                                  HOST MACHINE                                     |
|                                                                                   |
|    Developer / CI Pipeline                                                        |
|    - Pushes private images to: localhost:5000 (Auth Required)                     |
|    - Pulls cached hub images from: localhost:5001 (Mirror)                        |
|    - Browses catalog at: http://localhost:8080                                    |
|           |                                                                       |
|           v                                                                       |
|  +-----------------------------------------------------------------------------+  |
|  |                          DOCKER NETWORK: registry-net                       |  |
|  |                                                                             |  |
|  |   +---------------------------------------------------------------------+   |  |
|  |   |                       SERVICE: registry-primary                     |   |  |
|  |   |  Image: registry:2 (Port 5000, TLS Enabled, htpasswd Auth)          |   |  |
|  |   |  Storage Volume: primary_data (/var/lib/registry)                   |   |  |
|  |   +---------------------------------+-----------------------------------+   |  |
|  |                                     ^                                       |  |
|  |                                     | Manifest Inspection                   |  |
|  |   +---------------------------------+-----------------------------------+   |  |
|  |   |                       SERVICE: registry-ui                          |   |  |
|  |   |  Image: joxit/docker-registry-ui:latest (Port 8080)                 |   |  |
|  |   +---------------------------------------------------------------------+   |  |
|  |                                                                             |  |
|  |   +---------------------------------------------------------------------+   |  |
|  |   |                       SERVICE: registry-cache-mirror                |   |  |
|  |   |  Image: registry:2 (Port 5001)                                      |   |  |
|  |   |  Proxy Remote: https://registry-1.docker.io                         |   |  |
|  |   |  Storage Volume: cache_data (/var/lib/registry)                     |   |  |
|  |   +---------------------------------------------------------------------+   |  |
|  +-----------------------------------------------------------------------------+  |
+-----------------------------------------------------------------------------------+
```

---

## 2. Directory Structure & File Listing

```
local-registry-stack/
├── auth/
│   └── htpasswd
├── certs/
│   ├── generate-certs.sh
│   ├── registry.crt
│   └── registry.key
├── compose.yaml
└── config/
    ├── primary-config.yml
    └── mirror-config.yml
```

---

## 3. Complete Implementation

### File: `certs/generate-certs.sh`

```bash
#!/usr/bin/env sh
set -e

mkdir -p certs
openssl req -newkey rsa:4096 -nodes -sha256 -keyout certs/registry.key \
  -x509 -days 365 -out certs/registry.crt \
  -subj "/C=US/ST=DevOps/L=Local/O=Enterprise/CN=localhost" \
  -addext "subjectAltName=DNS:localhost,IP:127.0.0.1"

chmod 600 certs/registry.key
chmod 644 certs/registry.crt
echo "Registry TLS Certificates generated in ./certs/"
```

### File: `auth/generate-auth.sh`

```bash
#!/usr/bin/env sh
set -e

mkdir -p auth
# Creates htpasswd file with user 'admin' and password 'AdminSecretPassword2026!'
docker run --rm --entrypoint htpasswd httpd:2 -Bbn admin "AdminSecretPassword2026!" > auth/htpasswd
chmod 600 auth/htpasswd
echo "Created auth/htpasswd"
```

### File: `config/primary-config.yml`

```yaml
version: 0.1
log:
  level: info
  formatter: text
storage:
  cache:
    blobdescriptor: inmemory
  filesystem:
    rootdirectory: /var/lib/registry
  delete:
    enabled: true
http:
  addr: :5000
  tls:
    certificate: /certs/registry.crt
    key: /certs/registry.key
  headers:
    X-Content-Type-Options: [nosniff]
    Access-Control-Allow-Origin: ['*']
    Access-Control-Allow-Methods: ['HEAD', 'GET', 'OPTIONS', 'DELETE']
    Access-Control-Allow-Headers: ['Authorization', 'Accept']
    Access-Control-Max-Age: [1728000]
    Access-Control-Allow-Credentials: [true]
    Access-Control-Expose-Headers: ['Docker-Content-Digest']
auth:
  htpasswd:
    realm: Registry Realm
    path: /auth/htpasswd
```

### File: `config/mirror-config.yml`

```yaml
version: 0.1
log:
  level: info
storage:
  cache:
    blobdescriptor: inmemory
  filesystem:
    rootdirectory: /var/lib/registry
http:
  addr: :5000
proxy:
  remoteurl: https://registry-1.docker.io
```

### File: `compose.yaml`

```yaml
version: "3.8"

services:
  # Primary Authenticated Registry
  registry-primary:
    image: registry:2.8.3
    restart: unless-stopped
    ports:
      - "5000:5000"
    environment:
      REGISTRY_CONFIGURATION_PATH: /etc/docker/registry/config.yml
    volumes:
      - ./config/primary-config.yml:/etc/docker/registry/config.yml:ro
      - ./certs:/certs:ro
      - ./auth:/auth:ro
      - primary_registry_data:/var/lib/registry
    networks:
      - registry-net
    deploy:
      resources:
        limits:
          cpus: "1.0"
          memory: 512M

  # Registry Web Interface
  registry-ui:
    image: joxit/docker-registry-ui:2.5.7
    restart: unless-stopped
    ports:
      - "8080:80"
    environment:
      - SINGLE_REGISTRY=true
      - REGISTRY_TITLE=Private Container Registry
      - REGISTRY_URL=https://registry-primary:5000
      - DELETE_IMAGES=true
    depends_on:
      - registry-primary
    networks:
      - registry-net
    deploy:
      resources:
        limits:
          cpus: "0.5"
          memory: 128M

  # Pull-Through Cache Mirror for Docker Hub
  registry-cache-mirror:
    image: registry:2.8.3
    restart: unless-stopped
    ports:
      - "5001:5000"
    environment:
      REGISTRY_CONFIGURATION_PATH: /etc/docker/registry/config.yml
    volumes:
      - ./config/mirror-config.yml:/etc/docker/registry/config.yml:ro
      - mirror_cache_data:/var/lib/registry
    networks:
      - registry-net
    deploy:
      resources:
        limits:
          cpus: "1.0"
          memory: 512M

networks:
  registry-net:
    driver: bridge

volumes:
  primary_registry_data:
  mirror_cache_data:
```

---

## 4. Step-by-Step Verification & Testing

### Step 1: Initialize Certificates, Auth, and Launch

```bash
mkdir -p local-registry-stack/certs local-registry-stack/auth local-registry-stack/config
cd local-registry-stack

# Generate TLS Certs and Auth
chmod +x certs/generate-certs.sh auth/generate-auth.sh
./certs/generate-certs.sh
./auth/generate-auth.sh

# Trust certificate locally on Linux (or configure insecure-registries)
sudo cp certs/registry.crt /usr/local/share/ca-certificates/localhost-registry.crt
sudo update-ca-certificates || true

# Start Stack
docker compose up -d
docker compose ps
```

### Step 2: Test Authentication on Primary Registry

Attempt push without authentication:
```bash
docker tag alpine:3.19 localhost:5000/my-alpine:1.0
docker push localhost:5000/my-alpine:1.0
# Output: Error response from daemon: Get "https://localhost:5000/v2/": unauthorized: authentication required
```

Log in with generated credentials:
```bash
echo "AdminSecretPassword2026!" | docker login localhost:5000 -u admin --password-stdin
# Output: Login Succeeded
```

Push image:
```bash
docker push localhost:5000/my-alpine:1.0
# Output:
# The push refers to repository [localhost:5000/my-alpine]
# 7c73a0279d01: Pushed
# 1.0: digest: sha256:... size: 528
```

### Step 3: Browse Catalog via Web UI

Open browser to `http://localhost:8080`.
The UI displays the repository `my-alpine`, its tags, and digest sizes.

### Step 4: Test Pull-Through Cache Mirror

Pull an image through the local cache on port 5001:
```bash
# 1. Pull directly through local mirror
docker pull localhost:5001/library/busybox:latest

# 2. Inspect mirror data volume on host
docker exec $(docker compose ps -q registry-cache-mirror) ls -la /var/lib/registry/docker/registry/v2/repositories/library/
# Output confirms busybox blobs are stored locally in the mirror!
```

Subsequent pulls for `library/busybox:latest` across all internal nodes now serve directly from local NVMe/SSD cache at gigabit speeds without hitting Docker Hub's internet endpoint.

### Step 5: Teardown

```bash
docker compose down -v
```

---

## 5. Production Considerations

1. **Storage Backends**: By default, Docker registry stores blobs in local filesystem paths (`/var/lib/registry`). For cloud production, configure S3 or GCS storage backends in `config.yml`:
   ```yaml
   storage:
     s3:
       accesskey: "AWS_ACCESS_KEY"
       secretkey: "AWS_SECRET_KEY"
       region: "us-east-1"
       bucket: "my-enterprise-registry-bucket"
   ```
2. **Garbage Collection**: Deleting tags via API or Web UI leaves orphaned blobs on disk. Periodically execute garbage collection inside the registry container:
   ```bash
   docker exec registry-primary bin/registry garbage-collect /etc/docker/registry/config.yml
   ```

---

[<- 07-secure-hardened-image-pipeline.md](./07-secure-hardened-image-pipeline.md) | [Back to Projects Overview](./README.md) | [Next Module: 08-interview ->](../08-interview/README.md)
