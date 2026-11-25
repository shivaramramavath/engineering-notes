<!--
================================================================================
Metadata:
  Title: Reference Project 05 - Minimal Multi-Stage Go Microservice with Scratch/Distroless
  Track: Docker / DevOps
  Difficulty: Advanced
  Estimated Time: 50 minutes
  Prerequisites: 03-dockerfile/04-multi-stage-builds.md, 03-dockerfile/08-base-image-choice.md
  Key Terminology: Go Static Binary, CGO_ENABLED=0, Scratch Image, Distroless Static, ca-certificates, Stripped Symbols, Zero Vulnerability Image
================================================================================
-->

# Reference Project 05: Minimal Multi-Stage Go Microservice with Scratch/Distroless

[<- 04-nginx-reverse-proxy-with-tls-local.md](./04-nginx-reverse-proxy-with-tls-local.md) | [Back to Projects Overview](./README.md) | [Next Project: 06-full-dev-environment-with-compose-watch.md ->](./06-full-dev-environment-with-compose-watch.md)

---

## 1. Overview & Architecture

In cloud-native computing, container image size directly impacts attack surface, network transfer time, autoscaling cold-start latency, and storage costs. A typical Node.js or Python container image ranges from 200MB to 1GB. In contrast, compiled languages like Go can produce completely self-contained, statically linked binaries that run inside Docker's empty `scratch` image or Google's `gcr.io/distroless/static`.

This reference project demonstrates:
1. Static compilation without CGO (`CGO_ENABLED=0`).
2. Stripping debug information and symbol tables (`-ldflags="-s -w"`).
3. Injecting CA certificates, zoneinfo timezone data, and an unprivileged `/etc/passwd` entry into a `scratch` runtime.
4. Achieving a final image size of **under 15MB** with **zero known CVE vulnerabilities**.

### Architectural Diagram

```
+-----------------------------------------------------------------------------------+
|                            MULTI-STAGE BUILD PIPELINE                             |
|                                                                                   |
|  STAGE 1: BUILDER (golang:1.22-alpine) [Size: ~300MB]                             |
|  +-----------------------------------------------------------------------------+  |
|  | 1. Install ca-certificates & tzdata                                         |  |
|  | 2. Create unprivileged appuser (UID: 10001) in /etc/passwd                  |  |
|  | 3. Mount Go module cache (/go/pkg/mod) & build cache (/root/.cache/go-build)   |  |
|  | 4. Compile with: CGO_ENABLED=0 GOOS=linux GOARCH=amd64                     |  |
|  |    go build -ldflags="-s -w -X main.version=1.0.0" -o /bin/api-server       |  |
|  +-----------------------------------------------------------------------------+  |
|                                          |                                        |
|                          Only copies necessary artifacts                          |
|                                          v                                        |
|  STAGE 2: RUNTIME (scratch or distroless:static) [FINAL SIZE: ~12MB]              |
|  +-----------------------------------------------------------------------------+  |
|  | - /etc/ssl/certs/ca-certificates.crt (Enables outbound HTTPS/TLS requests)  |  |
|  | - /usr/share/zoneinfo (Enables timezone conversions)                        |  |
|  | - /etc/passwd (Declares unprivileged user 10001)                            |  |
|  | - /bin/api-server (Statically linked stripped ELF binary)                   |  |
|  |                                                                             |  |
|  | No shell (/bin/sh). No package manager. Zero OS CVE attack surface.         |  |
|  +-----------------------------------------------------------------------------+  |
+-----------------------------------------------------------------------------------+
```

---

## 2. Directory Structure & File Listing

```
minimal-go-microservice/
├── .dockerignore
├── compose.yaml
├── Dockerfile
├── go.mod
├── main.go
└── Makefile
```

---

## 3. Complete Implementation

### File: `go.mod`

```go
module github.com/company/minimal-go-api

go 1.22
```

### File: `main.go`

```go
package main

import (
	"context"
	"encoding/json"
	"fmt"
	"log"
	"net/http"
	"os"
	"os/signal"
	"syscall"
	"time"
)

// Version injected during build via -ldflags
var version = "dev"

type HealthResponse struct {
	Status    string    `json:"status"`
	Version   string    `json:"version"`
	Timestamp time.Time `json:"timestamp"`
	UptimeSec float64   `json:"uptime_sec"`
}

type User struct {
	ID   int    `json:"id"`
	Name string `json:"name"`
	Role string `json:"role"`
}

var startTime = time.Now()

func main() {
	port := os.Getenv("PORT")
	if port == "" {
		port = "8080"
	}

	mux := http.NewServeMux()

	// Health Check Endpoint
	mux.HandleFunc("GET /healthz", func(w http.ResponseWriter, r *http.Request) {
		resp := HealthResponse{
			Status:    "OK",
			Version:   version,
			Timestamp: time.Now().UTC(),
			UptimeSec: time.Since(startTime).Seconds(),
		}
		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(http.StatusOK)
		json.NewEncoder(w).Encode(resp)
	})

	// Sample REST API Endpoint
	mux.HandleFunc("GET /api/users", func(w http.ResponseWriter, r *http.Request) {
		users := []User{
			{ID: 1, Name: "Alice SRE", Role: "Admin"},
			{ID: 2, Name: "Bob Developer", Role: "Engineer"},
		}
		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(users)
	})

	server := &http.Server{
		Addr:         ":" + port,
		Handler:      mux,
		ReadTimeout:  5 * time.Second,
		WriteTimeout: 10 * time.Second,
		IdleTimeout:  15 * time.Second,
	}

	// Channel for listening to OS shutdown signals
	stopChan := make(chan os.Signal, 1)
	signal.Notify(stopChan, os.Interrupt, syscall.SIGTERM)

	go func() {
		log.Printf("Starting Minimal Go Server (version: %s) on port %s", version, port)
		if err := server.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			log.Fatalf("Server ListenAndServe error: %v", err)
		}
	}()

	<-stopChan
	log.Println("Shutting down server gracefully...")

	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	if err := server.Shutdown(ctx); err != nil {
		log.Fatalf("Graceful shutdown failed: %v", err)
	}
	log.Println("Server terminated cleanly.")
}
```

### File: `Dockerfile`

```dockerfile
# syntax=docker/dockerfile:1.7

# -------------------------------------------------------------
# Stage 1: Build & Compile Static Go Binary
# -------------------------------------------------------------
FROM golang:1.22-alpine3.19 AS builder

WORKDIR /src

# Install CA certificates and timezone database
RUN apk update && apk add --no-cache ca-certificates tzdata

# Create dedicated non-root user and group definition files
RUN echo "appuser:x:10001:10001:AppUser:/:/sbin/nologin" > /tmp/passwd && \
    echo "appgroup:x:10001:" > /tmp/group

# Cache dependency manifests
COPY go.mod ./
RUN go mod download

# Copy source code
COPY main.go ./

# Compile with optimization flags:
# - CGO_ENABLED=0: produces static binary without glibc/musl dependency
# - -ldflags="-s -w": strips debugging symbols (-s) and DWARF tables (-w)
ARG VERSION=1.0.0
RUN --mount=type=cache,target=/root/.cache/go-build \
    CGO_ENABLED=0 GOOS=linux GOARCH=amd64 \
    go build -a -installsuffix cgo \
    -ldflags="-s -w -X main.version=${VERSION} -extldflags '-static'" \
    -o /bin/api-server main.go

# -------------------------------------------------------------
# Stage 2: Distroless Static Minimal Runtime
# -------------------------------------------------------------
# Alternative: FROM scratch
FROM gcr.io/distroless/static-debian12:nonroot

WORKDIR /app

# Copy timezone data and CA certificates for outbound HTTPS
COPY --from=builder /usr/share/zoneinfo /usr/share/zoneinfo
COPY --from=builder /etc/ssl/certs/ca-certificates.crt /etc/ssl/certs/

# Copy statically compiled binary
COPY --from=builder /bin/api-server /app/api-server

# Distroless nonroot user is UID 65532:65532
USER nonroot:nonroot

EXPOSE 8080

ENTRYPOINT ["/app/api-server"]
```

### File: `.dockerignore`

```
.git
.gitignore
.env*
Dockerfile
compose*.yaml
README.md
Makefile
bin/
```

### File: `compose.yaml`

```yaml
version: "3.8"

services:
  go-api:
    build:
      context: .
      dockerfile: Dockerfile
      args:
        VERSION: 1.0.0
    image: company/minimal-go-api:1.0.0
    restart: unless-stopped
    ports:
      - "8080:8080"
    environment:
      - PORT=8080
      - TZ=UTC
    deploy:
      resources:
        limits:
          cpus: "0.5"
          memory: 64M
        reservations:
          cpus: "0.1"
          memory: 16M
    # In distroless/scratch without curl/wget, use TCP or HTTP socket healthchecks in orchestrator
```

### File: `Makefile`

```makefile
IMAGE_NAME := company/minimal-go-api:1.0.0

.PHONY: build run scan clean

build:
	docker buildx build -t $(IMAGE_NAME) .

run:
	docker run --rm -p 8080:8080 $(IMAGE_NAME)

scan:
	docker run --rm -v /var/run/docker.sock:/var/run/docker.sock aquasec/trivy:latest image $(IMAGE_NAME)

size:
	docker images $(IMAGE_NAME) --format "table {{.Repository}}:{{.Tag}}\t{{.Size}}"
```

---

## 4. Step-by-Step Verification & Testing

### Step 1: Build Image and Inspect Size

```bash
mkdir -p minimal-go-microservice && cd minimal-go-microservice
docker build -t company/minimal-go-api:1.0.0 .

# Inspect image size
docker images company/minimal-go-api:1.0.0 --format "table {{.Repository}}:{{.Tag}}\t{{.Size}}"
```

*Expected Output*:
```
REPOSITORY                 TAG       SIZE
company/minimal-go-api     1.0.0     11.8MB
```
*The complete production-ready container image is under 12 megabytes!*

### Step 2: Run and Test API Endpoints

```bash
docker run -d --name go-test -p 8080:8080 company/minimal-go-api:1.0.0

# 1. Test Healthcheck
curl -s http://localhost:8080/healthz
# Output: {"status":"OK","version":"1.0.0","timestamp":"...","uptime_sec":3.21}

# 2. Test User List
curl -s http://localhost:8080/api/users
# Output: [{"id":1,"name":"Alice SRE","role":"Admin"},{"id":2,"name":"Bob Developer","role":"Engineer"}]
```

### Step 3: Run Vulnerability Scan (Trivy)

```bash
docker run --rm -v /var/run/docker.sock:/var/run/docker.sock aquasec/trivy:latest image --severity HIGH,CRITICAL company/minimal-go-api:1.0.0
```

*Expected Output*:
```
company/minimal-go-api:1.0.0 (debian 12)
Total: 0 (HIGH: 0, CRITICAL: 0)
```
*Zero vulnerabilities detected.*

### Step 4: Verify Immutability (No Shell Access)

Attempt to open an interactive bash or sh session:
```bash
docker exec -it go-test /bin/sh
# Output: OCI runtime exec failed: exec: "/bin/sh": stat /bin/sh: no such file or directory
```
*Attackers cannot spawn interactive shells or download secondary malware because no shell binary exists.*

### Step 5: Teardown

```bash
docker rm -f go-test
```

---

## 5. Production Considerations

1. **Why Distroless over Pure `scratch`**: While `scratch` is 100% empty, Google's `distroless/static` includes `/etc/passwd`, `/etc/group`, a `/tmp` directory, and standard CA certificates out of the box, reducing Dockerfile boilerplate.
2. **Debugging Distroless**: Because no shell exists, live debugging requires ephemeral debug containers in Kubernetes (`kubectl debug`) or attaching a temporary sidecar container sharing the PID and network namespace (`docker run --net=container:go-test --pid=container:go-test nicolaka/netshoot`).

---

[<- 04-nginx-reverse-proxy-with-tls-local.md](./04-nginx-reverse-proxy-with-tls-local.md) | [Back to Projects Overview](./README.md) | [Next Project: 06-full-dev-environment-with-compose-watch.md ->](./06-full-dev-environment-with-compose-watch.md)
