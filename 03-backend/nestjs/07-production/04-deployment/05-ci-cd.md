---
title: "CI/CD Pipelines for NestJS with GitHub Actions"
description: "Building production-grade CI/CD pipelines: automated linting, test matrices, Docker image publishing, and deployment workflows"
tags: [nestjs, deployment, cicd, github-actions, automated-testing, docker]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# CI/CD Pipelines for NestJS with GitHub Actions

## Conceptual Overview
A reliable Continuous Integration and Continuous Deployment (CI/CD) pipeline ensures that every code change is automatically validated, tested, packaged, and safely deployed to production environments without manual intervention.

---

## Production GitHub Actions Workflow

```yaml
# .github/workflows/deploy.yml
name: NestJS CI/CD Pipeline

on:
  push:
    branches: [main]
  pull_request:
    branches: [main]

jobs:
  validate:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Setup Node.js 22
        uses: actions/setup-node@v4
        with:
          node-version: 22
          cache: 'npm'

      - name: Install Dependencies
        run: npm ci

      - name: Type Check & Lint
        run: |
          npm run lint
          npx tsc --noEmit

      - name: Run Unit Tests
        run: npm run test:cov

      - name: Run E2E Tests
        run: npm run test:e2e

  build-and-push:
    needs: validate
    if: github.ref == 'refs/heads/main'
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Set up Docker Buildx
        uses: actions/setup-buildx-action@v3

      - name: Log in to Registry
        uses: docker/login-action@v3
        with:
          registry: ghcr.io
          username: ${{ github.actor }}
          password: ${{ secrets.GITHUB_TOKEN }}

      - name: Build and Push Docker Image
        uses: docker/build-push-action@v5
        with:
          context: .
          push: true
          tags: ghcr.io/${{ github.repository }}:latest,ghcr.io/${{ github.repository }}:${{ github.sha }}
          cache-from: type=gha
          cache-to: type=gha,mode=max
```

---

## Key Takeaways & Cheat Sheet

- Gate deployments behind automated lint, type-check, unit test, and e2e test stages.
- Cache npm dependencies and Docker build layers using GitHub Actions cache.
- Tag Docker images with git commit SHAs for reliable rollbacks.

---

## Cross-References
- [03-docker.md](./03-docker.md) - Multi-stage Docker builds
- [10-production-checklist.md](./10-production-checklist.md) - Pre-flight checklist
