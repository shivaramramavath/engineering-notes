# Cheatsheet 07: PostgreSQL with Node.js (`pg` & Drizzle ORM)

> Production reference card for PostgreSQL database integration using raw `pg` pools and type-safe Drizzle ORM in Node.js 22 LTS.

---

### 1. `pg.Pool` Connection & Parameterized Queries
Always use parameterized `$1, $2` placeholders to guarantee immunity against SQL injection:
```javascript
import pg from 'pg';
const { Pool } = pg;

export const pool = new Pool({
  connectionString: process.env.DATABASE_URL,
  max: 20,                       // Maximum pooled sockets
  idleTimeoutMillis: 30000,
  connectionTimeoutMillis: 2000,
});

// Parameterized Query
const { rows } = await pool.query(
  'SELECT id, email, created_at FROM users WHERE email = $1 AND is_active = $2',
  ['user@example.com', true]
);
```

---

### 2. Manual ACID Transactions (`pg`)
```javascript
const client = await pool.connect();
try {
  await client.query('BEGIN');

  await client.query('UPDATE accounts SET balance = balance - $1 WHERE id = $2', [100, fromId]);
  await client.query('UPDATE accounts SET balance = balance + $1 WHERE id = $2', [100, toId]);

  await client.query('COMMIT');
} catch (err) {
  await client.query('ROLLBACK');
  throw err;
} finally {
  client.release(); // Always release connection back to pool!
}
```

---

### 3. Drizzle ORM Schema Definition
```typescript
import { pgTable, uuid, varchar, integer, timestamp, index } from 'drizzle-orm/pg-core';
import { sql } from 'drizzle-orm';

export const users = pgTable('users', {
  id: uuid('id').primaryKey().defaultRandom(),
  email: varchar('email', { length: 255 }).notNull().unique(),
  name: varchar('name', { length: 100 }).notNull(),
  createdAt: timestamp('created_at', { withTimezone: true }).default(sql`CURRENT_TIMESTAMP`).notNull(),
}, (table) => [
  index('idx_users_email').on(table.email),
]);
```

---

### 4. Drizzle ORM Queries
```typescript
import { drizzle } from 'drizzle-orm/node-postgres';
import { eq, desc } from 'drizzle-orm';
import { users } from './schema.js';

export const db = drizzle(pool);

// Insert
const [newUser] = await db.insert(users).values({
  email: 'grace@example.com',
  name: 'Grace Hopper',
}).returning();

// Select
const user = await db.select().from(users).where(eq(users.id, userId));
const allUsers = await db.select().from(users).orderBy(desc(users.createdAt)).limit(50);

// Update
const [updated] = await db.update(users)
  .set({ name: 'Admiral Grace Hopper' })
  .where(eq(users.id, userId))
  .returning();

// Drizzle Transaction
await db.transaction(async (tx) => {
  await tx.insert(users).values({ email: 'a@ex.com', name: 'A' });
  await tx.insert(users).values({ email: 'b@ex.com', name: 'B' });
});
```
