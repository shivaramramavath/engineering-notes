# Auditing & Optimistic Locking: Spring Data JPA Auditing & Concurrency Control

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate / Advanced |
| **Prerequisites** | `03-spring-data-jpa-fundamentals.md`, `04-entity-mapping-and-relationships.md`, Concurrency & Isolation levels |
| **Estimated Reading Time** | 29 minutes |
| **Status** | Active |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework** | 6.2.x / 7.0.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven / Gradle |
| **Tested With** | Spring Data JPA 3.4.x, Hibernate 6.6.x, PostgreSQL 16 |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
   - [5.1 Base Auditable Entity with @MappedSuperclass](#51-base-auditable-entity-with-mappedsuperclass)
   - [5.2 Spring Security Integration with AuditorAware](#52-spring-security-integration-with-auditoraware)
   - [5.3 Optimistic Concurrency Control with @Version](#53-optimistic-concurrency-control-with-version)
   - [5.4 Anti-Pattern: The Silent Lost Update Problem](#54-anti-pattern-the-silent-lost-update-problem)
   - [5.5 Resilient Optimistic Conflict Retries with Spring Retry](#55-resilient-optimistic-conflict-retries-with-spring-retry)
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
In multi-user enterprise systems, two critical challenges arise regarding state integrity:
1. **Compliance and Accountability**: Tracking *who* created or modified an entity and *when* without writing manual boilerplate code in every service update method.
2. **Concurrent Lost Updates**: When two users load the same entity simultaneously, modify attributes independently, and save back to the database, the second write silently overwrites the first write without warning, corrupting account balances, inventory stock, or order states.

### Historical Evolution
Before standardized auditing, developers wrote custom interceptors or database triggers (`BEFORE INSERT / UPDATE`) to stamp timestamps and usernames. Spring Data JPA introduced `@EnableJpaAuditing` and entity callback listeners (`AuditingEntityListener`) to automate creation/modification tracking seamlessly. To prevent lost updates without resorting to heavy database table locks (Pessimistic Locking), JPA standardized **Optimistic Locking** via the `@Version` annotation.

### Real-World Relevance
Regulatory standards (SOC2, HIPAA, PCI-DSS) strictly mandate immutable audit trails for sensitive data modifications. Simultaneously, modern high-concurrency microservices (e-commerce carts, seat reservations, bank ledgers) require non-blocking optimistic locking to ensure data consistency while maximizing transactional throughput.

---

## 2. Core Concepts

### JPA Auditing Annotations
- `@CreatedDate`: Automatically populated with the current timestamp when the entity is first persisted.
- `@CreatedBy`: Automatically populated with the current authenticated principal upon creation.
- `@LastModifiedDate`: Automatically updated with the current timestamp on every flush that contains modifications.
- `@LastModifiedBy`: Automatically updated with the modifying user's identifier on update.
- `AuditorAware<T>`: An SPI interface implemented by the application to supply the current auditor (e.g., from `SecurityContextHolder`).

### Optimistic Concurrency Control (@Version)
Optimistic locking assumes that multiple transactions can complete without affecting each other. Each entity table includes a `version` column (typically `Long` or `Integer`). When updating an entity, Hibernate appends the expected version to the SQL `WHERE` clause:
```sql
UPDATE products 
SET price = ?, version = version + 1 
WHERE id = ? AND version = ?;
```
If another transaction updated the row in the interim, the `version` in the database has changed. The `UPDATE` statement affects **0 rows**, prompting Hibernate to throw an `OptimisticLockException` (wrapped by Spring in `ObjectOptimisticLockingFailureException`).

---

## 3. How It Works Internally

```
+------------------------------------------------------------------------------------+
|                      Optimistic Locking Collision Workflow                         |
+------------------------------------------------------------------------------------+
|                                                                                    |
|  Transaction 1 (User A)                         Transaction 2 (User B)             |
|       |                                              |                             |
|       |-- 1. Read Product (id=1, price=100, ver=1) ->|                             |
|       |                                              |-- 2. Read Product (ver=1)   |
|       |                                              |                             |
|       |-- 3. Modify Price = $110                     |-- 4. Modify Price = $120    |
|       |                                              |                             |
|       |-- 5. Commit Transaction:                     |                             |
|       |   UPDATE ... ver=2 WHERE id=1 AND ver=1;     |                             |
|       |   (1 row affected -> SUCCESS!)               |                             |
|       v                                              |                             |
|                                                      |-- 6. Commit Transaction:    |
|                                                      |   UPDATE ... WHERE ver=1;   |
|                                                      |   (0 rows affected!)        |
|                                                      |         |                   |
|                                                      |         v                   |
|                                                      |   ROLLBACK!                 |
|                                                      |   Throws OptimisticLock-    |
|                                                      |   Exception!                |
|                                                      v                             |
+------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### Enabling JPA Auditing
```java
@Configuration
@EnableJpaAuditing(auditorAwareRef = "auditorAwareProvider")
public class JpaAuditingConfig {

    @Bean
    public AuditorAware<String> auditorAwareProvider() {
        return new SpringSecurityAuditorAware();
    }
}
```

### Auditable Base Entity
```java
@MappedSuperclass
@EntityListeners(AuditingEntityListener.class)
public abstract class AuditableEntity {

    @CreatedDate
    @Column(name = "created_at", nullable = false, updatable = false)
    private Instant createdAt;

    @CreatedBy
    @Column(name = "created_by", nullable = false, updatable = false)
    private String createdBy;

    @LastModifiedDate
    @Column(name = "last_modified_at")
    private Instant lastModifiedAt;

    @LastModifiedBy
    @Column(name = "last_modified_by")
    private String lastModifiedBy;

    @Version
    @Column(name = "version")
    private Long version;
}
```

---

## 5. Practical Examples

### 5.1 Base Auditable Entity with @MappedSuperclass
```java
package com.example.jpa.domain;

import jakarta.persistence.Column;
import jakarta.persistence.EntityListeners;
import jakarta.persistence.MappedSuperclass;
import jakarta.persistence.Version;
import org.springframework.data.annotation.CreatedBy;
import org.springframework.data.annotation.CreatedDate;
import org.springframework.data.annotation.LastModifiedBy;
import org.springframework.data.annotation.LastModifiedDate;
import org.springframework.data.jpa.domain.support.AuditingEntityListener;

import java.time.Instant;

@MappedSuperclass
@EntityListeners(AuditingEntityListener.class)
public abstract class BaseEntity {

    @CreatedDate
    @Column(name = "created_at", nullable = false, updatable = false)
    private Instant createdAt;

    @CreatedBy
    @Column(name = "created_by", nullable = false, updatable = false)
    private String createdBy;

    @LastModifiedDate
    @Column(name = "updated_at")
    private Instant updatedAt;

    @LastModifiedBy
    @Column(name = "updated_by")
    private String updatedBy;

    @Version
    @Column(name = "version", nullable = false)
    private Long version;

    public Instant getCreatedAt() { return createdAt; }
    public String getCreatedBy() { return createdBy; }
    public Instant getUpdatedAt() { return updatedAt; }
    public String getUpdatedBy() { return updatedBy; }
    public Long getVersion() { return version; }
}
```

### 5.2 Spring Security Integration with AuditorAware
```java
package com.example.jpa.config;

import org.springframework.data.domain.AuditorAware;
import org.springframework.security.core.Authentication;
import org.springframework.security.core.context.SecurityContext;
import org.springframework.security.core.context.SecurityContextHolder;
import org.springframework.stereotype.Component;

import java.util.Optional;

@Component("auditorAwareProvider")
public class SpringSecurityAuditorAware implements AuditorAware<String> {

    @Override
    public Optional<String> getCurrentAuditor() {
        return Optional.ofNullable(SecurityContextHolder.getContext())
            .map(SecurityContext::getAuthentication)
            .filter(Authentication::isAuthenticated)
            .map(Authentication::getName)
            .or(() -> Optional.of("SYSTEM_AUTOMATION"));
    }
}
```

### 5.3 Optimistic Concurrency Control with @Version
```java
package com.example.jpa.domain;

import jakarta.persistence.*;
import java.math.BigDecimal;

@Entity
@Table(name = "bank_accounts")
public class BankAccount extends BaseEntity {

    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @Column(nullable = false, unique = true)
    private String accountNumber;

    @Column(nullable = false)
    private BigDecimal balance;

    protected BankAccount() {}

    public BankAccount(String accountNumber, BigDecimal initialBalance) {
        this.accountNumber = accountNumber;
        this.balance = initialBalance;
    }

    public void debit(BigDecimal amount) {
        if (amount.compareTo(BigDecimal.ZERO) <= 0) {
            throw new IllegalArgumentException("Debit amount must be positive");
        }
        if (this.balance.compareTo(amount) < 0) {
            throw new IllegalStateException("Insufficient funds");
        }
        this.balance = this.balance.subtract(amount);
    }

    public Long getId() { return id; }
    public BigDecimal getBalance() { return balance; }
}
```

### 5.4 Anti-Pattern: The Silent Lost Update Problem
```java
// =========================================================================
// ANTI-PATTERN: Entity without @Version in high-concurrency environments
// =========================================================================
@Entity
public class UnsafeInventoryItem {
    @Id
    private Long id;
    private int availableStock;

    // NO @Version FIELD!
    // If Thread A reads stock = 10, and Thread B reads stock = 10:
    // Thread A reserves 3 -> writes 7.
    // Thread B reserves 5 -> writes 5.
    // The final stock in DB is 5! Thread A's reservation of 3 was SILENTLY LOST!
}

// =========================================================================
// PRODUCTION FIX: Enforcing @Version Concurrency Guard
// =========================================================================
@Entity
public class SafeInventoryItem {
    @Id
    private Long id;
    private int availableStock;

    @Version
    private Long version; // Guarantees thread collision detection!

    public void deductStock(int count) {
        if (this.availableStock < count) throw new IllegalStateException("Out of stock");
        this.availableStock -= count;
    }
}
```

### 5.5 Resilient Optimistic Conflict Retries with Spring Retry
```java
package com.example.jpa.service;

import com.example.jpa.domain.BankAccount;
import com.example.jpa.repository.BankAccountRepository;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.dao.OptimisticLockingFailureException;
import org.springframework.retry.annotation.Backoff;
import org.springframework.retry.annotation.Recover;
import org.springframework.retry.annotation.Retryable;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.math.BigDecimal;

@Service
public class ResilientPaymentService {

    private static final Logger log = LoggerFactory.getLogger(ResilientPaymentService.class);
    private final BankAccountRepository repository;

    public ResilientPaymentService(BankAccountRepository repository) {
        this.repository = repository;
    }

    /**
     * Automatically retries the transaction up to 3 times with exponential backoff
     * when a concurrent modification collision occurs.
     */
    @Retryable(
        retryFor = { OptimisticLockingFailureException.class },
        maxAttempts = 3,
        backoff = @Backoff(delay = 50, multiplier = 2.0)
    )
    @Transactional
    public void processPaymentWithRetry(Long accountId, BigDecimal amount) {
        log.info("Attempting payment deduction for account: {}", accountId);

        BankAccount account = repository.findById(accountId)
            .orElseThrow(() -> new IllegalArgumentException("Account not found: " + accountId));

        account.debit(amount);
        // If a collision occurs at commit time, OptimisticLockingFailureException is caught by @Retryable
    }

    /**
     * Recovery fallback if all retry attempts are exhausted.
     */
    @Recover
    public void handleLockFailure(OptimisticLockingFailureException ex, Long accountId, BigDecimal amount) {
        log.error("Exhausted retries processing payment for account {} due to persistent collisions", accountId, ex);
        throw new IllegalStateException("System busy; please retry your transaction shortly.");
    }
}
```

---

## 6. Best Practices

1. **Include `@Version` on All High-Concurrency Entities**: Any entity that can be updated concurrently by multiple users or background jobs must have a `@Version` attribute.
2. **Always Use `Long` or `Integer` for `@Version`**: Avoid using `java.sql.Timestamp` for versions; system clocks lack sufficient precision on modern multi-core processors, leading to collision false-positives.
3. **Use `@MappedSuperclass` for Auditing**: Consolidate audit metadata (`createdAt`, `createdBy`, `version`) into a reusable base class to ensure uniformity across the entire domain model.
4. **Implement Automatic Retry for Optimistic Lock Failures**: Pair optimistic locking with Spring Retry (`@Retryable(retryFor = OptimisticLockingFailureException.class)`) or Resilience4j to resolve transient conflicts transparently.
5. **Pass the Version in REST APIs**: In web applications, expose the entity version as an `ETag` header or DTO property. When updating, verify that the submitted version matches the current database version.
6. **Ensure `updatable = false` on Creation Fields**: Mark `@CreatedDate` and `@CreatedBy` with `@Column(updatable = false)` to prevent accidental mutation during entity update cycles.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Forgetting `@EnableJpaAuditing`** | Audit annotations (`@CreatedDate`, `@CreatedBy`) remain inactive, leaving fields `null`. | Add `@EnableJpaAuditing` to a `@Configuration` class. |
| **Manually Modifying the `@Version` Field** | Modifying the version programmatically breaks Hibernate's internal dirty tracking and collision checking. | Never write a public setter for the `@Version` property. |
| **Using `Timestamp` for Versioning** | High-speed concurrent writes within the same millisecond receive identical timestamps, allowing lost updates. | Use a numerical counter (`Long` or `Integer`). |
| **Auditing Native or Direct JDBC Updates** | `AuditingEntityListener` operates via JPA lifecycle events; raw SQL queries (`UPDATE ...`) bypass auditing listeners. | Manually populate audit columns in native SQL statements. |
| **Catching `OptimisticLockException` Inside the Same Transaction** | Once an optimistic locking exception occurs, the underlying database transaction is marked rollback-only. | Move retry logic outside the `@Transactional` boundary (e.g. using `@Retryable`). |
| **Missing `EntityListeners(AuditingEntityListener.class)`** | The entity does not register with Spring Data's auditing callback infrastructure. | Add the listener annotation to the entity or `@MappedSuperclass`. |

---

## 8. Performance & Security Notes

### Performance
- **Zero Lock Contention**: Optimistic locking does not hold database row or table locks (`SELECT FOR UPDATE`), keeping database throughput high and eliminating thread blocking in read-heavy applications.
- **Minimal Update Overhead**: Optimistic locking adds only a simple `WHERE version = ?` predicate to existing `UPDATE` statements, incurring negligible database CPU cost.

### Security
- **Immutable Audit Trails**: Auditing protects against repudiation by maintaining a strict, non-bypassable log of record authors and timestamps for compliance certifications.
- **Audit Tampering Prevention**: Restricting `@CreatedBy` with `updatable = false` prevents users from overwriting original record provenance during administrative updates.

---

## 9. Comparison / When to Use What

| Feature | Optimistic Locking (@Version) | Pessimistic Locking (SELECT FOR UPDATE) |
| :--- | :--- | :--- |
| **Mechanism** | Numerical check at update (`WHERE version = ?`) | Database row-level exclusive locks |
| **Contention Level** | Low to Moderate (Collisions are rare) | Very High (Collisions are frequent) |
| **Throughput** | **Extremely High** | Low (Transactions queue and wait) |
| **Deadlock Risk** | **Zero** | High |
| **Failure Handling** | Throws exception; requires retry | Application blocks until lock released or timeout |
| **Ideal For** | E-commerce orders, user profiles, document CMS | High-frequency stock tickers, seat reservations |

---

## 10. Interview Questions

<details>
<summary><strong>1. How does JPA Optimistic Locking work under the hood?</strong></summary>

Optimistic locking adds a `@Version` attribute (integer/long) to the entity. When an `UPDATE` is issued, Hibernate generates:
```sql
UPDATE entity_table SET column = ?, version = version + 1 
WHERE id = ? AND version = ?
```
If another transaction modified the row concurrently, the version in the database is higher, causing the statement to update zero rows. Hibernate detects that the affected row count is 0 and throws `OptimisticLockException`.
</details>

<details>
<summary><strong>2. What is the difference between Optimistic and Pessimistic Locking?</strong></summary>

- **Optimistic Locking** assumes conflicts are rare. It avoids locking the database row during read operations and checks for collisions only at write time via version comparisons.
- **Pessimistic Locking** assumes conflicts are frequent. It acquires exclusive database-level locks (e.g., `SELECT ... FOR UPDATE`) at the time of read, preventing any other transaction from reading or modifying the locked row until the transaction commits.
</details>

<details>
<summary><strong>3. Why should retry logic for <code>OptimisticLockingFailureException</code> reside outside the <code>@Transactional</code> method?</strong></summary>

When an optimistic locking collision occurs at commit time, the underlying JPA `EntityManager` and database transaction are marked as rollback-only. You cannot catch the exception inside the same transactional method and try to update again; you must restart a completely new transaction with a fresh read from the database.
</details>

<details>
<summary><strong>4. How does Spring Data JPA Auditing automatically populate the current user?</strong></summary>

Spring Data provides the `AuditorAware<T>` interface. The application implements this interface (typically extracting the current `Authentication` from Spring Security's `SecurityContextHolder`). When an entity is persisted or modified, the `AuditingEntityListener` queries the registered `AuditorAware` bean and injects the returned value into fields annotated with `@CreatedBy` or `@LastModifiedBy`.
</details>

<details>
<summary><strong>5. Why is a numerical version (Long/Integer) preferred over a Date/Timestamp for <code>@Version</code>?</strong></summary>

Java and database timestamps are bounded by system clock precision (often 1 millisecond or microsecond). Under extreme concurrency, two distinct transactions can complete within the exact same microsecond, producing identical timestamps and allowing a silent lost update. A numerical counter increments monotonically with zero risk of collision false-positives.
</details>

<details>
<summary><strong>6. What is the role of <code>@MappedSuperclass</code> in establishing auditing?</strong></summary>

`@MappedSuperclass` designates a class whose mapping information is inherited by child entities. It allows developers to define audit fields (`createdAt`, `updatedAt`, `createdBy`, `version`) and entity listeners once in a parent class, avoiding code duplication across dozens of entities without creating a separate database table.
</details>

<details>
<summary><strong>7. What happens if you execute a bulk <code>@Modifying</code> query on an entity with <code>@Version</code>?</strong></summary>

Direct bulk `@Modifying` queries (e.g., `UPDATE Product p SET p.price = ...`) bypass Hibernate's dirty-checking engine and lifecycle listeners. They do *not* automatically increment the version column unless explicitly written in the JPQL statement (`p.version = p.version + 1`).
</details>

<details>
<summary><strong>8. How can you expose optimistic locking to front-end clients via HTTP REST APIs?</strong></summary>

REST APIs typically expose the `@Version` field using the HTTP `ETag` response header (e.g., `ETag: "12"`). When updating a resource, the client sends the version back in the `If-Match: "12"` header. If the version in the database has advanced, the server returns HTTP `412 Precondition Failed` or `409 Conflict`.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Basic Auditable MappedSuperclass</strong></summary>

**Task**: Create an abstract base class `AuditableBase` annotated with `@MappedSuperclass` and `@EntityListeners(AuditingEntityListener.class)` containing `@CreatedDate` and `@LastModifiedDate` using `java.time.Instant`.

```java
@MappedSuperclass
@EntityListeners(AuditingEntityListener.class)
public abstract class AuditableBase {
    @CreatedDate
    @Column(nullable = false, updatable = false)
    private Instant createdAt;

    @LastModifiedDate
    private Instant updatedAt;

    public Instant getCreatedAt() { return createdAt; }
    public Instant getUpdatedAt() { return updatedAt; }
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Entity with Numerical @Version</strong></summary>

**Task**: Create a `Document` entity with fields `id`, `title`, `content`, and a `Long version` attribute configured for optimistic locking.

```java
@Entity
public class Document {
    @Id @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    private String title;
    private String content;

    @Version
    private Long version;

    // Getters and business methods...
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Security AuditorAware Implementation</strong></summary>

**Task**: Implement an `AuditorAware<UUID>` bean that retrieves the current user's UUID from a custom Spring Security `CustomUserPrincipal`.

```java
@Component
public class SecurityAuditorAware implements AuditorAware<UUID> {
    @Override
    public Optional<UUID> getCurrentAuditor() {
        Authentication auth = SecurityContextHolder.getContext().getAuthentication();
        if (auth != null && auth.getPrincipal() instanceof CustomUserPrincipal principal) {
            return Optional.of(principal.getUserId());
        }
        return Optional.empty();
    }
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Optimistic Locking Collision Unit Test</strong></summary>

**Task**: Write an integration test that simulates two concurrent transactions updating the same entity version, asserting that the second transaction throws `OptimisticLockingFailureException`.

```java
@Test
void testOptimisticLockingFailure() {
    Document doc = repository.save(new Document("Title", "V1"));

    Document doc1 = repository.findById(doc.getId()).orElseThrow();
    Document doc2 = repository.findById(doc.getId()).orElseThrow();

    doc1.setContent("Updated by Thread 1");
    repository.saveAndFlush(doc1); // Increments version to 1

    doc2.setContent("Updated by Thread 2");
    assertThatThrownBy(() -> repository.saveAndFlush(doc2))
        .isInstanceOf(OptimisticLockingFailureException.class);
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Spring Retry Integration for Optimistic Conflicts</strong></summary>

**Task**: Implement a resilient service method with `@Retryable` that re-reads and updates an account balance up to 4 times with exponential backoff upon encountering optimistic lock collisions.

```java
@Retryable(
    retryFor = OptimisticLockingFailureException.class,
    maxAttempts = 4,
    backoff = @Backoff(delay = 100, multiplier = 1.5)
)
@Transactional
public void deductFunds(Long accountId, BigDecimal amount) {
    Account account = accountRepository.findById(accountId).orElseThrow();
    account.withdraw(amount);
}
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Custom DateTimeProvider for Mocking Audited Time</strong></summary>

**Task**: Configure a custom `DateTimeProvider` bean to control time generation during automated auditing tests.

```java
@TestConfiguration
public class TestAuditConfig {
    @Bean
    public DateTimeProvider fixedDateTimeProvider() {
        return () -> Optional.of(Instant.parse("2026-01-01T00:00:00Z"));
    }
}

// In Configuration: @EnableJpaAuditing(dateTimeProviderRef = "fixedDateTimeProvider")
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------------+
| Enable Auditing: @EnableJpaAuditing(auditorAwareRef = "beanName")                     |
| Annotations:     @CreatedDate, @LastModifiedDate, @CreatedBy, @LastModifiedBy         |
| Base Class:      @MappedSuperclass + @EntityListeners(AuditingEntityListener.class)  |
| Concurrency:     @Version private Long version; (WHERE id = ? AND version = ?)        |
| Resilience:      @Retryable(retryFor = OptimisticLockingFailureException.class)       |
| HTTP Mapping:    Expose version via ETag / If-Match header -> Return 412 on conflict  |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Spring Data JPA Auditing Documentation](https://docs.spring.io/spring-data/jpa/reference/auditing.html)
- [Vlad Mihalcea: How Optimistic Locking Works in JPA and Hibernate](https://vladmihalcea.com/jpa-optimistic-locking/)
- [Spring Retry Official Reference Guide](https://github.com/spring-projects/spring-retry)
- [Hibernate 6.6 Concurrency Control User Guide](https://docs.jboss.org/hibernate/orm/6.6/userguide/html_single/Hibernate_User_Guide.html#locking)
