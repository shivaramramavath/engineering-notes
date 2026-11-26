# Unit of Work

**Intent:** maintain a list of objects affected by a business transaction, coordinate writing out the changes, and resolve concurrency problems, so all changes are saved together or not at all.

A use case often changes several objects (an order, its items, the stock levels, a payment record). The unit of work **tracks** all of those changes and **commits them as one transaction** at the end.

## The problem

Without coordination, each step saves immediately:

```java
orders.save(order);               // succeeds
inventory.decrease(item, qty);    // succeeds
payments.save(payment);           // fails!  ← order and stock are already saved
```

The system is left in an inconsistent state. You'd also issue many small database round trips instead of one batch.

## Core idea

```text
Start unit of work
   ├─ load objects        → tracked
   ├─ modify objects      → marked dirty
   ├─ add new objects     → marked new
   └─ remove objects      → marked removed
Commit
   └─ write all inserts, updates and deletes in ONE transaction
      (or roll back everything on failure)
```

## Structure

```text
Service ──► UnitOfWork
              + registerNew(obj)
              + registerDirty(obj)
              + registerRemoved(obj)
              + commit()
              + rollback()
                  │
                  ▼
           Repositories / Data mappers ──► Database (one transaction)
```

## A hand-rolled illustration

```java
public interface UnitOfWork extends AutoCloseable {
    void registerNew(Object entity);
    void registerDirty(Object entity);
    void registerRemoved(Object entity);
    void commit();
    void rollback();
    @Override void close();
}

public class JdbcUnitOfWork implements UnitOfWork {
    private final Connection connection;
    private final List<Object> newEntities = new ArrayList<>();
    private final List<Object> dirtyEntities = new ArrayList<>();
    private final List<Object> removedEntities = new ArrayList<>();

    public JdbcUnitOfWork(DataSource ds) throws SQLException {
        this.connection = ds.getConnection();
        this.connection.setAutoCommit(false);              // start a transaction
    }

    public void registerNew(Object e)     { newEntities.add(e); }
    public void registerDirty(Object e)   { if (!newEntities.contains(e)) dirtyEntities.add(e); }
    public void registerRemoved(Object e) { newEntities.remove(e); dirtyEntities.remove(e); removedEntities.add(e); }

    public void commit() {
        try {
            newEntities.forEach(this::insert);
            dirtyEntities.forEach(this::update);
            removedEntities.forEach(this::delete);
            connection.commit();
        } catch (SQLException ex) {
            rollback();
            throw new PersistenceException(ex);
        }
    }

    public void rollback() {
        try { connection.rollback(); } catch (SQLException ignored) { }
    }

    public void close() {
        try { connection.close(); } catch (SQLException ignored) { }
    }

    private void insert(Object e) { /* INSERT ... using the connection */ }
    private void update(Object e) { /* UPDATE ... */ }
    private void delete(Object e) { /* DELETE ... */ }
}
```

Usage:

```java
try (UnitOfWork uow = new JdbcUnitOfWork(dataSource)) {
    Order order = Order.create(customerId, items);
    uow.registerNew(order);

    Stock stock = stockRepository.find(sku);
    stock.decrease(qty);
    uow.registerDirty(stock);

    uow.commit();                  // both changes saved together, or neither
}
```

In real systems you rarely write this yourself. The point is to understand what your ORM or framework does for you.

## You probably already use one

| Technology | Its unit of work |
|---|---|
| JPA / Hibernate | `EntityManager` / `Session`: tracks loaded entities and flushes changes on commit ("dirty checking") |
| Spring | `@Transactional` defines the boundary within which the persistence context lives |
| Plain JDBC | `Connection` with `setAutoCommit(false)`, then `commit()` / `rollback()` |
| Entity Framework (.NET) | `DbContext` and `SaveChanges()` |

With JPA you often don't call `save` on modified entities at all:

```java
@Transactional
public void rename(long id, String newName) {
    Customer c = em.find(Customer.class, id);
    c.rename(newName);            // modified in memory
}                                 // at commit, the change is detected and written
```

## Transaction boundary placement

The unit of work should wrap **one use case**, which is why it's normally set at the [service layer](./04-service-layer.md), not in repositories or controllers.

```text
Controller → Service.placeOrder() [begin UoW]
                 ├─ orderRepository.save(...)
                 ├─ stockRepository ...
                 └─ paymentRepository ...
             [commit]  ← all or nothing
```

Repositories participate in the current unit of work; they don't commit on their own.

## Related concerns

| Concern | Detail |
|---|---|
| **Identity map** | Within a unit of work, loading the same row twice returns the same object, avoiding duplicates and conflicting edits |
| **Optimistic locking** | A version field detects that someone else changed the data since you read it, and commit fails instead of overwriting |
| **Batching** | Combining many inserts/updates into fewer round trips |
| **Flush order** | Deletes, inserts and updates must respect foreign-key constraints |

## When to use it

- A use case modifies several related objects that must stay consistent.
- You want fewer, batched database operations.
- You want explicit, testable transaction boundaries.

You usually get it from the ORM or framework, so the decision is more "where do I place the boundary" than "should I write one".

## Common mistakes

- **Committing inside repositories**, so each call is its own transaction and consistency is lost.
- **Very long-lived units of work** (for example, held across user think time), which hold locks and grow memory.
- **Transactions that include slow external calls** (HTTP, email), holding database locks while waiting. Do those outside, or after commit.
- **Forgetting to roll back** on exceptions, or swallowing errors so partial changes are committed.
- **Self-invocation with proxy-based `@Transactional`**: a call inside the same class bypasses the proxy, so no transaction starts (see [Proxy](../05-structural-patterns/07-proxy.md)).
- **Lazy-loading outside the session** (`LazyInitializationException`). Fetch what you need inside the boundary.
- **Assuming "save" writes immediately.** With an ORM, writes happen at flush or commit.

## Debugging

| Symptom | Likely cause |
|---|---|
| Changes not persisted | No active transaction, or the object isn't managed |
| Partial data saved | Multiple transactions where one use case was expected |
| `OptimisticLockException` | Concurrent modification; retry or reload |
| Unexpected extra SQL | Dirty checking flushing, or N+1 loads inside the unit |

## Quick Summary

- A unit of work tracks changes during a use case and commits them atomically.
- ORMs and `@Transactional` already provide it; you mainly decide the boundary.
- Place it at the service layer, one per use case, and keep it short.
- Avoid external calls inside it, and be careful with lazy loading and proxies.

## Related

[Repository](./01-repository.md) · [Service Layer](./04-service-layer.md) · [Proxy](../05-structural-patterns/07-proxy.md)

## Next

[03-specification.md](./03-specification.md)
