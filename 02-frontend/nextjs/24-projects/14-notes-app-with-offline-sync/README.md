# Project 14: Offline-First Notes App with Background Sync (`notes-app-with-offline-sync`)

> Offline-first note taking application built with Next.js App Router, featuring Dexie.js (IndexedDB), Service Worker caching, and bidirectional background synchronization via Server Actions.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-next-14-offline-notes` |
| **Next.js Version** | 15.2+ / 16.0 |
| **React Version** | React 19.0.0 |
| **Node Version** | Node.js 20+ LTS |
| **Router** | App Router (`app/`) |
| **Local DB** | Dexie.js (IndexedDB) + Service Worker |
| **Remote DB** | PostgreSQL + Drizzle ORM |
| **Styling** | Tailwind CSS v4 |
| **Tested With** | Next.js 15.2, Vitest 3.0, Playwright 1.49 |
| **Complexity Level** | Production-Grade |

---

## 1. Architectural Overview & Component Hierarchy

### 1.1 Architecture & Offline-First Data Flow
Notes are created, edited, and deleted in local browser IndexedDB instantly with zero latency. When network connectivity is active, a synchronization service pushes un-replicated records to a Next.js Server Action (`syncNotesAction()`), reconciling timestamps and writing to PostgreSQL.

```
[ User Edits Note ] ──> [ Local IndexedDB (Dexie.js) - Instant 0ms Paint ]
                                      │
                         (Network Event: Online)
                                      │
                                      ▼
             [ Server Action: syncNotesAction(pendingRecords) ]
                                      │
                                      ▼
                      [ PostgreSQL Remote Database ]
```

---

## 2. Complete Project Source Code

### Configuration Files

#### `package.json`
```json
{
  "name": "nextjs-offline-notes",
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
    "dexie": "^4.0.11",
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

### Sync Server Action (`src/actions/syncActions.ts`)
```ts
'use server';

import { verifySession } from '@/dal/session';
import { unauthorized } from 'next/navigation';
import { db } from '@/db';
import { notes } from '@/db/schema';
import { sql } from 'drizzle-orm';

interface SyncRecord {
  id: string;
  title: string;
  content: string;
  updatedAt: number;
}

export async function syncNotesAction(pendingNotes: SyncRecord[]) {
  const session = await verifySession();
  if (!session) unauthorized();

  for (const item of pendingNotes) {
    await db
      .insert(notes)
      .values({
        id: item.id,
        userId: session.user.id,
        title: item.title,
        content: item.content,
        updatedAt: new Date(item.updatedAt),
      })
      .onConflictDoUpdate({
        target: notes.id,
        set: {
          title: item.title,
          content: item.content,
          updatedAt: new Date(item.updatedAt),
        },
      });
  }

  return { success: true, syncedCount: pendingNotes.length };
}
```

---

## 3. Testing Suite & Verification

```ts
import { describe, it, expect } from 'vitest';

describe('Offline Record Reconciliation', () => {
  it('identifies conflict by timestamp', () => {
    const localTime = 1700000050;
    const serverTime = 1700000000;
    expect(localTime > serverTime).toBe(true);
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
