---
title: "Cloud Deployment on AWS: ECS Fargate, EKS, and App Runner"
description: "Architecting enterprise NestJS deployments on Amazon Web Services: ECS Fargate, ALB, RDS Aurora, ElastiCache Redis, and CloudWatch"
tags: [nestjs, deployment, aws, ecs, fargate, alb, aurora, elasticache]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Cloud Deployment on AWS: ECS Fargate, EKS, and App Runner

## Conceptual Overview
Deploying NestJS on AWS requires designing for high availability, multi-AZ fault tolerance, secure secret injection, and automated horizontal scaling.

Core AWS Architecture:
- **Compute**: AWS ECS Fargate (serverless container orchestration) or EKS (Kubernetes).
- **Networking**: Application Load Balancer (ALB) with SSL termination and target groups.
- **Database**: Amazon RDS Aurora PostgreSQL (Multi-AZ with read replicas).
- **Caching**: Amazon ElastiCache Redis cluster.
- **Secrets**: AWS Secrets Manager / Parameter Store injected at task startup.

---

## Key Takeaways & Cheat Sheet

- Deploy NestJS containers across at least two Availability Zones (AZs).
- Use ECS Fargate for low-maintenance serverless container execution.
- Inject secrets from AWS Secrets Manager directly into container task definitions.

---

## Cross-References
- [03-docker.md](./03-docker.md) - Docker containerization
- [07-kubernetes.md](./07-kubernetes.md) - Kubernetes orchestration
