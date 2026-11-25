<!--
================================================================================
Metadata:
  Title: Reference Project 01 - Production Node.js API with PostgreSQL and Redis
  Track: Docker / DevOps
  Difficulty: Intermediate
  Estimated Time: 60 minutes
  Prerequisites: 03-dockerfile/04-multi-stage-builds.md, 04-volumes-and-networking/01-volumes-and-bind-mounts.md, 05-compose/02-compose-multi-service.md
  Key Terminology: Multi-Stage Build, depends_on healthy, Healthcheck, Isolated Networks, Non-Root User, Redis Caching, Connection Pooling
================================================================================
-->

# Reference Project 01: Production Node.js API with PostgreSQL and Redis

[<- Back to Projects Overview](./README.md) | [Next Project: 02-python-fastapi-stack.md ->](./02-python-fastapi-stack.md)

---

## 1. Overview & Architecture

This reference project provides a production-grade, multi-service backend architecture implementing a high-throughput Node.js (TypeScript) REST API backed by a persistent PostgreSQL 16 database and an in-memory Redis 7 cache.

### Architectural Diagram

```
+-----------------------------------------------------------------------------------+
|                                  HOST MACHINE                                     |
|                                                                                   |
|    HTTP Client / Curl                                                             |
|           |                                                                       |
|           | Port 3000 (Published)                                                |
|           v                                                                       |
|  +-----------------------------------------------------------------------------+  |
|  |                          DOCKER NETWORK: app-frontend                       |  |
|  |                                                                             |  |
|  |   +---------------------------------------------------------------------+   |  |
|  |   |                       SERVICE: node-api                             |   |  |
|  |   |  Image: company/node-api:1.0.0 (Multi-stage, distroless-like alpine)|   |  |
|  |   |  User: node (UID 1000) | Port: 3000                                 |   |  |
|  |   |  Healthcheck: GET /healthz                                          |   |  |
|  |   +-----------------+---------------------------------+-----------------+   |  |
|  +---------------------|---------------------------------|---------------------+  |
|                        |                                 |                        |
|  +---------------------|---------------------------------|---------------------+  |
|  |                     v                                 v                     |  |
|  |        DNS: postgres-db:5432                    DNS: redis-cache:6379       |  |
|  |                                                                             |  |
|  |   +--------------------------+               +--------------------------+   |  |
|  |   |   SERVICE: postgres-db   |               |   SERVICE: redis-cache   |   |  |
|  |   |   Image: postgres:16     |               |   Image: redis:7-alpine  |   |  |
|  |   |   Health: pg_isready     |               |   Health: redis-cli ping |   |  |
|  |   |   No public port mapped  |               |   No public port mapped  |   |  |
|  |   +------------+-------------+               +--------------------------+   |  |
|  |                |                                                            |  |
|  |                v                                                            |  |
|  |   [VOLUME: postgres_data]                                                   |  |
|  |   Mounted at /var/lib/postgresql/data                                       |  |
|  |                                                                             |  |
|  |                     DOCKER NETWORK: app-backend (internal)                  |  |
|  +-----------------------------------------------------------------------------+  |
+-----------------------------------------------------------------------------------+
```

### Key Architectural Decisions
1. **Network Segmentation**: Two distinct networks are defined: `app-frontend` (bridge with published host port) and `app-backend` (`internal: true`). The database and cache reside strictly on `app-backend`, preventing direct external port access and securing internal traffic.
2. **Deterministic Startup Sequencing**: Uses Compose v2 `depends_on` with `condition: service_healthy` to guarantee that PostgreSQL and Redis are accepting socket connections before Node.js attempts database migrations.
3. **Multi-Stage Hardened Dockerfile**: Build dependencies (TypeScript compiler, headers) are discarded; runtime image contains only production `node_modules` running under unprivileged user `node` (UID 1000).

---

## 2. Directory Structure & File Listing

```
node-pg-redis-stack/
├── .dockerignore
├── .env.example
├── compose.yaml
├── Dockerfile
├── init-scripts/
│   └── 01-schema.sql
├── package.json
├── tsconfig.json
└── src/
    └── index.ts
```

---

## 3. Complete Implementation

### File: `package.json`

```json
{
  "name": "node-pg-redis-api",
  "version": "1.0.0",
  "description": "Production Node.js API with PostgreSQL and Redis",
  "main": "dist/index.js",
  "scripts": {
    "build": "tsc",
    "start": "node dist/index.js",
    "dev": "ts-node-dev --respawn --transpile-only src/index.ts"
  },
  "dependencies": {
    "express": "^4.19.2",
    "ioredis": "^5.4.1",
    "pg": "^8.11.5"
  },
  "devDependencies": {
    "@types/express": "^4.17.21",
    "@types/node": "^20.12.7",
    "@types/pg": "^8.11.5",
    "ts-node-dev": "^2.0.0",
    "typescript": "^5.4.5"
  }
}
```

### File: `tsconfig.json`

```json
{
  "compilerOptions": {
    "target": "ES2022",
    "module": "commonjs",
    "lib": ["ES2022"],
    "outDir": "./dist",
    "rootDir": "./src",
    "strict": true,
    "esModuleInterop": true,
    "skipLibCheck": true,
    "forceConsistentCasingInFileNames": true
  },
  "include": ["src/**/*"]
}
```

### File: `src/index.ts`

```typescript
import express, { Request, Response } from 'express';
import { Pool } from 'pg';
import Redis from 'ioredis';

const app = express();
const port = parseInt(process.env.PORT || '3000', 10);

app.use(express.json());

// 1. Initialize PostgreSQL Connection Pool
const pool = new Pool({
  host: process.env.DB_HOST || 'postgres-db',
  port: parseInt(process.env.DB_PORT || '5432', 10),
  database: process.env.DB_NAME || 'apidb',
  user: process.env.DB_USER || 'apiuser',
  password: process.env.DB_PASSWORD || 'apisecret',
  max: 20,
  idleTimeoutMillis: 30000,
  connectionTimeoutMillis: 2000,
});

// 2. Initialize Redis Client
const redis = new Redis({
  host: process.env.REDIS_HOST || 'redis-cache',
  port: parseInt(process.env.REDIS_PORT || '6379', 10),
  retryStrategy: (times) => Math.min(times * 100, 3000),
});

// 3. Healthcheck Endpoint (Deep Health Check)
app.get('/healthz', async (_req: Request, res: Response) => {
  try {
    const dbCheck = await pool.query('SELECT 1 AS healthy');
    const redisCheck = await redis.ping();

    if (dbCheck.rows[0].healthy === 1 && redisCheck === 'PONG') {
      res.status(200).json({ status: 'UP', postgres: 'OK', redis: 'OK' });
    } else {
      res.status(503).json({ status: 'DEGRADED' });
    }
  } catch (err: any) {
    res.status(503).json({ status: 'DOWN', error: err.message });
  }
});

// 4. REST Endpoints demonstrating DB query with Redis Cache-Aside
app.get('/api/users/:id', async (req: Request, res: Response): Promise<void> => {
  const userId = req.params.id;
  const cacheKey = `user:${userId}`;

  try {
    // Check Cache
    const cached = await redis.get(cacheKey);
    if (cached) {
      res.status(200).json({ source: 'cache', data: JSON.parse(cached) });
      return;
    }

    // Query Database
    const result = await pool.query('SELECT id, name, email FROM users WHERE id = $1', [userId]);
    if (result.rows.length === 0) {
      res.status(404).json({ error: 'User not found' });
      return;
    }

    const user = result.rows[0];
    // Cache for 60 seconds
    await redis.setex(cacheKey, 60, JSON.stringify(user));

    res.status(200).json({ source: 'database', data: user });
  } catch (err: any) {
    res.status(500).json({ error: err.message });
  }
});

// 5. User Creation Endpoint
app.post('/api/users', async (req: Request, res: Response): Promise<void> => {
  const { name, email } = req.body;
  if (!name || !email) {
    res.status(400).json({ error: 'name and email are required' });
    return;
  }

  try {
    const result = await pool.query(
      'INSERT INTO users (name, email) VALUES ($1, $2) RETURNING id, name, email',
      [name, email]
    );
    res.status(201).json(result.rows[0]);
  } catch (err: any) {
    res.status(500).json({ error: err.message });
  }
});

// 6. Graceful Shutdown Signal Handling
const server = app.listen(port, '0.0.0.0', () => {
  console.log(`Server listening on port ${port}`);
});

const shutdown = async () => {
  console.log('Received SIGTERM/SIGINT, shutting down gracefully...');
  server.close(async () => {
    console.log('HTTP server closed.');
    await pool.end();
    redis.disconnect();
    console.log('Pool and Redis connections closed. Exiting process.');
    process.exit(0);
  });
};

process.on('SIGTERM', shutdown);
process.on('SIGINT', shutdown);
```

### File: `Dockerfile`

```dockerfile
# syntax=docker/dockerfile:1.7

# -------------------------------------------------------------
# Stage 1: Build & Compile TypeScript
# -------------------------------------------------------------
FROM node:20.12.2-alpine3.19 AS builder

WORKDIR /app

# Copy dependency manifests
COPY package*.json tsconfig.json ./

# Install all dependencies including devDependencies with cache mount
RUN --mount=type=cache,target=/root/.npm \
    npm ci

# Copy application source code
COPY src/ ./src/

# Compile TypeScript to JavaScript in /app/dist
RUN npm run build

# Prune devDependencies to keep only production packages
RUN npm prune --production

# -------------------------------------------------------------
# Stage 2: Hardened Production Runtime
# -------------------------------------------------------------
FROM node:20.12.2-alpine3.19 AS runtime

WORKDIR /app
ENV NODE_ENV=production

# Install curl for reliable health check queries
RUN apk add --no-cache curl

# Copy compiled code and pruned node_modules from builder stage
COPY --from=builder --chown=node:node /app/dist ./dist
COPY --from=builder --chown=node:node /app/node_modules ./node_modules
COPY --from=builder --chown=node:node /app/package.json ./package.json

# Run as built-in non-root user (node: UID 1000)
USER node:node

# Define container healthcheck
HEALTHCHECK --interval=15s --timeout=3s --start-period=5s --retries=3 \
  CMD curl -f http://127.0.0.1:3000/healthz || exit 1

EXPOSE 3000

# Exec form ENTRYPOINT guarantees POSIX SIGTERM signal forwarding
ENTRYPOINT ["node", "dist/index.js"]
```

### File: `.dockerignore`

```
node_modules
dist
.git
.gitignore
.env*
npm-debug.log*
Dockerfile
compose*.yaml
README.md
init-scripts
```

### File: `init-scripts/01-schema.sql`

```sql
CREATE TABLE IF NOT EXISTS users (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    email VARCHAR(255) UNIQUE NOT NULL,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO users (name, email) VALUES
    ('Alice Dev', 'alice@example.com'),
    ('Bob SRE', 'bob@example.com')
ON CONFLICT (email) DO NOTHING;
```

### File: `.env.example`

```bash
# Application Port
PORT=3000

# Database Configuration
POSTGRES_DB=apidb
POSTGRES_USER=apiuser
POSTGRES_PASSWORD=SuperSecretDatabasePass2026!
POSTGRES_PORT=5432

# Redis Configuration
REDIS_PORT=6379
```

### File: `compose.yaml`

```yaml
version: "3.8"

services:
  node-api:
    build:
      context: .
      dockerfile: Dockerfile
    image: company/node-api:1.0.0
    restart: unless-stopped
    ports:
      - "3000:3000"
    environment:
      - PORT=3000
      - NODE_ENV=production
      - DB_HOST=postgres-db
      - DB_PORT=5432
      - DB_NAME=${POSTGRES_DB:-apidb}
      - DB_USER=${POSTGRES_USER:-apiuser}
      - DB_PASSWORD=${POSTGRES_PASSWORD:-SuperSecretDatabasePass2026!}
      - REDIS_HOST=redis-cache
      - REDIS_PORT=6379
    depends_on:
      postgres-db:
        condition: service_healthy
      redis-cache:
        condition: service_healthy
    networks:
      - app-frontend
      - app-backend
    deploy:
      resources:
        limits:
          cpus: "1.0"
          memory: 512M
        reservations:
          cpus: "0.2"
          memory: 128M

  postgres-db:
    image: postgres:16-alpine
    restart: unless-stopped
    environment:
      POSTGRES_DB: ${POSTGRES_DB:-apidb}
      POSTGRES_USER: ${POSTGRES_USER:-apiuser}
      POSTGRES_PASSWORD: ${POSTGRES_PASSWORD:-SuperSecretDatabasePass2026!}
    volumes:
      - postgres_data:/var/lib/postgresql/data
      - ./init-scripts:/docker-entrypoint-initdb.d:ro
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U ${POSTGRES_USER:-apiuser} -d ${POSTGRES_DB:-apidb}"]
      interval: 5s
      timeout: 3s
      retries: 5
      start_period: 10s
    networks:
      - app-backend
    deploy:
      resources:
        limits:
          cpus: "1.0"
          memory: 512M

  redis-cache:
    image: redis:7.4-alpine
    restart: unless-stopped
    command: ["redis-server", "--appendonly", "yes", "--maxmemory", "128mb", "--maxmemory-policy", "allkeys-lru"]
    volumes:
      - redis_data:/data
    healthcheck:
      test: ["CMD", "redis-cli", "ping"]
      interval: 5s
      timeout: 3s
      retries: 5
    networks:
      - app-backend
    deploy:
      resources:
        limits:
          cpus: "0.5"
          memory: 256M

networks:
  app-frontend:
    driver: bridge
  app-backend:
    driver: bridge
    internal: true

volumes:
  postgres_data:
  redis_data:
```

---

## 4. Step-by-Step Verification & Testing

### Step 1: Initialize Environment and Build Stack

```bash
# 1. Create directory and copy files
mkdir -p node-pg-redis-stack && cd node-pg-redis-stack
cp .env.example .env

# 2. Build images and start stack in detached mode
docker compose up -d --build

# 3. Observe startup ordering and health status transitions
docker compose ps
```

*Expected Output*:
```
NAME                           IMAGE                    COMMAND                  SERVICE       CREATED         STATUS                   PORTS
node-pg-redis-stack-node-api-1      company/node-api:1.0.0   "node dist/index.js"     node-api      2 minutes ago   Up 2 minutes (healthy)   0.0.0.0:3000->3000/tcp
node-pg-redis-stack-postgres-db-1   postgres:16-alpine       "docker-entrypoint.s…"   postgres-db   2 minutes ago   Up 2 minutes (healthy)   5432/tcp
node-pg-redis-stack-redis-cache-1   redis:7.4-alpine         "docker-entrypoint.s…"   redis-cache   2 minutes ago   Up 2 minutes (healthy)   6379/tcp
```

### Step 2: Test Healthcheck Endpoint

```bash
curl -i http://localhost:3000/healthz
```

*Expected Output*:
```http
HTTP/1.1 200 OK
Content-Type: application/json; charset=utf-8
Content-Length: 48

{"status":"UP","postgres":"OK","redis":"OK"}
```

### Step 3: Test Cache-Aside Behavior (Database vs Redis Cache)

```bash
# Query User ID 1 (First query: hits database)
curl -s http://localhost:3000/api/users/1
# Response: {"source":"database","data":{"id":1,"name":"Alice Dev","email":"alice@example.com"}}

# Query User ID 1 again (Second query: hits Redis cache)
curl -s http://localhost:3000/api/users/1
# Response: {"source":"cache","data":{"id":1,"name":"Alice Dev","email":"alice@example.com"}}
```

### Step 4: Verify Network Isolation

Attempt to connect directly to the PostgreSQL database from the host:
```bash
nc -z -v 127.0.0.1 5432
# Output: Connection to 127.0.0.1 5432 port [tcp/postgresql] failed: Connection refused
```
*The database port is completely unreachable from the host because it is constrained to the `internal: true` backend network!*

### Step 5: Teardown

```bash
# Stop and remove containers and networks (persisting volumes)
docker compose down

# To completely purge volumes as well:
# docker compose down -v
```

---

## 5. Production Considerations

1. **Connection Pool Sizing**: Node.js `pg.Pool` defaults to 10 connections. In multi-replica deployments (e.g. 10 API pods), total database connections will reach $10 \times 10 = 100$. Configure `max: 20` and set PostgreSQL `max_connections = 200` to prevent connection exhaustion.
2. **Secrets Decoupling**: In cloud production (AWS ECS, Kubernetes), avoid `.env` files. Inject `POSTGRES_PASSWORD` from AWS Secrets Manager or HashiCorp Vault.
3. **Read-Only Root Filesystem**: For maximum security hardening, configure the Node.js container with `read_only: true` and mount an in-memory `tmpfs: /tmp` for scratch files.
4. **Log Rotation**: In production, ensure `/etc/docker/daemon.json` or Compose service logging opts enforce `max-size: 50m` and `max-file: 3`.

---

[<- Back to Projects Overview](./README.md) | [Next Project: 02-python-fastapi-stack.md ->](./02-python-fastapi-stack.md)
