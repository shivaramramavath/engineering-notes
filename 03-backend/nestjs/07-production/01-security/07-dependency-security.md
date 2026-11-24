---
title: "Dependency Security and Supply Chain Protection in NestJS"
description: "Auditing third-party npm dependencies, SBOM generation, Dependabot, and automated vulnerability scanning in NestJS"
tags: [nestjs, security, npm-audit, snyk, sbom, supply-chain]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Dependency Security and Supply Chain Protection in NestJS

## Conceptual Overview
Modern Node.js and NestJS applications typically depend on hundreds of third-party packages in `node_modules`. Compromised packages, typosquatting, and unpatched CVEs represent major attack surfaces.

Supply chain defense requires:
- Continuous vulnerability scanning in CI/CD pipelines (`npm audit`, Snyk, Trivy).
- Software Bill of Materials (SBOM) generation (CycloneDX, SPDX).
- Lockfile pinning (`package-lock.json` or `pnpm-lock.yaml`).
- Enforcing least privilege via `npm ci --ignore-scripts`.

---

## Key Takeaways & Cheat Sheet

- Run `npm audit --audit-level=high` on every CI build.
- Use `npm ci --ignore-scripts` in Docker builds to prevent malicious pre/post-install scripts.
- Commit and strictly enforce lockfiles (`package-lock.json`).

---

## Cross-References
- [01-security-fundamentals.md](./01-security-fundamentals.md) - Security overview
- [08-security-checklist.md](./08-security-checklist.md) - Production security checklist
