# Project 05: High-Performance URL Shortener (`05-url-shortener`)

> Ultra-low-latency URL shortening engine built with NestFastifyApplication (Fastify 5), Redis caching, Base62 encoding, and click telemetry.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-nest-05-url-shortener` |
| **Platform** | Fastify 5 (`@nestjs/platform-fastify`) |
| **Database & Cache** | Redis 7 + PostgreSQL 16 |
| **Target Throughput** | > 50,000 requests/sec |
| **Complexity Level** | Performance Specialized Reference |

---

## 1. Fastify 5 High-Speed Redirect Controller

```typescript
// src/redirect/redirect.controller.ts
import { Controller, Get, Param, Res, NotFoundException } from '@nestjs/common';
import { FastifyReply } from 'fastify';
import { UrlService } from '../url/url.service';

@Controller()
export class RedirectController {
  constructor(private readonly urlService: UrlService) {}

  @Get(':code')
  async redirect(@Param('code') code: string, @Res() reply: FastifyReply) {
    const originalUrl = await this.urlService.resolveUrl(code);
    if (!originalUrl) {
      throw new NotFoundException('Short code does not exist');
    }

    // 301 Permanent Redirect to destination URL
    reply.status(301).redirect(originalUrl);
  }
}
```
