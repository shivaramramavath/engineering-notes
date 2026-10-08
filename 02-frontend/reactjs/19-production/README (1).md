# 19 — Production

Code that works on your machine isn't done. **Production** is everything between "it works locally" and "real users depend on it": configuring it per environment, building it reproducibly, shipping it safely, protecting it, and knowing when it breaks.

This folder follows the path your code takes after you stop editing it:

```text
 config ──► build ──► test ──► package ──► deploy ──► observe ──► (fix, repeat)
  (00)      (01)     (CI, 04)   (03)        (02)      (06, 07)
                         └──── security (05) applies at every step ────┘
                         └──── checklist (08) is the final gate ────────┘
```

## The production mindset

- **Reproducible**: the same commit builds the same artifact, every time, anywhere.
- **Automated**: humans don't copy files to servers; a pipeline does ([CI/CD](./04-ci-cd.md)).
- **Reversible**: every release can be rolled back quickly.
- **Observable**: when something breaks, *you* find out first, with enough context to fix it ([monitoring](./06-error-monitoring-and-logging.md)).
- **Secure by default**: the browser is hostile territory, and nothing secret ships to it ([security](./05-security.md)).
- **Boring**: the best deploys are uneventful.

## Prerequisites

- [Testing and debugging](../18-testing-and-debugging/README.md): the suite your pipeline will run
- [Performance](../14-performance/README.md): what you'll be measuring in production
- [API integration](../11-api-integration/README.md): auth, CORS, and error handling touch every production concern

## Contents

| # | File | What you'll learn |
|---|------|-------------------|
| 00 | [Environment variables](./00-environment-variables.md) | Vite env files, what's public, build-time vs runtime config |
| 01 | [Build](./01-build.md) | What `vite build` does, reproducibility, source maps, versioning |
| 02 | [Deployment](./02-deployment.md) | Static hosting, SPA fallback, caching, environments, rollbacks |
| 03 | [Docker](./03-docker.md) | Multi-stage images, nginx, runtime config, hardening |
| 04 | [CI/CD](./04-ci-cd.md) | Pipeline stages, GitHub Actions, previews, secrets |
| 05 | [Security](./05-security.md) | XSS, CSP, headers, supply chain, secrets |
| 06 | [Error monitoring and logging](./06-error-monitoring-and-logging.md) | Capturing errors, source maps, noise, privacy |
| 07 | [Performance monitoring](./07-performance-monitoring.md) | Real-user metrics, budgets, regression alerts |
| 08 | [Production checklist](./08-production-checklist.md) | A pre-launch and post-launch checklist |

## Suggested order

Read 00 → 01 → 02 in order (config, build, ship). 03 is optional if you deploy to a static host. 04 ties everything into a pipeline, 05 applies throughout, 06 and 07 close the loop, and 08 is a one-page review you return to before every launch.

## Scope and conventions

- These notes assume a **Vite single-page app** (the app built throughout this repo), served as static files. Server-rendered frameworks have additional concerns ([SSR](../15-concurrent-and-modern-react/07-server-components-and-ssr.md)).
- Hosts, CI tools, and monitoring vendors change constantly. The **principles** are stable; **specific config syntax** (host rewrites, GitHub Actions versions, vendor SDK options) should be checked against current docs, and the notes flag this where relevant.
