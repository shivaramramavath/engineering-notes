# Automated Testing, Ephemeral Test Stacks, and CI/CD Integration

```
Track: DevOps & Containerization
Module: 05-compose
Prerequisites: 05-compose/01-compose-basics.md, 05-compose/05-depends-on-and-healthchecks.md
Difficulty: Advanced
Estimated Duration: 45 minutes
Target Docker Version: 27.x+ (Compose Specification v2.x+)
Last Validated: February 2025
```

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [The CI Test Execution Architecture](#3-the-ci-test-execution-architecture)
4. [The Critical CLI Flags: `--exit-code-from` and `--abort-on-container-exit`](#4-the-critical-cli-flags---exit-code-from-and---abort-on-container-exit)
5. [Syntax & Implementation Reference](#5-syntax--implementation-reference)
6. [Practical Implementation: End-to-End Integration Test Pipeline](#6-practical-implementation-end-to-end-integration-test-pipeline)
7. [Production Best Practices & Antipatterns](#7-production-best-practices--antipatterns)
8. [Common Pitfalls & Troubleshooting](#8-common-pitfalls--troubleshooting)
9. [Performance Tuning & Resource Optimization](#9-performance-tuning--resource-optimization)
10. [Security Hardening & Threat Model](#10-security-hardening--threat-model)
11. [Architectural Comparison: Docker Compose vs Testcontainers](#11-architectural-comparison-docker-compose-vs-testcontainers)
12. [Real-World Scenario Walkthrough: GitHub Actions Workflow](#12-real-world-scenario-walkthrough-github-actions-workflow)
13. [Self-Assessment & Interview Questions](#13-self-assessment--interview-questions)
14. [Hands-on Practice Exercises](#14-hands-on-practice-exercises)
15. [Summary & Reference Cheat Sheet](#summary--reference-cheat-sheet)

---

## 1. What & Why

Unit testing with mock databases frequently masks critical production bugs:
- SQL query syntax that executes fine in SQLite fails against PostgreSQL.
- Database locking behaviors, constraints, transactions, and index migrations behave completely differently in memory than in real database engines.
- Network latency, connection timeouts, and connection pool exhaustion never surface in mock environments.

Docker Compose is the gold standard for orchestrating **ephemeral integration and End-to-End (E2E) testing environments**. It allows automated test runners (Jest, PyTest, JUnit, Playwright) to execute against real, production-identical database, cache, and broker instances in CI/CD pipelines, cleanly tearing down all state upon completion.

---

## 2. Core Concepts

### The Test Runner Pattern
In an automated testing stack, services fall into two categories:
1. **Infrastructure Dependencies (Background Daemons)**: PostgreSQL, Redis, RabbitMQ, Mock APIs. These must run continuously throughout the test suite.
2. **The Test Runner (Ephemeral Task)**: The test execution container running PyTest, Jest, or Go test suites. When this container finishes:
   - Its **exit code** must become the overall exit code of the CI job (0 = PASS, 1 = FAIL).
   - All background infrastructure containers must be automatically terminated.

### Ephemeral Project Namespacing in CI
Running multiple concurrent CI builds on the same host (e.g. Jenkins or GitLab runner) can cause container name and network collisions. Passing a unique project name per build (`-p build-${CI_JOB_ID}`) creates completely isolated, parallel test stacks on the same Docker host.

---

## 3. The CI Test Execution Architecture

```
+-------------------------------------------------------------------------+
|                         CI RUNNER (GitHub Actions)                      |
|                                                                         |
|  Command: docker compose -p test-104 up \                               |
|              --abort-on-container-exit \                                |
|              --exit-code-from test-suite                                |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                  EPHEMERAL COMPOSE TEST ENVIRONMENT                     |
|                                                                         |
|  [database] (postgres:17) <--------+                                    |
|     ^                              | (TCP 5432)                         |
|     | (healthcheck: healthy)       |                                    |
|     |                              |                                    |
|  [test-suite] ---------------------+                                    |
|    - Runs: pytest / npm test                                            |
|    - Passes: Exits with CODE 0  =================> CI JOB PASSES        |
|    - Fails:  Exits with CODE 1  =================> CI JOB FAILS         |
|                                                                         |
|  Compose intercepts exit code -> Automatically kills database           |
|  Teardown: docker compose -p test-104 down -v                           |
+-------------------------------------------------------------------------+
```

---

## 4. The Critical CLI Flags: `--exit-code-from` and `--abort-on-container-exit`

### `--abort-on-container-exit`
Tells Compose: if *any* container terminates (specifically the test runner), stop all other running containers immediately rather than leaving databases running forever in the background.

### `--exit-code-from <SERVICE_NAME>`
Tells Compose: capture the exact exit code of `<SERVICE_NAME>` (0 or non-zero) and make `docker compose up` return that exact code to the host shell.
- If the test suite passes, `docker compose up` exits with `0`.
- If a test fails, `docker compose up` exits with `1`, failing the CI pipeline!

Combined Invocation:
```bash
docker compose up --abort-on-container-exit --exit-code-from test-runner
```

---

## 5. Syntax & Implementation Reference

### Compose Manifest for Testing (`compose.test.yaml`)

```yaml
services:
  # 1. Real PostgreSQL database instance
  db:
    image: postgres:17-alpine
    environment:
      POSTGRES_DB: test_db
      POSTGRES_USER: test_user
      POSTGRES_PASSWORD: test_password
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U test_user -d test_db"]
      interval: 2s
      timeout: 2s
      retries: 10
      start_period: 3s
    # Mount tmpfs for test database speed (RAM is 10x faster than disk!)
    volumes:
      - type: tmpfs
        target: /var/lib/postgresql/data

  # 2. Real Redis Cache instance
  cache:
    image: redis:7-alpine
    healthcheck:
      test: ["CMD", "redis-cli", "ping"]
      interval: 2s
      timeout: 2s
      retries: 5

  # 3. Test Runner Container
  test-runner:
    build:
      context: .
      target: test
    environment:
      DATABASE_URL: "postgresql://test_user:test_password@db:5432/test_db"
      REDIS_URL: "redis://cache:6379"
      CI: "true"
    depends_on:
      db:
        condition: service_healthy
      cache:
        condition: service_healthy
```

---

## 6. Practical Implementation: End-to-End Integration Test Pipeline

### Automated Bash Wrapper with Trap Cleanup

```bash
#!/usr/bin/env bash
set -euo pipefail

# Generate unique project name based on Git commit or timestamp
PROJECT_NAME="test-${CI_COMMIT_SHORT_SHA:-$(date +%s)}"
COMPOSE_FILE="compose.test.yaml"

# Guarantee cleanup runs even if tests fail or script is cancelled
cleanup() {
  echo "--> Tearing down test environment: ${PROJECT_NAME}..."
  docker compose -p "${PROJECT_NAME}" -f "${COMPOSE_FILE}" down -v --remove-orphans || true
}
trap cleanup EXIT

echo "--> Building test runner image..."
docker compose -p "${PROJECT_NAME}" -f "${COMPOSE_FILE}" build test-runner

echo "--> Executing integration test suite..."
docker compose -p "${PROJECT_NAME}" -f "${COMPOSE_FILE}" up \
  --abort-on-container-exit \
  --exit-code-from test-runner

echo "--> All tests PASSED!"
```

---

## 7. Production Best Practices & Antipatterns

### Best Practices
- **Mount Database Data in `tmpfs` for Testing**: Mounting database data in a memory `tmpfs` mount accelerates integration tests by 5x-10x compared to physical disk writes.
- **Always Clean Up with `trap ... EXIT`**: In CI scripts, wrap Compose teardown in a Bash `trap` to ensure containers and volumes are purged even if tests fail or the build times out.
- **Avoid Hardcoding Host Port Bindings**: In `compose.test.yaml`, do **not** publish host ports (omit `ports:`). Let containers communicate strictly over the internal virtual bridge. This allows multiple CI test jobs to run in parallel on the same runner machine without port conflicts.
- **Use Dedicated Targets in Multi-Stage Builds**: Create a `target: test` in your Dockerfile containing test frameworks (PyTest, Jest) and development headers, keeping production images lean.

### Anti-Patterns
- **Using External Public Databases for CI**: Testing against shared remote databases leads to flaky tests, concurrency clashes, and data corruption when multiple builds execute simultaneously.
- **Forgetting `--exit-code-from`**: Running `docker compose up` without `--exit-code-from` causes the CI job to report success even if all tests failed!
- **Leaving Dangling Volumes**: Running `docker compose down` without `-v` leaves behind gigabytes of abandoned anonymous test volumes on CI runners.

---

## 8. Common Pitfalls & Troubleshooting

| Symptom | Root Cause | Diagnosis Command | Permanent Fix |
|:---|:---|:---|:---|
| CI job passes as "Green" even though tests failed | Omitted `--exit-code-from <service>` flag; Compose returned 0 because containers exited | Inspect CI console logs | Add `--exit-code-from test-runner --abort-on-container-exit`. |
| CI build fails with `port 5432 is already allocated` | Two parallel CI jobs running on same runner attempted to bind the same host port | Check `docker ps` on CI runner | Remove `ports:` from `compose.test.yaml`; use internal DNS discovery. |
| CI runner runs out of disk space after 20 builds | `docker compose down` run without `-v`, leaving orphaned volumes on host | `docker system df` | Always execute `docker compose down -v --remove-orphans`. |
| Tests fail with database connection timeout | Test runner started before database completed schema creation | Check test logs | Pair `depends_on` with `condition: service_healthy`. |
| CI job hangs indefinitely after tests complete | Background containers (db, redis) remain running because `--abort-on-container-exit` was omitted | CI job hits 60m timeout | Add `--abort-on-container-exit`. |
| Multi-stage test target builds entire production image unnecessarily | Multi-stage Dockerfile ordering builds production assets before test dependencies | Inspect Dockerfile stages | Reorder stages so `test` stage branches early from `builder` dependencies. |

---

## 9. Performance Tuning & Resource Optimization

1. **Tmpfs Mount for Ephemeral Databases**:
   Database tests execute thousands of insert/update/delete operations and schema migrations. Placing PostgreSQL or MySQL storage on a RAM-backed `tmpfs` mount eliminates disk I/O latency:
   ```yaml
   db:
     image: postgres:17-alpine
     volumes:
       - type: tmpfs
         target: /var/lib/postgresql/data
   ```
2. **BuildKit Layer Cache Mounts**:
   In your test Dockerfile, use cache mounts for package managers (`--mount=type=cache,target=/root/.cache/pip` or `npm ci`) so dependency downloads are cached across consecutive CI runs.

---

## 10. Security Hardening & Threat Model

```
+--------------------------------------------------------------+
| ATTACK SCENARIO: Leaking Production Secrets into Test Stacks |
+--------------------------------------------------------------+
| CI pipeline injects production AWS and database credentials  |
| into integration test runner containers.                     |
| Vulnerable third-party test dependency exfiltrates env vars. |
|                                                              |
| HARDENING:                                                   |
| 1. Use dedicated, isolated mock credentials in test manifests.|
| 2. Never mount host production configuration into CI tests.   |
| 3. Disable outbound internet access for test-runner via       |
|    `internal: true` network definition when running tests.    |
+--------------------------------------------------------------+
```

---

## 11. Architectural Comparison: Docker Compose vs Testcontainers

| Feature | Docker Compose for Testing | Testcontainers (Java/Go/Python/Node) |
|:---|:---|:---|
| **Orchestration Control** | Declarative YAML manifest | Programmatic code (JUnit/PyTest) |
| **Language Agnostic?** | **100% Yes** (Universal CLI) | Requires language-specific SDK |
| **Host Port Assignment** | Fixed or internal bridge | Dynamic randomized ports per test |
| **Parallel Test Isolation**| Project-level (`-p <name>`) | Native per-test-class isolation |
| **Lifecycle Coordination** | Handled via Compose CLI flags| Handled inside language runtime |
| **Best Used For** | Full E2E stacks, CI pipelines | Fine-grained unit/integration tests |

---

## 12. Real-World Scenario Walkthrough: GitHub Actions Workflow

### Complete GitHub Actions CI Configuration

```yaml
# .github/workflows/test.yml
name: Continuous Integration & Testing

on:
  pull_request:
    branches: [main]

jobs:
  integration-tests:
    runs-on: ubuntu-latest
    steps:
      - name: Checkout Code
        uses: actions/checkout@v4

      - name: Set up Docker Buildx
        uses: docker/setup-buildx-action@v3

      - name: Run Integration Tests via Docker Compose
        env:
          COMPOSE_PROJECT_NAME: ci-${{ github.run_id }}
        run: |
          # Guarantee cleanup on failure
          trap "docker compose -f compose.test.yaml down -v --remove-orphans" EXIT

          # Build test runner
          docker compose -f compose.test.yaml build test-runner

          # Run test suite and capture exit code
          docker compose -f compose.test.yaml up \
            --abort-on-container-exit \
            --exit-code-from test-runner
```

---

## 13. Self-Assessment & Interview Questions

<details>
<summary><strong>1. Why is `--abort-on-container-exit` mandatory when running test suites with Docker Compose?</strong></summary>

**Answer:**
By default, Docker Compose keeps a stack running as long as *any* container is alive. In a test stack, background services (like PostgreSQL and Redis) run indefinitely. When the test runner container finishes executing its tests and exits, Compose would otherwise leave the database running forever, causing CI jobs to hang until hitting their timeout. `--abort-on-container-exit` forces Compose to terminate all remaining containers as soon as the first container (the test runner) exits.
</details>

<details>
<summary><strong>2. How does `--exit-code-from <service>` ensure CI pipelines accurately detect test failures?</strong></summary>

**Answer:**
Standard `docker compose up` exits with status `0` as long as the CLI successfully communicated with the Docker daemon, even if individual containers crashed or exited with error codes. `--exit-code-from <service>` instructs Compose to capture the exact exit status of the specified test runner service and return it as the exit code of the `docker compose` command itself, enabling CI systems (GitHub Actions, GitLab CI) to flag the build as failed when tests fail.
</details>

<details>
<summary><strong>3. Why should you avoid defining host port mappings (`ports: - "5432:5432"`) in `compose.test.yaml`?</strong></summary>

**Answer:**
If host ports are mapped, only one instance of the test stack can run on the host at any given time. If multiple CI builds or parallel pull request jobs execute concurrently on the same runner node, subsequent jobs will crash with `port 5432 is already allocated`. By omitting host port mappings and relying strictly on internal Docker network DNS, dozens of independent test stacks can execute in parallel on the same host without port conflicts.
</details>

<details>
<summary><strong>4. How does mounting a database directory in `tmpfs` accelerate automated test execution?</strong></summary>

**Answer:**
Databases under test perform heavy write-ahead log (WAL) flushes, frequent table drops, and bulk inserts. Physical disk I/O and copy-on-write storage driver overhead add significant latency. Mounting `/var/lib/postgresql/data` as a `tmpfs` mount stores the entire database in volatile host RAM, bypassing disk controllers and increasing test execution speed by up to 10x.
</details>

<details>
<summary><strong>5. How do you prevent parallel CI jobs from interfering with each other's containers and networks on a shared Docker runner?</strong></summary>

**Answer:**
Assign an explicit, unique **Project Name** to each job using the `-p` flag or `COMPOSE_PROJECT_NAME` environment variable (e.g. `docker compose -p build-${CI_JOB_ID} ...`). This isolates all containers, networks, and volumes under a unique namespace, allowing completely independent parallel execution.
</details>

<details>
<summary><strong>6. Why should cleanup commands in CI scripts always be wrapped in a Bash `trap`?</strong></summary>

**Answer:**
If an integration test fails or an unexpected script error occurs, Bash terminates immediately (when `set -e` is active). Any cleanup commands placed after the test command will never execute, leaving orphaned containers, lingering volumes, and consumed memory on the runner host. Wrapping `docker compose down -v` in a `trap cleanup EXIT` guarantees that teardown occurs unconditionally on success, failure, or job cancellation.
</details>

<details>
<summary><strong>7. When would an engineering team prefer Testcontainers over Docker Compose for integration testing?</strong></summary>

**Answer:**
Testcontainers is preferred when tests require fine-grained programmatic control from within the test framework itself (e.g., creating fresh isolated database instances per test class, simulating network latency/chaos, or retrieving dynamic randomly allocated ports directly in test setup code). Docker Compose is preferred for language-agnostic stacks, full multi-service architectures, and standardized CI/CD pipelines.
</details>

<details>
<summary><strong>8. What is the role of `depends_on: { db: { condition: service_healthy } }` in automated testing?</strong></summary>

**Answer:**
It prevents the test runner from initiating test execution until the database has finished initialization and passed its health probe. Without this condition, the test suite boots immediately and crashes with database connection errors before the database has opened its listening socket.
</details>

---

## 14. Hands-on Practice Exercises

<details>
<summary><strong>Exercise 1: Executing a Passing Test Suite with Exit Code Propagation</strong></summary>

**Goal:** Create a Compose stack where a passing test exits with 0 and stops the database.

**Step-by-step Solution:**
1. Create `compose.test.yaml`:
   ```yaml
   services:
     db:
       image: redis:alpine
       healthcheck:
         test: ["CMD", "redis-cli", "ping"]
         interval: 1s
         timeout: 1s
         retries: 3
     tester:
       image: alpine
       command: sh -c "echo 'Running tests... OK' && exit 0"
       depends_on:
         db:
           condition: service_healthy
   ```
2. Run test:
   ```bash
   docker compose -f compose.test.yaml up --abort-on-container-exit --exit-code-from tester
   echo "Exit code: $?"
   # Exit code: 0
   ```
3. Tear down: `docker compose -f compose.test.yaml down -v`
</details>

<details>
<summary><strong>Exercise 2: Verifying Failure Detection on Test Error</strong></summary>

**Goal:** Verify that a failing test suite returns non-zero to the shell.

**Step-by-step Solution:**
1. Modify `tester` command to: `sh -c "echo 'Tests failed!' && exit 1"`
2. Run:
   ```bash
   docker compose -f compose.test.yaml up --abort-on-container-exit --exit-code-from tester
   echo "Exit code: $?"
   # Exit code: 1 (Build fails properly!)
   ```
</details>

<details>
<summary><strong>Exercise 3: Accelerating Test Database with Tmpfs</strong></summary>

**Goal:** Configure a PostgreSQL database entirely in RAM for test speed.

**Step-by-step Solution:**
1. Add `volumes: [ { type: tmpfs, target: /var/lib/postgresql/data } ]` to PostgreSQL service.
2. Verify inside container via `df -h /var/lib/postgresql/data` that filesystem is `tmpfs`.
</details>

<details>
<summary><strong>Exercise 4: Running Isolated Parallel Stacks with `-p`</strong></summary>

**Goal:** Launch two identical test stacks simultaneously on the same host.

**Step-by-step Solution:**
1. Terminal 1: `docker compose -p stack-alpha -f compose.test.yaml up -d db`
2. Terminal 2: `docker compose -p stack-beta -f compose.test.yaml up -d db`
3. Inspect `docker ps` to verify both databases coexist without collision.
4. Clean up both stacks.
</details>

<details>
<summary><strong>Exercise 5: Authoring a Bulletproof Bash Test Runner with Trap</strong></summary>

**Goal:** Verify that `trap` destroys containers even when tests abort.

**Step-by-step Solution:**
1. Author bash script containing `trap "docker compose down -v" EXIT`.
2. Introduce intentional failure.
3. Observe teardown executing cleanly upon exit.
</details>

<details>
<summary><strong>Exercise 6: Inspecting Docker System Space Reclaim After `-v`</strong></summary>

**Goal:** Verify that `-v` leaves zero residual anonymous volumes.

**Step-by-step Solution:**
1. Run `docker volume ls` before and after running `docker compose down -v`.
</details>

---

## 15. Summary & Reference Cheat Sheet

### The Gold Standard CI Test Execution Command
```bash
docker compose -p ci-${BUILD_ID} -f compose.test.yaml up \
  --build \
  --abort-on-container-exit \
  --exit-code-from <TEST_SERVICE>
```

### Essential Teardown Command
```bash
docker compose -p ci-${BUILD_ID} -f compose.test.yaml down -v --remove-orphans
```

### Top 4 Rules for Testing with Compose
1. Always pass **`--abort-on-container-exit`** so CI doesn't hang.
2. Always pass **`--exit-code-from <test_runner>`** so test failures fail the build.
3. Always wrap teardown in a Bash **`trap ... EXIT`**.
4. Use **`tmpfs`** mounts for databases to maximize I/O performance.

---

[← Previous: 07-compose-watch-and-dev-workflows.md](07-compose-watch-and-dev-workflows.md) | [Next: 09-compose-in-production-limits.md →](09-compose-in-production-limits.md)
