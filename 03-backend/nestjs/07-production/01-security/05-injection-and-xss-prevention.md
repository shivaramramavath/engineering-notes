---
title: "Injection and XSS Prevention in NestJS"
description: "Hardening NestJS applications against SQL injection, NoSQL injection, command execution, and Cross-Site Scripting (XSS)"
tags: [nestjs, security, sql-injection, xss, sanitization, validation]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Injection and XSS Prevention in NestJS

## Conceptual Overview
Injection attacks occur when untrusted user input is concatenated into interpreters (SQL engines, shell execution, HTML templates) without parameterization. Cross-Site Scripting (XSS) allows attackers to execute arbitrary JavaScript in victim browsers.

In NestJS:
- **SQL / NoSQL Injection** is prevented by using parameterized query builders (TypeORM, Prisma, Kysely, Mongoose) and avoiding raw string concatenation.
- **XSS** is prevented by context-aware HTML escaping, DOMPurify sanitization, and output encoding.

---

## Core Mechanisms & Architecture

### 1. Parameterized Queries vs String Concatenation

```typescript
// VULNERABLE TO SQL INJECTION:
const users = await dataSource.query(`SELECT * FROM users WHERE email = '${email}'`);

// SECURE: Parameterized Query
const users = await dataSource.query(`SELECT * FROM users WHERE email = $1`, [email]);
```

### 2. Input Sanitization with Custom Pipes

```typescript
// src/common/pipes/sanitize-html.pipe.ts
import { PipeTransform, Injectable } from '@nestjs/common';
import sanitizeHtml from 'sanitize-html';

@Injectable()
export class SanitizeHtmlPipe implements PipeTransform {
  transform(value: any) {
    if (typeof value === 'string') {
      return sanitizeHtml(value, {
        allowedTags: ['b', 'i', 'em', 'strong', 'a'],
        allowedAttributes: { a: ['href'] },
      });
    }
    return value;
  }
}
```

---

## Key Takeaways & Cheat Sheet

- Never concatenate variables into raw SQL or NoSQL queries; always use parameterized bindings.
- Use `ValidationPipe({ whitelist: true })` to prevent mass-assignment attacks.
- Sanitize rich text inputs using libraries like `sanitize-html`.

---

## Cross-References
- [01-security-fundamentals.md](./01-security-fundamentals.md) - Core security foundations
- [02-database-foundations](../../04-intermediate/02-database-foundations/README.md) - ORM integration
