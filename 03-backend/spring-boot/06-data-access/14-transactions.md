# Spring Transaction Management: Propagation, Isolation, Proxies & Pitfalls

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | `01-core-spring/05-spring-aop-and-proxies.md`, `03-spring-data-jpa-fundamentals.md`, ACID transaction theory |
| **Estimated Reading Time** | 32 minutes |
| **Status** | Active |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework** | 6.2.x / 7.0.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven / Gradle |
| **Tested With** | Spring Boot 3.4.x, PostgreSQL 16, HikariCP 5.1.x |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
   - [5.1 Mastering Propagation: REQUIRED vs REQUIRES_NEW for Audit Trails](#51-mastering-propagation-required-vs-requires_new-for-audit-trails)
   - [5.2 Programmatic Transaction Control with TransactionTemplate](#52-programmatic-transaction-control-with-transactiontemplate)
   - [5.3 Fixing the Self-Invocation Proxy Trap](#53-fixing-the-self-invocation-proxy-trap)
   - [5.4 Anti-Pattern: Checked Exception Silent Non-Rollback](#54-anti-pattern-checked-exception-silent-non-rollback)
   - [5.5 Transaction Event Listeners with @TransactionalEventListener](#55-transaction-event-listeners-with-transactionaleventlistener)
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
Managing enterprise transactions manually using JDBC (`connection.setAutoCommit(false)`, `commit()`, `rollback()`) requires repetitive try-catch-finally boilerplate, entangles persistence mechanics with business logic, and makes composing multi-service business workflows (e.g., placing an order, decrementing stock, charging payment, and emitting an audit log) exceedingly difficult. More critically, failing to manage transaction propagation correctly causes partial writes, deadlocks, and silent data corruption when nested operations fail.

### Historical Evolution
Spring revolutionized enterprise Java by introducing declarative transaction management. Through `@Transactional` and Spring AOP proxies, developers define transaction boundaries declaratively. Spring's `PlatformTransactionManager` abstracts away underlying persistence technologies—whether standard JDBC, Hibernate, JPA, or JTA distributed transactions—behind a unified transaction model.

### Real-World Relevance
Transactions are the foundation of data integrity (ACID). Misunderstanding transaction propagation, isolation levels, or the Spring AOP proxy self-invocation trap leads to catastrophic bugs: failed payments leaving orders marked "paid", audit logs vanishing upon order rollback, or database connection pool exhaustion from long-running transactions.

---

## 2. Core Concepts

### The 7 Propagation Behaviors
1. **`REQUIRED` (Default)**: Executes within an active transaction if one exists; creates a new one if none exists.
2. **`REQUIRES_NEW`**: Always creates a new, independent physical transaction, suspending any existing transaction. Commits or rolls back independently of the outer transaction.
3. **`NESTED`**: Executes within a nested transaction using database savepoints if an active transaction exists; behaves like `REQUIRED` if none exists.
4. **`MANDATORY`**: Requires an existing active transaction; throws `IllegalTransactionStateException` if none exists.
5. **`SUPPORTS`**: Executes non-transactionally if no transaction exists; executes within the active transaction if one exists.
6. **`NOT_SUPPORTED`**: Suspends any active transaction and executes non-transactionally.
7. **`NEVER`**: Executes non-transactionally; throws `IllegalTransactionStateException` if an active transaction exists.

### Isolation Levels
- `READ_UNCOMMITTED`: Allows dirty reads, non-repeatable reads, and phantom reads.
- `READ_COMMITTED` (PostgreSQL/Oracle default): Prevents dirty reads.
- `REPEATABLE_READ` (MySQL default): Prevents dirty and non-repeatable reads.
- `SERIALIZABLE`: Complete execution isolation; prevents dirty reads, non-repeatable reads, and phantom reads using range locks or MVCC serialization checks.

### Default Rollback Rule
By default, Spring `@Transactional` triggers rollback **ONLY for unchecked exceptions** (`RuntimeException` and `Error`). It **DOES NOT roll back for checked exceptions** (`java.lang.Exception`, `IOException`, `SQLException`) unless explicitly configured with `@Transactional(rollbackFor = Exception.class)`.

---

## 3. How It Works Internally

```
+------------------------------------------------------------------------------------+
|                         Spring @Transactional AOP Proxy Flow                       |
+------------------------------------------------------------------------------------+
|                                                                                    |
|  Caller Code                                                                       |
|       |                                                                            |
|       | orderService.placeOrder()                                                  |
|       v                                                                            |
|  OrderService$$SpringCGLIB$$0 (Dynamic Proxy)                                      |
|       |                                                                            |
|       v                                                                            |
|  TransactionInterceptor                                                            |
|       |                                                                            |
|       |-- 1. Consults PlatformTransactionManager (e.g. JpaTransactionManager)       |
|       |-- 2. Checks TransactionSynchronizationManager: Is transaction active?      |
|       |-- 3. Borrows Connection from DataSource / Opens Hibernate Session          |
|       |-- 4. Binds Connection to current thread (ThreadLocal)                      |
|       |-- 5. Disables AutoCommit (connection.setAutoCommit(false))                 |
|       |                                                                            |
|       v                                                                            |
|  Target Business Method: OrderServiceImpl.placeOrder()                             |
|       |                                                                            |
|       +-- Method succeeds normally:                                                |
|       |   TransactionInterceptor -> PlatformTransactionManager.commit()            |
|       |   Connection committed -> Released back to HikariCP pool                   |
|       |                                                                            |
|       +-- Method throws Exception:                                                 |
|           TransactionInterceptor checks rollbackFor rules:                         |
|           - If matches: PlatformTransactionManager.rollback()                     |
|           - If checked (no match): PlatformTransactionManager.commit() (DATA LEAK!)|
|                                                                                    |
+------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### Standard Declarative Transaction
```java
@Service
public class OrderService {

    // Enforce rollback for ALL exceptions (checked and unchecked)
    @Transactional(rollbackFor = Exception.class, timeout = 5)
    public void processOrder(OrderCommand command) {
        // Business logic...
    }
}
```

### Read-Only Optimization
```java
@Transactional(readOnly = true)
public OrderDto getOrderDetails(Long orderId) {
    // Informs driver and Hibernate to skip dirty checking snapshots
    return orderRepository.findDtoById(orderId);
}
```

---

## 5. Practical Examples

### 5.1 Mastering Propagation: REQUIRED vs REQUIRES_NEW for Audit Trails
```java
package com.example.tx.service;

import com.example.tx.domain.AuditLog;
import com.example.tx.repository.AuditLogRepository;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Propagation;
import org.springframework.transaction.annotation.Transactional;

@Service
public class AuditLogService {

    private final AuditLogRepository auditLogRepository;

    public AuditLogService(AuditLogRepository auditLogRepository) {
        this.auditLogRepository = auditLogRepository;
    }

    /**
     * CRITICAL: REQUIRES_NEW creates an independent transaction.
     * Even if the calling parent transaction (e.g. Order placement) fails and rolls back,
     * this audit record is safely COMMITTED and preserved for security compliance!
     */
    @Transactional(propagation = Propagation.REQUIRES_NEW)
    public void recordSecurityAudit(String action, String principal, String details) {
        AuditLog log = new AuditLog(action, principal, details);
        auditLogRepository.save(log);
    }
}
```

```java
package com.example.tx.service;

import com.example.tx.domain.Order;
import com.example.tx.repository.OrderRepository;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

@Service
public class OrderService {

    private final OrderRepository orderRepository;
    private final AuditLogService auditLogService;

    public OrderService(OrderRepository orderRepository, AuditLogService auditLogService) {
        this.orderRepository = orderRepository;
        this.auditLogService = auditLogService;
    }

    @Transactional(rollbackFor = Exception.class)
    public void placeOrder(Order order) {
        // 1. Audit record committed in independent REQUIRES_NEW transaction
        auditLogService.recordSecurityAudit("ORDER_ATTEMPT", order.getCustomerEmail(), "Starting order");

        // 2. Business logic in outer REQUIRED transaction
        orderRepository.save(order);

        if (order.getTotalAmount().doubleValue() > 10000.0) {
            // Simulated validation failure
            throw new IllegalStateException("High value order requires manual approval");
        }
        // If exception is thrown:
        // - Order is ROLLED BACK
        // - Audit log remains COMMITTED!
    }
}
```

### 5.2 Programmatic Transaction Control with TransactionTemplate
```java
package com.example.tx.service;

import org.springframework.stereotype.Service;
import org.springframework.transaction.PlatformTransactionManager;
import org.springframework.transaction.support.TransactionTemplate;

@Service
public class ExternalIntegrationService {

    private final TransactionTemplate transactionTemplate;
    private final PaymentRepository paymentRepository;
    private final ThirdPartyHttpClient paymentGatewayClient;

    public ExternalIntegrationService(
        PlatformTransactionManager txManager, 
        PaymentRepository paymentRepository, 
        ThirdPartyHttpClient paymentGatewayClient
    ) {
        this.transactionTemplate = new TransactionTemplate(txManager);
        this.paymentRepository = paymentRepository;
        this.paymentGatewayClient = paymentGatewayClient;
    }

    /**
     * Fine-grained programmatic transaction management:
     * NEVER hold a database transaction open while calling slow third-party HTTP endpoints!
     */
    public void executePaymentPipeline(Long paymentId) {
        // 1. Short DB transaction: Record pending payment
        transactionTemplate.executeWithoutResult(status -> {
            paymentRepository.markPending(paymentId);
        });

        // 2. Slow external network call executed OUTSIDE of any database transaction!
        // HikariCP connection is NOT held during this 3-second network wait!
        PaymentGatewayResponse response = paymentGatewayClient.chargeCreditCard(paymentId);

        // 3. Short DB transaction: Record gateway response
        transactionTemplate.executeWithoutResult(status -> {
            if (response.isSuccess()) {
                paymentRepository.markSettled(paymentId, response.transactionRef());
            } else {
                paymentRepository.markFailed(paymentId, response.errorMessage());
                status.setRollbackOnly(); // Programmatic rollback trigger
            }
        });
    }
}
```

### 5.3 Fixing the Self-Invocation Proxy Trap
```java
// =========================================================================
// ANTI-PATTERN: The Self-Invocation Proxy Trap (Silent Failure)
// =========================================================================
@Service
public class BrokenService {

    public void processAllUsers() {
        // SEVERE BUG: Direct internal 'this' method call bypasses Spring's AOP proxy!
        // @Transactional on updateUser() is COMPLETELY IGNORED!
        // Runs with NO transaction management, NO rollback handling!
        this.updateUser(123L);
    }

    @Transactional(propagation = Propagation.REQUIRES_NEW)
    public void updateUser(Long userId) {
        // DB operations...
    }
}

// =========================================================================
// PRODUCTION FIX 1: Delegate to a Separate Spring Service Bean (Recommended)
// =========================================================================
@Service
public class CorrectParentService {
    private final UserUpdaterService userUpdaterService;

    public CorrectParentService(UserUpdaterService userUpdaterService) {
        this.userUpdaterService = userUpdaterService;
    }

    public void processAllUsers() {
        // Invokes Spring AOP Proxy on UserUpdaterService: @Transactional works perfectly!
        userUpdaterService.updateUser(123L);
    }
}

@Service
public class UserUpdaterService {
    @Transactional(propagation = Propagation.REQUIRES_NEW)
    public void updateUser(Long userId) {
        // DB operations...
    }
}

// =========================================================================
// PRODUCTION FIX 2: Programmatic TransactionTemplate
// =========================================================================
@Service
public class TemplateBasedService {
    private final TransactionTemplate txTemplate;

    public TemplateBasedService(PlatformTransactionManager txManager) {
        this.txTemplate = new TransactionTemplate(txManager);
        this.txTemplate.setPropagationBehavior(TransactionTemplate.PROPAGATION_REQUIRES_NEW);
    }

    public void processAllUsers() {
        // Works seamlessly without external bean delegation
        txTemplate.executeWithoutResult(status -> {
            // DB operations...
        });
    }
}
```

### 5.4 Anti-Pattern: Checked Exception Silent Non-Rollback
```java
// =========================================================================
// ANTI-PATTERN: Default @Transactional with Checked Exceptions
// =========================================================================
@Service
public class DangerousBankingService {

    @Autowired private AccountRepository accountRepository;

    // CRITICAL DATA INTEGRITY BUG:
    // By default, Spring ONLY rolls back for RuntimeException and Error!
    // If transferFunds throws an IOException or custom CheckedException,
    // SPRING COMMITS THE TRANSACTION! Money is deducted without delivery!
    @Transactional
    public void transferFunds(Long fromId, Long toId, BigDecimal amount) throws InsufficientFundsCheckedException {
        accountRepository.deduct(fromId, amount);
        
        if (checkFraudRules(fromId)) {
            // Checked exception: TRANSACTION STILL COMMITS!
            throw new InsufficientFundsCheckedException("Fraud detected");
        }
        accountRepository.credit(toId, amount);
    }
}

// =========================================================================
// PRODUCTION FIX: Explicit rollbackFor = Exception.class
// =========================================================================
@Service
public class SafeBankingService {

    @Autowired private AccountRepository accountRepository;

    // SAFE: Guarantees rollback on ANY Exception (checked or unchecked)
    @Transactional(rollbackFor = Exception.class)
    public void transferFunds(Long fromId, Long toId, BigDecimal amount) throws InsufficientFundsCheckedException {
        accountRepository.deduct(fromId, amount);

        if (checkFraudRules(fromId)) {
            throw new InsufficientFundsCheckedException("Fraud detected");
        }
        accountRepository.credit(toId, amount);
    }
}
```

### 5.5 Transaction Event Listeners with @TransactionalEventListener
```java
package com.example.tx.listener;

import com.example.tx.event.OrderPlacedEvent;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Component;
import org.springframework.transaction.event.TransactionPhase;
import org.springframework.transaction.event.TransactionalEventListener;

@Component
public class OrderNotificationListener {

    private static final Logger log = LoggerFactory.getLogger(OrderNotificationListener.class);
    private final EmailNotificationClient emailClient;

    public OrderNotificationListener(EmailNotificationClient emailClient) {
        this.emailClient = emailClient;
    }

    /**
     * Executes ONLY AFTER the database transaction has successfully COMMITTED!
     * Guarantees that customers never receive confirmation emails for orders that rolled back!
     */
    @TransactionalEventListener(phase = TransactionPhase.AFTER_COMMIT)
    public void handleOrderPlacedAfterCommit(OrderPlacedEvent event) {
        log.info("Transaction committed successfully. Dispatching email for order: {}", event.orderId());
        emailClient.sendOrderConfirmation(event.customerEmail(), event.orderId());
    }

    /**
     * Executes if the transaction rolled back.
     */
    @TransactionalEventListener(phase = TransactionPhase.AFTER_ROLLBACK)
    public void handleOrderFailedAfterRollback(OrderPlacedEvent event) {
        log.warn("Transaction rolled back for order: {}. Triggering alert.", event.orderId());
    }
}
```

---

## 6. Best Practices

1. **Always Specify `rollbackFor = Exception.class`**: Override Spring's legacy default to ensure transactions roll back on checked exceptions as well as runtime exceptions.
2. **Keep Transactions as Short as Possible**: Never include network calls (REST, gRPC, SMTP) or heavy CPU hashing inside `@Transactional`. Use `TransactionTemplate` or separate service steps.
3. **Beware of the Self-Invocation Trap**: Calling a `@Transactional` method from another method within the same class bypasses the AOP proxy. Delegate to a separate bean or use `TransactionTemplate`.
4. **Use `AFTER_COMMIT` for Side Effects**: Trigger external emails, message queues (Kafka, RabbitMQ), or cache evictions using `@TransactionalEventListener(phase = AFTER_COMMIT)` to prevent phantom notifications if transactions fail.
5. **Always Set Explicit Timeouts**: Configure `@Transactional(timeout = 5)` (in seconds) to prevent runaway database locks from stalling worker threads indefinitely.
6. **Mark Read-Only Workflows with `readOnly = true`**: Enables Hibernate dirty checking bypass, sets JDBC connections to read-only, and allows routing to database read replicas.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Self-Invocation within Same Bean** | `this.method()` bypasses Spring CGLIB/JDK dynamic proxy; transaction advice is never executed. | Extract method to a separate bean or use `TransactionTemplate`. |
| **Throwing Checked Exceptions Without `rollbackFor`** | Spring defaults to rolling back only on `RuntimeException`; checked exceptions cause transactions to silently commit. | Add `@Transactional(rollbackFor = Exception.class)`. |
| **Catching Exceptions Internally in Catch Block** | Catching and swallowing an exception without rethrowing or setting `status.setRollbackOnly()` prevents rollback. | Allow exception to bubble up or mark `TestTransaction.flagForRollback()`. |
| **Long-Running Third-Party Calls Inside Transaction** | Holds HikariCP connection open for seconds, starving the connection pool and crashing other services. | Execute external I/O outside `@Transactional`. |
| **Using `@Transactional` on Private Methods** | Spring AOP proxies cannot intercept `private` methods; the annotation is silently ignored. | Make method `public` or use AspectJ compile-time weaving. |
| **`REQUIRES_NEW` Inside Same Connection Pool** | If pool size is small (e.g. 10), 5 concurrent requests executing nested `REQUIRES_NEW` require 2 connections each, causing immediate pool deadlock! | Ensure pool size is properly sized or eliminate `REQUIRES_NEW`. |

---

## 8. Performance & Security Notes

### Performance
- **Read-Only Transaction Routing**: `@Transactional(readOnly = true)` allows `AbstractRoutingDataSource` to route traffic directly to PostgreSQL read-replicas, offloading the primary master.
- **Connection Deadlock with `REQUIRES_NEW`**: If outer transaction holds Connection 1, and inner `REQUIRES_NEW` attempts to acquire Connection 2 when all connections are exhausted, threads deadlock permanently until connection timeout.

### Security
- **Consistent Audit Persistence**: Using `Propagation.REQUIRES_NEW` guarantees that security audit logs are committed even if the user's primary transaction fails due to permission violations or invalid state.

---

## 9. Comparison / When to Use What

| Propagation Level | Active Transaction Present | No Active Transaction | Common Use Case |
| :--- | :--- | :--- | :--- |
| **`REQUIRED`** | Joins active transaction | Creates new transaction | Standard business workflows (90% of cases) |
| **`REQUIRES_NEW`** | **Suspends active, creates new** | Creates new transaction | Immutable audit logs, notifications |
| **`NESTED`** | Creates savepoint | Creates new transaction | Partial rollback recovery |
| **`SUPPORTS`** | Joins active transaction | Runs non-transactionally | Read-only methods that adapt to caller |
| **`MANDATORY`** | Joins active transaction | **Throws Exception** | Subordinate operations requiring transactional lock |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the self-invocation proxy problem in Spring <code>@Transactional</code> and how does it happen?</strong></summary>

Spring manages transactions using AOP proxies (CGLIB or JDK dynamic proxies). When an external client calls a bean method, it invokes the proxy, which executes the transaction interceptor before delegating to the target object. When a method *within* the bean invokes another method on `this`, it calls the raw instance directly, bypassing the proxy. As a result, the transaction interceptor is never invoked, and `@Transactional` on the called method is completely ignored.
</details>

<details>
<summary><strong>2. What is the difference between <code>Propagation.REQUIRED</code> and <code>Propagation.REQUIRES_NEW</code>?</strong></summary>

- `REQUIRED` (default) checks if a transaction already exists. If yes, it joins that transaction; if no, it creates a new one. A failure anywhere in the transaction rolls back all operations.
- `REQUIRES_NEW` always suspends the current active transaction (if any) and starts a completely independent physical database transaction with its own connection. The inner transaction commits or rolls back independently of the outer transaction.
</details>

<details>
<summary><strong>3. By default, which exceptions trigger a transaction rollback in Spring?</strong></summary>

By default, Spring rolls back transactions only for **unchecked exceptions** (subclasses of `java.lang.RuntimeException` and `java.lang.Error`). Checked exceptions (`java.lang.Exception` and its direct subclasses) do not trigger a rollback; the transaction commits unless `rollbackFor = Exception.class` is explicitly specified.
</details>

<details>
<summary><strong>4. How does <code>@TransactionalEventListener(phase = AFTER_COMMIT)</code> work and why is it useful?</strong></summary>

Standard Spring `@EventListener` executes immediately when an event is published. If published inside a transaction that subsequently rolls back, side effects (like sending emails or emitting Kafka messages) cannot be undone. `@TransactionalEventListener(phase = AFTER_COMMIT)` registers a callback that executes only after the surrounding database transaction successfully sends the SQL `COMMIT` command, guaranteeing side effects only occur for durable data.
</details>

<details>
<summary><strong>5. Why is using <code>REQUIRES_NEW</code> dangerous when database connection pool sizes are small?</strong></summary>

Each `REQUIRES_NEW` transaction requires its own physical JDBC connection while the outer transaction remains suspended holding its own connection. If the connection pool has 10 connections, and 10 concurrent requests arrive, each outer transaction borrows 1 connection (10 connections used). When each request attempts to start its `REQUIRES_NEW` transaction, all 10 threads block waiting for an available connection from the exhausted pool, causing a permanent thread deadlock.
</details>

<details>
<summary><strong>6. How does <code>TransactionTemplate</code> improve on declarative <code>@Transactional</code>?</strong></summary>

`TransactionTemplate` provides programmatic transaction control. It allows developers to define exact, fine-grained transaction boundaries within a single method, ensuring that external HTTP calls, file I/O, or heavy computations are kept outside the database transaction, thereby minimizing database connection holding times.
</details>

<details>
<summary><strong>7. What does <code>@Transactional(readOnly = true)</code> do at the JDBC and Hibernate levels?</strong></summary>

1. **Hibernate**: Sets `FlushMode` to `MANUAL` and disables creation of dirty-check snapshots, saving memory and CPU cycles.
2. **JDBC**: Calls `connection.setReadOnly(true)`, allowing database drivers (like PostgreSQL or MySQL) to route queries directly to read-only replicas.
</details>

<details>
<summary><strong>8. Why should you avoid annotating <code>private</code> methods with <code>@Transactional</code>?</strong></summary>

Spring AOP proxies intercept calls by subclassing (CGLIB) or implementing interfaces (JDK). Private methods cannot be overridden by subclasses or exposed via interfaces. Therefore, Spring cannot generate proxy advice for private methods, and `@Transactional` is silently ignored without error.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Basic Declarative Transaction with Rollback Rule</strong></summary>

**Task**: Annotate a user registration service method to ensure it runs in a transaction, sets a 3-second timeout, and rolls back on all exceptions.

```java
@Service
public class UserService {
    @Transactional(rollbackFor = Exception.class, timeout = 3)
    public void registerUser(UserDto dto) throws Exception {
        // Registration logic
    }
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Programmatic Read-Only TransactionTemplate</strong></summary>

**Task**: Use `TransactionTemplate` to execute a read-only query that counts total accounts.

```java
public long countAccounts() {
    TransactionTemplate template = new TransactionTemplate(transactionManager);
    template.setReadOnly(true);
    return template.execute(status -> accountRepository.count());
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Audit Logging with REQUIRES_NEW</strong></summary>

**Task**: Create an `AuditService` with a method `logAction` configured with `REQUIRES_NEW` that commits an audit entry even if the caller transaction throws a `RuntimeException`.

```java
@Service
public class AuditService {
    private final AuditRepository repository;

    public AuditService(AuditRepository repository) { this.repository = repository; }

    @Transactional(propagation = Propagation.REQUIRES_NEW)
    public void logAction(String message) {
        repository.save(new AuditLog(message));
    }
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Testing Transaction Rollback with Custom Checked Exception</strong></summary>

**Task**: Write an integration test asserting that throwing a custom checked exception rolls back the transaction when `rollbackFor` is properly configured.

```java
@Test
void testRollbackOnCheckedException() {
    assertThatThrownBy(() -> bankingService.transferWithCheckedException(1L, 2L, BigDecimal.TEN))
        .isInstanceOf(CustomCheckedException.class);

    // Assert that source account balance was NOT deducted
    Account account = accountRepository.findById(1L).orElseThrow();
    assertThat(account.getBalance()).isEqualByComparingTo("100.00");
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): TransactionalEventListener After Commit Email Verification</strong></summary>

**Task**: Implement an application event publisher that sends an order event, and a listener with `@TransactionalEventListener(phase = AFTER_COMMIT)` verifying emails are sent only on successful commit.

```java
@Component
public class EmailDispatcher {
    @TransactionalEventListener(phase = TransactionPhase.AFTER_COMMIT)
    public void onOrderPlaced(OrderCreatedEvent event) {
        emailService.sendConfirmation(event.email());
    }
}
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Refactoring Self-Invocation with Self-Injection</strong></summary>

**Task**: Refactor a legacy class exhibiting the self-invocation proxy trap using lazy self-injection (`@Lazy private MyService self`).

```java
@Service
public class OrderService {
    @Lazy @Autowired private OrderService self;

    public void processBatch() {
        // Calls via proxy!
        self.processSingleOrderInNewTx(42L);
    }

    @Transactional(propagation = Propagation.REQUIRES_NEW)
    public void processSingleOrderInNewTx(Long id) {
        // Single order execution
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------------+
| Golden Rule 1: Always specify @Transactional(rollbackFor = Exception.class)           |
| Golden Rule 2: NEVER make network calls (REST/gRPC/Email) inside @Transactional       |
| Propagation:   REQUIRED (join/create) vs REQUIRES_NEW (always independent new tx)     |
| Self-Trap:     Calling this.method() bypasses proxy! Extract to separate bean!        |
| Events:        Use @TransactionalEventListener(phase = AFTER_COMMIT) for side effects |
| Programmatic:  Use TransactionTemplate for fine-grained sub-method boundaries         |
| Performance:   @Transactional(readOnly = true) disables dirty check overhead          |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Spring Framework Transaction Management Documentation](https://docs.spring.io/spring-framework/reference/data-access/transaction.html)
- [Vlad Mihalcea: How Does Spring @Transactional Work](https://vladmihalcea.com/spring-transactional-annotation/)
- [Spring Transaction Synchronization Manager Guide](https://docs.spring.io/spring-framework/docs/current/javadoc-api/org/springframework/transaction/support/TransactionSynchronizationManager.html)
- [Baeldung: Transaction Propagation and Isolation in Spring](https://www.baeldung.com/spring-transactional-propagation-isolation)
