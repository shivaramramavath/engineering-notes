# Self-Hosting, Standalone Docker & Deployment Adapters (`self-hosting-nextjs-and-deployment-adapters`)

> Deploying Next.js applications in production: Standalone output mode (`output: 'standalone'`), multi-stage Dockerfiles, deployment adapters (Vercel, Cloudflare, Netlify, AWS OpenNext), zero-downtime rolling deploys, and asset skew protection.

| Metadata | Details |
| :--- | :--- |
| **Next.js Version** | 15.2+ / 16.0 |
| **React Version** | React 19.0.0 |
| **Node Version** | Node.js 20+ LTS |
| **Router** | App Router (`app/`) |
| **Infrastructure** | Docker, Kubernetes, AWS OpenNext, Cloudflare |
| **Tested With** | Next.js 15.2, Vitest 3.0, TypeScript 5.7 |

---

## 1. Concept Overview & Mental Model

### 1.1 Beyond Vendor Lock-in
While Next.js was built by Vercel, it is an open-source framework that can be self-hosted anywhere:
- **`output: 'standalone'`**: Next.js uses Node File Trace (NFT) to analyze dependencies and compile a minimal standalone Node server into `.next/standalone/`, discarding gigabytes of unnecessary development dependencies.
- **Asset Skew Protection**: When rolling out a new version (e.g. `v2`), clients still browsing `v1` may request old chunk URLs (`/static/chunks/123-v1.js`). Without asset skew protection or CDN versioning, users experience 404 script errors.
- **Build Adapters**: Emerging OpenNext and cloud adapters compile Next.js into native AWS Lambda, Cloudflare Workers, or Netlify primitives.

```
[ Git Push to Production ] ──> [ CI / GitHub Actions ]
                                       │
                                       ▼ (next build with output: 'standalone')
                  [ Minimal Standalone Artifact: 120 MB ]
                                       │
                                       ├── Option A: Deploy to AWS Lambda / Cloudflare (via OpenNext)
                                       └── Option B: Docker Container ──> [ Kubernetes / Fly.io / ECS ]
```

---

## 2. Naive vs. Idiomatic Comparison

### Naive: Copying Entire `node_modules` into Docker
```dockerfile
# Dockerfile (Naive - Huge 1.8 GB Image!)
FROM node:20
WORKDIR /app
COPY . .
RUN npm install
RUN npm run build
EXPOSE 3000
CMD ["npm", "start"] # Huge attack surface, runs dev dependencies in production
```

### Idiomatic: Multi-Stage Standalone Dockerfile
```dockerfile
# Dockerfile (Idiomatic - Minimal 135 MB Production Image)
FROM node:20-alpine AS base

FROM base AS deps
WORKDIR /app
COPY package.json package-lock.json ./
RUN npm ci

FROM base AS builder
WORKDIR /app
COPY --from=deps /app/node_modules ./node_modules
COPY . .
ENV NEXT_TELEMETRY_DISABLED=1
RUN npm run build

FROM base AS runner
WORKDIR /app
ENV NODE_ENV=production
ENV PORT=3000
ENV HOSTNAME="0.0.0.0"

# Run as non-root user for container security
RUN addgroup --system --gid 1001 nodejs && adduser --system --uid 1001 nextjs

# Copy only the minimal standalone server bundle
COPY --from=builder /app/public ./public
COPY --from=builder --chown=nextjs:nodejs /app/.next/standalone ./
COPY --from=builder --chown=nextjs:nodejs /app/.next/static ./.next/static

USER nextjs
EXPOSE 3000
CMD ["node", "server.js"]
```

---

## 3. Five Progressive Examples

### Example 1: Enabling Standalone Output in `next.config.ts`
```ts
// next.config.ts
import type { NextConfig } from 'next';

const nextConfig: NextConfig = {
  output: 'standalone', // Generates .next/standalone/server.js
};

export default nextConfig;
```

### Example 2: Docker Compose for Multi-Container Stack
```yaml
# docker-compose.yml
version: '3.8'

services:
  app:
    build: .
    ports:
      - '3000:3000'
    environment:
      - DATABASE_URL=postgresql://postgres:secret@postgres:5432/app_db
      - REDIS_URL=redis://redis:6379
    depends_on:
      - postgres
      - redis

  postgres:
    image: postgres:16-alpine
    environment:
      POSTGRES_USER: postgres
      POSTGRES_PASSWORD: secret
      POSTGRES_DB: app_db
    ports:
      - '5432:5432'
    volumes:
      - pgdata:/var/lib/postgresql/data

  redis:
    image: redis:7-alpine
    ports:
      - '6379:6379'

volumes:
  pgdata:
```

### Example 3: Zero-Downtime Skew Protection Configuration
```ts
// next.config.ts
import type { NextConfig } from 'next';

const nextConfig: NextConfig = {
  // Pin deployment ID to Git commit hash for asset skew protection
  deploymentId: process.env.GIT_COMMIT_SHA,
  experimental: {
    // Keeps previous build assets accessible during rolling deployments
    skewProtection: true,
  },
};

export default nextConfig;
```

### Example 4: Graceful Shutdown in Node.js Server
```ts
// server.js (Embedded in standalone output)
process.on('SIGTERM', () => {
  console.log('SIGTERM received: Closing HTTP server gracefully...');
  server.close(() => {
    console.log('HTTP server closed. Exiting process.');
    process.exit(0);
  });
});
```

### Example 5: OpenNext AWS Lambda Adapter Overview
```bash
# Deploy Next.js to AWS Lambda & S3 without vendor lock-in
npx open-next build
# Generates open-next.output/ containing optimized Lambda handlers and S3 assets
```

---

## 4. Under the Hood & Internals
`output: 'standalone'` utilizes `@vercel/nft` (Node File Trace).
- During the build, NFT traces all `require()` and `import` calls starting from `server.js` and route handlers.
- It copies only the exact `.js` files in `node_modules` that are actively executed at runtime into `.next/standalone/node_modules/`.
- Development tools (TypeScript, ESLint, Tailwind, Vitest) are completely stripped, shrinking bundle size by over 90%.

---

## 5. Security & Container Hardening
- **Non-Root User Execution**: Always create a dedicated unprivileged user (`RUN adduser --system nextjs`) inside your Docker container. Running containerized Node.js as `root` exposes the host to container breakout vulnerabilities.
- **Read-Only Root Filesystem**: Configure container orchestration (Kubernetes / ECS) with `readOnlyRootFilesystem: true`, mounting ephemeral volumes only to `/tmp` for cache.

---

## 6. Common Mistakes & Anti-Patterns (Table)

| Anti-Pattern | Root Cause | Observable Symptom | Correct Architecture |
| :--- | :--- | :--- | :--- |
| Forgetting to copy `.next/static` | Static files not in standalone bundle | 404 errors on CSS and JS chunks | `COPY --from=builder /app/.next/static ./.next/static` |
| Running as root user in Docker | Default container user | Security scan failure / privilege escalation | Add and switch to `USER nextjs` |
| Asset skew on rolling deploy | CDN purges old chunks immediately | Users navigating experience chunk 404s | Enable `deploymentId` and skew protection |
| Missing `HOSTNAME="0.0.0.0"` | Node binds to localhost (127.0.0.1) | Container unreachable from Docker bridge | Set `ENV HOSTNAME="0.0.0.0"` in Dockerfile |

---

## 7. Performance & Optimization Guide
A standalone Alpine Docker container boots in under **250ms**, consumes less than **65 MB of idle RAM**, and processes over **4,500 requests per second** on a standard 2 vCPU cloud instance.

---

## 8. TypeScript & Strict Typing Recipes
```ts
export interface HealthStatus {
  status: 'healthy' | 'degraded' | 'unhealthy';
  uptimeSeconds: number;
  commitSha: string;
}
```

---

## 9. Testing & Quality Verification

```bash
# Verify standalone output compiles
npm run build
# Check standalone directory exists
test -f .next/standalone/server.js
```

---

## 10. When NOT to Use This Pattern
- If deploying to managed platforms like Vercel or Netlify, manual Docker containers are unnecessary; their native CI pipelines automatically deploy optimal edge and serverless architectures.

---

## 11. Frequently Asked Interview Questions

<details>
<summary>1. What does `output: 'standalone'` do in Next.js?</summary>
It automatically traces all runtime dependencies using Node File Trace and creates a minimal standalone folder (`.next/standalone`) containing only the code and modules required to run the Node server, reducing container sizes from 1.5GB to ~130MB.
</details>

<details>
<summary>2. Why must `.next/static` and `public` be copied separately in a standalone Dockerfile?</summary>
Because static assets and public media files are designed to be served by a CDN or reverse proxy (Nginx/Cloudflare) rather than through Node.js memory. If running standalone directly, they must be copied into the container root manually.
</details>

<details>
<summary>3. What is "Asset Skew" during zero-downtime rolling deployments?</summary>
When a new version of the app deploys, an active user with an older HTML version in their browser might click a link that requests an old JavaScript chunk. If the server only serves the new version's chunks, the client encounters a 404 script error.
</details>

<details>
<summary>4. How does Next.js resolve Asset Skew?</summary>
By setting `deploymentId: process.env.GIT_COMMIT_SHA`, Next.js appends unique deployment IDs to asset URLs and can serve previous version chunks from CDN storage during deployment transitions.
</details>

<details>
<summary>5. Why is setting `ENV HOSTNAME="0.0.0.0"` required in Docker?</summary>
By default, Node.js binds to `localhost` (`127.0.0.1`), which is inaccessible outside the Docker container. Binding to `0.0.0.0` allows Docker port forwarding to reach the server.
</details>

<details>
<summary>6. What is OpenNext?</summary>
OpenNext is an open-source build adapter that takes the output of `next build` and compiles it into serverless architectures optimized for AWS Lambda, Cloudflare Workers, and edge runtimes.
</details>

<details>
<summary>7. Why should Docker containers execute Next.js as a non-root user?</summary>
Running as `root` grants full privileges inside the container. If a remote code execution vulnerability occurs, an attacker could compromise the container and potentially escape to the host kernel.
</details>

<details>
<summary>8. How do you handle graceful shutdowns in containerized Next.js?</summary>
By listening for `SIGTERM` signals sent by Kubernetes or Docker, allowing existing in-flight HTTP requests to complete before exiting the process.
</details>

---

## 12. Hands-on Practice Exercises

<details>
<summary>Exercise 1 (Easy): Enable standalone build</summary>
Add `output: 'standalone'` to `next.config.ts` and inspect `.next/standalone`.
</details>

<details>
<summary>Exercise 2 (Medium): Write a production Dockerfile</summary>
Create a multi-stage Dockerfile running as a non-root `nextjs` user.
</details>

<details>
<summary>Exercise 3 (Hard): Configure Docker Compose with Postgres and Redis</summary>
Build and run a full-stack Next.js environment with containerized PostgreSQL and Redis.
</details>

---

## 13. Cross-References & Further Reading
- [Advanced Features: OpenTelemetry & Instrumentation](file:///E:/Learning/engineering-notes/02-frontend/nextjs/15-advanced-features/05-opentelemetry-and-instrumentation.md)
- [Official Next.js Self-Hosting Documentation](https://nextjs.org/docs/app/building-your-application/deploying#docker-image)
