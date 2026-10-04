# Production

Taking an application that works on your laptop and running it reliably, securely, and repeatably for real users.

## "It works on my machine"

Everything so far (routing, auth, databases, queues, tests, observability) happens before a user ever touches your code. This section is about the gap between `npm start` on your laptop and a service people depend on at 3 a.m.

| On your laptop | In production |
|---|---|
| One process, one user (you) | Many instances, thousands of concurrent users |
| You restart it by hand | It must recover from crashes and deploys by itself |
| Config lives in your head or a `.env` file | Config and secrets are managed, rotated, and audited |
| Failures are visible in your terminal | Failures happen unobserved, at night, in places you can't attach a debugger |
| Data is disposable | Data is the business; losing it is not an option |
| "Deploy" means `git pull` | Deploy means a repeatable, reversible, zero-downtime process |
| Security is optional | Anything on the internet is attacked within minutes |
| Cost is zero | Every instance, gigabyte, and request has a price |

None of this is about writing different *application* code. It's about the **surrounding system**: how the app is configured, packaged, started, stopped, observed, deployed, and rolled back.

---

## The journey of a change

```
 Developer laptop
      │  git push
      ▼
 ┌─────────────┐   ┌─────────────────────────────────────────────────────────────┐
 │ Git (PR)    │──▶│ CI pipeline (05)                                            │
 └─────────────┘   │  install → lint → test (13) → security scan → build image   │
                   └──────────────────────────────┬──────────────────────────────┘
                                                  │ immutable image tagged with the commit SHA (03)
                                                  ▼
                                         ┌─────────────────┐
                                         │ Container       │
                                         │ registry (ECR)  │
                                         └────────┬────────┘
                                                  │
                   ┌──────────────────────────────┼──────────────────────────────┐
                   ▼                                                             ▼
          ┌──────────────────┐    smoke tests, approval          ┌──────────────────────────┐
          │ Staging (06)     │ ────────────────────────────────▶ │ Production (06)          │
          │ prod-like        │                                   │  ALB / Nginx (04)        │
          └──────────────────┘                                   │   ├─ app instance ×N     │
                                                                 │   ├─ workers (11)        │
                                                                 │   └─ health checks (02)  │
                                                                 │  RDS, Redis, S3, ...     │
                                                                 └────────────┬─────────────┘
                                                                              │ metrics, logs, traces (14)
                                                                              ▼
                                                              dashboards and alerts → rollback if bad
```

Three ideas run through the whole pipeline:

1. **Build once, deploy many.** The *same* image artifact moves from staging to production. Only the configuration differs, so what you tested is what you ship.
2. **Everything as code.** Dockerfiles, CI workflows, infrastructure, dashboards, alerts: reviewed in Git, reproducible, and recoverable.
3. **Every deploy is reversible,** and you can tell within minutes whether it was good.

---

## The twelve-factor app (the foundation)

The [twelve-factor methodology](https://12factor.net) is a set of practices for apps that deploy cleanly to modern platforms. Most of this section applies it:

| Factor | In practice | Covered in |
|---|---|---|
| **Codebase:** one repo, many deploys | One repo → the same image to staging and prod | `05` |
| **Dependencies:** declared explicitly | `package.json` + lockfile + `npm ci` | `03` |
| **Config:** stored in the environment | Environment variables, never hard-coded or committed | `01` |
| **Backing services:** attached resources | Database, Redis, queue are URLs in config, swappable | `01`, `06` |
| **Build, release, run:** strictly separate stages | CI builds an image; a release = image + config; run it | `03`, `05` |
| **Processes:** stateless | No local state; sessions in Redis, files in S3 | `06` |
| **Port binding:** the app exports a port | `app.listen(process.env.PORT)` | `01`, `04` |
| **Concurrency:** scale out via processes | More containers, not bigger ones | `06` |
| **Disposability:** fast start, graceful stop | Crashes and deploys are routine | `02` |
| **Dev/prod parity:** keep environments similar | Same DB engine, same container locally | `03` |
| **Logs:** a stream of events to stdout | Pino JSON → platform collects | `14` |
| **Admin processes:** one-off tasks run like the app | Migrations as a task using the same image | `05` |

---

## What's in this section

| File | What you learn |
|---|---|
| `01-environment-management.md` | `NODE_ENV`, environment variables, validating config at startup, secrets and secret managers, rotation |
| `02-graceful-shutdown-and-health-checks.md` | Handling `SIGTERM`, draining connections, liveness vs readiness probes, zero-downtime deploys |
| `03-docker-and-compose.md` | Writing a small, secure, cache-friendly Dockerfile for Node; Compose for local and multi-service setups |
| `04-nginx.md` | Reverse proxy, TLS, compression, static files, rate limiting, WebSockets, load balancing |
| `05-ci-cd.md` | CI pipelines with GitHub Actions, deployment strategies, database migrations in deploys, rollbacks |
| `06-aws.md` | A reference architecture on AWS (ECS Fargate, RDS, ElastiCache, ALB), IAM, networking, cost |
| `07-deployment-checklist.md` | A single go/no-go checklist that pulls the entire course together |

Read in order: configuration (`01`) and lifecycle (`02`) first, since they shape the container (`03`) and the proxy (`04`); the pipeline (`05`) and cloud (`06`) put them together; the checklist (`07`) is what you return to before every launch.

---

## Prerequisites

- `02-core-modules/07-process.md`: `process.env`, signals, exit codes
- `08-authentication-security/`: what you're protecting; `07-helmet.md` and `06-rate-limiting.md` overlap with the proxy layer
- `13-testing/`: the pipeline's quality gate
- `14-logging-observability/`: you can't run what you can't see
- `11-async-processing/`: workers deploy and shut down alongside the API

---

## Environments

Most teams run several environments, each with a purpose:

| Environment | Purpose | Data | Who uses it |
|---|---|---|---|
| **Local** | Development | Throwaway / seeded | One developer |
| **CI** | Automated tests | Ephemeral containers | The pipeline |
| **Preview / ephemeral** | One per pull request, for review | Seeded | Reviewers, QA |
| **Staging** | A production-like rehearsal: final checks, load tests, migration dry-runs | Anonymized or synthetic copy | Team, QA |
| **Production** | Real users | Real data | Everyone |

Principles:

- **Keep them as similar as practical** (same OS image, same database engine and major version, same Node version), so surprises are caught before production. Differences should be *configuration*, not code paths.
- **Never** point a non-production environment at production data or services.
- **Separate credentials** per environment, so a leak in one doesn't compromise another.
- **Don't branch on environment in code** (`if (env === "production") ...`) beyond a few sanctioned places (logging format, TLS). Every such branch is code that tests never exercise.

---

## Node.js-specific production concerns

| Concern | Why it matters | Where |
|---|---|---|
| **`NODE_ENV=production`** | Enables optimizations and safer defaults in Express and many libraries | `01` |
| **Single-threaded event loop** | One blocked request stalls every other; scale with more processes/containers | `02-core-modules/10-cluster-and-worker-threads.md`, `06` |
| **Crashes on unhandled errors** | A process that dies must be restarted automatically | `02`, `06` |
| **Memory limits** | The V8 heap and the container limit must be aligned or the process gets OOM-killed | `03` |
| **Signals** | `SIGTERM` on deploy; Node ignores it unless you handle it, and `npm start` swallows it | `02`, `03` |
| **Keep-alive timeouts** | Node's defaults can cause intermittent `502`s behind a load balancer | `02`, `04`, `06` |
| **Native modules** | `bcrypt`, `sharp` must match the image's OS/libc (Alpine vs Debian) | `03` |
| **Dependencies** | Production installs should be exact (`npm ci`) and exclude devDependencies | `03` |
| **No process manager needed in containers** | The orchestrator restarts containers; PM2 is for bare servers | `03`, `06` |

### Do you need PM2?

| Setup | Process management |
|---|---|
| **Containers** (ECS, Kubernetes, Cloud Run, App Runner) | The platform restarts crashed containers and handles scaling. **Don't add PM2**; one Node process per container |
| **Bare VM / bare metal** | Use a supervisor: **systemd** (preferred, built in) or **PM2** (`pm2 start`, cluster mode, log rotation, zero-downtime `reload`) |
| **Using all CPU cores on one machine** | Run N containers/processes (or `cluster`/PM2 cluster mode) rather than one |

---

## Principles

1. **Automate everything repeatable.** A manual step (SSH in, copy files, edit config) is a future outage.
2. **Immutable artifacts, mutable configuration.** Never patch a running server; build a new image.
3. **Treat servers as cattle, not pets.** Any instance can be killed and replaced at any time without anyone noticing.
4. **Stateless processes.** Sessions, uploads, caches, and job state live in backing services.
5. **Fail fast and loudly at startup** on bad config, rather than limping along and failing later at 2 a.m.
6. **Design for deploys:** graceful shutdown, health checks, and backward-compatible database changes make releases boring.
7. **Least privilege everywhere:** containers run as non-root, IAM roles grant only what's needed, networks expose only what's required.
8. **Make rollback a one-click operation,** and practice it.
9. **Observe before you launch,** not after the first incident.
10. **Keep it as simple as the problem allows.** A single container behind a managed load balancer beats a Kubernetes cluster you can't operate.

## Next

**`01-environment-management.md`** starts with the foundation everything else depends on: how configuration and secrets get into your app safely, and how to make a misconfigured deploy fail at startup instead of in front of users.