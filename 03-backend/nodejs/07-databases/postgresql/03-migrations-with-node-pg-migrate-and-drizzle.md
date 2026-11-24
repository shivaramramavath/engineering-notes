# Database Migrations: `node-pg-migrate` & Drizzle Kit

> Managing repeatable schema evolutions, migration rollbacks, automated CI/CD checks, and zero-downtime column migrations.

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Tools** | `node-pg-migrate`, `drizzle-kit` |
| **Target Database** | PostgreSQL 16+ |

---

## 1. Why Migration Tooling is Essential

Never execute ad-hoc `ALTER TABLE` commands manually in production.
- **Repeatability**: Schema changes must be tracked in Git and applied deterministically across local, staging, and production databases.
- **Rollback Safety**: Every migration script should define both an `up` and a `down` operation.
- **Lock Management**: Migrations must avoid exclusive table locks (`ACCESS EXCLUSIVE`) that block incoming user traffic.

---

## 2. Using `node-pg-migrate`

### Creating a Migration
```bash
npx node-pg-migrate create add-users-table
```

### Migration File (`migrations/1700000000000_add-users-table.cjs`)
```javascript
exports.up = (pgm) => {
  pgm.createTable('users', {
    id: 'id', // Auto-incrementing primary key
    email: { type: 'varchar(255)', notNull: true, unique: true },
    password_hash: { type: 'varchar(255)', notNull: true },
    created_at: {
      type: 'timestamp',
      notNull: true,
      default: pgm.func('current_timestamp'),
    },
  });

  pgm.createIndex('users', 'email');
};

exports.down = (pgm) => {
  pgm.dropTable('users');
};
```

### Running Migrations in CI/CD
```bash
npx node-pg-migrate up
```

---

## 3. Schema-Driven Migrations with Drizzle Kit

With Drizzle ORM, you declare your schema in pure TypeScript (`schema.ts`):

```typescript
import { pgTable, serial, varchar, timestamp } from 'drizzle-orm/pg-core';

export const users = pgTable('users', {
  id: serial('id').primaryKey(),
  email: varchar('email', { length: 255 }).notNull().unique(),
  createdAt: timestamp('created_at').defaultNow().notNull(),
});
```

Generate SQL migration files automatically:
```bash
# Inspects schema.ts and generates timestamped SQL diffs in migrations/
npx drizzle-kit generate

# Applies migrations against production database
npx drizzle-kit migrate
```
