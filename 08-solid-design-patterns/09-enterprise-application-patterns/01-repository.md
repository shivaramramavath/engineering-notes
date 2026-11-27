# Repository Pattern

| Field | Value |
| :--- | :--- |
| **Category** | Enterprise Application Patterns (Persistence) |
| **Origin** | Martin Fowler (*PoEAA*, 2002) & Eric Evans (*DDD*, 2003) |
| **Difficulty** | Beginner → Intermediate |
| **Prerequisite(s)** | [Dependency Inversion Principle](file:///E:/Learning/engineering-notes/08-solid-design-patterns/02-solid-principles/05-dependency-inversion.md) |
| **Status** | Complete |

> **TL;DR**: Repository mediates between the domain and data-mapping layers using a **collection-like interface** (`findById`, `save`, `findAll(spec)`), hiding SQL/ORM details so domain logic can be tested with an in-memory fake.

---

## 1. The Problem: Why Does Repository Exist?

When application services execute raw SQL or ORM `EntityManager` queries directly:
1. **Duplicate Queries**: `SELECT * FROM orders WHERE status = 'PAID' AND total > 100` gets copy-pasted across 6 controllers.
2. **Untestable Business Logic**: Unit-testing a discount or billing rule requires booting PostgreSQL or mocking brittle ORM query-builder chains.
3. **Persistence Coupling**: Changing from SQL to an external API or switching ORMs forces rewriting domain services.

---

## 2. What Is the Repository Pattern?

A **Repository** presents an in-memory collection abstraction (`add`, `remove`, `findById`, `findMatching`) for **Aggregate Roots** (e.g., `OrderRepository`, `UserRepository`).

The domain defines the **interface** (`OrderRepository`); the infrastructure layer provides the **implementation** (`PostgresOrderRepository` for prod, `InMemoryOrderRepository` for fast unit tests).

---

## 3. How It Works

```mermaid
flowchart LR
    Svc["OrderService (Domain/App)"] --> RepoI["interface OrderRepository"]
    RepoI <|.. PgRepo["PostgresOrderRepository (Infra)"]
    RepoI <|.. MemRepo["InMemoryOrderRepository (Tests)"]
```

---

## 4. What Goes Wrong Without It

```python
# ❌ TIGHTLY COUPLED: Domain service directly runs SQL + cursor logic
def approve_order(conn, order_id: str):
    row = conn.execute("SELECT total, status FROM orders WHERE id = %s", (order_id,)).fetchone()
    if row[0] > 1000:
        conn.execute("UPDATE orders SET status = 'APPROVED' WHERE id = %s", (order_id,))
```

---

## 5. Multi-Language Examples

### Java 25 LTS

```java
import java.util.*;

public class RepositoryDemo {
    record Order(String id, long cents, boolean paid) {}

    interface OrderRepository {
        Optional<Order> findById(String id);
        void save(Order order);
    }

    static class InMemoryOrderRepository implements OrderRepository {
        private final Map<String, Order> store = new HashMap<>();
        @Override public Optional<Order> findById(String id) { return Optional.ofNullable(store.get(id)); }
        @Override public void save(Order order) { store.put(order.id(), order); }
    }

    public static void main(String[] args) {
        OrderRepository repo = new InMemoryOrderRepository();
        repo.save(new Order("ord-1", 2500, true));
        System.out.println("Found: " + repo.findById("ord-1"));
    }
}
```

### TypeScript 5.8

```ts
interface Order { readonly id: string; readonly cents: number; }

interface OrderRepository {
  findById(id: string): Promise<Order | undefined>;
  save(order: Order): Promise<void>;
}

class InMemoryOrderRepository implements OrderRepository {
  private readonly store = new Map<string, Order>();
  async findById(id: string) { return this.store.get(id); }
  async save(order: Order) { this.store.set(order.id, order); }
}

const repo: OrderRepository = new InMemoryOrderRepository();
repo.save({ id: 'ord-1', cents: 2500 }).then(() => repo.findById('ord-1')).then(console.log);
```

### Python 3.14

```python
from dataclasses import dataclass
from typing import Protocol

@dataclass(frozen=True)
class Order:
    id: str
    cents: int

class OrderRepository(Protocol):
    def find_by_id(self, order_id: str) -> Order | None: ...
    def save(self, order: Order) -> None: ...

class InMemoryOrderRepository:
    def __init__(self) -> None:
        self._store: dict[str, Order] = {}
    def find_by_id(self, order_id: str) -> Order | None:
        return self._store.get(order_id)
    def save(self, order: Order) -> None:
        self._store[order.id] = order

repo: OrderRepository = InMemoryOrderRepository()
repo.save(Order("ord-1", 2500))
print(repo.find_by_id("ord-1"))
```

---

## 6. Real-World Examples

1. **Spring Data JPA (`JpaRepository<Order, String>`)**: Separates persistence contract from generated SQL/Hibernate queries.
2. **TypeORM / Prisma / MikroORM**: Custom repository wrappers shielding NestJS services from raw query builders.
3. **Django / FastAPI Clean Architecture**: Abstract `UserRepo(Protocol)` backed by SQLAlchemy in prod and dict in pytest.

---

## 7. Variants & Related Patterns

- **Specification-Backed Repository**: `findAll(Specification<T>)` avoids explosion of `findByStatusAndDateAnd...` methods.
- **Generic `Repository<T, ID>` vs Aggregate-Specific**: Prefer aggregate-specific interfaces (`OrderRepository`) so callers only see meaningful domain queries.
- **Related**: [Unit of Work](file:///E:/Learning/engineering-notes/08-solid-design-patterns/09-enterprise-application-patterns/02-unit-of-work.md), [Data Mapper](file:///E:/Learning/engineering-notes/08-solid-design-patterns/09-enterprise-application-patterns/03-data-mapper-vs-active-record.md), [Specification](file:///E:/Learning/engineering-notes/08-solid-design-patterns/09-enterprise-application-patterns/04-specification.md).

---

## 8. Advantages

- **Testability**: Swap DB for `InMemoryOrderRepository` in milliseconds.
- **Centralized Query Logic**: Eliminates duplicated SQL/ORM queries.
- **Domain Isolation**: Keeps ORM annotations/SQL out of business logic.

---

## 9. Limitations & When NOT to Use

- **Simple CRUD Apps**: Adding a custom Repository wrapper around an ORM that is *already* a Repository (like Prisma or ActiveRecord) for simple 1-table CRUD adds boilerplate without value.
- **Complex Reporting / Analytics**: Don't force multi-table BI aggregations through Aggregate Repositories—use a dedicated Read Model / Query Service (CQRS).

---

## 10. Anti-Patterns & Common Mistakes

| Mistake | Why It Fails | Fix |
| :--- | :--- | :--- |
| Creating a Repository for every child table (`OrderItemRepository`) | Breaks Aggregate consistency invariants | Only create Repositories for **Aggregate Roots** (`OrderRepository`) |
| Returning `IQueryable` / raw `QueryBuilder` from Repository | Leaks persistence query semantics into domain/controllers | Return materialized domain objects (`List<Order>`, `Page<Order>`) |
| Calling `commit()` inside `repo.save()` | Prevents multi-repo atomic transactions | Let **Unit of Work** / Service Layer manage transaction boundaries |
| Putting business validation inside Repository | Mixes persistence with domain rules | Keep business logic in Domain Entities / Service Layer |
| Generic `IRepository<T>` with `GetAll()` returning 1M rows | Causes OOM in production | Expose paginated/filtered domain methods |
| Leaking ORM entities directly as API JSON | Couples DB schema to external clients | Map to DTOs at the API boundary |

---

## 11. Comparison: Repository vs DAO

| Aspect | Repository | Data Access Object (DAO) |
| :--- | :--- | :--- |
| **Abstraction Level** | Domain Aggregate collection | Table / row CRUD operations |
| **Granularity** | Aggregate Root (`Order` + `OrderItems`) | 1-to-1 with DB tables |
| **Vocabulary** | Domain-centric (`findOverdueInvoices()`) | Storage-centric (`selectByStatus()`) |

---

## 12. Design Checklist

- [ ] Is there one Repository per **Aggregate Root**, not per DB table?
- [ ] Does the Repository interface live in the domain/application layer (DIP)?
- [ ] Are transactions controlled outside the Repository (Unit of Work / Service Layer)?

---

## 13. Interview Questions

<details>
<summary>1. What is the difference between Repository and DAO?</summary>

A DAO is a lower-level abstraction around a database table's CRUD operations. A Repository is a domain-level collection abstraction for an entire Aggregate Root.

</details>

<details>
<summary>2. Why should Repositories only exist for Aggregate Roots?</summary>

Loading or saving a child entity (`OrderItem`) directly bypasses the Aggregate Root (`Order`) and can violate invariants like `order.total == sum(items)`.

</details>

<details>
<summary>3. Why shouldn't `Repository.save()` commit the database transaction?</summary>

A single use case often updates two repositories (e.g., `OrderRepository` and `InventoryRepository`). Committing inside each `save()` breaks atomicity; transaction boundaries belong in the Service Layer / Unit of Work.

</details>

<details>
<summary>4. Why is returning `IQueryable` or a lazy ORM query from a Repository an anti-pattern?</summary>

It leaks ORM/SQL capabilities to callers, meaning your `InMemoryRepository` fake behaves differently from your SQL repository (e.g., unsupported LINQ expressions).

</details>

<details>
<summary>5. How do you handle complex reporting queries with Repositories?</summary>

Bypass domain Repositories for read-heavy reporting and use a dedicated **Read Model / Query Service** (CQRS) that projects directly to DTOs.

</details>

<details>
<summary>6. How does Repository support the Dependency Inversion Principle?</summary>

The domain defines the `OrderRepository` interface, and the infrastructure layer depends on and implements it.

</details>

<details>
<summary>7. When is a Repository overkill?</summary>

In simple CRUD microservices or prototypes where Active Record / Prisma directly serves thin endpoints with minimal domain logic.

</details>

<details>
<summary>8. How do you prevent `findByXAndYAndZ` method explosion in Repositories?</summary>

Combine Repository with the **Specification** pattern (`findAll(Specification<Order> spec)`).

</details>

---

## 14. Practice Exercises

<details>
<summary>Exercise 1: In-Memory + Specification Repository</summary>

Implement `UserRepository` with `findByEmail` and `findAll(spec)` backed by an in-memory `Map`/`dict`.

</details>

<details>
<summary>Exercise 2: Pagination Contract</summary>

Add a `Page<T>(items, totalCount, page, pageSize)` return type to `OrderRepository.findRecent(...)`.

</details>

<details>
<summary>Exercise 3: Refactor SQL-in-Controller</summary>

Extract inline SQL queries from a sample `checkout()` handler into `OrderRepository` and `InventoryRepository` interfaces.

</details>

---

## 15. Cheat Sheet

- **One per Aggregate Root**, never per child table.
- **Collection semantics**: `findById`, `save`, `delete`, `findAll(spec)`.
- **No `commit()` inside `save()`** — let Unit of Work commit.

---

## 16. In Your Language

| Track | Relevant Deep-Dive |
| :--- | :--- |
| **Java** | [Spring Boot Data & JPA](file:///E:/Learning/engineering-notes/03-backend/spring-boot/README.md) |
| **TypeScript** | [NestJS Architecture & Patterns](file:///E:/Learning/engineering-notes/03-backend/nestjs/08-architecture-and-patterns/README.md) |
| **Python** | [Python Database Access](file:///E:/Learning/engineering-notes/01-languages/python/19-database-access/README.md) |

---

## 17. Navigation

| Previous | Up | Next |
| :--- | :--- | :--- |
| [Module 09 Overview](file:///E:/Learning/engineering-notes/08-solid-design-patterns/09-enterprise-application-patterns/00-overview.md) | [Module 09](file:///E:/Learning/engineering-notes/08-solid-design-patterns/09-enterprise-application-patterns/README.md) | [Unit of Work](file:///E:/Learning/engineering-notes/08-solid-design-patterns/09-enterprise-application-patterns/02-unit-of-work.md) |
