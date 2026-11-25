<!--
================================================================================
Metadata:
  Title: Technical Interview Preparation - Docker Compose v2 Deep Dive Q&A
  Track: Docker / DevOps
  Difficulty: Junior to Senior
  Estimated Time: 65 minutes
  Key Terminology: Compose v2, Compose Spec, depends_on healthy, Profiles, Compose Watch, compose.override.yaml, Variable Interpolation, Secrets
================================================================================
-->

# Docker Compose v2 Deep Dive: 25 In-Depth Interview Questions & Answers

[<- 03-networking-and-volumes-qa.md](./03-networking-and-volumes-qa.md) | [Back to Interview Overview](./README.md) | [Next Note: 05-security-qa.md ->](./05-security-qa.md)

---

## Overview

This guide provides 25 comprehensive technical interview questions covering Docker Compose v2 architecture, Compose Specification syntax, multi-service lifecycle orchestration, variable interpolation rules, and modern developer workflows.

---

## Questions Index

1. [Q01: What is the architectural difference between Docker Compose v1 and Docker Compose v2?](#q01-what-is-the-architectural-difference-between-docker-compose-v1-and-docker-compose-v2)
2. [Q02: What is the modern "Compose Specification" and why is the top-level `version:` tag now obsolete?](#q02-what-is-the-modern-compose-specification-and-why-is-the-top-level-version-tag-now-obsolete)
3. [Q03: How does `depends_on` with `condition: service_healthy` solve service startup race conditions?](#q03-how-does-depends_on-with-condition-service_healthy-solve-service-startup-race-conditions)
4. [Q04: What are the exact environment variable precedence rules in Docker Compose?](#q04-what-are-the-exact-environment-variable-precedence-rules-in-docker-compose)
5. [Q05: How does Docker Compose multi-file merging work and what role does `compose.override.yaml` play?](#q05-how-does-docker-compose-multi-file-merging-work-and-what-role-does-composeoverrideyaml-play)
6. [Q06: What are Docker Compose Profiles (`profiles:`) and how do you use them for optional services?](#q06-what-are-docker-compose-profiles-profiles-and-how-do-you-use-them-for-optional-services)
7. [Q07: How does Docker Compose Watch (`develop.watch`) work and how does it replace bind mounts?](#q07-how-does-docker-compose-watch-developwatch-work-and-how-does-it-replace-bind-mounts)
8. [Q08: What is the difference between `action: sync`, `action: sync+restart`, and `action: rebuild` in Compose Watch?](#q08-what-is-the-difference-between-action-sync-action-syncrestart-and-action-rebuild-in-compose-watch)
9. [Q09: How do Docker Secrets work in standalone Compose compared to Swarm mode?](#q09-how-do-docker-secrets-work-in-standalone-compose-compared-to-swarm-mode)
10. [Q10: What is the difference between `docker compose up`, `docker compose start`, and `docker compose run`?](#q10-what-is-the-difference-between-docker-compose-up-docker-compose-start-and-docker-compose-run)
11. [Q11: How do you perform one-off database migrations using `docker compose run`?](#q11-how-do-you-perform-one-off-database-migrations-using-docker-compose-run)
12. [Q12: What does `docker compose down -v` do and what are the catastrophic risks in production?](#q12-what-does-docker-compose-down--v-do-and-what-are-the-catastrophic-risks-in-production)
13. [Q13: How does Compose calculate service container names and default project prefixes?](#q13-how-does-compose-calculate-service-container-names-and-default-project-prefixes)
14. [Q14: How can you run multiple isolated instances of the same Compose project on a single host?](#q14-how-can-you-run-multiple-isolated-instances-of-the-same-compose-project-on-a-single-host)
15. [Q15: How does Compose handle resource limits (`deploy.resources.limits`) in standalone mode?](#q15-how-does-compose-handle-resource-limits-deployresourceslimits-in-standalone-mode)
16. [Q16: What is the `extends:` keyword in Compose and when should it be avoided?](#q16-what-is-the-extends-keyword-in-compose-and-when-should-it-be-avoided)
17. [Q17: How do you configure custom health checks inside Compose manifests?](#q17-how-do-you-configure-custom-health-checks-inside-compose-manifests)
18. [Q18: What is the difference between `env_file:` and `environment:` in Compose?](#q18-what-is-the-difference-between-env_file-and-environment-in-compose)
19. [Q19: How do you scale a stateless service horizontally using `docker compose up --scale`?](#q19-how-do-you-scale-a-stateless-service-horizontally-using-docker-compose-up---scale)
20. [Q20: Why can't you scale a service that binds to a static host port (`- "80:80"`)?](#q20-why-cant-you-scale-a-service-that-binds-to-a-static-host-port---8080)
21. [Q21: How do you isolate frontend and backend tiers using Compose networks?](#q21-how-do-you-isolate-frontend-and-backend-tiers-using-compose-networks)
22. [Q22: How do you integrate Compose with external, pre-existing Docker networks or volumes?](#q22-how-do-you-integrate-compose-with-external-pre-existing-docker-networks-or-volumes)
23. [Q23: What are the fundamental limitations of using Docker Compose in high-availability production?](#q23-what-are-the-fundamental-limitations-of-using-docker-compose-in-high-availability-production)
24. [Q24: How can you dry-run and validate a merged Compose file using `docker compose config`?](#q24-how-can-you-dry-run-and-validate-a-merged-compose-file-using-docker-compose-config)
25. [Q25: What is `stop_grace_period` and why is it critical for stateful worker queues in Compose?](#q25-what-is-stop_grace_period-and-why-is-it-critical-for-stateful-worker-queues-in-compose)

---

## Technical Q&A

<details>
<summary><strong>Q01: What is the architectural difference between Docker Compose v1 and Docker Compose v2?</strong></summary>

- **Compose v1 (`docker-compose`)**:
  - Written in **Python**.
  - Distributed as a standalone external binary.
  - Communicated with the Docker daemon via the Docker Python SDK.
  - Slower execution, lacked native BuildKit integration, and suffered from dependency conflicts (e.g. PyYAML versions).
  - Deprecated and reached End-Of-Life (EOL) in July 2023.
- **Compose v2 (`docker compose`)**:
  - Completely rewritten in **Go**.
  - Built directly into the Docker CLI as a native plugin.
  - Shares the identical Go codebase and network primitives as Docker Engine.
  - Up to 3x faster execution, natively supports BuildKit, multi-platform builds, Compose Watch, and unified CLI ergonomics.
</details>

<details>
<summary><strong>Q02: What is the modern "Compose Specification" and why is the top-level `version:` tag now obsolete?</strong></summary>

Historically, Compose files required top-level version declarations (`version: '2.4'`, `version: '3.8'`). This created confusion because version 3 was designed for Swarm clusters and actually omitted key developer features present in version 2.4.
In 2020, Docker published the **Compose Specification** (`compose-spec`) as an open standard. Under the modern Compose Specification:
- The `version:` tag is **deprecated and obsolete**.
- Modern Compose parsers ignore the `version` field.
- All modern features (`deploy.resources`, `develop.watch`, `secrets`) are supported in standard `compose.yaml` files regardless of version strings.
- Preferred filenames are `compose.yaml` or `compose.yml` (falling back to legacy `docker-compose.yml`).
</details>

<details>
<summary><strong>Q03: How does `depends_on` with `condition: service_healthy` solve service startup race conditions?</strong></summary>

In basic Compose, `depends_on` only verifies that a dependency container has been **spawned** (`status: started`), not that the application inside is actually ready to accept network sockets. A database (like PostgreSQL) takes 5–15 seconds to initialize; if the API container attempts to run migrations immediately, it crashes.
Using `condition: service_healthy`:
```yaml
depends_on:
  postgres-db:
    condition: service_healthy
```
Instructs Compose to block the startup of the dependent service until the database container's declared `healthcheck` executes, succeeds, and transitions to `healthy`.
</details>

<details>
<summary><strong>Q04: What are the exact environment variable precedence rules in Docker Compose?</strong></summary>

When resolving environment variables, Compose evaluates sources in the following strict order of precedence (highest to lowest):
1. **Host Shell Environment Variables**: Variables exported in the active shell (`export PORT=9000`).
2. **CLI Arguments**: Values passed via `--env` or `--env-file`.
3. **Compose Manifest `environment:` block**: Explicit keys in `compose.yaml`.
4. **Compose Manifest `env_file:` directive**: Files loaded via `env_file: [.env.prod]`.
5. **Project Root `.env` file**: The default `.env` file in the folder where `compose.yaml` resides.
6. **Container Image `ENV` instructions**: Values declared inside the Dockerfile.
</details>

<details>
<summary><strong>Q05: How does Docker Compose multi-file merging work and what role does `compose.override.yaml` play?</strong></summary>

Compose allows layering multiple configuration files using the `-f` flag:
```bash
docker compose -f compose.yaml -f compose.prod.yaml up -d
```
Compose merges arrays (e.g. `ports`, `volumes`) by appending entries, and overrides scalar values (e.g. `image`, `command`) with the rightmost file taking precedence.
**Automatic Override**: If a file named `compose.override.yaml` (or `docker-compose.override.yml`) exists in the directory, Compose merges it automatically with `compose.yaml` on every execution, making it the standard pattern for local developer overrides that are gitignored.
</details>

<details>
<summary><strong>Q06: What are Docker Compose Profiles (`profiles:`) and how do you use them for optional services?</strong></summary>

Profiles allow grouping services under logical tags so they only start when explicitly requested.
```yaml
services:
  api:
    image: my-api:latest
  swagger-ui:
    image: swaggerapi/swagger-ui
    profiles: ["debug", "dev"]
```
By default, running `docker compose up` starts **only** `api`.
To start the optional Swagger service:
```bash
docker compose --profile debug up -d
# Or via environment variable:
COMPOSE_PROFILES=debug docker compose up -d
```
Ideal for debugging proxies, load testing tools, database admin UIs (pgAdmin, phpMyAdmin), and seed scripts.
</details>

<details>
<summary><strong>Q07: How does Docker Compose Watch (`develop.watch`) work and how does it replace bind mounts?</strong></summary>

Compose Watch introduces a native file synchronizer built directly into the Compose CLI. Instead of continuously synchronizing thousands of host files over slow virtualization drivers (such as macOS VirtioFS), Compose Watch monitors host directories on the host OS and pushes only changed files into the running container filesystem.
*Benefits*:
- Eliminates `node_modules` pollution across host and container OS boundaries.
- Native performance on macOS and Windows workstations.
- Automates container rebuilds when dependency manifests (`package.json`, `pom.xml`) change.
</details>

<details>
<summary><strong>Q08: What is the difference between `action: sync`, `action: sync+restart`, and `action: rebuild` in Compose Watch?</strong></summary>

1. **`action: sync`**: Copies changed host files directly into the container filesystem in real time. Ideal for frontend files (React, Vue, HTML/CSS) where a Vite/Webpack dev server uses Hot Module Replacement (HMR).
2. **`action: sync+restart`**: Copies changed files into the container filesystem and immediately restarts the container process. Ideal for interpreted backend languages (Node.js, Python, Go with air).
3. **`action: rebuild`**: Automatically triggers a BuildKit layer rebuild of the container image and replaces the running container. Used when dependency manifests (`package.json`, `go.mod`, `requirements.txt`) change.
</details>

<details>
<summary><strong>Q09: How do Docker Secrets work in standalone Compose compared to Swarm mode?</strong></summary>

- **Standalone Compose**: Secrets declared under `secrets:` are mounted from host files into the container at `/run/secrets/<name>`. On the host, the secret file remains unencrypted on disk. Compose simply creates a bind mount behind the scenes.
- **Swarm Mode**: Secrets are transmitted over mTLS, stored strictly in the encrypted Raft log on manager nodes, and mounted onto worker containers as in-memory `tmpfs` files. The secret is never written to disk on worker nodes.
</details>

<details>
<summary><strong>Q10: What is the difference between `docker compose up`, `docker compose start`, and `docker compose run`?</strong></summary>

- **`docker compose up`**: Orchestrates the entire project lifecycle. Builds images (if missing or requested), creates networks/volumes, instantiates containers, and starts them conforming to dependency order.
- **`docker compose start`**: Starts **existing, stopped** containers without rebuilding images or re-evaluating configuration changes.
- **`docker compose run <service>`**: Spawns a **one-off, independent container** for the specified service with the same volumes and networks, typically used to execute ad-hoc commands (e.g. database migrations, tests, shell access).
</details>

<details>
<summary><strong>Q11: How do you perform one-off database migrations using `docker compose run`?</strong></summary>

```bash
docker compose run --rm api-service npm run db:migrate
```
- `--rm`: Ensures the temporary migration container is automatically deleted once the command finishes.
- The migration container connects to the existing `postgres-db` service over the shared Compose network, executes migrations, and terminates cleanly without interfering with running web containers.
</details>

<details>
<summary><strong>Q12: What does `docker compose down -v` do and what are the catastrophic risks in production?</strong></summary>

- `docker compose down`: Stops running containers and deletes container instances, networks, and internal links.
- **`-v` (or `--volumes`)**: Instructs Compose to **permanently delete all named and anonymous volumes** defined in the `volumes:` block of the Compose file.
*Catastrophic Risk*: In production or persistent staging environments, running `docker compose down -v` instantly purges all database data, uploaded media, and persistent storage with zero recovery possible unless external backups exist.
</details>

<details>
<summary><strong>Q13: How does Compose calculate service container names and default project prefixes?</strong></summary>

By default, container names follow the convention:
$$\text{<project\_name>}\_\text{<service\_name>}\_\text{<instance\_number>}$$
Example: `myproject_web_1` (or `myproject-web-1` in Compose v2).
- The default `<project_name>` is derived from the **name of the directory** containing the Compose file.
- It can be explicitly overridden via the `-p` CLI flag (`docker compose -p prod up -d`) or the top-level `name:` attribute in `compose.yaml`.
</details>

<details>
<summary><strong>Q14: How can you run multiple isolated instances of the same Compose project on a single host?</strong></summary>

Specify a distinct project name using `-p` (or `COMPOSE_PROJECT_NAME`):
```bash
docker compose -p review-pr-101 up -d
docker compose -p review-pr-102 up -d
```
Compose creates isolated networks (`review-pr-101_default`, `review-pr-102_default`), separate volumes, and unique container namespaces for each project name, allowing multiple identical environments (e.g. PR preview environments) to coexist without collision (provided published host ports are dynamic or mapped via a reverse proxy).
</details>

<details>
<summary><strong>Q15: How does Compose handle resource limits (`deploy.resources.limits`) in standalone mode?</strong></summary>

In Compose v1, `deploy:` configurations were ignored outside of Swarm mode unless the `--compatibility` flag was passed.
In modern **Compose v2**, the Compose engine translates `deploy.resources.limits` and `deploy.resources.reservations` directly into native Docker Engine host configs (`--memory`, `--cpus`) out of the box in standalone mode, guaranteeing resource enforcement without requiring Swarm mode.
</details>

<details>
<summary><strong>Q16: What is the `extends:` keyword in Compose and when should it be avoided?</strong></summary>

`extends:` allows a service to inherit configuration options from another service within the same file or across an external YAML file:
```yaml
services:
  worker:
    extends:
      file: common-services.yaml
      service: base-app
```
*When to avoid*: `extends:` does not share `links`, `depends_on`, or `volumes_from`. In complex projects, multi-file merging (`-f file1.yaml -f file2.yaml`) is generally preferred because it provides clearer visibility and uniform override inheritance.
</details>

<details>
<summary><strong>Q17: How do you configure custom health checks inside Compose manifests?</strong></summary>

```yaml
services:
  api:
    image: my-api:latest
    healthcheck:
      test: ["CMD", "curl", "-f", "http://localhost:8080/healthz"]
      interval: 10s
      timeout: 3s
      retries: 3
      start_period: 20s
```
- `start_period`: Grace period during which failing health check attempts do not count against maximum retries, allowing slow application boots to warm up caches.
</details>

<details>
<summary><strong>Q18: What is the difference between `env_file:` and `environment:` in Compose?</strong></summary>

- **`env_file:`**: Loads raw key-value pairs from an external file (e.g. `.env.local`) into the container environment. Useful for managing large sets of variables without cluttering Compose manifests.
- **`environment:`**: Explicitly defines environment variables in the Compose YAML. Supports variable interpolation from the host shell (e.g. `DB_PASS: ${SECRET_PASS}`).
*Precedence*: Keys declared in `environment:` override identical keys loaded from `env_file:`.
</details>

<details>
<summary><strong>Q19: How do you scale a stateless service horizontally using `docker compose up --scale`?</strong></summary>

```bash
docker compose up -d --scale web-api=5
```
Compose instantiates 5 identical containers for `web-api` (`web-api-1` through `web-api-5`). All 5 containers attach to the shared project network and share the same internal service DNS name (`web-api`).
</details>

<details>
<summary><strong>Q20: Why can't you scale a service that binds to a static host port (`- "80:80"`)?</strong></summary>

If a service defines a fixed host port mapping (`ports: ["80:80"]`), the first container binds successfully to host port 80. When Compose attempts to spawn the second container (`web-api-2`), the host kernel rejects the bind with:
`Error: bind: address already in use`.
*Solution*: Omit the host port (`ports: ["80"]`) to allocate dynamic ephemeral ports, or route traffic through a reverse proxy (like NGINX or Traefik) that listens on port 80 and load balances across container IPs internally.
</details>

<details>
<summary><strong>Q21: How do you isolate frontend and backend tiers using Compose networks?</strong></summary>

```yaml
services:
  web:
    image: nginx
    networks: [frontend]
  api:
    image: my-api
    networks: [frontend, backend]
  db:
    image: postgres
    networks: [backend]

networks:
  frontend:
  backend:
    internal: true # Drops all external routing
```
`web` can reach `api`, and `api` can reach `db`. However, `web` cannot reach or resolve `db`, and `db` is completely blocked from public egress.
</details>

<details>
<summary><strong>Q22: How do you integrate Compose with external, pre-existing Docker networks or volumes?</strong></summary>

Use the `external: true` attribute:
```yaml
networks:
  shared-gateway-net:
    external: true

volumes:
  corporate-data:
    external: true
```
Compose will connect to the pre-existing network or volume instead of attempting to create a new one prefixed with the project name.
</details>

<details>
<summary><strong>Q23: What are the fundamental limitations of using Docker Compose in high-availability production?</strong></summary>

1. **Single-Node Limitation**: Compose can only schedule containers on the local machine hosting the Docker daemon; it cannot schedule across multi-node clusters.
2. **No Dynamic Autoscaling**: Lacks Horizontal Pod Autoscaling (HPA) based on CPU/memory or queue metrics.
3. **No Dynamic Storage Orchestration**: Cannot dynamically provision or migrate cloud block storage volumes (AWS EBS) between nodes.
4. **Basic Ingress**: Lacks advanced Layer 7 routing controllers (Ingress/Gateway API) with automatic TLS cert provisioning.
</details>

<details>
<summary><strong>Q24: How can you dry-run and validate a merged Compose file using `docker compose config`?</strong></summary>

Running:
```bash
docker compose config
```
Parses all Compose manifests, merges overrides, resolves all variable interpolations (`${VAR}`), validates schema syntax, and prints the final, fully-resolved declarative YAML to stdout. Use `docker compose config -q` in CI scripts to validate syntax without output.
</details>

<details>
<summary><strong>Q25: What is `stop_grace_period` and why is it critical for stateful worker queues in Compose?</strong></summary>

When Compose stops a service, it sends `SIGTERM` and waits 10 seconds before sending `SIGKILL`.
For background worker queues (Celery, BullMQ, Kafka consumers) executing multi-minute batch operations:
```yaml
services:
  worker:
    image: worker:latest
    stop_grace_period: 2m
```
`stop_grace_period: 2m` extends the timeout to 120 seconds, allowing active jobs to finish processing and commit offsets before the process is killed.
</details>

---

[<- 03-networking-and-volumes-qa.md](./03-networking-and-volumes-qa.md) | [Back to Interview Overview](./README.md) | [Next Note: 05-security-qa.md ->](./05-security-qa.md)
