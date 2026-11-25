# File Synchronization and Hot Reloading with Compose Watch

```
Track: DevOps & Containerization
Module: 05-compose
Prerequisites: 05-compose/01-compose-basics.md, 04-volumes-and-networking/01-volumes-and-bind-mounts.md
Difficulty: Intermediate
Estimated Duration: 45 minutes
Target Docker Version: 27.x+ (Compose v2.22+)
Last Validated: February 2025
```

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [The Three Watch Actions](#4-the-three-watch-actions)
5. [Syntax & Configuration Reference](#5-syntax--configuration-reference)
6. [Practical Implementation & Multi-Stack Examples](#6-practical-implementation--multi-stack-examples)
   - [6.1 Node.js / Next.js / Express](#61-nodejs--nextjs--express)
   - [6.2 Python / FastAPI (Uvicorn)](#62-python--fastapi-uvicorn)
   - [6.3 Go Microservice](#63-go-microservice)
7. [Production Best Practices & Antipatterns](#7-production-best-practices--antipatterns)
8. [Common Pitfalls & Troubleshooting](#8-common-pitfalls--troubleshooting)
9. [Performance Tuning & Resource Optimization](#9-performance-tuning--resource-optimization)
10. [Security Hardening & Threat Model](#10-security-hardening--threat-model)
11. [Architectural Comparison: Bind Mounts vs Compose Watch](#11-architectural-comparison-bind-mounts-vs-compose-watch)
12. [Real-World Scenario Walkthrough](#12-real-world-scenario-walkthrough)
13. [Self-Assessment & Interview Questions](#13-self-assessment--interview-questions)
14. [Hands-on Practice Exercises](#14-hands-on-practice-exercises)
15. [Summary & Reference Cheat Sheet](#summary--reference-cheat-sheet)

---

## 1. What & Why

Historically, achieving a rapid "inner development loop" with Docker required clunky workarounds:
- **Full Host Bind Mounts (`-v $(pwd):/app`)**: Mounting local directories into containers often overwrites container `node_modules` with host files, suffers severe file I/O performance penalties on macOS and Windows (due to virtualization filesystem translation layers), and frequently misses Linux `inotify` file-change events across OS boundaries.
- **Manual Image Rebuilding (`docker compose up --build`)**: Waiting 30-60 seconds for a full image rebuild on every single frontend typo or API handler change destroys developer productivity.

Introduced in Docker Compose v2.22+, **Compose Watch** provides native, bidirectional file synchronization and automated container rebuilding directly within the Compose engine without relying on external file watchers or brittle bind mounts.

---

## 2. Core Concepts

### The Inner Development Loop
The cycle of editing code, observing results, and debugging. Compose Watch optimizes this loop by differentiating between:
1. **Source Code / Asset Changes**: Synchronized directly into the running container in real-time (sub-50ms), triggering language hot-reloading (Vite, Webpack, Nodemon, Uvicorn).
2. **Dependency Manifest Changes**: Modifying `package.json`, `go.mod`, or `requirements.txt` automatically triggers a background container rebuild and replacement.

### The `develop.watch` Section
Compose Watch is configured under a dedicated `develop` block within each service in `compose.yaml`. It requires **zero changes** to your production Dockerfile or production Compose overrides.

---

## 3. How It Works Internally

### File Event Streaming Architecture

```
+-------------------------------------------------------------+
|                     DEVELOPER WORKSTATION                   |
|                                                             |
|  Developer modifies: src/controllers/user.ts                |
+------------------------------+------------------------------+
                               |
                               v (Host OS File Event: inotify/FSEvents)
+-------------------------------------------------------------+
|                    Docker Compose CLI                       |
|  1. Intercepts file change event                            |
|  2. Matches path against develop.watch rules                |
|  3. Evaluates action: 'sync'                                |
+------------------------------+------------------------------+
                               |
                               v (gRPC Tar Stream over Docker Socket)
+-------------------------------------------------------------+
|                    Container Filesystem                     |
|  File copied directly into /app/src/controllers/user.ts     |
|                                                             |
|  In-Container Watcher (Nodemon / Vite) detects update       |
|  --> Executes Hot Module Reloading in < 100ms!              |
+-------------------------------------------------------------+
```

Because file updates are streamed as byte deltas over the Docker daemon socket (`docker cp` protocol), it completely bypasses the macOS VirtioFS/gRPC-FUSE virtualization translation layer, delivering native disk performance.

---

## 4. The Three Watch Actions

1. **`sync` (Fastest - < 50ms)**:
   - Copies modified files directly into the target directory inside the running container.
   - Ideal for: Frontend templates (HTML/CSS), static assets, and interpreted languages with active hot-reloaders (Node.js, Python, Ruby, PHP).
2. **`sync+restart` (Moderate - ~500ms)**:
   - Copies modified files into the container and immediately restarts the container process.
   - Ideal for: Compiled language artifacts or applications that lack native in-process file watching (e.g. Go binaries recompiled locally, Spring Boot).
3. **`rebuild` (Thorough - ~5-15s)**:
   - Triggers a full image rebuild and gracefully replaces the container.
   - Ideal for: Dependency manifest modifications (`package.json`, `requirements.txt`, `pom.xml`, `go.mod`) or Dockerfile modifications.

---

## 5. Syntax & Configuration Reference

```yaml
services:
  web:
    build: .
    ports:
      - "3000:3000"
    develop:
      watch:
        # Rule 1: Sync source code directly into container
        - action: sync
          path: ./src
          target: /app/src
          ignore:
            - "node_modules/"
            - "*.test.ts"

        # Rule 2: Rebuild image when dependency manifests change
        - action: rebuild
          path: package.json

        - action: rebuild
          path: package-lock.json

        # Rule 3: Sync static config and restart container process
        - action: sync+restart
          path: ./config.yaml
          target: /app/config.yaml
```

### CLI Commands to Launch Watch

```bash
# Option 1: Start stack and attach watch in foreground
docker compose up --watch

# Option 2: Start stack detached, then run watch in a dedicated terminal
docker compose up -d
docker compose watch
```

---

## 6. Practical Implementation & Multi-Stack Examples

### 6.1 Node.js / Next.js / Express

```yaml
services:
  frontend:
    build:
      context: .
      target: dev
    ports:
      - "3000:3000"
    environment:
      - NODE_ENV=development
    develop:
      watch:
        - action: sync
          path: ./src
          target: /app/src
          ignore:
            - "**/*.spec.ts"
        - action: sync
          path: ./public
          target: /app/public
        - action: rebuild
          path: ./package.json
```

### 6.2 Python / FastAPI (Uvicorn)

```yaml
services:
  api:
    build: .
    ports:
      - "8000:8000"
    command: ["uvicorn", "main:app", "--host", "0.0.0.0", "--port", "8000", "--reload"]
    develop:
      watch:
        - action: sync
          path: ./app
          target: /app/app
          ignore:
            - "__pycache__/"
            - "*.pyc"
        - action: rebuild
          path: ./requirements.txt
```

### 6.3 Go Microservice

```yaml
services:
  go-app:
    build: .
    ports:
      - "8080:8080"
    develop:
      watch:
        - action: sync+restart
          path: ./cmd
          target: /app/cmd
        - action: sync+restart
          path: ./internal
          target: /app/internal
        - action: rebuild
          path: ./go.mod
        - action: rebuild
          path: ./go.sum
```

---

## 7. Production Best Practices & Antipatterns

### Best Practices
- **Combine `sync` with Framework Hot Reloading**: In Node.js or Python, use `action: sync` paired with runtime reloaders (Vite, Nodemon, Uvicorn `--reload`) for instantaneous UI/API updates.
- **Always Ignore Generated Files**: Configure `ignore:` to exclude `.git`, `node_modules/`, `__pycache__/`, `.next/`, and test coverage directories. Syncing generated files triggers infinite reload loops.
- **Pair with Multi-Stage Dockerfiles**: Define a `target: dev` in your Dockerfile containing development dependencies, keeping runtime production stages lean.
- **Commit `develop.watch` to Version Control**: Keep watch configurations inside the repository `compose.yaml` so the entire team benefits from uniform dev ergonomics.

### Anti-Patterns
- **Using `action: rebuild` on Fast-Changing Source Code**: Setting `action: rebuild` on `./src` forces a 20-second image build on every keystroke, defeating the purpose of hot reloading.
- **Syncing Large Binary Directories**: Syncing gigabyte directories like `node_modules` or `.venv` across the watcher floods the Docker daemon event loop.
- **Running Compose Watch in Production**: Compose Watch is strictly a developer tool; it must never be invoked in staging or production CI/CD deployments.

---

## 8. Common Pitfalls & Troubleshooting

| Symptom | Root Cause | Diagnosis Command | Permanent Fix |
|:---|:---|:---|:---|
| `docker compose watch` fails with `unknown flag: --watch` | Docker Compose version is < 2.22.0 | `docker compose version` | Upgrade Docker Engine and Docker Compose to latest release. |
| In-container process does not detect synced files | Application file watcher (e.g. Nodemon) relies on polling rather than inotify events | Check dev server logs | Enable polling in app watcher (e.g., `CHOKIDAR_USEPOLLING=true` in Node). |
| Continuous rebuild loop triggers on every file change | An ignored build output folder (like `dist/` or `target/`) is being synced back and forth | Inspect `docker compose watch` logs | Add generated directories to `ignore:` in the watch configuration. |
| File permissions altered inside container after sync | Files synced from host inherit host user permissions rather than container UID | `docker compose exec web ls -la` | Ensure container runs as matching UID or set container permissions appropriately. |
| Rebuild action fails to install newly added package | `action: rebuild` was configured on `package.json`, but `package-lock.json` was not updated | Inspect build logs | Run `npm install` locally first to update lockfile, or add lockfile to watch rules. |

---

## 9. Performance Tuning & Resource Optimization

1. **CPU Usage Savings over Bind Mounts**:
   On macOS, mounting a full source tree with 30,000 files via bind mounts (`-v $(pwd):/app`) causes continuous CPU spikes due to VirtioFS filesystem event scanning. Compose Watch synchronizes only individual touched files, reducing idle CPU usage from 40% to < 1%.
2. **Targeted Ignore Patterns**:
   Keep `ignore:` rules tightly scoped:
   ```yaml
   ignore:
     - "dist/"
     - ".next/"
     - "*.log"
   ```

---

## 10. Security Hardening & Threat Model

```
+--------------------------------------------------------------+
| ATTACK: Malicious Code Injection via Unrestricted Watch       |
+--------------------------------------------------------------+
| A developer runs Compose Watch on a shared workstation.      |
| An attacker modifies files in a public `/tmp/shared-code` dir.|
| Compose Watch detects changes -> Automatically pushes code   |
| into the running container without human approval!           |
|                                                              |
| MITIGATION:                                                  |
| 1. Only watch tightly scoped, version-controlled project dirs.|
| 2. Never run Compose Watch against world-writable directories.|
+--------------------------------------------------------------+
```

---

## 11. Architectural Comparison: Bind Mounts vs Compose Watch

| Feature | Full Host Bind Mount (`-v $(pwd):/app`) | Compose Watch (`develop.watch`) |
|:---|:---|:---|
| **macOS / Windows Performance** | Slow (Heavy VM translation overhead) | **Ultra-Fast (Direct socket stream)** |
| **Node_modules Overwrite Risk**| High (Host clobbers container modules)| **Zero (Isolated by design)** |
| **Dependency Auto-Rebuild** | Impossible (Manual intervention) | **Automatic (`action: rebuild`)** |
| **Process Restart Capability**| Requires external tooling (nodemon/air)| **Native (`action: sync+restart`)** |
| **Production Footprint** | None | **Zero (Clean develop block)** |

---

## 12. Real-World Scenario Walkthrough

### Scenario: Eliminating the macOS "Docker Desktop Sluggishness"

#### The Issue
A front-end engineering team developing a Next.js / TypeScript application on macOS M3 laptops suffered 8-second page reload times. CPU utilization on Docker Desktop sat at 120% continuously due to a full bind mount of the project directory containing 60,000 files in `node_modules`.

#### The Fix
1. Removed the top-level bind mount (`- .:/app`) from `compose.yaml`.
2. Implemented Compose Watch:
   ```yaml
   develop:
     watch:
       - action: sync
         path: ./src
         target: /app/src
       - action: sync
         path: ./public
         target: /app/public
       - action: rebuild
         path: ./package.json
   ```
3. Executed `docker compose up --watch`.
4. Page reload latency dropped from 8 seconds to **120 milliseconds**. Docker Desktop idle CPU dropped from 120% to **2%**.

---

## 13. Self-Assessment & Interview Questions

<details>
<summary><strong>1. What is the fundamental difference between `action: sync` and `action: sync+restart` in Compose Watch?</strong></summary>

**Answer:**
`action: sync` copies the modified file directly into the container filesystem while leaving the container process running uninterrupted; it relies on the application's runtime (like Vite, Nodemon, or Uvicorn) to detect the change and hot-reload. `action: sync+restart` copies the file and immediately restarts the container process; it is intended for compiled binaries or frameworks that do not have built-in file watching.
</details>

<details>
<summary><strong>2. Why does Compose Watch perform significantly faster than standard bind mounts on macOS and Windows?</strong></summary>

**Answer:**
On macOS and Windows, Docker runs inside a lightweight Linux virtual machine. Full bind mounts require continuous bi-directional filesystem synchronization across the virtualization boundary (using VirtioFS or gRPC-FUSE), which incurs massive latency when navigating thousands of files. Compose Watch monitors files natively on the host OS and streams only the specific changed files directly across the Docker socket using tar streams, completely avoiding virtualization filesystem bottlenecks.
</details>

<details>
<summary><strong>3. How does `action: rebuild` work, and what is its typical use case?</strong></summary>

**Answer:**
`action: rebuild` monitors dependency manifests (such as `package.json`, `go.mod`, or `requirements.txt`). When a developer updates dependencies, Compose automatically builds a new image layer in the background using BuildKit and restarts the container with the newly built image, eliminating the need to manually execute `docker compose up --build`.
</details>

<details>
<summary><strong>4. How do you launch a Compose stack with Compose Watch enabled?</strong></summary>

**Answer:**
Either:
1. `docker compose up --watch`: Boots the stack and immediately attaches the file watcher in the foreground.
2. `docker compose up -d` followed by `docker compose watch`: Starts services in detached mode and runs the watcher in a dedicated terminal window.
</details>

<details>
<summary><strong>5. Why is the `ignore:` directive critical when configuring `develop.watch`?</strong></summary>

**Answer:**
`ignore:` prevents unnecessary or generated files (such as `.git/`, `node_modules/`, `dist/`, or `.pyc` files) from triggering watch events. If build output folders are not ignored, an update can trigger an in-container compilation that modifies the output directory, which Compose Watch detects, triggering an infinite loop of synchronizations.
</details>

<details>
<summary><strong>6. Does defining `develop.watch` affect production deployments using the same `compose.yaml`?</strong></summary>

**Answer:**
No. The `develop` configuration block is strictly ignored by standard `docker compose up` commands unless explicitly invoked via the `--watch` flag or `docker compose watch`. It has zero impact on production builds or container lifecycles.
</details>

<details>
<summary><strong>7. How does Compose Watch solve the "node_modules collision" problem common with bind mounts?</strong></summary>

**Answer:**
With standard bind mounts (`-v .:/app`), the host machine's directory completely masks the container's directory. If the host lacks `node_modules` (or has macOS-compiled binaries), the container crashes. With Compose Watch, `node_modules` remains completely isolated inside the container's internal filesystem layer; only the `./src` code files are synchronized.
</details>

<details>
<summary><strong>8. What minimum version of Docker Compose is required to use Compose Watch?</strong></summary>

**Answer:**
Docker Compose **v2.22.0** or newer.
</details>

---

## 14. Hands-on Practice Exercises

<details>
<summary><strong>Exercise 1: Setting Up a Basic File Sync Watcher</strong></summary>

**Goal:** Configure Compose Watch to sync an HTML file into an Nginx container in real-time.

**Step-by-step Solution:**
1. Create `html/index.html`: `<h1>Version 1</h1>`
2. Create `compose.yaml`:
   ```yaml
   services:
     web:
       image: nginx:alpine
       ports: ["8085:80"]
       develop:
         watch:
           - action: sync
             path: ./html
             target: /usr/share/nginx/html
   ```
3. Run: `docker compose up --watch`
4. Test: `curl http://localhost:8085` -> "Version 1"
5. Edit `html/index.html` to `<h1>Version 2</h1>` on host.
6. Test: `curl http://localhost:8085` -> Immediately displays "Version 2" without container restart!
</details>

<details>
<summary><strong>Exercise 2: Testing `sync+restart` on a Python Script</strong></summary>

**Goal:** Verify process restart on file change for a script without a hot-reloader.

**Step-by-step Solution:**
1. Create `app.py`: `import time; print("Booted v1"); time.sleep(3600)`
2. Configure `compose.yaml` with `action: sync+restart`.
3. Launch with `--watch`.
4. Modify `app.py` -> Observe console log verifying container process restart.
</details>

<details>
<summary><strong>Exercise 3: Automated Image Rebuild on Dependency Update</strong></summary>

**Goal:** Trigger an automatic image rebuild by touching `package.json`.

**Step-by-step Solution:**
1. Configure `action: rebuild` on `package.json`.
2. Run `docker compose watch`.
3. Update version in `package.json` -> Watch console trigger automated BuildKit rebuild.
</details>

<details>
<summary><strong>Exercise 4: Configuring Exclusions via `ignore:`</strong></summary>

**Goal:** Ignore test files from triggering sync actions.

**Step-by-step Solution:**
1. Add `ignore: ["*.test.js"]` under `action: sync`.
2. Edit a `.test.js` file -> Verify no synchronization event occurs in watcher log.
</details>

<details>
<summary><strong>Exercise 5: Decoupled Watch Execution via Separate Terminals</strong></summary>

**Goal:** Run services in background and watcher in a separate shell.

**Step-by-step Solution:**
1. Terminal 1: `docker compose up -d`
2. Terminal 2: `docker compose watch`
3. Edit files and monitor Terminal 2 output.
</details>

<details>
<summary><strong>Exercise 6: Multi-Service Watch Architecture</strong></summary>

**Goal:** Configure independent watch rules for an API service and a Frontend service simultaneously.

**Step-by-step Solution:**
1. Declare `develop.watch` blocks under both `api` and `frontend` services.
2. Launch `docker compose up --watch` and observe unified watcher tracking both services.
</details>

---

## 15. Summary & Reference Cheat Sheet

### Syntax Quick Reference
```yaml
services:
  app:
    develop:
      watch:
        - action: sync          # Instant file copy (< 50ms)
          path: ./src
          target: /app/src
          ignore: [node_modules/]
        - action: sync+restart  # File copy + process restart
          path: ./config.yaml
          target: /app/config.yaml
        - action: rebuild       # Trigger image rebuild
          path: ./package.json
```

### CLI Commands
```bash
docker compose up --watch   # Start and watch in foreground
docker compose watch        # Attach watch to running stack
```

### Top Rules
1. Use **`sync`** for source code + live reloaders.
2. Use **`sync+restart`** for compiled artifacts or static configs.
3. Use **`rebuild`** for package dependency manifests.
4. Always **`ignore`** `.git`, `node_modules`, and build caches.

---

[← Previous: 06-profiles-and-overrides.md](06-profiles-and-overrides.md) | [Next: 08-compose-for-testing.md →](08-compose-for-testing.md)
