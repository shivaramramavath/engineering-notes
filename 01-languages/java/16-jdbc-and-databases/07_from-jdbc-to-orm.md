# From JDBC to ORM

Raw JDBC makes you write the same things repeatedly: SQL for every operation, code to copy columns into objects and back, and hand-built joins and graph loading. An **ORM** (Object-Relational Mapper) automates that. You describe how classes map to tables, and the framework generates SQL, tracks changes to your objects, and writes them back.

In Java the standard is **JPA** (now *Jakarta Persistence*), and the dominant implementation is **Hibernate**. Most Spring applications use them through **Spring Data JPA**. An ORM is a big productivity win for typical CRUD-with-relationships domains, and also a source of the most notorious performance and correctness traps in Java backends. Everything it does is JDBC underneath, so the earlier notes are the debugging manual for it.

**Prerequisites:** all earlier notes in this module, especially [Transactions](04_transactions.md) and [JDBC Patterns](06_jdbc-patterns.md).

---

## 1. Why ORMs exist: the impedance mismatch

Objects and tables model data differently:

| Object world | Relational world |
|---|---|
| Object **graphs** (references, collections) | **Rows** linked by foreign keys |
| Identity = reference (`==`) | Identity = primary key |
| Inheritance and polymorphism | No direct equivalent (single table, joined tables, ...) |
| Navigate: `user.getOrders()` | Join or a separate query |
| Types: enums, value objects, `Instant` | Columns: `VARCHAR`, `TIMESTAMP`, ... |

An ORM bridges this gap, and the bridge has costs: hidden queries, a more complex mental model, and behaviors (lazy loading, dirty checking) that surprise you when you don't know they exist.

---

## 2. The spectrum of data-access options

| Option | You write | Strengths | Watch out for |
|---|---|---|---|
| **Plain JDBC** | SQL + mapping + plumbing | Total control, no magic | Boilerplate, easy to leak/forget |
| **Spring `JdbcClient` / `JdbcTemplate`, JDBI** | SQL; the library handles connections, parameters, mapping | Thin, transparent, productive | Still hand-written SQL and mapping |
| **MyBatis** | SQL in XML/annotations mapped to methods | SQL-first with mapping help | Another config layer |
| **jOOQ** | Type-safe SQL in Java (generated from your schema) | Compile-time-checked SQL, great for complex queries | Commercial licensing for some databases, learning curve |
| **JPA / Hibernate** | Entity classes + annotations; queries in JPQL | Productivity for CRUD and object graphs, caching, change tracking | Lazy-loading traps, N+1, hidden SQL, large conceptual surface |
| **Spring Data JPA** | Repository *interfaces* | Minimal code for common queries | Same traps as JPA, plus "magic" method names |

```java
// Spring JdbcClient (Spring Framework 6.1+): SQL stays visible, the mapping is automated
Optional<UserRow> user = jdbcClient.sql("SELECT id, email, name FROM users WHERE email = :email")
        .param("email", email)
        .query(UserRow.class)           // maps columns to a record by name
        .optional();
```

These aren't mutually exclusive. Many successful systems use **JPA for writes and simple reads, and plain SQL/jOOQ for complex reports**.

---

## 3. JPA and Hibernate: the core model

### Entities

```java
import jakarta.persistence.*;

@Entity
@Table(name = "users")
public class User {

    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @Column(nullable = false, unique = true)
    private String email;

    @Column(nullable = false)
    private String name;

    @OneToMany(mappedBy = "user")                  // inverse side: the FK lives in orders.user_id
    private List<Order> orders = new ArrayList<>();

    @Version
    private long version;                          // optimistic locking ([Transactions](04_transactions.md#optimistic-locking))

    protected User() { }                           // JPA requires a no-arg constructor (not private)
    public User(String email, String name) { this.email = email; this.name = name; }
    // getters, and setters only where mutation makes sense
}

@Entity
@Table(name = "orders")
public class Order {
    @Id @GeneratedValue(strategy = GenerationType.IDENTITY) private Long id;

    @ManyToOne(fetch = FetchType.LAZY, optional = false)     // the owning side: has the FK column
    @JoinColumn(name = "user_id")
    private User user;

    @Column(nullable = false) private BigDecimal total;
    @Enumerated(EnumType.STRING) private OrderStatus status;  // STRING, never ORDINAL
    private Instant createdAt;
    protected Order() { }
}
```

Rules to remember:

- The package is **`jakarta.persistence`** (since Jakarta EE 9). Older code and tutorials use `javax.persistence`, so mixing them is a classic upgrade failure.
- Entities need a **non-private no-arg constructor** and, per the specification, can't be `final` (Hibernate generates subclasses/proxies for lazy loading). That's why **records are not a good fit for entities**. Use records for DTOs and query projections ([Records](../12-modern-java/01_records.md#7-practical-usage)).
- `java.time` types map directly. Enums should be `EnumType.STRING`.
- In a bidirectional relationship, one side **owns** the foreign key (`@ManyToOne` / `@JoinColumn`), and the other is the **inverse** (`mappedBy`). Keep both sides in sync in code (`order.setUser(u); u.getOrders().add(order);`).

### The persistence context (the idea that explains everything)

The `EntityManager` (Hibernate's `Session`) holds a **persistence context**: a cache and bookkeeping area of the entities it has loaded or saved in the current unit of work, which usually matches **one transaction**.

```text
 entity states:
   new/transient ──persist()──► managed ──remove()──► removed
                                  │  ▲
                        close/clear│  │merge()
                                  ▼  │
                               detached
```

```java
@Transactional
public void rename(long id, String newName) {
    User u = em.find(User.class, id);     // SELECT, and u becomes MANAGED
    u.setName(newName);                   // no save() call!
}                                         // commit → flush → Hibernate notices the change (dirty checking) → UPDATE users SET ...
```

Consequences:

- **Identity map:** within one persistence context, loading the same row twice returns the **same Java object** (a first-level cache).
- **Dirty checking:** modifying a managed entity is enough. At **flush** time (before a query that might be affected, or on commit) Hibernate compares state to the loaded snapshot and issues the `UPDATE`s. Calling `save()` on a managed entity is redundant.
- **Flush ≠ commit.** Flush sends SQL to the database inside the transaction. Commit makes it permanent. Statements are often **delayed and reordered**, so exceptions (constraint violations) may appear at commit, not at the line that "caused" them.
- **Detached** entities (the transaction ended) are no longer tracked: changes to them aren't saved unless re-attached (`merge`) in a new transaction.
- **Lazy loading** needs an open persistence context (section 4).

### Queries

```java
// JPQL: queries over ENTITIES and their fields, not tables and columns
List<User> users = em.createQuery(
        "select u from User u where lower(u.email) like :p order by u.id", User.class)
    .setParameter("p", prefix.toLowerCase() + "%")
    .setMaxResults(20)
    .getResultList();
```

JPQL, the Criteria API, and native SQL (`createNativeQuery`) are all available. Hibernate turns each into parameterized JDBC statements, so the injection rules from [Prepared Statements](02_statements-and-prepared-statements.md) still apply: never concatenate user input into JPQL either.

---

## 4. The classic traps

Almost every ORM production problem is one of these. All of them are *invisible in the code* and visible in the SQL log.

### 4.1 The N+1 select problem

```java
List<User> users = em.createQuery("select u from User u", User.class).getResultList();   // 1 query
for (User u : users) {
    System.out.println(u.getOrders().size());     // lazy collection: 1 query PER USER → N more queries
}
```

The ORM equivalent of [the N+1 problem](06_jdbc-patterns.md#5-the-n1-problem), made easy by `getOrders()` looking like a harmless getter. Fixes, from most to least explicit:

```java
// fetch join: load the association in the same query
"select distinct u from User u left join fetch u.orders where u.id in :ids"

// entity graph (declarative fetch plan per query)
@EntityGraph(attributePaths = "orders")
List<User> findByIdIn(Collection<Long> ids);

// batch fetching: load lazy associations for many parents at once (WHERE user_id IN (?, ?, ...))
//   e.g. @BatchSize(size = 50) or the hibernate.default_batch_fetch_size setting

// or don't load entities at all: use a DTO projection with exactly the data you need
```

### 4.2 `LazyInitializationException`

```text
org.hibernate.LazyInitializationException: could not initialize proxy - no Session
```

You touched a lazy association **after the persistence context closed** (typically while rendering a view or serializing JSON). Fixes: load what you need inside the transaction (fetch join/entity graph), return **DTOs** from the service layer instead of entities, and don't "fix" it by making everything `EAGER`. That turns lazy problems into N+1 and giant joins everywhere.

**Open Session in View** (Spring Boot's `spring.jpa.open-in-view`, on by default with a startup warning) keeps the session open for the whole web request. It hides the exception, and lets lazy loading fire queries from controllers and serializers, while holding a pooled connection longer ([Connection Pooling](05_connection-pooling.md#4-failure-modes)). Many teams turn it off and use DTOs.

### 4.3 Fetch type defaults

| Association | Default fetch |
|---|---|
| `@ManyToOne`, `@OneToOne` | **EAGER** (usually change to `LAZY`) |
| `@OneToMany`, `@ManyToMany` | LAZY |

Eager `@ManyToOne` chains produce wide joins or extra queries on every load. Prefer `LAZY` everywhere and decide per query what to fetch.

### 4.4 Multiple collections and pagination

- Fetching two `List` collections in one query throws `MultipleBagFetchException` (and would produce a cartesian product). Fetch one collection per query, or use batch fetching.
- `join fetch` of a collection **combined with `setMaxResults`** can't paginate in SQL. Hibernate warns that it applies the limit **in memory** (after loading all rows). Paginate the parent IDs first, then fetch associations for those IDs.

### 4.5 Writes: batching and bulk work

- `GenerationType.IDENTITY` forces an immediate insert per entity to get the key, which **disables JDBC insert batching**. Use a `SEQUENCE` strategy (with a sensible allocation size) and set `hibernate.jdbc.batch_size` for bulk inserts ([Prepared Statements](02_statements-and-prepared-statements.md#5-batching)).
- Loading or inserting **huge numbers of entities in one transaction** bloats the persistence context (every entity is tracked in memory). Process in chunks and `flush()` + `clear()`, or use bulk JPQL updates / native SQL / Hibernate's stateless session for mass operations.
- A bulk JPQL `UPDATE`/`DELETE` bypasses the persistence context: already-loaded entities become stale.

### 4.6 Smaller traps

| Trap | Advice |
|---|---|
| `equals`/`hashCode` on entities | Don't use all fields, and don't use a generated ID that's `null` before persist in a `HashSet`. Use a stable business key, or follow a well-established ID-based pattern ([equals and hashCode](../04-oop/14_equals-and-hashcode.md)) |
| `toString()` printing associations | Triggers lazy loading (and infinite recursion in bidirectional links). Exclude associations |
| `@Enumerated(ORDINAL)` | Reordering the enum corrupts data. Use `STRING` |
| Cascades (`CascadeType.ALL`, `orphanRemoval`) | Powerful and dangerous: they can delete more than you expect. Use deliberately, on true parent-owned children |
| `@Transactional` boundaries and self-invocation | A proxy mechanism: [Transactions](04_transactions.md#7-transactions-in-frameworks) |
| Auto-generated schema in production (`ddl-auto=update/create`) | Use migrations ([JDBC Patterns](06_jdbc-patterns.md#7-schema-migrations)) |
| Treating the second-level cache as a free speed-up | It adds consistency and invalidation concerns. Measure first |
| Updating every column | Hibernate's default `UPDATE` sets all columns. `@DynamicUpdate` limits it (with its own cost) |

### Make the SQL visible

You can't tune what you can't see. Turn on SQL logging in development and in test assertions:

```properties
logging.level.org.hibernate.SQL=DEBUG                          # statements
logging.level.org.hibernate.orm.jdbc.bind=TRACE                # bound parameter values (development only)
spring.jpa.properties.hibernate.generate_statistics=true       # statement counts, cache hits
```

(Logger names and options evolve with Hibernate versions. Check yours.) Then **count queries per request** and fail the test when the count grows unexpectedly.

---

## 5. Spring Data JPA

Spring Data generates repository implementations from **interfaces**:

```java
public interface UserRepository extends JpaRepository<User, Long> {

    Optional<User> findByEmail(String email);                              // derived query from the method name
    List<User> findByNameStartingWithIgnoreCase(String prefix);
    boolean existsByEmail(String email);

    @Query("select u from User u left join fetch u.orders where u.id = :id")
    Optional<User> findWithOrders(@Param("id") Long id);                    // explicit JPQL

    List<UserSummary> findByCreatedAtAfter(Instant since);                  // DTO projection (a record)
}

public record UserSummary(Long id, String email) {}
```

- There's **no implementation class**. Spring creates a **dynamic proxy** at startup that executes the queries: the mechanism described in [Dynamic Proxies](../13-advanced-language-features/02_dynamic-proxies.md#4-a-complete-example-annotation-driven-proxy).
- `JpaRepository` gives you `save`, `findById`, `findAll(Pageable)`, `delete`, and more. `save` on a *new* entity inserts, and on a detached one it merges. On a managed one it's redundant.
- **Pagination:** `Page<User> p = repo.findAll(PageRequest.of(0, 20, Sort.by("id")))`. This issues a count query as well. Prefer `Slice` or keyset approaches for large tables ([Pagination](../25-real-world-patterns/01_pagination.md)).
- **Projections** (interfaces, records/DTOs) fetch only the columns you need, which is often the best fix for both N+1 and over-fetching on read paths.
- Derived-query method names are convenient for simple cases, but long names like `findByStatusAndCreatedAtBetweenAndUserEmailContaining...` are a smell. Use `@Query`, specifications, or SQL.
- The standard **Jakarta Data** specification is emerging as a portable repository API (Hibernate and others implement it).

---

## 6. Where things stand (October 2026)

- **Jakarta Persistence 3.2** is the current specification, implemented by **Hibernate ORM 7.x** (and EclipseLink). **Jakarta Persistence 4.0** is under development with a late-2026 target, and Hibernate 8 is being built toward it.
- **Spring Framework 7 / Spring Boot 4** build on Jakarta Persistence 3.2 and Hibernate ORM 7.1+/7.2. Spring Boot 3.x uses Hibernate 6.x. Hibernate 6.x versions are now in limited support.
- The `javax.persistence` → `jakarta.persistence` namespace change happened with Jakarta EE 9 (Hibernate 6 / Spring Boot 3 era). Mixing old libraries into a new stack is a common upgrade failure.

Versions move quickly. Check the current Hibernate, Spring, and Jakarta Persistence documentation before depending on version-specific behavior.

---

## 7. Choosing

```text
 Mostly CRUD on a rich domain with relationships, and the team knows JPA?     → JPA / Spring Data JPA
 Reporting, analytics, complex joins, window functions, bulk operations?      → SQL: jOOQ, JdbcClient, or native queries
 Few queries, simple tables, want full transparency?                          → JdbcClient / JDBI / plain JDBC
 Both?                                                                        → JPA for writes + SQL/projections for reads
```

Whatever you choose:

- **You must still understand SQL, indexes, transactions, and the pool.** An ORM saves typing, not understanding. Production performance problems are solved at those layers.
- Keep **entities inside the persistence layer** and expose **DTOs/records** at API boundaries.
- **Look at the generated SQL** during development, count queries in tests, and review `EXPLAIN` plans for the heavy ones ([SQL Essentials](00_sql-and-indexing-essentials.md#6-explain-asking-the-database-what-it-will-do)).
- Prefer **explicit fetch plans** (fetch joins, entity graphs, projections) over relying on lazy defaults.

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Lazy collection access in a loop (N+1) | Fetch join / entity graph / batch fetching / projection |
| Making associations `EAGER` to avoid `LazyInitializationException` | Fetch what you need per query, and return DTOs |
| Returning entities from controllers/APIs | Map to DTOs (records) |
| Assuming `save()` is needed (or does the commit) | Managed entities are tracked. Commit/flush writes them |
| `IDENTITY` ids with bulk inserts, then wondering why batching doesn't work | Sequence ids + batch settings |
| One giant transaction loading millions of entities | Chunk, `flush`/`clear`, or use bulk SQL |
| `@Enumerated` with ORDINAL | `EnumType.STRING` |
| Using a record or `final` class as an entity | Class entities, record DTOs |
| Relying on `ddl-auto` for production schemas | Flyway/Liquibase migrations |
| Not looking at SQL logs | Log and count statements, in development and CI |
| Entity `equals`/`hashCode` on mutable/generated fields | Stable business key or a careful ID-based pattern |
| Assuming ORM replaces knowing SQL | Learn SQL, indexes, and transactions first |

### Debugging

- **Slow page, many identical queries in the log** → N+1. Add a fetch join, entity graph, or projection.
- **`LazyInitializationException`** → access outside the transaction. Fetch inside it, or map to a DTO.
- **`HHH000104`-style "firstResult/maxResults specified with collection fetch; applying in memory"** → paginating a collection fetch. Paginate IDs first.
- **Updates "not saved"** → the entity is detached, outside a transaction, or the flush never happened (read-only transaction, wrong propagation).
- **`OptimisticLockException`** → a version conflict ([Transactions](04_transactions.md#optimistic-locking)). Reload and retry.
- **Constraint violation appearing at commit time** → delayed flush. Call `flush()` when you need the error at a known point.
- **Memory growth during bulk jobs** → persistence context bloat. Clear periodically.

---

## Quick Summary

- ORMs map objects to tables and generate SQL. JPA/Jakarta Persistence is the standard, Hibernate the main implementation, Spring Data JPA the common convenience layer. Everything still runs over JDBC.
- Core idea: the **persistence context**. Entities are **managed** in a transaction, changes are detected by **dirty checking** and written at **flush/commit**, and **detached** entities are no longer tracked.
- Entities are classes (non-final, no-arg constructor), annotated with `jakarta.persistence`. Use **records for DTOs**, `EnumType.STRING`, `LAZY` associations, and `@Version` for optimistic locking.
- The big traps: **N+1**, `LazyInitializationException`/open-session-in-view, eager defaults, collection-fetch pagination, `IDENTITY` blocking batching, and persistence-context bloat. **Make the SQL visible and count queries.**
- Spring Data JPA repositories are **dynamic proxies** over interfaces. Use projections and `@Query` for non-trivial reads.
- Choose by workload: JPA for domain CRUD, SQL/jOOQ/`JdbcClient` for reporting and complex queries, and always understand the SQL underneath.

**Next module:** [JSON and Data Formats](../17-json-and-data-formats/README.md)