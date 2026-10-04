# ECS and Fargate: Running Containers

**ECS (Elastic Container Service)** is AWS's container orchestrator: you describe the containers you want running, and ECS starts them, restarts them when they die, spreads them across AZs, and wires them to load balancers. **Fargate** is the *serverless compute engine* underneath: you don't provision or patch the machines the containers run on.

Together they are the usual answer to "I have a Docker image and want it running reliably on AWS without managing servers". It sits between [Lambda](./02-lambda.md) (functions, short-lived) and [EC2](./01-ec2.md) (you run everything).

Prerequisites: Docker basics, [IAM](../01-foundations/03-iam.md) (two different roles matter here), and a rough idea of [VPC](../04-networking/01-vpc.md) subnets and security groups.

---

## The vocabulary

```
Cluster ──────────────────────────────────────────────  logical grouping
 └── Service  (keeps N copies of a task running, attaches load balancer)
      └── Task  (one running instance of a task definition)
           └── Container(s)  (defined in the task definition)

Task definition = blueprint: image, CPU/memory, ports, env, logging, roles
```

| Term | Meaning |
|---|---|
| **Task definition** | Versioned blueprint (`family:revision`). Declares containers, resources, roles, logging. |
| **Task** | A running instantiation of a task definition. A task can hold several containers that share a network and lifecycle (like a Kubernetes Pod). |
| **Service** | Long-running desired state: "keep 3 copies running, behind this load balancer, replace unhealthy ones". |
| **Standalone task** | A one-off run (batch job, migration), not managed by a service. |
| **Cluster** | Namespace/grouping for services and tasks. On Fargate it carries no machines. |
| **Capacity** | Where tasks run: **Fargate** (serverless) or **EC2** (your instances). |

---

## Fargate vs ECS-on-EC2

| | Fargate | EC2 capacity |
|---|---|---|
| Servers | None to manage | You manage the instances (AMIs, patching, scaling) |
| Billing | Per task vCPU + memory per second | Per instance, regardless of packing |
| Control | Limited (no host access, no GPUs on standard Fargate, fixed CPU/memory combinations) | Full (instance types, GPUs, daemons, host-level tuning) |
| Cheaper when | Variable/small/medium workloads, less ops | Large steady fleets packed efficiently, special hardware |

Default to **Fargate**; move to EC2 capacity when you need specific hardware or have a steady large fleet where packing efficiency pays off. Fargate also offers **Spot** capacity at a discount for interruptible work, and **ARM (Graviton)** for lower cost if your image is built for `arm64`.

ECS vs EKS: both orchestrate containers. **ECS** is simpler and AWS-native; **EKS** is managed Kubernetes, worth it if you need Kubernetes' ecosystem or portability, at the price of more complexity.

---

## The two roles (a classic confusion)

| Role | Used by | Needs |
|---|---|---|
| **Task execution role** | **ECS itself**, to *launch* the task | Pull image from ECR, write logs to CloudWatch, fetch secrets referenced in the task definition |
| **Task role** | **Your application code** inside the container | Whatever your app calls: S3, DynamoDB, SQS... |

Mixing them up is the number-one cause of "image won't pull" (execution role problem) versus "app gets AccessDenied calling S3" (task role problem). Credentials for the task role reach your SDK automatically, so no keys are needed in the container.

---

## A minimal task definition

```json
{
  "family": "web",
  "requiresCompatibilities": ["FARGATE"],
  "networkMode": "awsvpc",
  "cpu": "512",
  "memory": "1024",
  "runtimePlatform": { "cpuArchitecture": "ARM64", "operatingSystemFamily": "LINUX" },
  "executionRoleArn": "arn:aws:iam::111122223333:role/ecsTaskExecutionRole",
  "taskRoleArn":      "arn:aws:iam::111122223333:role/webTaskRole",
  "containerDefinitions": [
    {
      "name": "app",
      "image": "111122223333.dkr.ecr.ap-south-1.amazonaws.com/web:1.4.2",
      "essential": true,
      "portMappings": [{ "containerPort": 8080 }],
      "environment": [{ "name": "NODE_ENV", "value": "production" }],
      "secrets": [
        { "name": "DB_PASSWORD",
          "valueFrom": "arn:aws:secretsmanager:ap-south-1:111122223333:secret:prod/db-AbCdEf" }
      ],
      "logConfiguration": {
        "logDriver": "awslogs",
        "options": {
          "awslogs-group": "/ecs/web",
          "awslogs-region": "ap-south-1",
          "awslogs-stream-prefix": "app"
        }
      }
    }
  ]
}
```

Points worth knowing:

- On Fargate, **`cpu` and `memory` are set at the task level** and must be one of the **allowed combinations** (e.g. 512 CPU units / 1 GB and up). Invalid pairs are rejected at registration.
- **`networkMode: awsvpc`** is required on Fargate: each task gets its **own network interface (ENI)** in your subnet, with its own private IP and security group.
- **`essential: true`**: if that container stops, the whole task stops.
- **`secrets`** are injected at launch from Secrets Manager or Parameter Store using the **execution role**, so you don't bake secrets into images or plain environment variables ([Security and Secrets](../06-operations/03-security-and-secrets.md)).
- **Pin image tags** (a version or digest) instead of `latest`, so deployments are reproducible and rollbacks are real.
- Images normally live in **ECR** (AWS's container registry). Build for the right architecture. An `amd64` image on an `ARM64` task fails with an exec-format error.
- The `awslogs` log group must exist (or be created by the task definition setup) before the task starts.

---

## Running it

A **service** with a load balancer is the typical web deployment:

```bash
aws ecs register-task-definition --cli-input-json file://taskdef.json

aws ecs create-service \
  --cluster prod \
  --service-name web \
  --task-definition web \
  --desired-count 2 \
  --launch-type FARGATE \
  --network-configuration 'awsvpcConfiguration={subnets=[subnet-aaa,subnet-bbb],securityGroups=[sg-0abc1234],assignPublicIp=DISABLED}' \
  --load-balancers 'targetGroupArn=arn:aws:elasticloadbalancing:...,containerName=app,containerPort=8080'
```

For a one-off job (a migration, a batch run):

```bash
aws ecs run-task --cluster prod --task-definition migrate \
  --launch-type FARGATE \
  --network-configuration 'awsvpcConfiguration={subnets=[subnet-aaa],securityGroups=[sg-0abc1234],assignPublicIp=DISABLED}'
```

Architecture of the typical setup:

```
Internet ─► ALB (public subnets) ─► Fargate tasks (private subnets, 2+ AZs)
                                        │
                                        ├─► RDS / DynamoDB / S3 (via task role)
                                        └─► ECR / CloudWatch (via NAT or VPC endpoints)
```

See [Load Balancing and API Gateway](../04-networking/03-load-balancing-and-api-gateway.md) for the ALB side.

### Networking decisions

- **Private subnets + NAT** (or VPC endpoints for ECR, S3, CloudWatch Logs, Secrets Manager) is the production pattern: tasks aren't directly reachable from the internet.
- **Public subnet with `assignPublicIp=ENABLED`** is the simple/cheap route for experiments. Tasks still need *some* route to ECR to pull their image.
- The task's **security group** should allow inbound only from the load balancer's security group.

---

## Deployments and scaling

- **Rolling update** (the default): ECS starts tasks from the new revision and drains the old ones, controlled by `minimumHealthyPercent` and `maximumPercent`. It keeps capacity while swapping.
- **Deployment circuit breaker**: automatically fails and can roll back a deployment whose new tasks can't reach a healthy state. Turn it on, so a bad image doesn't leave you in a long failing loop.
- ECS also supports **blue/green** style deployments for safer traffic shifting. Check the current ECS docs for the supported options.
- **Health checks** matter at two levels: the **ALB target group** check (is the app responding?) and an optional container-level health check. A too-strict or wrong-path ALB health check makes ECS kill healthy tasks repeatedly.
- **Autoscaling** uses Application Auto Scaling on the service: target tracking on CPU/memory or ALB request count per target is the usual start.
- Give your app a **graceful shutdown** (handle `SIGTERM`; ECS sends it, then `SIGKILL` after the stop timeout) so in-flight requests finish during deployments and scale-in.

### ECS Express Mode

For simple web apps and APIs, **ECS Express Mode** (announced November 2025) reduces setup: you provide a container image plus a task execution role and an infrastructure role, and it provisions a Fargate service with a load balancer, HTTPS endpoint, autoscaling and monitoring. It is Fargate-only and trades away fine-grained control (for example, it doesn't support the EC2 launch type), so move to a standard ECS service when you need that control. There's no extra charge for Express Mode itself; you pay for the underlying resources.

---

## Cost notes

Fargate bills for **requested vCPU and memory per second** while a task runs (with a minimum billing period), so right-size the task: oversized tasks waste money, undersized ones get OOM-killed. Beyond compute, the usual extras are the **load balancer**, **NAT Gateway** (hourly + per GB), **CloudWatch Logs**, and cross-AZ traffic. Fargate Spot and ARM both cut compute cost when your workload allows.

---

## Common mistakes

- Confusing the **task role** and **task execution role**.
- Tasks in a subnet with **no route to ECR** → `CannotPullContainerError`.
- Wrong CPU/memory combination at registration.
- Using `latest` tags, then not knowing what's actually running.
- ALB health check on the wrong path or port, causing a restart loop.
- Container listens on `127.0.0.1` instead of `0.0.0.0`, so the load balancer can't reach it.
- Putting secrets in plain `environment` values instead of `secrets`.
- Architecture mismatch between the image and the task (`amd64` vs `ARM64`).
- Treating the container filesystem as persistent. Fargate task storage is ephemeral; persist to S3, a database, or EFS.
- Not setting `desired-count` ≥ 2 across AZs, so a single task failure is an outage.

---

## Debugging

Start with *why did the task stop*:

```bash
aws ecs describe-services --cluster prod --services web \
  --query 'services[0].events[:10].[createdAt,message]' --output table

aws ecs list-tasks --cluster prod --service-name web --desired-status STOPPED
aws ecs describe-tasks --cluster prod --tasks <task-arn> \
  --query 'tasks[0].[stoppedReason,containers[].[name,exitCode,reason]]'
```

| Symptom | Likely cause |
|---|---|
| `CannotPullContainerError` | No route to ECR (need NAT/VPC endpoints/public IP), wrong image URI/tag, or execution role missing ECR permissions |
| Task stops immediately, exit code non-zero | App crash: read the CloudWatch logs |
| `ResourceInitializationError` | Often can't fetch secrets or reach required endpoints during startup (network or execution-role issue) |
| Task `OutOfMemoryError` / exit 137 | Exceeded the task/container memory limit: raise memory |
| Service flapping (start, stop, start) | Failing ALB health check, wrong port, or the app slow to start (add a health check grace period) |
| App `AccessDenied` calling AWS APIs | **Task role** missing permission |
| No logs in CloudWatch | Log group missing, or execution role can't write logs |
| `exec format error` | Image architecture doesn't match the task's CPU architecture |

To get a shell in a running container, use **ECS Exec** (needs it enabled on the service/task, the SSM permissions on the task role, and the Session Manager plugin):

```bash
aws ecs execute-command --cluster prod --task <task-id> --container app \
  --interactive --command "/bin/sh"
```

---

## Choosing between Lambda, Fargate and EC2

- **Lambda:** event-driven, spiky, short (≤15 min), want zero idle cost and minimal ops.
- **Fargate:** an existing container, steady web service, long-running or background workers, jobs over 15 min, no server management.
- **EC2:** you need host-level control, special hardware/GPU, or a huge steady fleet where you'll optimise packing.

---

## Quick Summary

- **ECS** orchestrates containers; **Fargate** runs them without servers. Default to Fargate, drop to EC2 capacity for special hardware or large steady fleets.
- Hierarchy: **cluster → service → task → container**, defined by a versioned **task definition**.
- Two roles: **execution role** (ECS: pull image, logs, secrets) vs **task role** (your app's AWS permissions).
- Fargate uses **awsvpc**: each task has its own ENI and security group. Tasks need a network path to ECR.
- Use services behind an ALB across 2+ AZs, health checks, circuit-breaker deployments, graceful `SIGTERM` handling, and autoscaling.
- Debug with service events and `describe-tasks` (`stoppedReason`), then CloudWatch logs.
- ECS Express Mode is a shortcut for simple Fargate web services.

**Next:** [S3](../03-storage-and-databases/01-s3.md)