# Drizzle & Kysely Typed SQL Cheat Sheet

```typescript
// Kysely Query in NestJS:
const user = await db
  .selectFrom('users')
  .selectAll()
  .where('email', '=', email)
  .executeTakeFirst();
```
