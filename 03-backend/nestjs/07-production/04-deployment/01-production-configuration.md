---
title: "Production Configuration in NestJS"
description: "Managing twelve-factor environment configuration, Zod/Joi schema validation, and secrets loading in production NestJS apps"
tags: [nestjs, deployment, configuration, twelve-factor, zod, joi, env]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Production Configuration in NestJS

## Conceptual Overview
The Twelve-Factor App methodology mandates strict separation of config from code. In production NestJS applications, configuration must never be hardcoded into source files or compiled bundles.

Production configuration requires:
- Centralized configuration via `@nestjs/config`.
- Strict schema validation at boot time using **Zod** or **Joi** to fail fast if critical environment variables are missing or malformed.
- Immutable, strongly-typed configuration services.

---

## Implementation Details & NestJS APIs

### 1. Zod-Validated Configuration Module

```typescript
// src/config/env.schema.ts
import { z } from 'zod';

export const envSchema = z.object({
  NODE_ENV: z.enum(['development', 'production', 'test']).default('development'),
  PORT: z.coerce.number().default(3000),
  DATABASE_URL: z.string().url(),
  REDIS_URL: z.string().url(),
  JWT_SECRET: z.string().min(32),
  CORS_ALLOWED_ORIGINS: z.string().transform((val) => val.split(',')),
});

export type EnvConfig = z.infer<typeof envSchema>;
```

```typescript
// src/app.module.ts
import { Module } from '@nestjs/common';
import { ConfigModule } from '@nestjs/config';
import { envSchema } from './config/env.schema';

@Module({
  imports: [
    ConfigModule.forRoot({
      isGlobal: true,
      validate: (config) => {
        const parsed = envSchema.safeParse(config);
        if (!parsed.success) {
          console.error('Environment validation errors:', parsed.error.format());
          throw new Error('Invalid environment configuration');
        }
        return parsed.data;
      },
    }),
  ],
})
export class AppModule {}
```

---

## Key Takeaways & Cheat Sheet

- Validate environment variables at application startup using Zod or Joi schemas.
- Fail fast: if `DATABASE_URL` is missing, prevent the container from booting.
- Use `ConfigService` as a strongly-typed injectable singleton.

---

## Cross-References
- [06-secrets-management.md](../01-security/06-secrets-management.md) - Secrets management
- [03-docker.md](./03-docker.md) - Containerizing configuration
