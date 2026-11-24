# Module 15: Production Engineering & Deployment

Welcome to **Module 15: Production Engineering & Deployment**. This module covers the operational engineering required to package, containerize, deploy, and maintain robust FastAPI services across modern cloud environments.

---

## Module Overview

| File | Title | Description |
| :--- | :--- | :--- |
| [01-production-configuration.md](./01-production-configuration.md) | **Production Configuration** | Safe environment parsing, immutable settings, and secret masking. |
| [02-running-with-uvicorn-gunicorn-and-process-managers.md](./02-running-with-uvicorn-gunicorn-and-process-managers.md)| **Process Supervision** | Gunicorn master with Uvicorn workers, Systemd, and process managers. |
| [03-docker-multi-stage-builds-with-uv.md](./03-docker-multi-stage-builds-with-uv.md)| **Docker Multi-Stage with uv** | Lightning-fast Docker builds using `uv`, non-root execution, and small images. |
| [04-nginx-and-reverse-proxy-settings.md](./04-nginx-and-reverse-proxy-settings.md) | **Nginx & Reverse Proxies** | Forwarded headers (`X-Forwarded-Proto`), SSL termination, and `root_path`. |
| [05-kubernetes-deployment-and-probes.md](./05-kubernetes-deployment-and-probes.md)| **Kubernetes Manifests** | Deployments, Services, ConfigMaps, Secrets, HPA, and lifecycle preStop hooks. |
| [06-ci-cd-with-github-actions.md](./06-ci-cd-with-github-actions.md) | **CI/CD Pipelines** | GitHub Actions workflows for linting, matrix testing, and automated deployment. |
| [07-aws-deployment.md](./07-aws-deployment.md) | **AWS Deployment Options** | ECS Fargate, EKS, and serverless Lambda deployment using Mangum. |
| [08-serverless-considerations.md](./08-serverless-considerations.md) | **Serverless Considerations** | Cold starts, connection pool exhaustion in serverless, and timeout constraints. |
| [09-zero-downtime-deploys-and-migrations.md](./09-zero-downtime-deploys-and-migrations.md)| **Zero-Downtime Deployments** | Blue/green, canary deployments, and backward-compatible Alembic migrations. |
| [10-secrets-and-config-in-production.md](./10-secrets-and-config-in-production.md) | **Production Secrets** | AWS Secrets Manager, HashiCorp Vault, and dynamic secret rotation. |
| [11-graceful-shutdown.md](./11-graceful-shutdown.md) | **Graceful Shutdown** | Handling SIGTERM, draining in-flight HTTP requests, and releasing pools. |
| [12-twelve-factor-with-fastapi.md](./12-twelve-factor-with-fastapi.md) | **12-Factor App Principles** | Implementing the Twelve-Factor App methodology cleanly with FastAPI. |
| [13-incident-runbooks.md](./13-incident-runbooks.md) | **Incident Runbooks** | Standard operating procedures for outage diagnosis, 502 Bad Gateways, and high OOM kills. |
