# Project 07: Multi-Channel Notification Dispatcher (`07-notification-service`)

> Distributed notification engine dispatching transactional Email, SMS (Twilio), Push, and Webhooks using BullMQ queues and exponential retry backoff.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-nest-07-notification-service` |
| **Queues** | BullMQ + Redis 7 |
| **Channels** | Nodemailer (SMTP), Twilio (SMS), Webhook POST |
| **Complexity Level** | Advanced Distributed Reference |

---

## 1. BullMQ Notification Processor

```typescript
// src/notifications/notification.processor.ts
import { Processor, WorkerHost } from '@nestjs/bullmq';
import { Job } from 'bullmq';
import { Logger } from '@nestjs/common';

@Processor('notifications', { concurrency: 10 })
export class NotificationProcessor extends WorkerHost {
  private readonly logger = new Logger(NotificationProcessor.name);

  async process(job: Job<{ channel: string; recipient: string; content: string }>) {
    this.logger.log(`Processing notification [${job.id}] via [${job.data.channel}]`);

    switch (job.data.channel) {
      case 'EMAIL':
        // Send email logic...
        break;
      case 'SMS':
        // Send SMS logic...
        break;
      default:
        throw new Error(`Unsupported channel: ${job.data.channel}`);
    }
  }
}
```
