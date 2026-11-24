# Module 14: Production & Deployment

Welcome to **Module 14: Production & Deployment**. This module guides engineers through deploying and running Spring Boot applications in production: production profiles, layered JAR packaging, Cloud Native Buildpacks, Google Jib, Kubernetes deployments, Helm charts, CI/CD pipelines, AWS deployment targets, zero-downtime database migrations, and Twelve-Factor compliance.

---

## Module Contents

| # | Note | Description | Level | Status |
|---|---|---|---|---|
| 01 | [01-production-configuration-and-profiles.md](file:///03-backend/spring-boot/14-production-and-deployment/01-production-configuration-and-profiles.md) | Hardening `application-prod.yaml`, disabling debug endpoints, secrets injection | Intermediate | Complete |
| 02 | [02-packaging-jar-layers-and-buildpacks.md](file:///03-backend/spring-boot/14-production-and-deployment/02-packaging-jar-layers-and-buildpacks.md) | Layered JAR extraction for Docker caching, Paketo Cloud Native Buildpacks | Intermediate | Complete |
| 03 | [03-docker-and-jib.md](file:///03-backend/spring-boot/14-production-and-deployment/03-docker-and-jib.md) | Multi-stage Dockerfiles vs Google Jib daemonless container builds | Intermediate | Complete |
| 04 | [04-kubernetes-deployment.md](file:///03-backend/spring-boot/14-production-and-deployment/04-kubernetes-deployment.md) | Writing Kubernetes Deployments, Services, Ingress, PodDisruptionBudgets, and HPA | Intermediate | Complete |
| 05 | [05-helm-basics.md](file:///03-backend/spring-boot/14-production-and-deployment/05-helm-basics.md) | Packaging Spring Boot Kubernetes applications into parameterized Helm charts | Intermediate | Complete |
| 06 | [06-ci-cd-pipelines.md](file:///03-backend/spring-boot/14-production-and-deployment/06-ci-cd-pipelines.md) | GitHub Actions pipeline for compiling, testing, containerizing, and deploying | Intermediate | Complete |
| 07 | [07-aws-deployment.md](file:///03-backend/spring-boot/14-production-and-deployment/07-aws-deployment.md) | Deploying Spring Boot to AWS ECS Fargate, EKS, and AWS Lambda with SnapStart | Advanced | Complete |
| 08 | [08-zero-downtime-deploys-and-db-migrations.md](file:///03-backend/spring-boot/14-production-and-deployment/08-zero-downtime-deploys-and-db-migrations.md) | Expand/Contract database migration pattern paired with Kubernetes rolling updates | Advanced | Complete |
| 09 | [09-secrets-and-config-in-production.md](file:///03-backend/spring-boot/14-production-and-deployment/09-secrets-and-config-in-production.md) | HashiCorp Vault, AWS Secrets Manager, and External Secrets Operator in K8s | Advanced | Complete |
| 10 | [10-graceful-shutdown-and-health-probes.md](file:///03-backend/spring-boot/14-production-and-deployment/10-graceful-shutdown-and-health-probes.md) | Coordinating Kubernetes `preStop` hooks with Spring Boot `server.shutdown=graceful` | Advanced | Complete |
| 11 | [11-twelve-factor-with-spring.md](file:///03-backend/spring-boot/14-production-and-deployment/11-twelve-factor-with-spring.md) | Verifying compliance with all Twelve Factors for cloud-native applications | Intermediate | Complete |
| 12 | [12-incident-runbooks.md](file:///03-backend/spring-boot/14-production-and-deployment/12-incident-runbooks.md) | Operational playbooks for OOM crashes, connection pool exhaustion, CPU spikes | Advanced | Complete |

---

## Next Steps
Proceed to **[Module 15: Architecture & Design Patterns](file:///03-backend/spring-boot/15-architecture-and-patterns/)** to master Hexagonal Architecture, Domain-Driven Design, and Spring Modulith.
