---
topic: "Amazon ECS & EKS Container Orchestration Cheatsheet"
module: "10-cheatsheets"
track: "aws"
difficulty: "reference"
last-tested: "2026-11"
tags:
  - "cheatsheet"
  - "containers"
  - "ecs"
  - "fargate"
  - "eks"
  - "karpenter"
  - "kubectl"
---

# Amazon ECS & EKS Container Orchestration Cheatsheet

A high-density reference card for Amazon ECS Fargate task definitions, debugging exit codes, and Amazon EKS / Karpenter manifest templates.

---

## 1. ECS Fargate Task Definition JSON Template

```json
{
  "family": "api-microservice-task",
  "networkMode": "awsvpc",
  "requiresCompatibilities": ["FARGATE"],
  "cpu": "512",
  "memory": "1024",
  "executionRoleArn": "arn:aws:iam::123456789012:role/ecs-task-execution-role",
  "taskRoleArn": "arn:aws:iam::123456789012:role/ecs-app-task-role",
  "containerDefinitions": [
    {
      "name": "api-container",
      "image": "123456789012.dkr.ecr.us-east-1.amazonaws.com/api:v1.2",
      "essential": true,
      "portMappings": [
        {
          "containerPort": 8080,
          "hostPort": 8080,
          "protocol": "tcp"
        }
      ],
      "environment": [
        { "name": "NODE_ENV", "value": "production" }
      ],
      "secrets": [
        {
          "name": "DB_PASSWORD",
          "valueFrom": "arn:aws:secretsmanager:us-east-1:123456789012:secret:prod/db:password::"
        }
      ],
      "logConfiguration": {
        "logDriver": "awslogs",
        "options": {
          "awslogs-group": "/ecs/api-microservice",
          "awslogs-region": "us-east-1",
          "awslogs-stream-prefix": "app"
        }
      },
      "stopTimeout": 30
    }
  ]
}
```

---

## 2. ECS Container Exit Codes & Triage

| Exit Code | Reason / Signal | Root Cause & Remediation |
|:---|:---|:---|
| **0** | Normal Clean Exit | Container finished its execution command normally. |
| **1** | Application Runtime Error | Unhandled application exception or missing environment variable; check CloudWatch logs. |
| **137** | `SIGKILL` (Out of Memory - OOM) | The container exceeded its configured task memory limit. **Increase `memory` allocation**. |
| **139** | `SIGSEGV` (Segmentation Fault) | Memory access violation in compiled binary / native C-extension. |
| **143** | `SIGTERM` (Graceful Termination)| ECS stopped the task (service scaling in or new deployment rolling update). |

---

## 3. ECS CLI Quick Troubleshooting Commands

```bash
# Describe task failure reason
aws ecs describe-tasks \
    --cluster production \
    --tasks <task-arn> \
    --query "tasks[0].[stoppedReason,containers[0].[name,exitCode,reason]]"

# Force new deployment pulling latest image
aws ecs update-service --cluster production --service web-api --force-new-deployment

# Open interactive shell inside running Fargate container
aws ecs execute-command \
    --cluster production \
    --task <task-arn> \
    --container api-container \
    --interactive \
    --command "/bin/sh"
```

---

## 4. Amazon EKS: IAM Roles for Service Accounts (IRSA)

Annotate the Kubernetes ServiceAccount with the IAM Role ARN:

```yaml
apiVersion: v1
kind: ServiceAccount
metadata:
  name: order-service-sa
  namespace: production
  annotations:
    eks.amazonaws.com/role-arn: arn:aws:iam::123456789012:role/EKSOrderServiceRole
```

Corresponding IAM Role Trust Policy (OIDC):

```json
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Effect": "Allow",
      "Principal": {
        "Federated": "arn:aws:iam::123456789012:oidc-provider/oidc.eks.us-east-1.amazonaws.com/id/EXAMPLED55D4DB2D224B"
      },
      "Action": "sts:AssumeRoleWithWebIdentity",
      "Condition": {
        "StringEquals": {
          "oidc.eks.us-east-1.amazonaws.com/id/EXAMPLED55D4DB2D224B:sub": "system:serviceaccount:production:order-service-sa",
          "oidc.eks.us-east-1.amazonaws.com/id/EXAMPLED55D4DB2D224B:aud": "sts.amazonaws.com"
        }
      }
    }
  ]
}
```

---

## 5. Karpenter NodePool Manifest Template (EKS Autoscaling)

```yaml
apiVersion: karpenter.sh/v1beta1
kind: NodePool
metadata:
  name: default-nodepool
spec:
  template:
    spec:
      requirements:
        - key: "karpenter.k8s.aws/instance-category"
          operator: In
          values: ["c", "m", "r"]
        - key: "kubernetes.io/arch"
          operator: In
          values: ["arm64", "amd64"]
        - key: "karpenter.sh/capacity-type"
          operator: In
          values: ["spot", "on-demand"]
      nodeClassRef:
        name: default-ec2nodeclass
  limits:
    cpu: 1000
    memory: 1000Gi
  disruption:
    consolidationPolicy: WhenUnderutilized
    expireAfter: 720h # 30 days
```
