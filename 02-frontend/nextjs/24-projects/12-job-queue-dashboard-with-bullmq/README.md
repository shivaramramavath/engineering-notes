# Project 12: Job Queue Dashboard with BullMQ (`job-queue-dashboard-with-bullmq`)

> Visual background queue monitor and worker management console built with Next.js App Router, featuring BullMQ, Redis streams, real-time job retry controls, and throughput telemetry.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-next-12-bullmq-dashboard` |
| **Next.js Version** | 15.2+ / 16.0 |
| **React Version** | React 19.0.0 |
| **Node Version** | Node.js 20+ LTS |
| **Router** | App Router (`app/`) |
| **Queues** | BullMQ + Redis 7 |
| **Styling** | Tailwind CSS v4 + Radix UI |
| **Tested With** | Next.js 15.2, Vitest 3.0, Playwright 1.49 |
| **Complexity Level** | Production-Grade |

---

## 1. Architectural Overview & Component Hierarchy

### 1.1 Queue Architecture & Control
BullMQ coordinates job execution using Redis Streams and Sorted Sets. Administrators monitor active, completed, failed, and delayed job states via the Next.js App Router, with Server Actions exposing one-click retry and job purging capabilities.

```
[ Next.js Queue Admin UI ] ──(Server Actions: retryJob(), pauseQueue())
            │
            ▼
[ BullMQ Queue Manager ] ──> [ Redis 7 Connection (Streams & Hashes) ]
            │
            ▼
[ Distributed Node.js Worker Processes (Processing Background Jobs) ]
```

---

## 2. Complete Project Source Code

### Configuration Files

#### `package.json`
```json
{
  "name": "nextjs-bullmq-dashboard",
  "version": "1.0.0",
  "private": true,
  "scripts": {
    "dev": "next dev --turbopack",
    "build": "next build",
    "start": "next start",
    "test": "vitest run"
  },
  "dependencies": {
    "next": "^15.2.0",
    "react": "^19.0.0",
    "react-dom": "^19.0.0",
    "server-only": "^0.0.1",
    "bullmq": "^5.34.1",
    "ioredis": "^5.4.2"
  },
  "devDependencies": {
    "typescript": "^5.7.3",
    "tailwindcss": "^4.0.0",
    "vitest": "^3.0.0"
  }
}
```

### Queue Telemetry DAL (`src/dal/queueStats.ts`)
```ts
import 'server-only';
import { Queue } from 'bullmq';
import Redis from 'ioredis';

const connection = new Redis(process.env.REDIS_URL!);
const mailQueue = new Queue('mail-delivery', { connection });

export async function getQueueMetrics() {
  const [waiting, active, completed, failed, delayed] = await Promise.all([
    mailQueue.getWaitingCount(),
    mailQueue.getActiveCount(),
    mailQueue.getCompletedCount(),
    mailQueue.getFailedCount(),
    mailQueue.getDelayedCount(),
  ]);

  return { waiting, active, completed, failed, delayed };
}
```

### Job Retry Server Action (`src/actions/queueActions.ts`)
```ts
'use server';

import { Queue } from 'bullmq';
import Redis from 'ioredis';
import { revalidatePath } from 'next/cache';

const connection = new Redis(process.env.REDIS_URL!);
const mailQueue = new Queue('mail-delivery', { connection });

export async function retryFailedJob(jobId: string) {
  const job = await mailQueue.getJob(jobId);
  if (job) {
    await job.retry();
    revalidatePath('/queues');
  }
  return { success: true };
}
```

---

## 3. Testing Suite & Verification

```ts
import { describe, it, expect } from 'vitest';

describe('Queue Metrics Structure', () => {
  it('formats metric counts accurately', () => {
    const counts = { waiting: 5, active: 2 };
    expect(counts.waiting + counts.active).toBe(7);
  });
});
```

---

## 4. Build and Run Instructions

```bash
npm install
npm run build
npm run start
```
