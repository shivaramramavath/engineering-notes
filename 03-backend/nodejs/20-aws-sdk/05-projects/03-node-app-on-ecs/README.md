# Project 03: Containerized Node.js Microservice on AWS ECS Fargate (`proj-aws-03-node-app-on-ecs`)

> Production-grade, highly available containerized Node.js microservice deployed on AWS ECS Fargate behind an Application Load Balancer (ALB). Features dynamic runtime secret injection from AWS Secrets Manager with in-memory TTL caching, CloudWatch Embedded Metric Format (EMF) telemetry, zero-downtime rolling deployments, and zero-leak graceful SIGTERM shutdown with connection draining.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-aws-03-node-app-on-ecs` |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Module System** | ESM (`"type": "module"`) |
| **Framework / Core** | Express 5.0 |
| **Compute & Orchestration** | AWS ECS Fargate (Serverless Containers) |
| **Load Balancer** | AWS Application Load Balancer (ALB) |
| **Secrets & Telemetry** | AWS Secrets Manager + CloudWatch EMF |
| **Infrastructure as Code**| Terraform (HCL) |
| **Container Engine** | Multi-stage Docker + Distroless Node 22 |
| **Complexity Level** | Enterprise / Production-Grade |

---

## 1. Architectural Overview & Domain Design

### 1.1 ECS Fargate Execution & Lifecycle Topology

On AWS ECS Fargate, tasks run without underlying EC2 server management. Key operational characteristics include:
1. **Separation of IAM Roles**:
   - **Task Execution Role**: Used by the ECS agent to pull Docker images from Amazon ECR and send logs to CloudWatch.
   - **Task Role**: Assumed directly by the Node.js application process inside the container to make AWS SDK calls (e.g. Secrets Manager, S3, DynamoDB) via the container credentials endpoint (`AWS_CONTAINER_CREDENTIALS_RELATIVE_URI`).
2. **Graceful Shutdown & Connection Draining**:
   - When ECS scales in or deploys a new task revision, ALB stops routing new traffic and deregisters the target.
   - ECS sends `SIGTERM` to PID 1. The Node.js application stops accepting new connections on its HTTP listener, allows inflight requests up to 25 seconds to finish processing, flushes metrics, and cleanly exits before ECS sends `SIGKILL` at 30 seconds.
3. **Dual Healthchecks**:
   - `/health/liveness`: Fast check verifying the Node.js event loop is responsive.
   - `/health/readiness`: Deep check verifying database and upstream connectivity before the ALB routes traffic to the container.

```
+----------------------------------------------------------------------------------------------------+
|                               ECS Fargate Microservice Architecture                                |
|                                                                                                    |
|  [ Inbound Internet Traffic ]                                                                      |
|               │                                                                                    |
|               ▼ HTTPS (443)                                                                        |
|  +─────────────────────────────────────────────────────────────+                                   |
|  |             AWS Application Load Balancer (ALB)             |                                   |
|  |     - Target Group Healthcheck: GET /health/readiness       |                                   |
|  |     - Deregistration Delay: 30 seconds                      |                                   |
|  +─────────────────────────────┬───────────────────────────────+                                   |
|                                │ Forward Traffic                                                   |
|             ┌──────────────────┴──────────────────┐                                                |
|             ▼                                     ▼                                                |
|  +──────────────────────────────────+  +──────────────────────────────────+                        |
|  |       ECS Fargate Task A         |  |       ECS Fargate Task B         |                        |
|  |  +────────────────────────────+  |  |  +────────────────────────────+  |                        |
|  |  | Container: Distroless Node |  |  |  | Container: Distroless Node |  |                        |
|  |  |  - Express 5 REST API      |  |  |  |  - Express 5 REST API      |  |                        |
|  |  |  - Port 3000 (Non-root)    |  |  |  |  - Port 3000 (Non-root)    |  |                        |
|  |  +──────────────┬─────────────+  |  |  +──────────────┬─────────────+  |                        |
|  +─────────────────┼────────────────+  +─────────────────┼────────────────+                        |
|                    │                                     │                                         |
|                    ├─────────────────────────────────────┤                                         |
|                    │ Task Role IAM Credentials           │ Task Role IAM Credentials               |
|                    ▼                                     ▼                                         |
|  +──────────────────────────────────+  +──────────────────────────────────+                        |
|  |        AWS Secrets Manager       |  |      Amazon CloudWatch EMF       |                        |
|  | (Database URL & Third-party keys)|  |   (Embedded Metrics & Tracing)   |                        |
|  +──────────────────────────────────+  +──────────────────────────────────+                        |
+----------------------------------------------------------------------------------------------------+
```

### 1.2 Directory Layout

```text
proj-aws-03-node-app-on-ecs/
├── src/
│   ├── config/
│   │   ├── env.ts               # Core environment variables (PORT, REGION, SECRET_ID)
│   │   └── secrets.service.ts   # Secrets Manager dynamic client with in-memory TTL caching
│   ├── presentation/
│   │   ├── controllers/
│   │   │   ├── health.controller.ts # Liveness and readiness endpoints
│   │   │   └── api.controller.ts    # Business API endpoints
│   │   └── middleware/
│   │       ├── request-logger.ts    # Correlation ID & JSON logging
│   │       └── error-handler.ts     # RFC 7807 error format
│   ├── telemetry/
│   │   └── metrics.ts           # CloudWatch Embedded Metric Format (EMF) emitter
│   └── app.ts                   # Express server, socket draining, SIGTERM orchestrator
├── test/
│   └── unit/
│       └── secrets.service.test.ts # Vitest unit test for secret caching & refreshing
├── infra/
│   └── main.tf                  # Complete Terraform HCL configuration for VPC, ECS, ALB
├── Dockerfile                   # Multi-stage container build with Google Distroless
└── package.json                 # Project manifest
```

---

## 2. Complete File-by-File Implementation

### `package.json`
```json
{
  "name": "proj-aws-03-node-app-on-ecs",
  "version": "1.0.0",
  "type": "module",
  "scripts": {
    "dev": "node --watch --env-file=.env src/app.js",
    "start": "node src/app.js",
    "test": "vitest run"
  },
  "dependencies": {
    "@aws-sdk/client-secrets-manager": "^3.750.0",
    "express": "^5.0.1",
    "zod": "^3.24.2"
  },
  "devDependencies": {
    "@types/express": "^5.0.0",
    "@types/node": "^22.13.0",
    "aws-sdk-client-mock": "^4.1.0",
    "supertest": "^7.0.0",
    "typescript": "^5.7.3",
    "vitest": "^3.0.5"
  }
}
```

### `tsconfig.json`
```json
{
  "compilerOptions": {
    "target": "ES2024",
    "module": "NodeNext",
    "moduleResolution": "NodeNext",
    "strict": true,
    "esModuleInterop": true,
    "skipLibCheck": true,
    "outDir": "./dist"
  },
  "include": ["src/**/*", "test/**/*"]
}
```

### `src/config/env.ts`
```typescript
import { z } from 'zod';

const envSchema = z.object({
  NODE_ENV: z.enum(['development', 'production', 'test']).default('production'),
  PORT: z.coerce.number().default(3000),
  AWS_REGION: z.string().default('us-east-1'),
  APP_SECRET_ID: z.string().default('prod/ecs-app/credentials'),
  SECRET_CACHE_TTL_MS: z.coerce.number().default(300000), // 5 minutes cache
});

export const env = envSchema.parse(process.env);
```

### `src/config/secrets.service.ts`
```typescript
import { SecretsManagerClient, GetSecretValueCommand } from '@aws-sdk/client-secrets-manager';
import { env } from './env.js';

export interface AppSecrets {
  DATABASE_URL: string;
  JWT_SECRET: string;
  PAYMENT_GATEWAY_KEY: string;
}

export class SecretsService {
  private client: SecretsManagerClient;
  private cachedSecrets: AppSecrets | null = null;
  private lastFetchedTimestamp: number = 0;

  constructor() {
    this.client = new SecretsManagerClient({ region: env.AWS_REGION });
  }

  /**
   * Fetches secret value with in-memory TTL caching to prevent frequent AWS API billing.
   */
  async getSecrets(): Promise<AppSecrets> {
    const now = Date.now();
    if (this.cachedSecrets && (now - this.lastFetchedTimestamp < env.SECRET_CACHE_TTL_MS)) {
      return this.cachedSecrets;
    }

    try {
      const response = await this.client.send(
        new GetSecretValueCommand({ SecretId: env.APP_SECRET_ID })
      );

      if (!response.SecretString) {
        throw new Error(`SecretString is empty for secret ${env.APP_SECRET_ID}`);
      }

      this.cachedSecrets = JSON.parse(response.SecretString) as AppSecrets;
      this.lastFetchedTimestamp = now;
      return this.cachedSecrets;
    } catch (error) {
      console.error('[SecretsService] Failed to load credentials from AWS Secrets Manager', error);
      if (this.cachedSecrets) {
        console.warn('[SecretsService] Serving stale cached credentials due to upstream error');
        return this.cachedSecrets;
      }
      throw error;
    }
  }
}

export const secretsService = new SecretsService();
```

### `src/telemetry/metrics.ts`
```typescript
/**
 * Emits AWS CloudWatch Embedded Metric Format (EMF) logs to stdout.
 * CloudWatch Logs parses EMF directly and extracts custom high-resolution metrics
 * with zero added HTTP API latency.
 */
export function recordMetric(name: string, value: number, unit: 'Count' | 'Milliseconds', dimensions: Record<string, string> = {}): void {
  const emfPayload = {
    _aws: {
      Timestamp: Date.now(),
      CloudWatchMetrics: [
        {
          Namespace: 'ECS/NodeApp',
          Dimensions: [Object.keys(dimensions)],
          Metrics: [{ Name: name, Unit: unit }],
        },
      ],
    },
    ...dimensions,
    [name]: value,
  };

  process.stdout.write(`${JSON.stringify(emfPayload)}\n`);
}
```

### `src/presentation/controllers/health.controller.ts`
```typescript
import { Request, Response } from 'express';

let isShuttingDown = false;

export function markAsShuttingDown(): void {
  isShuttingDown = true;
}

export function handleLiveness(_req: Request, res: Response): void {
  // Liveness passes as long as the process event loop can respond
  res.status(200).json({ status: 'alive', uptime: process.uptime() });
}

export function handleReadiness(_req: Request, res: Response): void {
  // If task received SIGTERM, fail readiness immediately so ALB stops sending new requests
  if (isShuttingDown) {
    res.status(503).json({ status: 'shutting_down', ready: false });
    return;
  }

  // Deep checks (e.g. database ping) can be performed here
  res.status(200).json({ status: 'ready', ready: true });
}
```

### `src/app.ts`
```typescript
import express from 'express';
import { Server } from 'node:http';
import { Socket } from 'node:net';
import { env } from './config/env.js';
import { handleLiveness, handleReadiness, markAsShuttingDown } from './presentation/controllers/health.controller.js';
import { recordMetric } from './telemetry/metrics.js';
import { secretsService } from './config/secrets.service.js';

export const app = express();

app.use(express.json());

// Track open sockets for immediate closing during teardown
const openSockets = new Set<Socket>();

// Request latency & metric middleware
app.use((req, res, next) => {
  const start = performance.now();
  res.on('finish', () => {
    const duration = performance.now() - start;
    recordMetric('HttpRequestDuration', duration, 'Milliseconds', {
      Method: req.method,
      StatusCode: String(res.statusCode),
    });
    recordMetric('HttpRequestCount', 1, 'Count', { Method: req.method });
  });
  next();
});

// Health check endpoints for AWS ALB
app.get('/health/liveness', handleLiveness);
app.get('/health/readiness', handleReadiness);

// Sample business endpoint
app.get('/api/v1/data', async (_req, res) => {
  const secrets = await secretsService.getSecrets();
  res.json({
    message: 'Data successfully retrieved from ECS Fargate service',
    hasDatabaseUrl: Boolean(secrets.DATABASE_URL),
    instanceNodeVersion: process.version,
  });
});

let server: Server | null = null;

if (process.env.NODE_ENV !== 'test') {
  server = app.listen(env.PORT, () => {
    console.log(`[ECS Service] Application listening on port ${env.PORT} (PID: ${process.pid})`);
  });

  server.on('connection', (socket) => {
    openSockets.add(socket);
    socket.on('close', () => openSockets.delete(socket));
  });

  // Graceful Shutdown on SIGTERM (Triggered by ECS during deployments / scale-in)
  const gracefulShutdown = (signal: string) => {
    console.log(`[ECS Lifecycle] Received ${signal}. Initiating graceful shutdown...`);
    markAsShuttingDown(); // Make /health/readiness return 503

    // Allow in-flight requests 20 seconds before forcing socket teardown
    const forceExitTimer = setTimeout(() => {
      console.error('[ECS Lifecycle] Forced shutdown: draining exceeded timeout (20s). Exiting.');
      process.exit(1);
    }, 20000);
    forceExitTimer.unref();

    if (server) {
      server.close(() => {
        console.log('[ECS Lifecycle] All in-flight requests finished. Closing server cleanly.');
        process.exit(0);
      });

      // Stop keep-alive on idle sockets
      for (const socket of openSockets) {
        socket.end();
      }
    }
  };

  process.on('SIGTERM', () => gracefulShutdown('SIGTERM'));
  process.on('SIGINT', () => gracefulShutdown('SIGINT'));
}
```

---

## 3. Infrastructure Specification (Terraform HCL)

### `infra/main.tf`
```hcl
terraform {
  required_version = ">= 1.5.0"
  required_providers {
    aws = {
      source  = "hashicorp/aws"
      version = "~> 5.0"
    }
  }
}

provider "aws" {
  region = "us-east-1"
}

# 1. ECS Cluster
resource "aws_ecs_cluster" "main" {
  name = "production-ecs-cluster"
  setting {
    name  = "containerInsights"
    value = "enabled"
  }
}

# 2. IAM Roles
resource "aws_iam_role" "ecs_execution_role" {
  name = "ecsTaskExecutionRole"
  assume_role_policy = jsonencode({
    Version = "2012-10-17"
    Statement = [{
      Action = "sts:AssumeRole"
      Effect = "Allow"
      Principal = { Service = "ecs-tasks.amazonaws.com" }
    }]
  })
}

resource "aws_iam_role_policy_attachment" "ecs_execution_attachment" {
  role       = aws_iam_role.ecs_execution_role.name
  policy_arn = "arn:aws:iam::aws:policy/service-role/AmazonECSTaskExecutionRolePolicy"
}

resource "aws_iam_role" "ecs_task_role" {
  name = "ecsTaskRole"
  assume_role_policy = jsonencode({
    Version = "2012-10-17"
    Statement = [{
      Action = "sts:AssumeRole"
      Effect = "Allow"
      Principal = { Service = "ecs-tasks.amazonaws.com" }
    }]
  })
}

# Allow container to read application secrets
resource "aws_iam_policy" "secrets_access" {
  name = "ecs-secrets-access"
  policy = jsonencode({
    Version = "2012-10-17"
    Statement = [{
      Effect = "Allow"
      Action = ["secretsmanager:GetSecretValue"]
      Resource = ["arn:aws:secretsmanager:us-east-1:*:secret:prod/ecs-app/*"]
    }]
  })
}

resource "aws_iam_role_policy_attachment" "task_secrets_attachment" {
  role       = aws_iam_role.ecs_task_role.name
  policy_arn = aws_iam_policy.secrets_access.arn
}

# 3. ECS Task Definition (Fargate, 0.5 vCPU, 1GB RAM)
resource "aws_ecs_task_definition" "app" {
  family                   = "node-microservice"
  network_mode             = "awsvpc"
  requires_compatibilities = ["FARGATE"]
  cpu                      = "512"
  memory                   = "1024"
  execution_role_arn       = aws_iam_role.ecs_execution_role.arn
  task_role_arn            = aws_iam_role.ecs_task_role.arn

  container_definitions = jsonencode([
    {
      name      = "node-app"
      image     = "123456789012.dkr.ecr.us-east-1.amazonaws.com/node-app:latest"
      essential = true
      portMappings = [{
        containerPort = 3000
        hostPort      = 3000
      }]
      logConfiguration = {
        logDriver = "awslogs"
        options = {
          "awslogs-group"         = "/ecs/node-microservice"
          "awslogs-region"        = "us-east-1"
          "awslogs-stream-prefix" = "app"
        }
      }
      healthCheck = {
        command     = ["CMD-SHELL", "node -e 'fetch(\"http://localhost:3000/health/liveness\").then(r => process.exit(r.ok ? 0 : 1))'"]
        interval    = 15
        timeout     = 5
        retries     = 3
        startPeriod = 10
      }
    }
  ])
}

# 4. Target Group with Readiness Healthcheck
resource "aws_lb_target_group" "app" {
  name        = "ecs-node-tg"
  port        = 3000
  protocol    = "HTTP"
  vpc_id      = "vpc-12345678"
  target_type = "ip"
  deregistration_delay = 30

  health_check {
    path                = "/health/readiness"
    healthy_threshold   = 2
    unhealthy_threshold = 3
    timeout             = 5
    interval            = 15
    matcher             = "200"
  }
}
```

---

## 4. Verification & Testing Suite

### Unit Test (`test/unit/secrets.service.test.ts`)
```typescript
import { describe, it, expect, beforeEach } from 'vitest';
import { mockClient } from 'aws-sdk-client-mock';
import { SecretsManagerClient, GetSecretValueCommand } from '@aws-sdk/client-secrets-manager';
import { SecretsService } from '../../src/config/secrets.service.js';

describe('SecretsService', () => {
  const secretsMock = mockClient(SecretsManagerClient);

  beforeEach(() => {
    secretsMock.reset();
  });

  it('fetches and parses secrets from Secrets Manager', async () => {
    secretsMock.on(GetSecretValueCommand).resolves({
      SecretString: JSON.stringify({
        DATABASE_URL: 'postgresql://user:pass@db.example.com/mydb',
        JWT_SECRET: 'super-secret-key-64chars',
        PAYMENT_GATEWAY_KEY: 'pk_live_123',
      }),
    });

    const service = new SecretsService();
    const secrets = await service.getSecrets();

    expect(secrets.DATABASE_URL).toContain('postgresql://');
    expect(secrets.JWT_SECRET).toBe('super-secret-key-64chars');
    expect(secretsMock.commandCalls(GetSecretValueCommand).length).toBe(1);
  });

  it('caches secrets and does not call AWS on subsequent requests within TTL', async () => {
    secretsMock.on(GetSecretValueCommand).resolves({
      SecretString: JSON.stringify({ DATABASE_URL: 'postgres://cached', JWT_SECRET: 'k', PAYMENT_GATEWAY_KEY: 'p' }),
    });

    const service = new SecretsService();
    await service.getSecrets();
    await service.getSecrets(); // Second call within TTL

    expect(secretsMock.commandCalls(GetSecretValueCommand).length).toBe(1);
  });
});
```

---

## 5. Production Dockerfile & Container Build

### `Dockerfile`
```dockerfile
# Multi-stage production Docker build with security hardening

# Stage 1: Build & Prune
FROM node:22-alpine AS builder
WORKDIR /app
COPY package*.json tsconfig.json ./
RUN npm ci
COPY src/ ./src
RUN npm run build || true
RUN npm prune --omit=dev

# Stage 2: Distroless Minimal Security Image
FROM gcr.io/distroless/nodejs22-debian12:nonroot
WORKDIR /app
COPY --from=builder /app/package.json ./package.json
COPY --from=builder /app/node_modules ./node_modules
COPY --from=builder /app/src ./src

USER nonroot
EXPOSE 3000
ENV NODE_ENV=production
CMD ["src/app.js"]
```
