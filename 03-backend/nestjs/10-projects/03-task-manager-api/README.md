# Project 03: Collaborative Task Manager API (`03-task-manager-api`)

> Real-time Kanban board backend built with NestJS 11, Prisma ORM, PostgreSQL, Server-Sent Events (SSE), and Optimistic Concurrency Control.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-nest-03-task-manager-api` |
| **NestJS Version** | NestJS 11+ |
| **Database & ORM** | PostgreSQL 16 + Prisma ORM 5+ |
| **Realtime Transport** | Server-Sent Events (SSE) with RxJS Subject |
| **Complexity Level** | Intermediate Production Reference |

---

## 1. Architectural Overview & Prisma Schema

```prisma
// prisma/schema.prisma
datasource db {
  provider = "postgresql"
  url      = env("DATABASE_URL")
}

generator client {
  provider = "prisma-client-js"
}

model Board {
  id        String   @id @default(uuid())
  title     String
  columns   Column[]
  createdAt DateTime @default(now())
}

model Column {
  id        String   @id @default(uuid())
  title     String
  position  Float
  boardId   String
  board     Board    @relation(fields: [boardId], references: [id], onDelete: Cascade)
  tasks     Task[]
}

model Task {
  id          String   @id @default(uuid())
  title       String
  description String?
  position    Float
  version     Int      @default(1) // Optimistic concurrency lock
  columnId    String
  column      Column   @relation(fields: [columnId], references: [id], onDelete: Cascade)
}
```

---

## 2. Realtime SSE Notifications Controller

```typescript
// src/events/task-events.controller.ts
import { Controller, Sse, Param } from '@nestjs/common';
import { Observable } from 'rxjs';
import { map } from 'rxjs/operators';
import { TaskEventsService } from './task-events.service';

@Controller('boards/:boardId/events')
export class TaskEventsController {
  constructor(private readonly eventsService: TaskEventsService) {}

  @Sse()
  streamEvents(@Param('boardId') boardId: string): Observable<MessageEvent> {
    return this.eventsService.getBoardStream(boardId).pipe(
      map((event) => ({
        data: event,
      } as MessageEvent)),
    );
  }
}
```
