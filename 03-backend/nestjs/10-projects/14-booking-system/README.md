# Project 14: Realtime Hotel & Seat Booking Engine (`14-booking-system`)

> High-concurrency seat and hotel reservation platform: Redlock distributed locking, temporary reservation holds, expiration timers via BullMQ, and payment finalization.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-nest-14-booking-system` |
| **Concurrency Lock** | Redis Redlock + DB Pessimistic Locking |
| **Expiration Timers** | BullMQ Delayed Jobs (15m hold window) |
| **Complexity Level** | Advanced Enterprise Reference |

---

## 1. Distributed Seat Hold Service

```typescript
// src/booking/booking.service.ts
import { Injectable, ConflictException } from '@nestjs/common';
import { InjectQueue } from '@nestjs/bullmq';
import { Queue } from 'bullmq';
import { RedisService } from '../redis/redis.service';

@Injectable()
export class BookingService {
  constructor(
    @InjectQueue('booking-timeouts') private readonly timeoutQueue: Queue,
    private readonly redis: RedisService,
  ) {}

  async holdSeat(seatId: string, userId: string) {
    // Atomic acquire lock in Redis
    const lockKey = `seat:lock:${seatId}`;
    const acquired = await this.redis.getClient().set(lockKey, userId, 'PX', 15 * 60 * 1000, 'NX');

    if (!acquired) {
      throw new ConflictException('Seat is currently held by another customer');
    }

    // Schedule 15-minute expiration job in BullMQ
    await this.timeoutQueue.add('expireHold', { seatId, userId }, { delay: 15 * 60 * 1000 });
    return { status: 'HELD', seatId, expiresInSeconds: 900 };
  }
}
```
