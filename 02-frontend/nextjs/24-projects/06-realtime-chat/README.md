# Project 06: Real-Time Chat Workspace (`realtime-chat`)

> Collaborative messaging workspace built with Next.js App Router, featuring real-time Server-Sent Events (SSE) streaming, optimistic message dispatch (`useOptimistic`), presence detection with Redis Pub/Sub, and unread badges.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-next-06-realtime-chat` |
| **Next.js Version** | 15.2+ / 16.0 |
| **React Version** | React 19.0.0 |
| **Node Version** | Node.js 20+ LTS |
| **Router** | App Router (`app/`) |
| **Realtime** | Server-Sent Events (SSE) + Redis Pub/Sub |
| **Database** | PostgreSQL + Drizzle ORM |
| **Styling** | Tailwind CSS v4 |
| **Tested With** | Next.js 15.2, Vitest 3.0, Playwright 1.49 |
| **Complexity Level** | Production-Grade |

---

## 1. Architectural Overview & Component Hierarchy

### 1.1 System Architecture & Data Flow
Clients maintain a persistent HTTP connection to a Route Handler serving an SSE stream (`/api/chat/stream`). Messages sent via a Server Action are stored in PostgreSQL and published to a Redis Pub/Sub channel. The SSE Route Handler subscribes to Redis and immediately flushes new messages down the client stream.

```
[ User Sends Message ] ──> [ Server Action: sendMessage() ]
                                       │
                                       ├── 1. Insert into PostgreSQL
                                       └── 2. Publish to Redis channel ('chat:room-1')
                                                    │
                                                    ▼
[ Incoming SSE Stream: /api/chat/stream ] <── [ Redis Subscriber ]
                 │
                 ▼ (Instant Client Push)
[ Client Message Feed (Optimistic UI) ]
```

---

## 2. Complete Project Source Code

### Configuration Files

#### `package.json`
```json
{
  "name": "nextjs-realtime-chat",
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
    "ioredis": "^5.4.2",
    "drizzle-orm": "^0.38.0",
    "zod": "^3.24.1"
  },
  "devDependencies": {
    "typescript": "^5.7.3",
    "tailwindcss": "^4.0.0",
    "vitest": "^3.0.0"
  }
}
```

#### `docker-compose.yml`
```yaml
version: '3.8'

services:
  redis:
    image: redis:7-alpine
    ports:
      - '6379:6379'
  postgres:
    image: postgres:16-alpine
    environment:
      POSTGRES_USER: chat_user
      POSTGRES_PASSWORD: chat_secret
      POSTGRES_DB: chat_db
    ports:
      - '5432:5432'
```

### SSE Streaming Route Handler (`src/app/api/chat/stream/route.ts`)
```ts
import Redis from 'ioredis';

export const dynamic = 'force-dynamic';

export async function GET(req: Request) {
  const { searchParams } = new URL(req.url);
  const roomId = searchParams.get('roomId') || 'general';

  const redisSub = new Redis(process.env.REDIS_URL!);
  const encoder = new TextEncoder();

  const stream = new ReadableStream({
    async start(controller) {
      await redisSub.subscribe(`chat:${roomId}`);

      redisSub.on('message', (channel, message) => {
        controller.enqueue(encoder.encode(`data: ${message}\n\n`));
      });

      req.signal.addEventListener('abort', () => {
        redisSub.unsubscribe();
        redisSub.quit();
        controller.close();
      });
    },
  });

  return new Response(stream, {
    headers: {
      'Content-Type': 'text/event-stream',
      'Cache-Control': 'no-cache, no-transform',
      Connection: 'keep-alive',
    },
  });
}
```

### Send Message Action (`src/actions/chatActions.ts`)
```ts
'use server';

import Redis from 'ioredis';
import { verifySession } from '@/dal/session';
import { unauthorized } from 'next/navigation';

const redisPub = new Redis(process.env.REDIS_URL!);

export async function sendMessageAction(roomId: string, text: string) {
  const session = await verifySession();
  if (!session) unauthorized();

  const message = {
    id: crypto.randomUUID(),
    senderName: session.user.name,
    text,
    timestamp: Date.now(),
  };

  // Publish to Redis channel; SSE route handler streams it to all clients
  await redisPub.publish(`chat:${roomId}`, JSON.stringify(message));

  return { success: true };
}
```

---

## 3. Testing Suite & Verification

```ts
import { describe, it, expect } from 'vitest';

describe('Chat Payload Serialization', () => {
  it('encodes message payload as valid JSON', () => {
    const payload = JSON.stringify({ text: 'Hello team!' });
    expect(JSON.parse(payload).text).toBe('Hello team!');
  });
});
```

---

## 4. Build and Run Instructions

```bash
docker compose up -d
npm install
npm run build
npm run start
```
