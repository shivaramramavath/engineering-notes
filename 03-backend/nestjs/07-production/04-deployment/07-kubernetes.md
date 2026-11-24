---
title: "Kubernetes Orchestration for NestJS"
description: "Production Kubernetes manifests: Deployment, Service, Ingress, Horizontal Pod Autoscaler (HPA), and PodDisruptionBudgets for NestJS"
tags: [nestjs, deployment, kubernetes, k8s, hpa, ingress, deployment-manifests]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Kubernetes Orchestration for NestJS

## Conceptual Overview
Kubernetes (K8s) provides declarative container management, zero-downtime rolling deployments, and automated scaling for enterprise microservices.

---

## Production Kubernetes Manifests

```yaml
# k8s/deployment.yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: nestjs-api
  namespace: production
  labels:
    app: nestjs-api
spec:
  replicas: 3
  strategy:
    type: RollingUpdate
    rollingUpdate:
      maxSurge: 1
      maxUnavailable: 0
  selector:
    matchLabels:
      app: nestjs-api
  template:
    metadata:
      labels:
        app: nestjs-api
    spec:
      containers:
        - name: api
          image: ghcr.io/org/nestjs-api:latest
          imagePullPolicy: IfNotPresent
          ports:
            - containerPort: 3000
          resources:
            requests:
              cpu: 250m
              memory: 256Mi
            limits:
              cpu: 1000m
              memory: 512Mi
          livenessProbe:
            httpGet:
              path: /health/liveness
              port: 3000
            initialDelaySeconds: 15
            periodSeconds: 10
          readinessProbe:
            httpGet:
              path: /health/readiness
              port: 3000
            initialDelaySeconds: 5
            periodSeconds: 5
          envFrom:
            - configMapRef:
                name: api-config
            - secretRef:
                name: api-secrets
---
apiVersion: autoscaling/v2
kind: HorizontalPodAutoscaler
metadata:
  name: nestjs-api-hpa
  namespace: production
spec:
  scaleTargetRef:
    apiVersion: apps/v1
    kind: Deployment
    name: nestjs-api
  minReplicas: 3
  maxReplicas: 20
  metrics:
    - type: Resource
      resource:
        name: cpu
        target:
          type: Utilization
          averageUtilization: 70
```

---

## Key Takeaways & Cheat Sheet

- Configure `maxUnavailable: 0` for zero-downtime rolling updates.
- Set accurate resource requests and limits to prevent pod OOM kills.
- Configure HPA to scale pods based on CPU and memory thresholds.

---

## Cross-References
- [04-health-checks.md](../03-observability/04-health-checks.md) - Terminus health probes
- [08-horizontal-scaling.md](./08-horizontal-scaling.md) - Scaling patterns
