# 08. Transactional Testing & Database State Management in Spring Boot

| Metadata | Value |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | `01-testing-fundamentals-in-spring.md`, `06-data-access/14-transactions.md`, ACID Properties |
| **Estimated Reading Time** | 35 minutes |
| **Status** | Complete |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | OpenJDK 21, Spring Boot 3.4.1, PostgreSQL 16 |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance & Security Notes](#8-performance--security-notes)
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### Problem Statement
Integration tests that touch a database face a fundamental conflict:
- **Test Isolation**: Every test must run in a pristine, predictable environment. If Test $A$ inserts a customer record with ID 100, that row must not remain in the database to corrupt Test $B$.
- **Transaction Fidelity**: In production, service methods commit transactions. If an integration test wraps everything in an automatic rollback, code that relies on transaction commit hooks (like `@TransactionalEventListener(phase = AFTER_COMMIT)`) or asynchronous jobs will never execute during tests.

Furthermore, naive cleanup strategies—such as executing `DELETE FROM table` before every test—drastically slow down test suites and fail when foreign key constraints form complex dependency trees.

Understanding **Transactional Testing semantics**, mastering **`TestTransaction`**, and implementing automated rollback and database state seeding ensures test isolation without sacrificing architectural fidelity.

### Historical Evolution
- **Spring 1.x - 2.x**: Relied on `AbstractTransactionalSpringContextTests`. Rollback was hardcoded and rigid.
- **Spring 3.x - 4.x**: Introduced annotation-driven transactional testing via `TransactionalTestExecutionListener`, `@Rollback`, and `@Commit`.
- **Spring 5.x - 6.x / Boot 3.x/4.x**: Integrated `TestTransaction` static utilities for programmatic mid-test commits, enhanced support for Testcontainers, and non-transactional web test boundary testing.

### Real-World Relevance
Flawless database state management prevents intermittent, order-dependent "flaky tests" in continuous integration pipelines, saving hundreds of engineering hours spent debugging false-positive build failures.

---

## 2. Core Concepts

### Architectural Contracts
- **`TransactionalTestExecutionListener`**: The core listener that starts a transaction before each `@Test` method annotated with `@Transactional` (or on a class annotated with `@Transactional`) and automatically rolls it back after execution.
- **`@Rollback`**: Explicitly configures whether the test transaction should be rolled back (default: `true`).
- **`@Commit`**: Convenient direct alias for `@Rollback(false)`. Instructs the listener to commit the transaction upon test completion.
- **`TestTransaction`**: Static utility class allowing developers to programmatically inspect, end, commit, flag for rollback, or restart the active test transaction inside a test method.
- **`@Sql`**: Declarative annotation to execute SQL scripts before or after test methods to seed test data or purge state.
- **`@SqlConfig`**: Fine-grained configuration for `@Sql` (error handling, comment prefixes, transaction modes).

### The Mental Model
```
[ JUnit 5 Test Method Execution ]
         │
         ├── 1. TransactionalTestExecutionListener starts Transaction TX-1
         │
         ├── 2. Test executes repository operations:
         │      - Inserts Order #100
         │      - Queries database (sees Order #100 within TX-1)
         │
         ├── 3. Test asserts business assertions (PASS)
         │
         └── 4. TransactionalTestExecutionListener executes:
                ROLLBACK TX-1!
                Database returns to pristine original state immediately!
```

### Invariants
1. By default, `@Transactional` on a test method or test class **always rolls back** upon test completion, unlike `@Transactional` on production services which commits upon completion.
2. If an integration test makes HTTP calls over a real network port (`webEnvironment = RANDOM_PORT`), the HTTP server runs on a separate worker thread; the test's rollback transaction **cannot** isolate the HTTP server's database operations.
3. Hibernate Second-Level Cache and in-memory caches (Redis) do **not** automatically roll back when a database transaction rolls back; caches must be evicted explicitly if modified during tests.

---

## 3. How It Works Internally

### The Test Transaction Lifecycle
```
Test Class Execution
   │
   ├── 1. TransactionalTestExecutionListener.beforeTestMethod()
   │      ├─ Looks for @Transactional annotation
   │      ├─ Resolves PlatformTransactionManager
   │      ├─ Starts a new transaction with PROPAGATION_REQUIRED
   │      └─ Binds TransactionStatus to ThreadLocal TransactionSynchronizationManager
   │
   ├── 2. Test method runs:
   │      ├─ SUT executes JPA / JDBC code (participates in current test transaction)
   │      ├─ Developers can call TestTransaction.flagForCommit() or TestTransaction.end()
   │
   └── 3. TransactionalTestExecutionListener.afterTestMethod()
          ├─ Checks rollback flag (default = true)
          ├─ If rollback: tm.rollback(status) ──► Reverts all SQL inserts/updates!
          ├─ If commit: tm.commit(status) ────► Persists data permanently
          └─ Cleans up ThreadLocal transaction synchronization state
```

---

## 4. Syntax & Usage

### 1. Standard Auto-Rollback Test
```java
package com.example.repository;

import com.example.domain.Customer;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.transaction.annotation.Transactional;

import static org.assertj.core.api.Assertions.assertThat;

@SpringBootTest
@Transactional // Automatically rolls back all database modifications after each test!
class CustomerRepositoryTransactionalTest {

    @Autowired
    private CustomerRepository customerRepository;

    @Test
    void shouldInsertCustomerAndRollbackAutomatically() {
        Customer customer = new Customer("Alice", "alice@example.com");
        customerRepository.save(customer);

        assertThat(customer.getId()).isNotNull();
        assertThat(customerRepository.findById(customer.getId())).isPresent();
        // At the end of this method, ROLLBACK is executed! Database remains empty!
    }
}
```

---

## 5. Practical Examples

### Example 1: Using `TestTransaction` to Commit and Query in a Clean Transaction
Testing that an entity is genuinely committed to the database and can be read by a completely independent transaction.

```java
package com.example.repository;

import com.example.domain.Invoice;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.test.context.transaction.TestTransaction;
import org.springframework.transaction.annotation.Transactional;

import java.math.BigDecimal;

import static org.assertj.core.api.Assertions.assertThat;

@SpringBootTest
@Transactional
class InvoicePersistenceTest {

    @Autowired
    private InvoiceRepository invoiceRepository;

    @Test
    void shouldPersistAndReadFromNewTransaction() {
        // 1. In test transaction: Save entity
        Invoice invoice = new Invoice("INV-001", new BigDecimal("499.00"));
        invoiceRepository.save(invoice);

        // 2. Commit current transaction programmatically
        TestTransaction.flagForCommit();
        TestTransaction.end(); // Commits data to database!

        assertThat(TestTransaction.isActive()).isFalse();

        // 3. Start a brand new test transaction
        TestTransaction.start();
        assertThat(TestTransaction.isActive()).isTrue();

        // 4. Read entity from the new transaction
        Invoice retrieved = invoiceRepository.findByInvoiceNumber("INV-001").orElseThrow();
        assertThat(retrieved.getAmount()).isEqualByComparingTo("499.00");

        // 5. Clean up committed state manually or flag for rollback
        invoiceRepository.delete(retrieved);
        TestTransaction.flagForCommit();
        TestTransaction.end();
    }
}
```

### Example 2: Declarative Data Seeding via `@Sql`
Loading baseline lookup data before test execution and purging records afterward.

```java
package com.example.repository;

import com.example.domain.Country;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.test.context.jdbc.Sql;
import org.springframework.test.context.jdbc.SqlConfig;
import org.springframework.transaction.annotation.Transactional;

import java.util.List;

import static org.assertj.core.api.Assertions.assertThat;

@SpringBootTest
@Transactional
class CountryRepositorySqlTest {

    @Autowired
    private CountryRepository countryRepository;

    @Test
    @Sql(scripts = "/test-data/seed-countries.sql", config = @SqlConfig(commentPrefix = "--"))
    @Sql(scripts = "/test-data/cleanup-countries.sql", executionPhase = Sql.ExecutionPhase.AFTER_TEST_METHOD)
    void shouldFindAllSeededEuropeanCountries() {
        List<Country> european = countryRepository.findByContinent("EUROPE");

        assertThat(european)
            .extracting(Country::getCode)
            .contains("FR", "DE", "ES");
    }
}
```

### Example 3: Edge Case – Testing `@TransactionalEventListener(phase = AFTER_COMMIT)`
Event listeners configured with `phase = TransactionPhase.AFTER_COMMIT` will **never** fire if the test transaction rolls back. You must commit the transaction programmatically to test after-commit event listeners.

```java
package com.example.service;

import com.example.domain.Order;
import com.example.event.OrderAuditEventListener;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.test.context.bean.override.mockito.MockitoSpyBean;
import org.springframework.test.context.transaction.TestTransaction;
import org.springframework.transaction.annotation.Transactional;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

@SpringBootTest
@Transactional
class OrderEventCommitListenerTest {

    @Autowired
    private OrderService orderService;

    @MockitoSpyBean
    private OrderAuditEventListener eventListener;

    @Test
    void eventListenerShouldOnlyFireAfterCommit() {
        Order order = orderService.createOrder("ORD-100");

        // BEFORE COMMIT: Event listener has NOT executed yet!
        verify(eventListener, never()).onOrderCommitted(any());

        // Commit transaction programmatically
        TestTransaction.flagForCommit();
        TestTransaction.end();

        // AFTER COMMIT: Event listener executed successfully!
        verify(eventListener).onOrderCommitted(order.getId());
    }
}
```

### Example 4: Failure Anti-Pattern & Production Fix
**Anti-Pattern**: Testing web controllers over HTTP (`webEnvironment = RANDOM_PORT`) with `@Transactional` on the test class, expecting database mutations made by the HTTP controller to roll back.

```java
// BUGGY: RANDOM_PORT runs on a separate Tomcat thread!
// The test's @Transactional does NOT cover the Tomcat worker thread!
// Database rows created by the HTTP call remain committed permanently, corrupting subsequent tests!
@SpringBootTest(webEnvironment = SpringBootTest.WebEnvironment.RANDOM_PORT)
@Transactional
class BrokenHttpTest {
    @Autowired private TestRestTemplate restTemplate;

    @Test
    void testCreate() {
        restTemplate.postForEntity("/api/users", new User("Bob"), Void.class);
        // Bob is NOT rolled back! Bob remains in DB!
    }
}
```

**Production Fix**:
In full HTTP tests, do not use `@Transactional`. Instead, clean up data explicitly using an `@AfterEach` method or run against a throwaway Testcontainer.

```java
// FIXED: Explicit cleanup or isolated testcontainer state
@SpringBootTest(webEnvironment = SpringBootTest.WebEnvironment.RANDOM_PORT)
class CorrectHttpTest {
    @Autowired private UserRepository userRepository;
    @Autowired private TestRestTemplate restTemplate;

    @AfterEach
    void cleanUp() {
        userRepository.deleteAll(); // Explicitly clean up data across thread boundaries
    }
}
```

### Example 5: High-Performance Database Reset with Flyway Test Utilities
Resetting the database cleanly between non-transactional tests without dropping and recreating container instances.

```java
package com.example.testing;

import org.flywaydb.core.Flyway;
import org.junit.jupiter.api.AfterEach;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;

@SpringBootTest
public abstract class CleanDatabaseIntegrationTest {

    @Autowired
    private Flyway flyway;

    @AfterEach
    void resetDatabaseState() {
        // Drops all tables and re-executes all migrations in < 150ms
        flyway.clean();
        flyway.migrate();
    }
}
```

---

## 6. Best Practices

1. **Use `@Transactional` on Service and Repository Tests**: Guarantees fast, automatic rollback with zero database pollution.
2. **Never Use `@Transactional` on Live HTTP Tests**: Tests running with `RANDOM_PORT` execute on separate threads where test transactions have no effect.
3. **Use `TestTransaction` to Test Commit Listeners**: When testing code dependent on `AFTER_COMMIT` lifecycle hooks, use `TestTransaction.flagForCommit()` and `TestTransaction.end()`.
4. **Use `@Sql` for Seed Data**: Keep test code concise by moving large schema inserts or lookup tables into external `.sql` resource files.
5. **Flush the EntityManager before Assertions**: When testing JPA with `@Transactional`, call `entityManager.flush()` before querying to force SQL `INSERT`/`UPDATE` execution within the transaction.
6. **Evict Caches Manually**: Remember that Redis, Caffeine, and Hibernate L2 caches do not roll back automatically with database transactions.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Forgetting `entityManager.flush()` | Asserting directly via JDBC fails because Hibernate has delayed SQL generation in the write-behind cache. | Call `entityManager.flush()` before assertions. |
| Expecting `AFTER_COMMIT` listeners to fire | Test transactions roll back by default, so after-commit listeners are skipped. | Call `TestTransaction.flagForCommit()` and `TestTransaction.end()`. |
| `@Transactional` on `RANDOM_PORT` tests | HTTP server runs on another thread; transactions cannot roll back external thread mutations. | Clean up database in `@AfterEach` or use Flyway clean. |
| Incomplete SQL cleanup scripts | Foreign key constraints block deletions, causing cleanup scripts to fail with integrity violations. | Delete rows in reverse order of foreign key dependency. |
| Dirty in-memory caches after rollback | Entity is rolled back in DB, but cached copy remains in Redis or Caffeine. | Clear cache explicitly in `@AfterEach`. |
| Modifying sequences during tests | Database auto-increment sequences are non-transactional and do not roll back. | Assert on relative entity properties rather than hardcoded sequence IDs. |

---

## 8. Performance & Security Notes

- **Rollback Speed**: Transaction rollback is instantaneous (< 1ms). Rolling back transactions is over 100x faster than deleting rows via SQL `DELETE` or truncating tables.
- **Connection Leakage**: Ensure tests do not leave unclosed database connections. `TransactionalTestExecutionListener` closes active transactions deterministically.
- **Virtual Threads Integration**: Database transactions running on Java 21 Virtual Threads unmount cleanly during JDBC network socket waits without carrier thread blocking.

---

## 9. Comparison / When to Use What

| Database Strategy | Isolation Level | Execution Speed | Commit Behavior | Best Use Case |
|---|---|---|---|---|
| **`@Transactional` Test**| High (Auto-Rollback) | **Instant (< 1ms)** | Never commits | Repositories, domain services, JPA mappings |
| **`TestTransaction`** | High | Fast | Explicit Control | Testing `@TransactionalEventListener(AFTER_COMMIT)` |
| **`@Sql` Clean Scripts** | Moderate | Medium | Real Commits | Non-transactional batch or multi-thread tests |
| **Flyway Clean & Migrate**| Maximum (Pristine DB)| Slower (100-300ms) | Real Commits | End-to-end integration and API gateway tests |

---

## 10. Interview Questions

<details>
<summary>1. Why does `@Transactional` behave differently on a test class compared to a production service?</summary>

**Answer:**
On a production service class, `@Transactional` commits the transaction upon successful method completion and rolls back only on unhandled runtime exceptions.
On a test class, `TransactionalTestExecutionListener` intercepts the execution and **automatically rolls back** the transaction at the end of the test method by default (even if the method succeeds). This guarantees that test data does not pollute the database for subsequent tests.
</details>

<details>
<summary>2. Why must you call `entityManager.flush()` when testing JPA repositories inside a transactional test?</summary>

**Answer:**
JPA and Hibernate utilize a "write-behind" caching strategy. When you invoke `repository.save(entity)`, Hibernate does not immediately execute an SQL `INSERT`; it queues the operation in the first-level persistence context until flush time. If your test executes a custom JPQL query or native JDBC query to verify state, Hibernate might not have executed the SQL yet. Calling `entityManager.flush()` forces Hibernate to synchronize all pending entity changes to the database within the current transaction.
</details>

<details>
<summary>3. What is `TestTransaction` and when would you use it?</summary>

**Answer:**
`TestTransaction` is a static utility provided by Spring TestContext Framework that gives programmatic control over the active test-managed transaction. It allows you to:
- Check if a transaction is currently active (`TestTransaction.isActive()`).
- Flag the transaction for commit rather than rollback (`TestTransaction.flagForCommit()`).
- Immediately commit and end the transaction (`TestTransaction.end()`).
- Start a fresh transaction (`TestTransaction.start()`).
It is critical when testing code that requires real commits, such as `@TransactionalEventListener(phase = AFTER_COMMIT)`.
</details>

<details>
<summary>4. Why doesn't `@Transactional` on a test class work when using `webEnvironment = RANDOM_PORT`?</summary>

**Answer:**
When using `RANDOM_PORT`, Spring Boot boots an embedded web container (Tomcat) on a separate worker thread. The HTTP client (`TestRestTemplate` or `RestClient`) initiates an HTTP request over network sockets that is processed by Tomcat's thread pool. Thread-bound transactional contexts (`ThreadLocal`) do not cross thread boundaries. Therefore, database operations performed by the web server commit in their own transaction and are not covered by the test's rollback transaction.
</details>

<details>
<summary>5. How does `@Commit` differ from `@Rollback`?</summary>

**Answer:**
`@Commit` is simply a direct alias for `@Rollback(false)`. It indicates that the test-managed transaction should be committed to the database upon test method completion rather than rolled back.
</details>

<details>
<summary>6. Do database sequences roll back when a test transaction rolls back?</summary>

**Answer:**
No. In relational databases (PostgreSQL, Oracle, MySQL), sequence generators (`nextval()`) execute outside the transaction boundary to prevent concurrency bottlenecks. Even if the transaction is rolled back, consumed sequence numbers are lost forever. Tests must never assert on hardcoded sequence ID values (e.g., expecting `id == 1`).
</details>

<details>
<summary>7. How can you execute an SQL script before and after a specific test method?</summary>

**Answer:**
Using the `@Sql` annotation:
```java
@Test
@Sql(scripts = "/seed.sql", executionPhase = Sql.ExecutionPhase.BEFORE_TEST_METHOD)
@Sql(scripts = "/purge.sql", executionPhase = Sql.ExecutionPhase.AFTER_TEST_METHOD)
void testMethod() {}
```
</details>

<details>
<summary>8. How do you handle caching when using transactional test rollbacks?</summary>

**Answer:**
In-memory caches (Caffeine, Redis) and Hibernate's Second-Level Cache are not transaction-aware by default. When an entity is inserted and cached, and the database transaction rolls back, the stale entity remains in the cache. To prevent test pollution, you must explicitly evict or clear the cache in an `@AfterEach` method: `cacheManager.getCache("myCache").clear()`.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Basic Transactional Auto-Rollback Test (Easy)</summary>

**Task**: Write an integration test for `ProductRepository` ensuring that a saved product can be found within the test method, but is rolled back after the test completes.

**Solution**:
```java
@SpringBootTest
@Transactional
class ProductRollbackTest {

    @Autowired
    private ProductRepository repository;

    @Test
    void shouldRollbackAutomatically() {
        Product p = repository.save(new Product("Laptop", 1200.0));
        assertThat(repository.findById(p.getId())).isPresent();
    }
}
```
</details>

<details>
<summary>Exercise 2: Testing Explicit Commit with `@Commit` (Easy)</summary>

**Task**: Write a test annotated with `@Commit` that persists an entity and manually cleans it up in an `@AfterEach` method.

**Solution**:
```java
@SpringBootTest
@Transactional
class ExplicitCommitTest {

    @Autowired
    private CustomerRepository repository;

    @Test
    @Commit
    void shouldCommitToDatabase() {
        repository.save(new Customer("TestUser"));
    }

    @AfterEach
    void cleanUp() {
        repository.deleteAll();
    }
}
```
</details>

<details>
<summary>Exercise 3: Programmatic Commit via `TestTransaction` (Medium)</summary>

**Task**: Write a test that saves an entity, commits it using `TestTransaction.end()`, starts a new transaction with `TestTransaction.start()`, and verifies the entity is still present.

**Solution**:
```java
@SpringBootTest
@Transactional
class TestTransactionProgrammaticTest {

    @Autowired
    private UserRepository userRepository;

    @Test
    void shouldCommitAndReopenTransaction() {
        User user = userRepository.save(new User("Alice"));

        TestTransaction.flagForCommit();
        TestTransaction.end(); // Commits data

        TestTransaction.start(); // Starts new transaction

        assertThat(userRepository.findById(user.getId())).isPresent();

        userRepository.deleteById(user.getId());
        TestTransaction.flagForCommit();
        TestTransaction.end();
    }
}
```
</details>

<details>
<summary>Exercise 4: Declarative Seeding with `@Sql` (Medium)</summary>

**Task**: Annotate a test method with `@Sql` to seed a list of categories from `classpath:seed-categories.sql` and verify that `categoryRepository.count()` equals 5.

**Solution**:
```java
@SpringBootTest
@Transactional
class CategorySqlSeedingTest {

    @Autowired
    private CategoryRepository repository;

    @Test
    @Sql("/seed-categories.sql")
    void shouldHaveFiveCategories() {
        assertThat(repository.count()).isEqualTo(5L);
    }
}
```
</details>

<details>
<summary>Exercise 5: Testing `AFTER_COMMIT` Event Listeners (Hard)</summary>

**Task**: Write an integration test verifying that a domain event listener configured with `TransactionPhase.AFTER_COMMIT` is only called once `TestTransaction.end()` commits the transaction.

**Solution**:
```java
@SpringBootTest
@Transactional
class AfterCommitEventTest {

    @Autowired
    private OrderService orderService;

    @MockitoSpyBean
    private OrderEventNotifier notifier;

    @Test
    void shouldOnlyNotifyAfterCommit() {
        orderService.placeOrder("ORD-500");

        verify(notifier, never()).notifyCustomer(any());

        TestTransaction.flagForCommit();
        TestTransaction.end(); // Trigger commit hooks

        verify(notifier).notifyCustomer("ORD-500");
    }
}
```
</details>

<details>
<summary>Exercise 6: Isolated Non-Transactional Cleanup in HTTP Tests (Hard)</summary>

**Task**: Write an integration test using `RANDOM_PORT` that posts a new user via `TestRestTemplate` and cleans up the database state in `@AfterEach` to prevent test contamination.

**Solution**:
```java
@SpringBootTest(webEnvironment = SpringBootTest.WebEnvironment.RANDOM_PORT)
class HttpUserCreationIntegrationTest {

    @Autowired
    private TestRestTemplate restTemplate;

    @Autowired
    private UserRepository userRepository;

    @AfterEach
    void tearDown() {
        userRepository.deleteAll();
    }

    @Test
    void shouldCreateUserOverRealHttp() {
        ResponseEntity<Void> response = restTemplate.postForEntity(
            "/api/users", new CreateUserRequest("Charlie"), Void.class
        );

        assertThat(response.getStatusCode()).isEqualTo(HttpStatus.CREATED);
        assertThat(userRepository.findByUsername("Charlie")).isPresent();
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Automatic Rollback**: `@Transactional` on a test method or class automatically rolls back all changes upon completion.
- **`TestTransaction` Utility**: Provides programmatic control to commit (`flagForCommit()`, `end()`) and start (`start()`) transactions mid-test.
- **Flush JPA Changes**: Call `entityManager.flush()` to force SQL execution before asserting database state.
- **`RANDOM_PORT` Runs on Other Threads**: Test `@Transactional` does not roll back HTTP mutations made over network ports; clean up explicitly in `@AfterEach`.
- **Use `@Sql` for Data Seeding**: Declaratively run initialization and cleanup SQL scripts per test.

---

## 13. Further Reading
- [Spring Framework Reference: Transactional Test Execution](https://docs.spring.io/spring-framework/reference/testing/testcontext-framework/tx.html)
- [Spring TestContext Framework: TestTransaction API](https://docs.spring.io/spring-framework/docs/current/javadoc-api/org/springframework/test/context/transaction/TestTransaction.html)
- [Flyway Database Migration Testing](https://flywaydb.org/documentation/usage/plugins/springboot)
- [Vlad Mihalcea: High-Performance Java Persistence Testing](https://vladmihalcea.com/tutorials/spring/)
- [PostgreSQL Documentation: Transaction Isolation Levels](https://www.postgresql.org/docs/current/transaction-iso.html)
