# Repository

**Intent:** mediate between the domain and data-mapping layers using a collection-like interface for accessing domain objects.

To the domain code, a repository looks like an in-memory collection of aggregates: you add objects, find them by criteria, and remove them. How and where they're stored (SQL, NoSQL, files, a remote API) is hidden.

## The problem

Without a repository, business logic and data access intermingle:

```java
public class OrderService {
    public void cancel(String orderId) {
        try (Connection c = dataSource.getConnection();
             PreparedStatement ps = c.prepareStatement("SELECT * FROM orders WHERE id = ?")) {
            ps.setString(1, orderId);
            ResultSet rs = ps.executeQuery();
            // map columns to an Order, check rules, run UPDATE ...
        }
    }
}
```

Consequences:

- Business logic can't be tested without a database.
- SQL is duplicated across services.
- Changing the persistence technology touches business code.
- This directly violates [Dependency Inversion](../02-solid-principles/05-dependency-inversion.md).

## Structure

```text
Service ──► OrderRepository (interface, owned by the domain)
                  ▲
        ┌─────────┴──────────┐
JpaOrderRepository     InMemoryOrderRepository (tests)
        │
        ▼
     Database
```

The interface lives with the **domain**; the implementation lives in **infrastructure**.

## Example

```java
// Domain layer
public interface OrderRepository {
    Optional<Order> findById(OrderId id);
    List<Order> findByCustomer(CustomerId customerId);
    void save(Order order);          // add or update
    void remove(Order order);
}

// Service uses only the interface
public class OrderService {
    private final OrderRepository orders;

    public OrderService(OrderRepository orders) { this.orders = orders; }

    public void cancel(OrderId id) {
        Order order = orders.findById(id).orElseThrow(() -> new OrderNotFound(id));
        order.cancel();                  // business rule lives in the domain object
        orders.save(order);
    }
}
```

An in-memory implementation for tests:

```java
public class InMemoryOrderRepository implements OrderRepository {
    private final Map<OrderId, Order> store = new HashMap<>();

    public Optional<Order> findById(OrderId id) { return Optional.ofNullable(store.get(id)); }
    public List<Order> findByCustomer(CustomerId c) {
        return store.values().stream().filter(o -> o.customerId().equals(c)).toList();
    }
    public void save(Order order) { store.put(order.id(), order); }
    public void remove(Order order) { store.remove(order.id()); }
}
```

A production implementation (with JPA, for example) implements the same interface:

```java
public class JpaOrderRepository implements OrderRepository {
    private final EntityManager em;
    public JpaOrderRepository(EntityManager em) { this.em = em; }

    public Optional<Order> findById(OrderId id) { return Optional.ofNullable(em.find(Order.class, id)); }
    public void save(Order order) { em.persist(order); }
    // ...
}
```

## Design guidelines

| Guideline | Why |
|---|---|
| **One repository per aggregate root**, not per table | Aggregates are the unit of consistency; child entities are reached through the root |
| **Express queries in domain terms** (`findOverdueInvoices`) | Callers shouldn't know about SQL or tables |
| **Return domain objects, not persistence types** (no `ResultSet`, no rows) | Keeps the domain free of infrastructure |
| **Don't leak persistence exceptions** | Translate or wrap them |
| **Keep it free of business rules** | It stores and retrieves; the domain decides |
| **Offer pagination** for potentially large results | Avoid loading everything |

## Repository vs DAO

| | Repository | DAO (Data Access Object) |
|---|---|---|
| Perspective | Domain: "a collection of aggregates" | Persistence: "access to a table or data source" |
| Unit | Aggregate root | Usually one table/entity |
| Language | Domain terms | Data terms (`insertUser`, `updateRow`) |
| Layer owner | Domain | Infrastructure |

In practice the line blurs, and many "repositories" are really DAOs. The distinguishing mark is whether the interface is shaped by the domain's needs.

## The generic repository trap

```java
public interface Repository<T, ID> {
    T findById(ID id);
    List<T> findAll();
    void save(T t);
    void delete(T t);
}
```

Problems:

- Offers operations that don't make sense for every aggregate (can you really "delete" an `Invoice`?).
- `findAll()` invites loading entire tables.
- Domain-specific queries still need specific methods, so the generic interface alone isn't enough.

A generic *base* for implementation reuse is fine, but each aggregate should expose its **own, specific interface** to the domain.

## Frameworks

Spring Data repositories generate implementations from interfaces:

```java
public interface OrderJpaRepository extends JpaRepository<OrderEntity, Long> {
    List<OrderEntity> findByCustomerId(String customerId);
}
```

These are convenient, but they expose a broad generic API (`deleteAll`, `findAll`). Many teams keep a smaller domain-owned repository interface and implement it by delegating to the Spring Data interface. Decide whether the abstraction is worth it for your application's complexity.

## Combining with Specification

For flexible queries, accept a [Specification](./03-specification.md) instead of adding a method per combination:

```java
List<Order> findMatching(Specification<Order> spec);
```

## When to use it

- The domain has real business logic you want to test without a database.
- You may change or mix storage technologies.
- You want queries named in domain language.

Skip it (or keep it very thin) for simple CRUD applications where the ORM or framework repository is already adequate.

## Common mistakes

- **One repository per table** rather than per aggregate.
- **Leaking entities, query objects or ORM sessions** to higher layers.
- **Business logic inside repositories** (filters that are really domain rules).
- **`findAll()` on large tables.**
- **N+1 queries** from lazily loading children inside loops. Provide methods that fetch what the use case needs.
- **Repositories calling other repositories**, hiding dependencies. Orchestrate in the service.
- **Mocking everywhere.** Prefer an in-memory fake for readable tests.

## Quick Summary

- A repository gives the domain a collection-like view of persistent aggregates.
- The interface belongs to the domain; the implementation to infrastructure.
- One per aggregate root, with domain-language methods and no business logic.
- Distinct from DAO by perspective; avoid the one-size-fits-all generic interface.

## Related

[Unit of Work](./02-unit-of-work.md) · [Specification](./03-specification.md) · [Dependency Inversion](../02-solid-principles/05-dependency-inversion.md) · [Domain-driven design](../08-architecture/04-domain-driven-design.md)

## Next

[02-unit-of-work.md](./02-unit-of-work.md)
