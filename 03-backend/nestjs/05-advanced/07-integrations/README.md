# 05-07 — Third-Party Integrations and External Services

Enterprise applications constantly interface with external services: cloud object storage, email gateways, multi-channel notifications, and third-party SaaS APIs.

This module provides production-grade architectural blueprints for integrating external services cleanly into NestJS: from multipart file uploads to direct **AWS S3 / Cloudflare R2** pre-signed uploads, templated transactional **email**, multi-channel **notification engines**, and resilient **HTTP API clients** featuring exponential backoff retries and circuit breakers.

---

## Module Index

| # | File | Core Topics |
|---|---|---|
| 01 | [File Uploads](./01-file-upload.md) | Multer, `FileInterceptor`, `ParseFilePipe`, magic-byte MIME validation, disk vs memory |
| 02 | [Cloud Storage](./02-cloud-storage.md) | AWS SDK v3, AWS S3, Cloudflare R2, pre-signed upload/download URLs, S3 CORS |
| 03 | [Email Delivery](./03-email.md) | Nodemailer, Handlebars templating, CSS inlining, Mailpit dev setup, queue offloading |
| 04 | [Notifications](./04-notifications.md) | Unified Notification Channel pattern, SMS (Twilio), Push (FCM), user preferences |
| 05 | [HTTP Client & APIs](./05-http-client-and-third-party-apis.md) | `@nestjs/axios`, keep-alive agents, exponential backoff with jitter, error normalization |

---

## Key Learning Outcomes

1. Secure multipart uploads against spoofing using binary magic-byte inspection.
2. Eliminate upload proxy bottlenecks using S3 pre-signed direct upload URLs.
3. Decouple transactional emails and SMS alerts from HTTP endpoints via queues.
4. Build modular, preference-aware notification engines routing across multiple channels.
5. Create resilient HTTP client wrappers capable of withstanding transient third-party API outages.
