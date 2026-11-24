# Configuration

Configuration is everything that changes **between environments but not between releases**: database URLs, ports, API keys, feature toggles, timeouts. The goal is simple: the same build artifact runs anywhere, and the environment decides how it behaves.

```text
  .env files / process.env / secret store
              │
              ▼
   ConfigModule.forRoot()   ── validate ──►  fail fast at startup if invalid
              │
              ▼
   registerAs() namespaces   (optional: typed, grouped config)
              │
              ▼
   ConfigService.get(...)  /  @Inject(dbConfig.KEY)
              │
              ▼
   modules, services, forRootAsync factories
```

> Applies to NestJS 10/11 with `@nestjs/config`.

## Reading order

| # | Note | Answers |
|---|------|---------|
| 01 | [Configuration basics](./01-configuration-basics.md) | `ConfigModule`, `ConfigService`, `.env` files, precedence, the `process.env` timing trap |
| 02 | [Configuration validation](./02-configuration-validation.md) | Failing fast with Joi, class-validator, or Zod; type conversion |
| 03 | [Custom configuration](./03-custom-configuration.md) | `registerAs`, namespaces, typed injection, `forFeature`, YAML/remote sources |

## Prerequisites

- [Modules](../../02-fundamentals/02-modules.md) and [dependency injection](../../02-fundamentals/05-dependency-injection.md)
- [Dynamic modules](../04-modules-and-di/03-dynamic-modules.md) (helps explain `forRoot` / `forRootAsync`)

## Related

- [Global modules](../04-modules-and-di/02-global-modules.md)
- [Secrets management](../../07-production/01-security/06-secrets-management.md)
- [Production configuration](../../07-production/04-deployment/01-production-configuration.md)
- [Configuration quick reference](../../13-quick-reference/14-configuration.md)
