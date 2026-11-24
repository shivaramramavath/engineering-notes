# Module 06: Data Access & Persistence

Welcome to **Module 06: Data Access & Persistence**. This module explores enterprise persistence patterns: `JdbcClient`, HikariCP connection pooling, Spring Data JPA, Hibernate 6.6+ SQM internals, N+1 query elimination, keyset pagination, transaction propagation, Flyway schema migrations, and Testcontainers.

---

## Module Contents

| # | Note | Description | Level | Status |
|---|---|---|---|---|
| 01 | [01-jdbc-template-and-jdbc-client.md](file:///03-backend/spring-boot/06-data-access/01-jdbc-template-and-jdbc-client.md) | `JdbcTemplate` vs the modern fluent `JdbcClient` introduced in Spring 6.1 | Intermediate | Complete |
| 02 | [02-datasource-and-hikari-pooling.md](file:///03-backend/spring-boot/06-data-access/02-datasource-and-hikari-pooling.md) | HikariCP configuration, sizing formulas, connection leak detection, validation | Advanced | Complete |
| 03 | [03-spring-data-jpa-fundamentals.md](file:///03-backend/spring-boot/06-data-access/03-spring-data-jpa-fundamentals.md) | Core concepts of JPA, EntityManager, entity lifecycle states, persistence context | Beginner | Complete |
| 04 | [04-entity-mapping-and-relationships.md](file:///03-backend/spring-boot/06-data-access/04-entity-mapping-and-relationships.md) | `@OneToMany`, `@ManyToOne`, `@ManyToMany`, composite keys, embedded values | Intermediate | Complete |
| 05 | [05-repositories-and-derived-queries.md](file:///03-backend/spring-boot/06-data-access/05-repositories-and-derived-queries.md) | `JpaRepository`, `CrudRepository`, method naming parser, dynamic derivation | Beginner | Complete |
| 06 | [06-jpql-native-and-specifications.md](file:///03-backend/spring-boot/06-data-access/06-jpql-native-and-specifications.md) | `@Query` with JPQL, native SQL queries, dynamic criteria with `Specification<T>` | Intermediate | Complete |
| 07 | [07-projections-and-dtos.md](file:///03-backend/spring-boot/06-data-access/07-projections-and-dtos.md) | Interface-based vs class/record-based projections to minimize query overhead | Intermediate | Complete |
| 08 | [08-pagination-and-sorting.md](file:///03-backend/spring-boot/06-data-access/08-pagination-and-sorting.md) | `Pageable`, `Slice`, and high-performance Keyset (Window) pagination | Intermediate | Complete |
| 09 | [09-auditing-and-optimistic-locking.md](file:///03-backend/spring-boot/06-data-access/09-auditing-and-optimistic-locking.md) | `@CreatedDate`, `@LastModifiedBy`, `@Version` optimistic concurrency control | Intermediate | Complete |
| 10 | [10-fetching-n-plus-one-and-entity-graphs.md](file:///03-backend/spring-boot/06-data-access/10-fetching-n-plus-one-and-entity-graphs.md) | Eliminating N+1 query loops using `JOIN FETCH` and `@EntityGraph` | Advanced | Complete |
| 11 | [11-lazy-loading-and-open-in-view-pitfalls.md](file:///03-backend/spring-boot/06-data-access/11-lazy-loading-and-open-in-view-pitfalls.md) | `LazyInitializationException` mechanics and disabling Open-Session-In-View (OSIV) | Advanced | Complete |
| 12 | [12-spring-data-jdbc.md](file:///03-backend/spring-boot/06-data-access/12-spring-data-jdbc.md) | Simpler persistence without JPA/Hibernate caching, dirty-tracking, or proxies | Intermediate | Complete |
| 13 | [13-hibernate-caching-and-second-level-cache.md](file:///03-backend/spring-boot/06-data-access/13-hibernate-caching-and-second-level-cache.md) | First-level session cache vs Second-level cache (L2C) with Redis / Redisson | Advanced | Complete |
| 14 | [14-transactions.md](file:///03-backend/spring-boot/06-data-access/14-transactions.md) | Propagation levels, isolation levels, self-invocation proxy traps, read-only | Advanced | Complete |
| 15 | [15-batching-and-bulk-operations.md](file:///03-backend/spring-boot/06-data-access/15-batching-and-bulk-operations.md) | JDBC batch inserts (`batch_size`), bulk update queries, avoiding memory saturation | Advanced | Complete |
| 16 | [16-flyway-and-liquibase-migrations.md](file:///03-backend/spring-boot/06-data-access/16-flyway-and-liquibase-migrations.md) | Schema version control, idempotent migrations, baseline, zero-downtime expand/contract | Intermediate | Complete |
| 17 | [17-multiple-datasources.md](file:///03-backend/spring-boot/06-data-access/17-multiple-datasources.md) | Configuring multiple datasources, transaction managers, and routing datasources | Advanced | Complete |
| 18 | [18-jooq-and-mybatis-overview.md](file:///03-backend/spring-boot/06-data-access/18-jooq-and-mybatis-overview.md) | Type-safe SQL querying with jOOQ vs MyBatis mapper templates | Intermediate | Complete |
| 19 | [19-spring-data-mongodb.md](file:///03-backend/spring-boot/06-data-access/19-spring-data-mongodb.md) | Document mapping, `MongoTemplate`, criteria queries, index creation, replica sets | Intermediate | Complete |
| 20 | [20-spring-data-redis.md](file:///03-backend/spring-boot/06-data-access/20-spring-data-redis.md) | `RedisTemplate`, key-value operations, Hashes, Lists, pub/sub, Lua scripts | Intermediate | Complete |
| 21 | [21-spring-data-elasticsearch-overview.md](file:///03-backend/spring-boot/06-data-access/21-spring-data-elasticsearch-overview.md) | Document indexing, Elasticsearch repositories, full-text queries, highlights | Intermediate | Complete |
| 22 | [22-soft-delete-and-multi-tenancy-in-jpa.md](file:///03-backend/spring-boot/06-data-access/22-soft-delete-and-multi-tenancy-in-jpa.md) | Hibernate `@SQLDelete`, `@SQLRestriction`, tenant schema switching via `CurrentTenantIdentifierResolver` | Advanced | Complete |
| 23 | [23-database-testing-with-testcontainers.md](file:///03-backend/spring-boot/06-data-access/23-database-testing-with-testcontainers.md) | Real PostgreSQL integration tests using Testcontainers and `@ServiceConnection` | Intermediate | Complete |
| 24 | [24-database-performance-and-query-plans.md](file:///03-backend/spring-boot/06-data-access/24-database-performance-and-query-plans.md) | EXPLAIN ANALYZE analysis, index optimization, query execution plan tuning | Advanced | Complete |

---

## Next Steps
Proceed to **[Module 07: Security & Identity](file:///03-backend/spring-boot/07-security/)** to master modern Spring Security 6/7 filter chains and token authorization.
