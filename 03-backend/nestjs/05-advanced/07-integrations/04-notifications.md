# Multi-Channel Notification Systems in NestJS

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 05-advanced / 07-integrations
- **Prerequisites**: [03-email](./03-email.md), [05-advanced/04-realtime/01-gateways-and-events](../04-realtime/01-gateways-and-events.md), [05-advanced/02-background-processing/02-bullmq](../02-background-processing/02-bullmq.md)

---

## 1. Context & Purpose

Modern consumer and enterprise platforms rarely communicate through a single channel. A critical event—such as an order shipment or security alert—frequently requires multi-channel delivery:
1. **In-App Notification**: Real-time WebSocket or database notification badge.
2. **Push Notification**: Mobile push notifications via Apple APNs or Google Firebase Cloud Messaging (FCM).
3. **SMS**: Urgent delivery via Twilio or MessageBird.
4. **Email**: Formal delivery with rich attachments.
5. **Chat Webhooks**: Slack or Microsoft Teams channels.

Building a **Unified Multi-Channel Notification Engine** in NestJS abstracts provider-specific SDKs behind a standardized `Notification` interface, routing channels dynamically according to user preferences and delivery priorities.

---

## 2. Theoretical Foundations

### 2.1 The Notification Channel Pattern
- **Notification Object**: Encapsulates domain event context (`OrderShippedNotification(order)`).
- **Channels**: Independent delivery adapters (`EmailChannel`, `SmsChannel`, `PushChannel`, `InAppChannel`).
- **User Preferences**: Determines active channels for each user (e.g. User A opts out of marketing SMS but keeps security push alerts).

---

## 3. Architecture & Internal Mechanics

```
┌────────────────────────────────────────────────────────┐
│                   Domain Event Occurs                  │
│       notificationService.send(user, notification)     │
└───────────────────────────┬────────────────────────────┘
                            │
               Evaluates User Notification Preferences
                            │
              Enqueues jobs to BullMQ (`notifications`)
                            │
 ┌──────────────────────────┼───────────────────────────┐
 │                          │                           │
 ▼                          ▼                           ▼
Email Channel              SMS Channel                 Push Channel
(Nodemailer/SendGrid)      (Twilio)                    (Firebase FCM)
 │                          │                           │
 ▼                          ▼                           ▼
User Inbox                 User Phone                  Mobile Device
```

---

## 4. Concrete Implementation

### 4.1 Defining Notification Contracts

```typescript
// src/notifications/interfaces/notification.interface.ts
export type NotificationChannel = 'email' | 'sms' | 'push' | 'in_app';

export interface Recipient {
  id: string;
  email: string;
  phone?: string;
  fcmToken?: string;
  preferences: {
    channels: NotificationChannel[];
  };
}

export interface Notification {
  via(recipient: Recipient): NotificationChannel[];
  toEmail?(recipient: Recipient): { subject: string; template: string; context: any };
  toSms?(recipient: Recipient): { message: string };
  toPush?(recipient: Recipient): { title: string; body: string; data?: any };
  toInApp?(recipient: Recipient): { title: string; content: string };
}
```

---

### 4.2 Concrete Notification Class Example

```typescript
// src/orders/notifications/order-shipped.notification.ts
import { Notification, Recipient, NotificationChannel } from '../../notifications/interfaces/notification.interface';

export class OrderShippedNotification implements Notification {
  constructor(
    private readonly orderId: string,
    private readonly trackingNumber: string,
  ) {}

  via(recipient: Recipient): NotificationChannel[] {
    // Deliver via user's preferred channels
    return recipient.preferences.channels.filter((c) =>
      ['email', 'sms', 'in_app'].includes(c),
    );
  }

  toEmail(recipient: Recipient) {
    return {
      subject: `Order #${this.orderId} Shipped!`,
      template: 'order-shipped',
      context: { orderId: this.orderId, trackingNumber: this.trackingNumber },
    };
  }

  toSms(recipient: Recipient) {
    return {
      message: `Your order #${this.orderId} is on the way! Tracking: ${this.trackingNumber}`,
    };
  }

  toInApp(recipient: Recipient) {
    return {
      title: 'Order Shipped',
      content: `Order #${this.orderId} has left our warehouse.`,
    };
  }
}
```

---

### 4.3 Centralized Multi-Channel Dispatcher

```typescript
// src/notifications/notification.service.ts
import { Injectable, Logger } from '@nestjs/common';
import { InjectQueue } from '@nestjs/bullmq';
import { Queue } from 'bullmq';
import { Notification, Recipient } from './interfaces/notification.interface';

@Injectable()
export class NotificationService {
  private readonly logger = new Logger(NotificationService.name);

  constructor(@InjectQueue('notifications') private readonly queue: Queue) {}

  async send(recipient: Recipient, notification: Notification): Promise<void> {
    const channels = notification.via(recipient);

    this.logger.log(`Dispatching notification to user ${recipient.id} via channels: ${channels.join(', ')}`);

    for (const channel of channels) {
      switch (channel) {
        case 'email':
          if (notification.toEmail) {
            await this.queue.add('email-channel', {
              to: recipient.email,
              ...notification.toEmail(recipient),
            });
          }
          break;
        case 'sms':
          if (notification.toSms && recipient.phone) {
            await this.queue.add('sms-channel', {
              to: recipient.phone,
              ...notification.toSms(recipient),
            });
          }
          break;
        case 'in_app':
          if (notification.toInApp) {
            await this.queue.add('in-app-channel', {
              userId: recipient.id,
              ...notification.toInApp(recipient),
            });
          }
          break;
      }
    }
  }
}
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Push Notification Token Invalidation
When users uninstall an iOS/Android app, Firebase FCM marks their device token as `NotRegistered`. Subsequent push attempts throw 404 or 410 errors.
- **Handling**: Listen for FCM invalidation errors and delete stale tokens from the user database to prevent repeated failed requests.

### 5.2 SMS Character Limits and Splitting
SMS messages exceeding 160 characters (GSM-7) or 70 characters (Unicode) are segmented into multi-part messages, doubling or tripling delivery costs.
- **Rule**: Strictly truncate or validate SMS notification payloads before dispatch.

---

## 6. Performance & Memory Implications

- **Queue Isolation**: By offloading channel delivery to background queues, sending a notification across 4 channels executes in $< 15\text{ms}$ on the API thread.
- **Batching**: Mobile push notifications (FCM) can be sent in batches of up to 500 tokens per API request.

---

## 7. Anti-Patterns & Common Mistakes

- **Hardcoding Email/SMS Logic in Controllers**: Sending an email directly in the checkout controller, then adding an SMS, then adding push... resulting in spaghetti code. Centralize via the Notification pattern.
- **Ignoring User Opt-Outs**: Sending marketing notifications via SMS to users who unchecked SMS in their account settings, violating TCPA and GDPR compliance laws.

---

## 8. Production Checklist & Operational Readiness

- [ ] User notification preference checks enforced before dispatch.
- [ ] Push notification credentials (APNs / FCM) secured in environment variables.
- [ ] Stale FCM device tokens cleaned up automatically upon delivery failure.
- [ ] Background queue handles delivery retries independently per channel.

---

## 9. Migration & Version History

- **Standardization**: Adopts the Laravel-inspired Notification Channel Pattern for clean TypeScript architecture.

---

## 10. Verification & Testing

Unit testing notification channel dispatching:

```typescript
import { NotificationService } from './notification.service';
import { OrderShippedNotification } from '../orders/notifications/order-shipped.notification';

describe('NotificationService', () => {
  let service: NotificationService;
  let mockQueue: any;

  beforeEach(() => {
    mockQueue = { add: vi.fn().mockResolvedValue({}) };
    service = new NotificationService(mockQueue);
  });

  it('enqueues jobs only for channels enabled in recipient preferences', async () => {
    const recipient = {
      id: 'u1',
      email: 'alex@example.com',
      phone: '+15551234567',
      preferences: { channels: ['email' as any] }, // Only email enabled
    };

    const notification = new OrderShippedNotification('ord_100', 'TRACK123');
    await service.send(recipient, notification);

    expect(mockQueue.add).toHaveBeenCalledTimes(1);
    expect(mockQueue.add).toHaveBeenCalledWith('email-channel', expect.any(Object));
  });
});
```

---

## 11. Comprehensive Examples

### Firebase Cloud Messaging (FCM) Push Channel Processor

```typescript
// src/notifications/processors/push-channel.processor.ts
import { Processor, WorkerHost } from '@nestjs/bullmq';
import { Logger } from '@nestjs/common';
import { Job } from 'bullmq';
import * as admin from 'firebase-admin';

@Processor('notifications')
export class PushChannelProcessor extends WorkerHost {
  private readonly logger = new Logger(PushChannelProcessor.name);

  async process(job: Job<any>): Promise<void> {
    if (job.name !== 'push-channel') return;

    const { fcmToken, title, body } = job.data;
    try {
      await admin.messaging().send({
        token: fcmToken,
        notification: { title, body },
      });
      this.logger.log(`Push notification delivered to device: ${fcmToken.slice(0, 8)}...`);
    } catch (err: any) {
      this.logger.error(`Failed to send push notification: ${err.message}`);
    }
  }
}
```

---

## 12. Cross-References & Further Reading

- [03-email.md](./03-email.md) - Email delivery
- [05-advanced/04-realtime/01-gateways-and-events](../04-realtime/01-gateways-and-events.md) - Real-time in-app badges
- [Firebase Cloud Messaging Documentation](https://firebase.google.com/docs/cloud-messaging)
