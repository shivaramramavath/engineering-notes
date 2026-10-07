# Transactions

TypeORM offers three ways to run work in a transaction: the **`DataSource.transaction()` callback** (use this by default), a manual **`QueryRunner`** (for fine control), and **locking options** for concurrent updates. The theory (ACID, isolation levels, why transactions matter, keeping them short) is in [Database foundations: transactions](../02-database-foundations/04-transactions.md); this note is the TypeORM mechanics.

Prerequisites: [Repositories](./04-repositories.md), [database transactions concepts](../02-database-foundations/04-transactions.md).

## The default: `dataSource.transaction`

```ts
@Injectable()
export class TransfersService {
  constructor(@InjectDataSource() private readonly dataSource: DataSource) {}

  async transfer(fromId: string, toId: string, amount: number) {
    await this.dataSource.transaction(async (manager) => {
      const debited = await manager
        .createQueryBuilder()
        .update(Account)
        .set({ balance: () => 'balance - :amount' })
        .where('id = :fromId AND balance >= :amount', { fromId, amount })
        .execute();

      if (!debited.affected) throw new BadRequestException('Insufficient funds');   // rolls back

      await manager.increment(Account, { id: toId }, 'balance', amount);
    });
  }
}
```

How it works:

- TypeORM opens a transaction, calls your callback with a transaction-bound **`EntityManager`**, **commits** if the callback resolves, and **rolls back** if it throws.
- **Use `manager` for every operation inside.** An injected `Repository<Account>` uses the default connection, so its queries run **outside** the transaction (and can even deadlock against it).
- The callback's return value is returned by `transaction()`.

### Repositories inside a transaction

```ts
await this.dataSource.transaction(async (manager) => {
  const orders = manager.getRepository(Order);
  const items = manager.getRepository(OrderItem);

  const order = await orders.save(orders.create({ userId, total }));
  await items.save(lines.map((l) => items.create({ ...l, orderId: order.id })));
});
```

`manager.save(Entity, data)`, `manager.find(...)`, `manager.update(...)`, and so on all work the same as on repositories.

### Isolation level

```ts
await this.dataSource.transaction('SERIALIZABLE', async (manager) => { /* ... */ });
```

Accepts `'READ UNCOMMITTED'`, `'READ COMMITTED'`, `'REPEATABLE READ'`, `'SERIALIZABLE'` (support varies by database). Stricter levels can raise serialization failures you must retry ([errors and retries](../02-database-foundations/07-database-errors.md)).

## Passing the manager to your own repositories

If you wrapped repositories in your own classes ([repository pattern](../02-database-foundations/03-repository-pattern.md)), they must accept the transaction's manager:

```ts
@Injectable()
export class AccountsRepository {
  constructor(@InjectRepository(Account) private readonly repo: Repository<Account>) {}

  private r(manager?: EntityManager) {
    return manager ? manager.getRepository(Account) : this.repo;
  }

  debit(id: string, amount: number, manager?: EntityManager) {
    return this.r(manager).decrement({ id }, 'balance', amount);
  }
}

// service
await this.dataSource.transaction(async (manager) => {
  await this.accounts.debit(fromId, amount, manager);
  await this.accounts.credit(toId, amount, manager);
});
```

This explicit parameter is simple and obvious. Community libraries can provide an ambient (`AsyncLocalStorage`-based) transaction context so you don't thread `manager` through every call; they add implicit behavior and their own caveats (nested transactions, propagation), so evaluate them before adopting. TypeORM's old `@Transaction()`/`@TransactionManager()` decorators were removed in 0.3.

## `QueryRunner`: manual control

Use it when you need to commit/rollback explicitly, run steps conditionally, or do work between phases.

```ts
const queryRunner = this.dataSource.createQueryRunner();
await queryRunner.connect();
await queryRunner.startTransaction();            // optionally: startTransaction('SERIALIZABLE')

try {
  await queryRunner.manager.save(order);
  await queryRunner.manager.save(items);
  await queryRunner.commitTransaction();
} catch (err) {
  await queryRunner.rollbackTransaction();
  throw err;
} finally {
  await queryRunner.release();                   // ALWAYS: returns the connection to the pool
}
```

Rules:

- `release()` goes in `finally`. A missed release leaks a pooled connection; after enough leaks the whole app hangs waiting for connections ([connection pool](../02-database-foundations/02-database-connection.md)).
- Roll back in `catch`, rethrow the error.
- Use `queryRunner.manager` for all operations.
- The `dataSource.transaction()` callback does all of this for you; prefer it unless you need the extra control.

## Locking concurrent updates

Transactions alone don't stop two requests from reading the same row and overwriting each other (**lost update**). Choose one of:

### 1. Atomic update (best when it fits)

```ts
await accounts.decrement({ id }, 'balance', amount);
// or: UPDATE ... SET balance = balance - :amount WHERE id = :id AND balance >= :amount
```

No read, no race, no lock to manage. Check `affected` for "insufficient funds".

### 2. Pessimistic locking (`SELECT ... FOR UPDATE`)

Locks the selected rows until the transaction ends; other transactions wanting them wait.

```ts
await this.dataSource.transaction(async (manager) => {
  const account = await manager.findOne(Account, {
    where: { id },
    lock: { mode: 'pessimistic_write' },         // must be inside a transaction
  });
  if (!account || account.balance < amount) throw new BadRequestException('Insufficient funds');

  account.balance -= amount;
  await manager.save(account);
});
```

Modes: `pessimistic_read`, `pessimistic_write`, `pessimistic_partial_write` (skip locked rows; useful for job queues), `pessimistic_write_or_fail`, plus others that depend on the database. With QueryBuilder: `.setLock('pessimistic_write')`. Locks require an active transaction; outside one TypeORM throws. Lock rows in a **consistent order** across code paths to avoid deadlocks.

### 3. Optimistic locking (`@VersionColumn`)

```ts
@Entity()
export class Document {
  @VersionColumn()
  version: number;
}
```

`@VersionColumn` is incremented automatically on each `save`. **That alone does not detect conflicts.** To enforce it, pass the version you originally read when loading with an optimistic lock:

```ts
const doc = await manager.findOne(Document, {
  where: { id },
  lock: { mode: 'optimistic', version: expectedVersion },   // throws OptimisticLockVersionMismatchError if different
});
```

In a typical API the client sends back the version it saw (an `If-Match` header or a body field), and you compare. Catch `OptimisticLockVersionMismatchError` and return `409 Conflict`, or retry. Optimistic locking suits low-contention updates; pessimistic suits high contention.

## Side effects and transaction boundaries

- **Keep transactions short** and database-only. No HTTP calls, emails, or queue publishes inside the callback.
- Do side effects **after** the transaction resolves. For a reliable "write + publish event", use the [transactional outbox](../../08-architecture-and-patterns/04-real-world-patterns/03-transactional-outbox.md): insert the event row inside the transaction and publish it afterward.
- Put the transaction boundary in the **service method** representing the business operation, not in controllers or individual repository methods.

## Nested transactions

Calling `dataSource.transaction()` (or `startTransaction()`) while already inside one doesn't create an independent transaction on the same connection; behavior can involve savepoints or simply joining the outer one depending on how you call it. Avoid relying on nesting semantics: pass the **outer `manager`** to inner functions instead of starting new transactions. If you truly need savepoint-style partial rollback, test the exact behavior in your TypeORM version.

## Retrying

Deadlocks and serialization failures are expected under contention. Retry the **whole** `transaction()` call a bounded number of times with backoff, only when the operation is safe to repeat ([database errors](../02-database-foundations/07-database-errors.md)).

## Testing

[Integration tests](../01-testing/05-integration-testing.md) should verify rollback: make a step throw halfway, then assert that earlier writes are absent. A mocked repository can't prove that.

```ts
await expect(service.transfer(a, b, 50_000)).rejects.toThrow();
expect((await accounts.findOneByOrFail({ id: a })).balance).toBe(initialBalance);
```

## Common mistakes

- **Using injected repositories instead of `manager`** inside the transaction callback (work happens outside it).
- **Forgetting `queryRunner.release()`**, leaking connections.
- **Read-modify-write without a lock or atomic update.**
- **Assuming `@VersionColumn` alone prevents lost updates.**
- **`pessimistic_*` locks outside a transaction.**
- **External calls inside the transaction.**
- **Swallowing the error in `catch`** so the transaction commits partial work or the caller thinks it succeeded.
- **Relying on nested `transaction()` calls** instead of passing the manager down.
- **Locking rows in inconsistent order**, causing deadlocks.
- **Wrapping single statements** in transactions "to be safe".

## Debugging

- Partial data after a failure: some writes used a repository/default connection instead of `manager`/`queryRunner.manager`.
- App hangs or "timeout acquiring connection": leaked `QueryRunner` or long transactions; check for missing `finally { release() }`.
- `TransactionNotStartedError` / lock errors: pessimistic lock used outside a transaction.
- Deadlock detected: standardize lock ordering, shorten transactions, add retries.
- Enable `logging: ['query']` to see `START TRANSACTION`, statements, and `COMMIT`/`ROLLBACK` in order.
- In PostgreSQL, inspect `pg_stat_activity` for sessions stuck "idle in transaction".

## Quick Summary

- Default to `dataSource.transaction(async (manager) => ...)`; throwing rolls back; **use `manager` for everything inside**.
- Use `QueryRunner` for manual control and always `release()` in `finally`.
- Prevent lost updates with atomic updates first, then pessimistic locks (inside a transaction), or optimistic locking via `lock: { mode: 'optimistic', version }`.
- Pass the `manager` through your own repositories; avoid relying on nested transaction semantics.
- Keep transactions short and database-only; do side effects after commit (outbox for reliability); verify rollback in integration tests.

## Next

Section complete. Continue with [Prisma](../04-prisma/README.md) or [Mongoose](../05-mongoose/README.md) to compare approaches, or revisit the [ORM comparison](../02-database-foundations/08-orm-comparison.md).

← Back to [TypeORM overview](./README.md)
