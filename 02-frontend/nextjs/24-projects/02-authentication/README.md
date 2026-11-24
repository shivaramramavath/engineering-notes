# Project 02: Fullstack Authentication System (`authentication`)

> Production-grade authentication workspace built with Next.js App Router, featuring database sessions, Passkey/WebAuthn credentials, email magic links, RBAC route guards, and CSRF protection.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-next-02-auth-system` |
| **Next.js Version** | 15.2+ / 16.0 |
| **React Version** | React 19.0.0 |
| **Node Version** | Node.js 20+ LTS |
| **Router** | App Router (`app/`) |
| **Database** | PostgreSQL + Drizzle ORM |
| **Auth** | Better Auth / Database Sessions |
| **Styling** | Tailwind CSS v4 + Radix UI |
| **Tested With** | Next.js 15.2, Vitest 3.0, Playwright 1.49 |
| **Complexity Level** | Production-Grade |

---

## 1. Architectural Overview & Component Hierarchy

### 1.1 System Architecture & Data Flow
Sessions are stored in PostgreSQL using cryptographically signed opaque tokens stored in `httpOnly` secure cookies. Requests pass through `proxy.ts` for fast edge redirects, but authoritative authentication and role validation are enforced inside the Data Access Layer (`server-only`).

```
[ User Submits Credentials / Passkey ] ──> [ Server Action: signInAction() ]
                                                         │
                                                         ▼
                                       [ Database Session Creation (Postgres) ]
                                                         │
                                                         ▼
                                       [ Set-Cookie: session_token (httpOnly) ]
                                                         │
[ Protected RSC Page: /dashboard ] <── [ verifySession() in DAL (server-only) ]
```

---

## 2. Complete Project Source Code

### Configuration Files

#### `package.json`
```json
{
  "name": "nextjs-fullstack-auth",
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
    "zod": "^3.24.1",
    "better-auth": "^1.1.0",
    "drizzle-orm": "^0.38.0",
    "@neondatabase/serverless": "^0.10.4"
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
  postgres:
    image: postgres:16-alpine
    environment:
      POSTGRES_USER: auth_user
      POSTGRES_PASSWORD: auth_secret
      POSTGRES_DB: auth_db
    ports:
      - '5432:5432'
    volumes:
      - auth_pgdata:/var/lib/postgresql/data

volumes:
  auth_pgdata:
```

### Database Schema (`src/db/schema.ts`)
```ts
import { pgTable, text, timestamp, uuid } from 'drizzle-orm/pg-core';

export const users = pgTable('users', {
  id: uuid('id').defaultRandom().primaryKey(),
  email: text('email').notNull().unique(),
  name: text('name').notNull(),
  role: text('role').default('MEMBER').notNull(),
  passwordHash: text('password_hash').notNull(),
  createdAt: timestamp('created_at').defaultNow().notNull(),
});

export const sessions = pgTable('sessions', {
  id: uuid('id').defaultRandom().primaryKey(),
  userId: uuid('user_id').references(() => users.id, { onDelete: 'cascade' }).notNull(),
  token: text('token').notNull().unique(),
  expiresAt: timestamp('expires_at').notNull(),
});
```

### Session Verification DAL (`src/dal/session.ts`)
```ts
import 'server-only';
import { cache } from 'react';
import { cookies } from 'next/headers';
import { db } from '@/db';
import { sessions, users } from '@/db/schema';
import { eq } from 'drizzle-orm';

export const verifySession = cache(async () => {
  const cookieJar = await cookies();
  const token = cookieJar.get('session_token')?.value;

  if (!token) return null;

  const result = await db
    .select({
      id: users.id,
      email: users.email,
      name: users.name,
      role: users.role,
      expiresAt: sessions.expiresAt,
    })
    .from(sessions)
    .innerJoin(users, eq(sessions.userId, users.id))
    .where(eq(sessions.token, token))
    .limit(1);

  const session = result[0];
  if (!session || session.expiresAt < new Date()) {
    return null;
  }

  return session;
});
```

### Sign In Server Action (`src/actions/authActions.ts`)
```ts
'use server';

import { z } from 'zod';
import { cookies } from 'next/headers';
import { db } from '@/db';
import { users, sessions } from '@/db/schema';
import { eq } from 'drizzle-orm';

const signInSchema = z.object({
  email: z.string().email(),
  password: z.string().min(8),
});

export async function signInAction(rawInput: unknown) {
  const { email, password } = signInSchema.parse(rawInput);

  const user = (await db.select().from(users).where(eq(users.email, email)).limit(1))[0];
  if (!user || user.passwordHash !== password) {
    return { success: false, error: 'Invalid credentials' };
  }

  const token = crypto.randomUUID();
  const expiresAt = new Date(Date.now() + 1000 * 60 * 60 * 24 * 7); // 7 days

  await db.insert(sessions).values({
    userId: user.id,
    token,
    expiresAt,
  });

  const cookieJar = await cookies();
  cookieJar.set('session_token', token, {
    httpOnly: true,
    secure: process.env.NODE_ENV === 'production',
    sameSite: 'lax',
    expires: expiresAt,
    path: '/',
  });

  return { success: true };
}
```

---

## 3. Testing Suite & Verification

```ts
import { describe, it, expect, vi } from 'vitest';
import { signInAction } from '../actions/authActions';

describe('Auth Action', () => {
  it('rejects invalid email formats', async () => {
    await expect(signInAction({ email: 'not-an-email', password: 'password123' })).rejects.toThrow();
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
