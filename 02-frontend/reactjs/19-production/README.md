# 19 — Production Engineering, Security & Deployment

Shipping a React application to production requires more than running `npm run build`. This module covers the engineering practices required to deploy, secure, containerize, monitor, and maintain mission-critical enterprise React applications in production.

---

## What you'll be able to do after this module

- **Manage Environment Configurations**: Handle `import.meta.env` safely, inject build-time variables, and manage runtime configurations across environments.
- **Master Production Builds**: Optimize Rollup/Vite output, analyze bundle sizes, configure source maps securely, and configure asset hashing.
- **Deploy to Global CDNs**: Host Single Page Applications on Vercel, Netlify, Cloudflare Pages, or AWS S3 + CloudFront with clean URL rewrite rules.
- **Containerize with Docker**: Write production multi-stage Dockerfiles with optimized Nginx configurations for caching and SPA routing.
- **Automate CI/CD Pipelines**: Build GitHub Actions workflows for continuous integration (lint, typecheck, test) and automated preview deployments.
- **Harden Frontend Security**: Audit dependencies, prevent secret leakage in client bundles, and eliminate XSS vulnerabilities.
- **Implement Strict CSP & Trusted Types**: Enforce nonce-based Content Security Policies and DOMPurify sanitization.
- **Monitor Errors & Telemetry**: Integrate Sentry with source maps, capture error breadcrumbs, and stream Core Web Vitals telemetry.
- **Execute Production Launches**: Use the structured production checklist to guarantee zero-downtime releases with verified rollbacks.

---

## Module Syllabus

| # | Note | Description | Key Concept / Tool |
|---|------|-------------|:---:|
| 00 | [00-environment-variables.md](./00-environment-variables.md) | `import.meta.env`, VITE_ prefixes, secrets safety, runtime injection | Configuration |
| 01 | [01-build.md](./01-build.md) | Production Vite/Rollup builds, minification, tree-shaking, source maps | Build Optimization |
| 02 | [02-deployment.md](./02-deployment.md) | Static hosting, SPA fallback rewrites, edge headers, CDN caching | Deployment |
| 03 | [03-docker.md](./03-docker.md) | Multi-stage Docker builds, Alpine Nginx, container security, caching | Docker & Containers |
| 04 | [04-ci-cd.md](./04-ci-cd.md) | GitHub Actions CI/CD workflows, automated testing, preview environments | CI / CD |
| 05 | [05-security.md](./05-security.md) | OWASP frontend top 10, dependency audits, client secret isolation | Security |
| 06 | [06-error-monitoring-and-logging.md](./06-error-monitoring-and-logging.md) | Sentry integration, error boundaries, sourcemap uploads, breadcrumbs | Error Monitoring |
| 07 | [07-performance-monitoring.md](./07-performance-monitoring.md) | Real User Monitoring (RUM), web-vitals telemetry, Core Web Vitals | Telemetry & Observability |
| 08 | [08-production-checklist.md](./08-production-checklist.md) | Comprehensive launch readiness checklist, rollback procedures | Quality Assurance |
| 09 | [09-csp-and-xss-hardening.md](./09-csp-and-xss-hardening.md) | Strict Content Security Policy, nonces, Trusted Types, DOMPurify | CSP & Hardening |

---

## Production Security & Caching Matrix

| Asset Type | Cache-Control Header | Invalidation Strategy |
| :--- | :--- | :--- |
| **`index.html`** | `no-cache, no-store, must-revalidate` | Always revalidated on origin / edge |
| **JS / CSS Assets (`*.hash.js`)** | `public, max-age=31536000, immutable` | Instant cache busting via content hashes |
| **Fonts & Static Images** | `public, max-age=31536000, immutable` | Permanent CDN cache |
| **API Endpoints (`/api/*`)** | `no-store, private` | Dynamic real-time data |

---

## Boundary with Other Modules

- **Performance Optimization**: Read [`14-performance/`](../14-performance/README.md) for code splitting and bundle reduction.
- **Testing & QA**: Read [`18-testing-and-debugging/`](../18-testing-and-debugging/README.md) for automated pre-deployment testing.
- **Architecture**: Read [`20-frontend-architecture/`](../20-frontend-architecture/README.md) for enterprise project organization.

---
