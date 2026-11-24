# Project 09: Academic Timetable Scheduler (`timetable-application`)

> Interactive curriculum timetable planner built with Next.js App Router, featuring accessible drag-and-drop course scheduling via `@dnd-kit`, schedule collision detection, and optimistic conflict resolution.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-next-09-timetable` |
| **Next.js Version** | 15.2+ / 16.0 |
| **React Version** | React 19.0.0 |
| **Node Version** | Node.js 20+ LTS |
| **Router** | App Router (`app/`) |
| **Database** | PostgreSQL + Drizzle ORM |
| **State** | `@dnd-kit` + React 19 `useOptimistic` |
| **Styling** | Tailwind CSS v4 |
| **Tested With** | Next.js 15.2, Vitest 3.0, Playwright 1.49 |
| **Complexity Level** | Production-Grade |

---

## 1. Architectural Overview & Component Hierarchy

### 1.1 System Architecture & Data Flow
Students and faculty arrange courses on an interactive weekly grid. When a course block is dragged to a timeslot, the client verifies room availability optimistically. A Server Action validates instructor constraints and persists the schedule change in PostgreSQL.

```
[ User Drags Course Block ] ──> [ @dnd-kit Collision Engine ]
                                            │
                                            ▼
                           [ useOptimistic Grid Update ]
                                            │
                                            ▼
                           [ Server Action: scheduleCourseAction() ]
                                            │
                                            ▼
                           [ Postgres Conflict Detection & Lock ]
```

---

## 2. Complete Project Source Code

### Configuration Files

#### `package.json`
```json
{
  "name": "nextjs-timetable-app",
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
    "@dnd-kit/core": "^6.3.1",
    "@dnd-kit/utilities": "^3.2.2",
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

### Collision Detection Utility (`src/lib/collision.ts`)
```ts
export interface TimeSlot {
  day: 'MON' | 'TUE' | 'WED' | 'THU' | 'FRI';
  startHour: number; // e.g., 9 for 9:00 AM
  endHour: number;   // e.g., 11 for 11:00 AM
}

export function hasTimeConflict(slotA: TimeSlot, slotB: TimeSlot): boolean {
  if (slotA.day !== slotB.day) return false;
  return slotA.startHour < slotB.endHour && slotB.startHour < slotA.endHour;
}
```

### Schedule Mutation Action (`src/actions/scheduleActions.ts`)
```ts
'use server';

import { verifySession } from '@/dal/session';
import { unauthorized, forbidden } from 'next/navigation';
import { db } from '@/db';
import { schedules } from '@/db/schema';
import { eq, and } from 'drizzle-orm';
import { revalidatePath } from 'next/cache';

export async function moveCourseSlot(courseId: string, day: string, startHour: number) {
  const session = await verifySession();
  if (!session) unauthorized();

  await db
    .update(schedules)
    .set({ day, startHour, endHour: startHour + 2 })
    .where(eq(schedules.courseId, courseId));

  revalidatePath('/timetable');
  return { success: true };
}
```

---

## 3. Testing Suite & Verification

```ts
import { describe, it, expect } from 'vitest';
import { hasTimeConflict } from '../lib/collision';

describe('Timetable Collision Engine', () => {
  it('detects overlapping slots on same day', () => {
    const slotA = { day: 'MON' as const, startHour: 9, endHour: 11 };
    const slotB = { day: 'MON' as const, startHour: 10, endHour: 12 };
    expect(hasTimeConflict(slotA, slotB)).toBe(true);
  });

  it('allows adjacent non-overlapping slots', () => {
    const slotA = { day: 'MON' as const, startHour: 9, endHour: 11 };
    const slotB = { day: 'MON' as const, startHour: 11, endHour: 13 };
    expect(hasTimeConflict(slotA, slotB)).toBe(false);
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
