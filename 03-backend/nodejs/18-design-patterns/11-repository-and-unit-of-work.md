# Repository & Unit of Work Patterns in Node.js

> Decoupling data access logic from business rules using the Repository pattern and coordinating atomic multi-entity mutations with Unit of Work.

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Patterns** | Repository Pattern, Unit of Work |

---

## 1. The Repository Pattern

A **Repository** mediates between the domain model and data mapping layers:
- Exposes a collection-like interface (`find()`, `save()`, `delete()`).
- Encapsulates low-level database dialect details (SQL syntax, Prisma, MongoDB queries).
- Allows domain logic to be tested in memory by substituting a fake repository.

```typescript
export interface UserRepository {
  findById(id: string): Promise<User | null>;
  findByEmail(email: string): Promise<User | null>;
  save(user: User): Promise<void>;
}
```

---

## 2. The Unit of Work Pattern

When a business operation involves modifying multiple entities across different repositories (e.g. creating an Order, updating Inventory, and deducting User Credit), each repository must share the **same database transaction**:

```typescript
export class UnitOfWork {
  private client: pg.PoolClient | null = null;

  async start() {
    this.client = await pool.connect();
    await this.client.query('BEGIN');
  }

  async commit() {
    await this.client?.query('COMMIT');
    this.release();
  }

  async rollback() {
    await this.client?.query('ROLLBACK');
    this.release();
  }

  private release() {
    this.client?.release();
    this.client = null;
  }

  // Factory methods providing repositories bound to the active transaction client:
  get orders(): OrderRepository {
    return new PostgresOrderRepository(this.client!);
  }

  get inventory(): InventoryRepository {
    return new PostgresInventoryRepository(this.client!);
  }
}
```
Usage:
```typescript
const uow = new UnitOfWork();
await uow.start();
try {
  await uow.orders.save(order);
  await uow.inventory.decrement(itemId, quantity);
  await uow.commit();
} catch (err) {
  await uow.rollback();
  throw err;
}
```
Guarantees atomicity across multiple repository operations.
