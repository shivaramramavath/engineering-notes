# Delayed and Repeatable Jobs in BullMQ

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 05-advanced / 02-background-processing
- **Prerequisites**: [02-bullmq](./02-bullmq.md), [03-producers-and-consumers](./03-producers-and-consumers.md)

---

## 1. Context & Purpose

Not all background work needs to execute immediately. Many business workflows require timed delays:
- *"Send an abandoned cart reminder email 2 hours after a user leaves the site."*
- *"Cancel an unpaid order reservation 15 minutes after checkout initiation."*
- *"Generate daily billing reports every midnight, or poll an external vendor API every 5 minutes."*

BullMQ natively supports both **Delayed Jobs** (executing once after a specified millisecond delay) and **Repeatable Jobs** (recurring cron-like schedules managed natively in Redis). Unlike local in-process cron libraries, repeatable jobs in BullMQ are distributed across all worker pods, guaranteeing that a recurring job runs exactly once across the entire cluster without duplicate executions.

---

## 2. Theoretical Foundations

### 2.1 Delayed Jobs Architecture
BullMQ stores delayed jobs in a Redis Sorted Set (`delayed`) where the **score** is the Unix timestamp (milliseconds) when the job becomes runnable.
- A lightweight internal timer or incoming stream event checks if `score <= Date.now()`.
- When the target timestamp is reached, BullMQ atomically moves the job from `delayed` into `wait` via a Lua script.

### 2.2 Repeatable Jobs vs Local In-Memory Cron
- **Local Cron (`@nestjs/schedule`)**: Runs in the local Node.js process memory. If you deploy 5 Kubernetes pods, the cron job executes **5 times simultaneously** unless a distributed database lock is acquired.
- **Repeatable BullMQ Jobs**: Scheduled in Redis. Only one worker across the entire cluster claims and executes the recurring job.

---

## 3. Architecture & Internal Mechanics

```
PRODUCER ADDS DELAYED JOB:
await queue.add('cart-reminder', { cartId: 101 }, { delay: 7200000 }) // 2 hours
                      │
                      ▼
               Redis Sorted Set (ZSET)
          `bull:mail:delayed`
          Score: Timestamp (Now + 2h)
                      │
                      ▼ (2 hours pass...)
           Timestamp <= Date.now()
                      │
                      ▼
    BullMQ atomically moves job to `bull:mail:wait`
                      │
                      ▼
           Worker picks up job and runs!
```

---

## 4. Concrete Implementation

### 4.1 Scheduling Delayed Jobs

```typescript
// src/cart/cart-reminder.service.ts
import { Injectable, Logger } from '@nestjs/common';
import { InjectQueue } from '@nestjs/bullmq';
import { Queue } from 'bullmq';

@Injectable()
export class CartReminderService {
  private readonly logger = new Logger(CartReminderService.name);

  constructor(@InjectQueue('cart') private readonly cartQueue: Queue) {}

  async scheduleAbandonedCartReminder(cartId: string, email: string) {
    const delayMs = 2 * 60 * 60 * 1000; // 2 hours in ms

    // Use deterministic jobId so we can cancel it if user checks out!
    const job = await this.cartQueue.add(
      'send-reminder',
      { cartId, email },
      {
        jobId: `reminder:${cartId}`,
        delay: delayMs,
        removeOnComplete: true,
      },
    );

    this.logger.log(`Scheduled cart reminder for cart ${cartId} with 2h delay (JobID: ${job.id})`);
    return job;
  }

  // Cancel delayed reminder if the customer completes checkout
  async cancelCartReminder(cartId: string) {
    const job = await this.cartQueue.getJob(`reminder:${cartId}`);
    if (job) {
      await job.remove();
      this.logger.log(`Cancelled abandoned cart reminder for cart ${cartId}`);
    }
  }
}
```

---

### 4.2 Scheduling Repeatable (Cron-like) Jobs

Repeatable jobs can be scheduled using standard cron expressions or fixed intervals:

```typescript
// src/reports/report-scheduler.service.ts
import { Injectable, OnModuleInit, Logger } from '@nestjs/common';
import { InjectQueue } from '@nestjs/bullmq';
import { Queue } from 'bullmq';

@Injectable()
export class ReportSchedulerService implements OnModuleInit {
  private readonly logger = new Logger(ReportSchedulerService.name);

  constructor(@InjectQueue('reports') private readonly reportQueue: Queue) {}

  async onModuleInit() {
    await this.scheduleDailyMetrics();
    await this.scheduleHeartbeatPoll();
  }

  private async scheduleDailyMetrics() {
    // Cron schedule: Run at 00:00 every midnight in UTC
    await this.reportQueue.add(
      'daily-metrics',
      { type: 'FINANCIAL_SUMMARY' },
      {
        repeat: {
          pattern: '0 0 * * *',
          tz: 'UTC',
        },
        removeOnComplete: 100,
      },
    );
    this.logger.log('Registered repeatable job: daily-metrics (Midnight UTC)');
  }

  private async scheduleHeartbeatPoll() {
    // Fixed interval: Run every 60 seconds
    await this.reportQueue.add(
      'health-check-poll',
      {},
      {
        repeat: {
          every: 60000, // 60,000 ms
        },
        removeOnComplete: 50,
      },
    );
  }
}
```

---

### 4.3 Managing and Removing Repeatable Jobs

Repeatable jobs create internal schedules in Redis. To cancel or modify a repeatable job schedule:

```typescript
// src/reports/report-scheduler.service.ts (Continued)
async removeRepeatableJob(name: string, cronPattern: string) {
  // Query all registered repeatable jobs in the queue
  const repeatableJobs = await this.reportQueue.getRepeatableJobs();

  const target = repeatableJobs.find(
    (job) => job.name === name && job.pattern === cronPattern,
  );

  if (target) {
    await this.reportQueue.removeRepeatableByKey(target.key);
    this.logger.log(`Successfully removed repeatable job: ${name} (${cronPattern})`);
  }
}
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Duplicate Registrations on Multi-Pod Startup
If 10 pods boot up and all run `onModuleInit()` containing `queue.add('job', {}, { repeat: { pattern: '...' } })`:
- **BullMQ Behavior**: BullMQ automatically deduplicates repeatable jobs with the same name and pattern. Only one repeatable job entry is registered in Redis.

### 5.2 Timezone Drift
Cron expressions without an explicit timezone default to the system server time. If servers are configured in UTC and operators assume local time, jobs run hours early or late.
- **Rule**: Always specify `tz: 'UTC'` or an explicit IANA timezone (`tz: 'America/New_York'`).

---

## 6. Performance & Memory Implications

- **Redis Sorted Set Efficiency**: Delay checking uses $O(\log(N) + M)$ Redis range queries, effortlessly handling hundreds of thousands of concurrent delayed jobs with sub-millisecond CPU impact.
- **Repeatable Cleanup**: Each run of a repeatable job creates a new job record. Ensure `removeOnComplete` is set on the repeatable job options to avoid bloating Redis memory.

---

## 7. Anti-Patterns & Common Mistakes

- **Using `setTimeout()` for Hours-Long Delays**: Writing `setTimeout(() => sendEmail(), 7200000)`. If the container restarts or crashes during those 2 hours, the timer is lost forever.
- **Changing Job Name When Modifying Schedule**: Adding a new repeatable job with a different name while leaving the old repeatable job running in Redis, executing both simultaneously.
- **Using Unbounded Delayed Payloads**: Passing sensitive data that might become obsolete or invalid over the delay window. Pass IDs and query fresh state in the consumer.

---

## 8. Production Checklist & Operational Readiness

- [ ] Delayed jobs assigned deterministic `jobId` to permit cancellation.
- [ ] Explicit IANA timezone (`tz`) specified on all cron repeat patterns.
- [ ] `removeOnComplete` configured on repeatable jobs.
- [ ] Repeatable job status verifiable via Bull Board or API management endpoints.

---

## 9. Migration & Version History

- **Bull 3 to BullMQ**: Replaced complex custom repeat schedulers with clean Redis Streams and native `repeat.pattern` / `repeat.every` options.

---

## 10. Verification & Testing

Unit testing delayed job scheduling:

```typescript
import { Test, TestingModule } from '@nestjs/testing';
import { getQueueToken } from '@nestjs/bullmq';
import { CartReminderService } from './cart-reminder.service';

describe('CartReminderService (Delayed)', () => {
  let service: CartReminderService;
  let mockQueue: any;

  beforeEach(async () => {
    mockQueue = {
      add: vi.fn().mockResolvedValue({ id: 'reminder:c123' }),
      getJob: vi.fn(),
    };

    const module: TestingModule = await Test.createTestingModule({
      providers: [
        CartReminderService,
        { provide: getQueueToken('cart'), useValue: mockQueue },
      ],
    }).compile();

    service = module.get<CartReminderService>(CartReminderService);
  });

  it('schedules delayed reminder with 2 hour delay', async () => {
    await service.scheduleAbandonedCartReminder('c123', 'test@test.com');

    expect(mockQueue.add).toHaveBeenCalledWith(
      'send-reminder',
      { cartId: 'c123', email: 'test@test.com' },
      expect.objectContaining({
        jobId: 'reminder:c123',
        delay: 7200000,
      }),
    );
  });
});
```

---

## 11. Comprehensive Examples

### Auto-Expiring Order Reservation Pattern

```typescript
// src/orders/order-reservation.service.ts
import { Injectable } from '@nestjs/common';
import { InjectQueue } from '@nestjs/bullmq';
import { Queue } from 'bullmq';

@Injectable()
export class OrderReservationService {
  constructor(@InjectQueue('orders') private readonly orderQueue: Queue) {}

  async reserveItems(orderId: string) {
    // 15-minute reservation TTL
    const expirationMs = 15 * 60 * 1000;

    await this.orderQueue.add(
      'expire-reservation',
      { orderId },
      {
        jobId: `order-expire:${orderId}`,
        delay: expirationMs,
      },
    );
  }

  async markOrderPaid(orderId: string) {
    // Payment received: cancel the reservation expiration job!
    const job = await this.orderQueue.getJob(`order-expire:${orderId}`);
    if (job) {
      await job.remove();
    }
  }
}
```

---

## 12. Cross-References & Further Reading

- [02-bullmq.md](./02-bullmq.md) - BullMQ setup
- [08-scheduling-with-cron.md](./08-scheduling-with-cron.md) - In-process cron comparison
- [BullMQ Repeatable Jobs Documentation](https://docs.bullmq.io/guide/jobs/repeatable)
