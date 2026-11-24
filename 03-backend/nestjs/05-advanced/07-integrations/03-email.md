# Email Delivery Systems in NestJS

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 05-advanced / 07-integrations
- **Prerequisites**: [05-advanced/02-background-processing/02-bullmq](../02-background-processing/02-bullmq.md), [03-core-concepts/04-modules-and-di/01-dependency-injection](../../03-core-concepts/04-modules-and-di/01-dependency-injection.md)

---

## 1. Context & Purpose

Transactional email delivery is critical for user acquisition, identity verification, security alerts, and transactional invoices:
- Welcome emails and email verification magic links.
- Password reset tokens.
- Order confirmations and PDF receipts.
- Security notices (e.g. *"New login from unfamiliar location"*).

Sending emails synchronously inside HTTP route handlers causes severe latency spikes ($1–3\text{ seconds}$ per SMTP handshake). Furthermore, handling templating (HTML/CSS inlining, Handlebars/EJS rendering), tracking delivery status, and switching between local development transports (Mailpit / MailHog) and production email APIs (SendGrid, AWS SES, Resend, Postmark) requires a modular, queue-backed architecture.

---

## 2. Theoretical Foundations

### 2.1 The Asynchronous Email Pipeline
1. **Producer**: HTTP Controller validates input and enqueues an email job to BullMQ (`emailQueue.add('send-welcome', data)`). HTTP endpoint responds in $< 10\text{ms}$.
2. **Worker**: Background worker dequeues the job, loads the template, injects data variables, inlines CSS styles (critical for Outlook and Gmail rendering), and dispatches the payload to the email provider via SMTP or REST API.
3. **Provider**: Cloud provider (SendGrid, AWS SES) delivers the email and notifies the application of delivery status or bounces via webhooks.

---

## 3. Architecture & Internal Mechanics

```
┌────────────────────────────────────────────────────────┐
│                   HTTP Request Route                   │
│             AuthService.register(user)                 │
└───────────────────────────┬────────────────────────────┘
                            │
               Enqueues Job (< 5ms)
                            │
                            ▼
┌────────────────────────────────────────────────────────┐
│                   BullMQ Email Queue                   │
│                 `bull:email-delivery`                  │
└───────────────────────────┬────────────────────────────┘
                            │
              Worker Dequeues Job
                            │
                            ▼
┌────────────────────────────────────────────────────────┐
│                     Email Service                      │
│  1. Render Template: `templates/welcome.hbs`           │
│  2. Inline CSS (juice / nodemailer-html-to-text)       │
│  3. Dispatch via Transporter (Nodemailer / Resend)     │
└───────────────────────────┬────────────────────────────┘
                            │
              SMTP / HTTPS API (TLS Port 587 / 465)
                            │
                            ▼
               AWS SES / SendGrid / Mailpit
```

---

## 4. Concrete Implementation

### 4.1 Installing Dependencies

```bash
npm install --save nodemailer handlebars
npm install --save-dev @types/nodemailer
```

---

### 4.2 Building a Modular Mail Service

```typescript
// src/mail/mail.service.ts
import { Injectable, Logger } from '@nestjs/common';
import { ConfigService } from '@nestjs/config';
import * as nodemailer from 'nodemailer';
import * as handlebars from 'handlebars';
import * as fs from 'fs';
import * as path from 'path';

export interface SendMailOptions {
  to: string;
  subject: string;
  template: string;
  context: Record<string, any>;
}

@Injectable()
export class MailService {
  private readonly transporter: nodemailer.Transporter;
  private readonly logger = new Logger(MailService.name);
  private readonly compiledTemplates = new Map<string, handlebars.TemplateDelegate>();

  constructor(private readonly config: ConfigService) {
    this.transporter = nodemailer.createTransport({
      host: this.config.get<string>('SMTP_HOST', 'localhost'),
      port: this.config.get<number>('SMTP_PORT', 1025), // Default to Mailpit port 1025 for dev
      secure: this.config.get<boolean>('SMTP_SECURE', false),
      auth: {
        user: this.config.get<string>('SMTP_USER', ''),
        pass: this.config.get<string>('SMTP_PASS', ''),
      },
    });
  }

  async sendMail(options: SendMailOptions): Promise<void> {
    const html = this.renderTemplate(options.template, options.context);

    const fromAddress = this.config.get<string>('MAIL_FROM', '"Acme Platform" <no-reply@example.com>');

    const info = await this.transporter.sendMail({
      from: fromAddress,
      to: options.to,
      subject: options.subject,
      html,
    });

    this.logger.log(`Email dispatched to ${options.to}. MessageId: ${info.messageId}`);
  }

  private renderTemplate(templateName: string, context: Record<string, any>): string {
    if (!this.compiledTemplates.has(templateName)) {
      const templatePath = path.resolve(
        process.cwd(),
        'src/mail/templates',
        `${templateName}.hbs`,
      );
      const templateSource = fs.readFileSync(templatePath, 'utf8');
      this.compiledTemplates.set(templateName, handlebars.compile(templateSource));
    }

    const templateFn = this.compiledTemplates.get(templateName)!;
    return templateFn(context);
  }
}
```

---

### 4.3 Email Queue Worker Integration

Combine with BullMQ for asynchronous background processing:

```typescript
// src/mail/mail.consumer.ts
import { Processor, WorkerHost } from '@nestjs/bullmq';
import { Logger } from '@nestjs/common';
import { Job } from 'bullmq';
import { MailService, SendMailOptions } from './mail.service';

@Processor('mail')
export class MailConsumer extends WorkerHost {
  private readonly logger = new Logger(MailConsumer.name);

  constructor(private readonly mailService: MailService) {
    super();
  }

  async process(job: Job<SendMailOptions>): Promise<void> {
    this.logger.log(`Sending email job ${job.id} to: ${job.data.to}`);
    await this.mailService.sendMail(job.data);
  }
}
```

---

### 4.4 Handlebars Template Sample

```html
<!-- src/mail/templates/welcome.hbs -->
<!DOCTYPE html>
<html>
<head>
  <style>
    body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; background-color: #f9fafb; margin: 0; padding: 20px; }
    .card { background-color: #ffffff; border-radius: 8px; padding: 32px; max-width: 500px; margin: 0 auto; box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1); }
    .btn { display: inline-block; background-color: #4f46e5; color: #ffffff; text-decoration: none; padding: 12px 24px; border-radius: 6px; font-weight: 600; margin-top: 16px; }
  </style>
</head>
<body>
  <div class="card">
    <h2>Welcome to Acme, {{name}}!</h2>
    <p>Please click the button below to verify your email address and activate your account:</p>
    <a href="{{verifyUrl}}" class="btn">Verify Account</a>
    <p style="color: #6b7280; font-size: 14px; margin-top: 24px;">If you did not create an account, you can safely ignore this email.</p>
  </div>
</body>
</html>
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Email Deliverability and Bounces
Sending emails without proper DNS configuration causes major providers (Gmail, Outlook) to route messages directly to Spam or reject them completely.
- **Remedy**: Always configure **SPF** (`v=spf1 include:...`), **DKIM** (public key in DNS), and **DMARC** records on your sending domain.

### 5.2 SMTP Server Connection Leaks
Creating a new `nodemailer.createTransport()` instance inside every function call exhausts socket limits.
- **Handling**: Instantiate the transporter **once** as a singleton in `MailService` constructor and reuse it.

---

## 6. Performance & Memory Implications

- **Template Caching**: Compiling Handlebars templates into memory functions on the first load avoids repeated synchronous disk reads on every email.
- **Asynchronous Decoupling**: Queueing email jobs keeps HTTP response times under 10ms, completely decoupled from SMTP network roundtrips.

---

## 7. Anti-Patterns & Common Mistakes

- **Sending Emails Synchronously in Route Handlers**: Calling `await mailService.send()` inside the register endpoint. If SMTP times out, the user experiences a 504 Gateway Timeout despite their account being created.
- **Using External Public CSS Stylesheets**: Linking `<link rel="stylesheet">` in email HTML. Email clients (especially Outlook and Gmail) strip external stylesheets; all styles must be inline `<style>` or inlined attributes.

---

## 8. Production Checklist & Operational Readiness

- [ ] SPF, DKIM, and DMARC DNS records verified for production domain.
- [ ] Email dispatch offloaded to background BullMQ queue.
- [ ] Local development utilizes Mailpit or MailHog to capture emails locally.
- [ ] Bounce and complaint webhooks configured from cloud provider (AWS SES / SendGrid).

---

## 9. Migration & Version History

- **Nodemailer 6.x**: De facto standard Node.js email transport library.

---

## 10. Verification & Testing

Unit testing email dispatch:

```typescript
import { MailService } from './mail.service';

describe('MailService', () => {
  let service: MailService;
  let mockTransporter: any;

  beforeEach(() => {
    mockTransporter = {
      sendMail: vi.fn().mockResolvedValue({ messageId: 'msg_123' }),
    };

    const mockConfig: any = {
      get: vi.fn((key: string, defaultVal: any) => defaultVal),
    };

    service = new MailService(mockConfig);
    (service as any).transporter = mockTransporter;
    (service as any).renderTemplate = vi.fn().mockReturnValue('<html>Test</html>');
  });

  it('dispatches email with rendered HTML', async () => {
    await service.sendMail({
      to: 'user@example.com',
      subject: 'Welcome',
      template: 'welcome',
      context: { name: 'User' },
    });

    expect(mockTransporter.sendMail).toHaveBeenCalledWith(
      expect.objectContaining({
        to: 'user@example.com',
        subject: 'Welcome',
        html: '<html>Test</html>',
      }),
    );
  });
});
```

---

## 11. Comprehensive Examples

### Local Mailpit Docker Compose Setup

```yaml
# docker-compose.mail.yml
services:
  mailpit:
    image: axllent/mailpit:latest
    container_name: dev-mailpit
    restart: unless-stopped
    ports:
      - "1025:1025" # SMTP server
      - "8025:8025" # Web UI dashboard
```

Inspect sent emails locally at `http://localhost:8025`.

---

## 12. Cross-References & Further Reading

- [05-advanced/02-background-processing/02-bullmq](../02-background-processing/02-bullmq.md) - BullMQ queues
- [04-notifications.md](./04-notifications.md) - Multi-channel notifications
- [Nodemailer Documentation](https://nodemailer.com)
