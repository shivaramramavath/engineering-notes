---
title: "Soft Delete Pattern in NestJS"
description: "Implementing soft deletion with TypeORM, Prisma, and PostgreSQL: deleted_at columns, unique constraint conflicts, and cascading"
tags: [nestjs, patterns, soft-delete, typeorm, prisma, postgresql]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Soft Delete Pattern in NestJS

## Conceptual Overview
Hard deletion (`DELETE FROM table WHERE id = $1`) permanently purges records, breaking foreign key relationships, audit histories, and recovery capabilities. The **Soft Delete Pattern** flags records as inactive using a timestamp column (`deleted_at: TIMESTAMPTZ | null`).

---

## The Partial Unique Index Solution
A major challenge with soft delete is unique constraints. If a user deletes `test@example.com`, they cannot re-register with that email if `UNIQUE(email)` exists.

**PostgreSQL Partial Index Fix**:
```sql
-- Enforce uniqueness ONLY on active (non-deleted) records:
CREATE UNIQUE INDEX idx_users_email_active 
ON users (email) 
WHERE deleted_at IS NULL;
```

---

## Implementation Details with TypeORM

```typescript
@Entity('users')
export class UserEntity {
  @PrimaryGeneratedColumn('uuid')
  id: string;

  @Column()
  email: string;

  @DeleteDateColumn({ name: 'deleted_at', nullable: true })
  deletedAt: Date | null;
}

// In repository:
await userRepo.softDelete(id); // Sets deleted_at = NOW()
const user = await userRepo.findOneBy({ id }); // Automatically filters WHERE deleted_at IS NULL
```

---

## Key Takeaways & Cheat Sheet

- Use `deleted_at: Date | null` instead of boolean `is_deleted` to preserve audit timing.
- Use PostgreSQL partial unique indexes (`WHERE deleted_at IS NULL`) to avoid re-registration conflicts.
- Filter soft-deleted records globally using ORM interceptors or extensions.

---

## Cross-References
- [01-audit-logging.md](./01-audit-logging.md) - Audit logging
- [02-database-foundations](../../04-intermediate/02-database-foundations/README.md) - Database configuration
