# Spring Declarative Transactions Cheatsheet

| Metadata | Details |
|---|---|
| **Topic** | Declarative Transactions & Isolation Cheatsheet |
| **Track** | Java Spring Boot |
| **Difficulty** | Intermediate to Advanced |
| **Prerequisites** | Java 21, ACID Database Transactions, Spring AOP |
| **Java Version** | Java 21 / 25 LTS |
| **Spring Boot** | 3.4.x / 4.x |
| **Tested With** | PostgreSQL, HikariCP, Spring Data JPA |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [Internals & Architecture](#3-internals--architecture)
4. [Syntax & Categorized Reference](#4-syntax--categorized-reference)
5. [5 Practical Production Examples](#5-5-practical-production-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes to Avoid](#7-common-mistakes-to-avoid)
8. [Performance & Security Considerations](#8-performance--security-considerations)
9. [Comparison Table: Propagation Behaviors](#9-comparison-table-propagation-behaviors)
10. [Technical Interview Questions & Answers](#10-technical-interview-questions--answers)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading & References](#13-further-reading--references)

---

## 1. What & Why
Database transactions maintain data consistency, atomicity, and isolation across concurrent writes. Spring's declarative transaction management via `@Transactional` abstracts underlying connection management, commit/rollback coordination, and transaction synchronization.

Incorrect configuration of propagation, isolation, or rollback rules can cause data corruption, phantom reads, connection pool starvation, and silent transaction rollbacks.

---

## 2. Core Concepts
- **Transaction Manager**: `PlatformTransactionManager` (or reactive `ReactiveTransactionManager`) managing JDBC connections and commits.
- **Propagation**: Dictates how transaction boundaries interact when one transactional method calls another.
- **Isolation**: Determines visibility of in-flight changes across concurrent transactions (Read Committed, Repeatable Read, Serializable).
- **Rollback Rules**: By default, Spring rolls back transactions only on unchecked exceptions (`RuntimeException` and `Error`).
- **Programmatic Control**: `TransactionTemplate` for fine-grained transaction boundaries without proxy constraints.

---

## 3. Internals & Architecture
Spring Transaction Interception Pipeline via AOP Proxy:

```
[Client Call: orderService.placeOrder()]
                 |
                 v
+-------------------------------------------------------------+
|                     Spring AOP Proxy                        |
+-------------------------------------------------------------+
                 |
                 v
+-------------------------------------------------------------+
|                 TransactionInterceptor                      |
|  1. Obtain PlatformTransactionManager (e.g. JpaTransaction) |
|  2. Query TransactionAttribute (propagation, isolation)     |
|  3. Call getTransaction() -> binds Connection to ThreadLocal|
|     (TransactionSynchronizationManager)                     |
+-------------------------------------------------------------+
                 |
                 v
+-------------------------------------------------------------+
|                  Target Service Invocation                  |
|                  orderService.placeOrder()                  |
|            (Executes SQL via Persistence Context)           |
+-------------------------------------------------------------+
                 |
        +--------+--------+
        |                 |
(Exception Thrown)   (Normal Exit)
        |                 |
        v                 v
+----------------+ +------------------------------------------+
|  Rollback Check| | commitTransactionAfterReturning()        |
|  Match rules   | |  - Flushes JPA Persistence Context       |
|  tx.rollback() | |  - Commits JDBC connection               |
+----------------+ |  - Closes & unbinds connection           |
                   +------------------------------------------+
```

---

## 4. Syntax & Categorized Reference

### 4.1 `@Transactional` Attributes
| Attribute | Type | Default | Description |
|---|---|---|---|
| `propagation` | `Propagation` | `REQUIRED` | Behavior when an active transaction exists |
| `isolation` | `Isolation` | `DEFAULT` | Database transaction isolation level |
| `timeout` | `int` (seconds) | `-1` | Maximum duration before rollback |
| `readOnly` | `boolean` | `false` | Read-only hint optimizing Hibernate flush mode |
| `rollbackFor` | `Class<? extends Throwable>[]` | `{}` | Checked exceptions triggering rollback |
| `noRollbackFor` | `Class<? extends Throwable>[]` | `{}` | Exceptions that should not trigger rollback |

### 4.2 Propagation Levels
- `REQUIRED`: Joins existing transaction or creates a new one if none exists (Default).
- `REQUIRES_NEW`: Suspends any current transaction and creates a new, independent physical transaction.
- `MANDATORY`: Must execute within an existing transaction; throws `IllegalTransactionStateException` if none exists.
- `SUPPORTS`: Executes within an existing transaction if one exists; executes non-transactionally otherwise.
- `NOT_SUPPORTED`: Suspends current transaction and executes non-transactionally.
- `NEVER`: Throws exception if an active transaction exists.
- `NESTED`: Executes within a nested transaction using database savepoints (JDBC only).

---

## 5. 5 Practical Production Examples

### Example 1: Standard Service Transaction with Checked Exception Rollback
```java
@Service
public class OrderProcessingService {

    private final OrderRepository orderRepository;
    private final PaymentGateway paymentGateway;

    public OrderProcessingService(OrderRepository orderRepository, PaymentGateway paymentGateway) {
        this.orderRepository = orderRepository;
        this.paymentGateway = paymentGateway;
    }

    @Transactional(rollbackFor = {PaymentFailedException.class, InventoryShortageException.class}, timeout = 10)
    public OrderConfirmation processOrder(OrderCommand command) throws PaymentFailedException {
        Order order = orderRepository.save(new Order(command));
        paymentGateway.charge(order.getId(), command.amount());
        return new OrderConfirmation(order.getId(), OrderStatus.CONFIRMED);
    }
}
```

### Example 2: Audit Logging in an Independent Transaction (REQUIRES_NEW)
```java
@Service
public class AuditLogService {

    private final AuditRepository auditRepository;

    public AuditLogService(AuditRepository auditRepository) {
        this.auditRepository = auditRepository;
    }

    @Transactional(propagation = Propagation.REQUIRES_NEW)
    public void recordSecurityEvent(String username, String action, String ipAddress) {
        auditRepository.save(new SecurityAuditEvent(username, action, ipAddress, Instant.now()));
    }
}
```

### Example 3: Read-Only Optimized Transaction
```java
@Service
@Transactional(readOnly = true)
public class CatalogQueryService {

    private final ProductRepository productRepository;

    public CatalogQueryService(ProductRepository productRepository) {
        this.productRepository = productRepository;
    }

    public ProductDetails getProduct(UUID id) {
        return productRepository.findById(id)
                .map(ProductDetails::fromEntity)
                .orElseThrow(() -> new ResourceNotFoundException("Product not found: " + id));
    }
}
```

### Example 4: Fine-Grained Programmatic Boundary with TransactionTemplate
```java
@Service
public class ReportGenerationService {

    private final TransactionTemplate transactionTemplate;
    private final ReportDataRepository reportDataRepository;
    private final EmailService emailService;

    public ReportGenerationService(PlatformTransactionManager txManager,
                                   ReportDataRepository reportDataRepository,
                                   EmailService emailService) {
        this.transactionTemplate = new TransactionTemplate(txManager);
        this.transactionTemplate.setIsolationLevel(TransactionDefinition.ISOLATION_READ_COMMITTED);
        this.transactionTemplate.setTimeout(15);
        this.reportDataRepository = reportDataRepository;
        this.emailService = emailService;
    }

    public void generateAndEmailReport(UUID reportId) {
        // Step 1: Database read/write strictly inside transaction boundary
        ReportData data = transactionTemplate.execute(status -> {
            ReportData d = reportDataRepository.fetchReportData(reportId);
            reportDataRepository.markAsExported(reportId);
            return d;
        });

        // Step 2: Slow external email I/O outside transaction (avoids holding DB connection)
        emailService.sendReport(data);
    }
}
```

### Example 5: Post-Commit Hook via TransactionSynchronizationManager
```java
@Service
public class CustomerRegistrationService {

    private final CustomerRepository customerRepository;
    private final WelcomeEmailEventPublisher publisher;

    public CustomerRegistrationService(CustomerRepository customerRepository, WelcomeEmailEventPublisher publisher) {
        this.customerRepository = customerRepository;
        this.publisher = publisher;
    }

    @Transactional
    public Customer register(CustomerDto dto) {
        Customer customer = customerRepository.save(new Customer(dto));

        TransactionSynchronizationManager.registerSynchronization(new TransactionSynchronization() {
            @Override
            public void afterCommit() {
                // Guaranteed to run only after the database transaction commits successfully
                publisher.publishWelcomeEmail(customer.getId(), customer.getEmail());
            }
        });

        return customer;
    }
}
```

---

## 6. Best Practices
1. **Keep Transactions Short**: Do not make external HTTP, REST, or message queue calls inside `@Transactional` methods to avoid connection pool starvation.
2. **Explicitly Specify `rollbackFor = Exception.class`**: Prevents checked business exceptions from silently failing to roll back.
3. **Use `readOnly = true` for Reads**: Allows Hibernate to disable dirty-checking flush cycles and enables database replica routing.
4. **Beware of Self-Invocation**: Internal calls to `@Transactional` methods bypass proxy advice.
5. **Use `@TransactionalEventListener(phase = TransactionPhase.AFTER_COMMIT)`**: Decouples side effects (emails, webhooks) from transaction commits.

---

## 7. Common Mistakes to Avoid

| Mistake | Consequence | Remedy |
|---|---|---|
| Self-invocation of `@Transactional` method | Proxy is bypassed; method executes without an active transaction | Inject self bean or refactor into a separate service |
| Placing `@Transactional` on private methods | Silently ignored by Spring AOP dynamic proxies | Make the method `public` |
| Calling slow HTTP services inside `@Transactional` | Holds DB connection open during network latency, exhausting pool | Extract network calls outside transaction boundary |
| Relying on default rollback for checked exceptions | Transaction commits even when checked exception is thrown | Add `rollbackFor = Exception.class` |
| Using `REQUIRES_NEW` without pool headroom | Nested transactions take 2 connections per thread, risking pool deadlock | Size connection pool properly or refactor orchestration |
| Catching exceptions inside service without re-throwing | Transaction manager assumes method completed cleanly and commits | Re-throw exception or call `TransactionAspectSupport.currentTransactionStatus().setRollbackOnly()` |

---

## 8. Performance & Security Considerations
- **Connection Leak Prevention**: Always specify `timeout` on `@Transactional` methods to abort stalled queries.
- **Connection Pool Sizing**: Nested `REQUIRES_NEW` transactions require at least 2 connections per request thread. A pool smaller than `(max_threads * 2)` will deadlock.
- **Dirty Checking Elimination**: `readOnly = true` instructs Hibernate's session not to track entity snapshot changes, saving CPU and memory.

---

## 9. Comparison Table: Propagation Behaviors

| Propagation | Active Transaction Exists | No Active Transaction |
|---|---|---|
| `REQUIRED` | Joins active transaction | Creates new transaction |
| `REQUIRES_NEW` | Suspends active; creates new | Creates new transaction |
| `MANDATORY` | Joins active transaction | Throws exception |
| `SUPPORTS` | Joins active transaction | Executes non-transactionally |
| `NOT_SUPPORTED` | Suspends active; runs without | Executes non-transactionally |
| `NEVER` | Throws exception | Executes non-transactionally |

---

## 10. Technical Interview Questions & Answers

<details>
<summary>1. Why does Spring not roll back transactions on checked exceptions by default?</summary>
Spring follows the EJB tradition where checked exceptions are considered predictable business outcomes (e.g. `InsufficientFundsException`), while unchecked exceptions (`RuntimeException`) represent unexpected system failures.
</details>

<details>
<summary>2. How do you resolve the self-invocation limitation in Spring @Transactional?</summary>
1. Refactor the method into a separate Spring bean.
2. Self-inject the bean using `@Autowired private MyService self;`.
3. Use AspectJ compile-time or load-time weaving instead of Spring dynamic proxies.
</details>

<details>
<summary>3. What happens if a REQUIRES_NEW transaction throws an exception?</summary>
The `REQUIRES_NEW` transaction rolls back. If the exception is not caught by the calling method, it propagates and triggers rollback in the outer calling transaction as well.
</details>

<details>
<summary>4. What optimization does `@Transactional(readOnly = true)` provide in Hibernate?</summary>
It sets the Hibernate FlushMode to `MANUAL`, disabling dirty-checking on entity snapshots during flush, and sets the JDBC connection read-only flag, enabling read replica routing.
</details>

<details>
<summary>5. How does TransactionSynchronizationManager work?</summary>
It uses `ThreadLocal` storage to associate transaction resources (e.g., active JDBC `Connection`, Hibernate `Session`) with the executing thread.
</details>

<details>
<summary>6. What is the difference between `@Transactional` and `TransactionTemplate`?</summary>
`@Transactional` is declarative, AOP-proxy-based, and wraps entire methods. `TransactionTemplate` is programmatic, enabling precise programmatic blocks within a method to avoid holding connections during non-database operations.
</details>

<details>
<summary>7. How does `@TransactionalEventListener` guarantee after-commit execution?</summary>
It registers a `TransactionSynchronization` callback with the transaction manager that invokes the event listener only after the database transaction successfully issues a commit.
</details>

<details>
<summary>8. Can `@Transactional` be used with virtual threads?</summary>
Yes. In Java 21+, virtual threads support `ThreadLocal` variables used by `TransactionSynchronizationManager`, though long pinning in synchronized blocks should be avoided.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary>Exercise 1: Configure checked exception rollback</summary>
Write a bank transfer method with `@Transactional` configured to roll back on custom `InsufficientBalanceException`.
</details>

<details>
<summary>Exercise 2: Implement independent audit logging with REQUIRES_NEW</summary>
Create a test showing that if the primary transaction fails and rolls back, the audit log in `REQUIRES_NEW` still commits.
</details>

<details>
<summary>Exercise 3: Refactor with TransactionTemplate to avoid long connections</summary>
Take a method that calls an external payment gateway inside `@Transactional` and refactor it with `TransactionTemplate`.
</details>

<details>
<summary>Exercise 4: Test self-invocation proxy bypass</summary>
Write a test demonstrating that calling `@Transactional` internally from within the same class fails to create a transaction.
</details>

<details>
<summary>Exercise 5: Implement @TransactionalEventListener</summary>
Publish an `OrderCreatedEvent` and verify the consumer only executes after successful database commit.
</details>

<details>
<summary>Exercise 6: Configure programmatic rollback</summary>
Use `TransactionAspectSupport.currentTransactionStatus().setRollbackOnly()` in a catch block and assert rollback.
</details>

---

## 12. Summary & Cheat Sheet

```
+---------------------------------------------------------------------------------+
|                  SPRING TRANSACTIONS CHEAT SHEET                                |
+---------------------------------------------------------------------------------+
| SYNTAX         | @Transactional(propagation = REQUIRED, isolation = DEFAULT)   |
| ROLLBACK       | rollbackFor = Exception.class, timeout = 10                     |
| READ-ONLY      | @Transactional(readOnly = true)                                |
| INDEPENDENT    | propagation = Propagation.REQUIRES_NEW                         |
| PROGRAMMATIC   | transactionTemplate.execute(status -> { ... })                 |
| POST-COMMIT    | @TransactionalEventListener(phase = AFTER_COMMIT)              |
+---------------------------------------------------------------------------------+
```

---

## 13. Further Reading & References
- [Spring Framework Transaction Management Reference](https://docs.spring.io/spring-framework/reference/data-access/transaction.html)
- [Vlad Mihalcea - Spring @Transactional Best Practices](https://vladmihalcea.com/spring-transactional-best-practices/)
- [PostgreSQL Transaction Isolation Documentation](https://www.postgresql.org/docs/current/transaction-iso.html)
