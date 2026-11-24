# Classic GoF Design Patterns in Spring Boot

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Version** | 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.x |
| **Tested With** | Spring Core, Spring AOP, Java 21 Pattern Matching |

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
10. [Interview Questions & Deep Dives](#10-interview-questions--deep-dives)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### The Problem
When building complex enterprise applications, developers repeatedly confront recurring structural challenges: dynamically selecting algorithms at runtime, decoupling object creation from usage, coordinating cross-cutting concerns (logging, security, transactions) without polluting business logic, and broadcasting state notifications across decoupled subsystems. Reinventing bespoke solutions for each challenge results in fragile, unmaintainable code and anti-patterns.

### The Evolution
The **Gang of Four (GoF)** design patterns (published in 1994 by Gamma, Helm, Johnson, and Vlissides) cataloged 23 fundamental object-oriented patterns. Spring Framework was designed from the ground up as a masterclass in applying GoF design patterns in enterprise Java:
- **Factory Pattern**: Realized via `BeanFactory` and `FactoryBean`.
- **Strategy Pattern**: Realized via polymorphic bean injection and Spring converters.
- **Template Method Pattern**: Realized via `JdbcTemplate`, `TransactionTemplate`, and `RestClient`.
- **Proxy Pattern**: Realized via Spring AOP dynamic proxies for `@Transactional`, `@Async`, and `@Cacheable`.
- **Observer Pattern**: Realized via `ApplicationEventPublisher` and `@EventListener`.
- **Decorator / Adapter Pattern**: Realized via Spring Web filters and HTTP client request interceptors.

### Production Relevance
Understanding how Spring implements and harnesses GoF design patterns enables developers to write idiomatic, extensible Spring Boot applications. Instead of writing cumbersome manual boilerplate, engineers leverage Spring's native container features to implement clean patterns effortlessly.

---

## 2. Core Concepts

### GoF Patterns Mapped to Spring Framework Core

```
+-----------------------------------------------------------------------------------------+
|                                GOF PATTERNS IN SPRING BOOT                              |
+-----------------------------------------------------------------------------------------+
| Pattern Category | GoF Pattern Name       | Spring Boot Implementation                  |
+------------------+------------------------+---------------------------------------------+
| Creational       | Singleton              | Default Spring Bean Scope                   |
| Creational       | Factory Method         | @Bean, BeanFactory, FactoryBean             |
| Creational       | Builder                | RestClient.builder(), MockMvcBuilders       |
+------------------+------------------------+---------------------------------------------+
| Structural       | Proxy                  | Spring AOP (CGLIB / JDK Dynamic Proxies)    |
| Structural       | Adapter                | HandlerAdapter, TaskExecutorAdapter         |
| Structural       | Decorator              | HttpComponentsClientHttpRequestFactory      |
+------------------+------------------------+---------------------------------------------+
| Behavioral       | Strategy               | Map<String, StrategyBean> injection         |
| Behavioral       | Template Method        | JdbcTemplate, JmsTemplate, TransactionTemplate|
| Behavioral       | Observer               | ApplicationEventPublisher, @EventListener   |
| Behavioral       | Chain of Responsibility| FilterChain, HandlerInterceptor[]           |
+-----------------------------------------------------------------------------------------+
```

### Architectural Invariants
- **Open-Closed Principle (OCP)**: Adding a new strategy (e.g., a new `PaymentGateway`) requires adding only a new `@Component` class; zero changes to existing switch statements or controller code.
- **Inversion of Control (IoC)**: The container instantiates, configures, and manages bean lifecycles, replacing manual `new` operator invocations.
- **Non-Invasive Interception**: Proxies encapsulate non-functional cross-cutting concerns (transactions, security, caching) without changing the domain interface signature.

---

## 3. How It Works Internally

### Deep Dive: Spring's Strategy Pattern via Dynamic Map/List Injection

```
                                  [ Spring ApplicationContext ]
                                                |
                                                v
                           Discovers all beans implementing 'PaymentStrategy'
                                                |
               +--------------------------------+--------------------------------+
               |                                |                                |
               v                                v                                v
    [ CreditCardPaymentStrategy ]      [ PayPalPaymentStrategy ]       [ CryptoPaymentStrategy ]
       (@Component("CREDIT_CARD"))        (@Component("PAYPAL"))          (@Component("CRYPTO"))
               |                                |                                |
               +--------------------------------+--------------------------------+
                                                |
                                                v
                             Injected automatically into Service:
                             Map<String, PaymentStrategy> paymentStrategies
                                                |
                                                v
[ Client Request: type="PAYPAL" ] ---> [ PaymentService.pay() ] ---> paymentStrategies.get("PAYPAL")
```

1. **Component Scanning**: During startup, Spring's `DefaultListableBeanFactory` scans for classes implementing the target interface (`PaymentStrategy`).
2. **Keyed Injection**: When a class requests a `Map<String, PaymentStrategy>` or `List<PaymentStrategy>`, Spring automatically populates the Map with the bean names as keys and the corresponding singleton bean instances as values.
3. **Runtime Resolution**: The consumer service executes `paymentStrategies.get(request.type())` in $O(1)$ time, completely eliminating rigid `switch` statements or `if-else` blocks.

---

## 4. Syntax & Usage

### 1. Strategy Pattern via Typed Enums & Spring Bean Injection

```java
package com.example.pattern.strategy;

import java.math.BigDecimal;

public interface PaymentStrategy {
    PaymentType getType();
    PaymentResult process(BigDecimal amount, String customerId);
}

public enum PaymentType {
    CREDIT_CARD,
    PAYPAL,
    APPLE_PAY
}
```

```java
package com.example.pattern.strategy;

import org.springframework.stereotype.Service;

import java.math.BigDecimal;
import java.util.List;
import java.util.Map;
import java.util.function.Function;
import java.util.stream.Collectors;

@Service
public class PaymentProcessingService {

    private final Map<PaymentType, PaymentStrategy> strategyMap;

    // Spring automatically injects all beans implementing PaymentStrategy
    public PaymentProcessingService(List<PaymentStrategy> strategies) {
        this.strategyMap = strategies.stream()
                .collect(Collectors.toUnmodifiableMap(PaymentStrategy::getType, Function.identity()));
    }

    public PaymentResult executePayment(PaymentType type, BigDecimal amount, String customerId) {
        PaymentStrategy strategy = strategyMap.get(type);
        if (strategy == null) {
            throw new UnsupportedOperationException("No payment strategy registered for type: " + type);
        }
        return strategy.process(amount, customerId);
    }
}
```

---

## 5. Practical Examples

### Example 1: Template Method Pattern via `TransactionTemplate`

```java
package com.example.pattern.template;

import org.springframework.stereotype.Service;
import org.springframework.transaction.PlatformTransactionManager;
import org.springframework.transaction.support.TransactionTemplate;

@Service
public class OrderSettlementService {

    private final TransactionTemplate transactionTemplate;

    public OrderSettlementService(PlatformTransactionManager transactionManager) {
        // GoF Template Method: encapsulates transaction begin, commit, rollback logic
        this.transactionTemplate = new TransactionTemplate(transactionManager);
        this.transactionTemplate.setTimeout(10); // 10 seconds timeout
    }

    public String settleOrder(Long orderId) {
        // Callback defines the variable business step; Template handles invariant transaction steps
        return transactionTemplate.execute(status -> {
            try {
                // Step 1: Execute DB update
                updateLedgerBalance(orderId);
                
                // Step 2: Mark order settled
                return "SETTLED";
            } catch (Exception ex) {
                status.setRollbackOnly();
                throw new SettlementFailedException(orderId, ex);
            }
        });
    }

    private void updateLedgerBalance(Long orderId) { /* ... */ }
}
```

### Example 2: Proxy Pattern via Spring AOP Custom Performance Annotation

```java
package com.example.pattern.proxy;

import org.aspectj.lang.ProceedingJoinPoint;
import org.aspectj.lang.annotation.Around;
import org.aspectj.lang.annotation.Aspect;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Component;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

// 1. Custom Marker Annotation
@Target(ElementType.METHOD)
@Retention(RetentionPolicy.RUNTIME)
public @interface TrackExecutionTime {}

// 2. GoF Dynamic Proxy Aspect
@Aspect
@Component
public class ExecutionTimeLoggingAspect {

    private static final Logger log = LoggerFactory.getLogger(ExecutionTimeLoggingAspect.class);

    @Around("@annotation(TrackExecutionTime)")
    public Object profileMethod(ProceedingJoinPoint joinPoint) throws Throwable {
        long start = System.currentTimeMillis();
        
        // Target method execution through proxy boundary
        Object result = joinPoint.proceed();
        
        long duration = System.currentTimeMillis() - start;
        log.info("Method [{}] executed in {} ms", joinPoint.getSignature().toShortString(), duration);
        return result;
    }
}
```

### Example 3: Observer Pattern via Strongly-Typed Domain Events

```java
package com.example.pattern.observer;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.context.ApplicationEventPublisher;
import org.springframework.context.event.EventListener;
import org.springframework.scheduling.annotation.Async;
import org.springframework.stereotype.Component;
import org.springframework.stereotype.Service;

import java.util.UUID;

// 1. Observer Subject (Event)
public record UserRegisteredEvent(UUID userId, String email) {}

// 2. Concrete Observable Service
@Service
public class UserService {
    private final ApplicationEventPublisher eventPublisher;

    public UserService(ApplicationEventPublisher eventPublisher) {
        this.eventPublisher = eventPublisher;
    }

    public UUID registerUser(String email) {
        UUID userId = UUID.randomUUID();
        // Save user to DB...
        
        // Notify observers without coupling to them directly
        eventPublisher.publishEvent(new UserRegisteredEvent(userId, email));
        return userId;
    }
}

// 3. Concrete Observers (Listeners)
@Component
public class WelcomeEmailObserver {
    private static final Logger log = LoggerFactory.getLogger(WelcomeEmailObserver.class);

    @Async
    @EventListener
    public void onUserRegistered(UserRegisteredEvent event) {
        log.info("Observer 1: Sending welcome email to {}", event.email());
    }
}

@Component
public class UserAnalyticsObserver {
    private static final Logger log = LoggerFactory.getLogger(UserAnalyticsObserver.class);

    @EventListener
    public void onUserRegistered(UserRegisteredEvent event) {
        log.info("Observer 2: Recording user signup metrics for userId {}", event.userId());
    }
}
```

### Example 4: Anti-Pattern (Giant Switch Statement) vs Strategy Pattern

#### The Anti-Pattern: Violation of Open-Closed Principle
```java
// BAD: Every new notification provider requires modifying this monolithic class!
@Service
public class BadNotificationService {

    public void send(String type, String message, String recipient) {
        if ("SMS".equalsIgnoreCase(type)) {
            // Twilio SMS logic...
        } else if ("EMAIL".equalsIgnoreCase(type)) {
            // SendGrid Email logic...
        } else if ("SLACK".equalsIgnoreCase(type)) {
            // Slack webhook logic...
        } else if ("PUSH".equalsIgnoreCase(type)) {
            // Firebase push logic...
        } else {
            throw new IllegalArgumentException("Unknown type: " + type);
        }
    }
}
```

#### Production Fix: Strategy Pattern with Spring Bean Registry
```java
// GOOD: Open for extension, closed for modification
public interface NotificationStrategy {
    boolean supports(String channel);
    void send(String recipient, String message);
}

@Component
public class TwilioSmsStrategy implements NotificationStrategy {
    @Override public boolean supports(String channel) { return "SMS".equalsIgnoreCase(channel); }
    @Override public void send(String recipient, String message) { /* Twilio call */ }
}

@Component
public class SendGridEmailStrategy implements NotificationStrategy {
    @Override public boolean supports(String channel) { return "EMAIL".equalsIgnoreCase(channel); }
    @Override public void send(String recipient, String message) { /* SendGrid call */ }
}

@Service
public class CleanNotificationService {
    private final List<NotificationStrategy> strategies;

    public CleanNotificationService(List<NotificationStrategy> strategies) {
        this.strategies = strategies;
    }

    public void send(String channel, String recipient, String message) {
        strategies.stream()
            .filter(s -> s.supports(channel))
            .findFirst()
            .orElseThrow(() -> new IllegalArgumentException("Unsupported channel: " + channel))
            .send(recipient, message);
    }
}
```

### Example 5: Factory Pattern via Spring `FactoryBean` for Third-Party Client Creation

```java
package com.example.pattern.factory;

import org.springframework.beans.factory.FactoryBean;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Component;

import java.net.http.HttpClient;
import java.time.Duration;

// GoF Factory Pattern encapsulating complex client construction
@Component
public class ResilientHttpClientFactoryBean implements FactoryBean<HttpClient> {

    @Value("${app.http.connect-timeout:5s}")
    private Duration connectTimeout;

    @Override
    public HttpClient getObject() {
        return HttpClient.newBuilder()
                .version(HttpClient.Version.HTTP_2)
                .connectTimeout(connectTimeout)
                .followRedirects(HttpClient.Redirect.NORMAL)
                .build();
    }

    @Override
    public Class<?> getObjectType() {
        return HttpClient.class;
    }

    @Override
    public boolean isSingleton() {
        return true;
    }
}
```

---

## 6. Best Practices

1. **Favor Strategy Bean Maps over Rigid Conditionals**: When supporting multiple payment processors, file parsers, or authentication providers, inject a `Map<String, StrategyBean>` or `List<StrategyBean>`.
2. **Use Template Method for Invariant Resource Management**: Use `TransactionTemplate` or `JdbcClient` rather than manual `try-catch-finally` connection handling.
3. **Leverage Spring AOP Proxies for Cross-Cutting Concerns**: Do not clutter domain services with metrics, security checks, and transaction calls; extract them into `@Aspect` advice.
4. **Use Java 21 Records as Observer Payloads**: Make events immutable, thread-safe, and self-describing records.
5. **Mark Strategies with Distinct Identifiers**: Either give beans unique `@Component("NAME")` identifiers or implement a `getType()` / `supports()` method on the strategy interface.
6. **Prefer `FactoryBean` for Complex 3rd-Party Object Construction**: When an external SDK client has a multi-step builder configuration, wrap it in a `FactoryBean<T>`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| **Calling Proxy Methods Internally (Self-Invocation)** | Invocations like `this.anotherMethod()` bypass the Spring AOP proxy; `@Transactional` or `@Async` fails silently! | Inject `Self` reference via constructor or refactor the method into a separate `@Service`. |
| **Monolithic Switch Statements** | Adding a new enum branch requires modifying multiple service classes, violating OCP. | Refactor to Strategy Pattern using Spring's automatic `List<Strategy>` injection. |
| **Overusing `ApplicationEventPublisher` for Synchronous Flow**| Obscures code readability and makes debugging stack traces difficult across synchronous calls. | Use direct method injection for simple 1-to-1 synchronous interactions; use events for decoupled side effects. |
| **Modifying Strategy Map at Runtime** | `ConcurrentModificationException` or race conditions under concurrent multi-threaded requests. | Use `Collections.unmodifiableMap()` during constructor initialization. |
| **Creating Unnecessary Manual Singletons**| Writing private static constructors (`getInstance()`) instead of letting Spring manage the bean lifecycle. | Delete the manual singleton boilerplate and let Spring's `@Service` singleton scope manage it. |
| **Heavy Logic inside `supports()` Method** | Running database queries or HTTP calls inside `strategy.supports()` degrades selection performance. | Keep `supports()` evaluation to fast memory checks (e.g., enum or string matching). |

---

## 8. Performance & Security Notes

### Performance
- **Proxy Overhead**: Spring AOP CGLIB/JDK dynamic proxies introduce minimal overhead (~1-2 nanoseconds per method invocation). However, nesting multiple heavy aspects (`@Around`) on tight inner loops can degrade CPU throughput.
- **Map Lookup vs Switch**: A `Map.get()` lookup on a small enum/string map has $O(1)$ complexity, equivalent in speed to a modern JVM bytecode `tableswitch` instruction, but with complete decoupling.

### Security
- **Defense Against Malicious Strategy Injection**: Never pass untrusted user input directly into `context.getBean(userInput)`. An attacker could request internal Spring beans (`dataSource`, `applicationContext`). Always validate against a strict enum or pre-populated whitelist map.

---

## 9. Comparison / When to Use What

| GoF Pattern | Spring Implementation | Primary Use Case | Over-engineering Risk |
|---|---|---|---|
| **Strategy** | `Map<Type, Strategy>` | Dynamic algorithm selection (Payment, Auth, Export) | Pointless when only 1 fixed algorithm will ever exist |
| **Template Method** | `JdbcTemplate`, `TransactionTemplate` | Enforcing invariant lifecycle steps around callbacks | Low risk; standard Spring idiom |
| **Proxy** | Spring AOP (`@Transactional`, `@Cacheable`) | Non-invasive cross-cutting concerns (Security, Timing) | Hard to debug if over-stacked with custom aspects |
| **Observer** | `@EventListener`, `ApplicationEventPublisher` | Decoupled side effects (Welcome email, Audit log) | Excessive indirection for simple synchronous flows |
| **Factory** | `@Bean`, `FactoryBean<T>` | Encapsulating complex object creation | Manual factories when `@Bean` method is sufficient |
| **Chain of Resp.**| `HandlerInterceptor[]`, `SecurityFilterChain` | Sequential pipeline processing with early abort | Complex ordering bugs across filter lists |

---

## 10. Interview Questions & Deep Dives

<details>
<summary><strong>1. How does Spring resolve and inject all implementations of an interface into a single class?</strong></summary>

Spring's `DefaultListableBeanFactory` provides built-in dependency resolution for collection and map types:
- When a constructor parameter has type `List<MyInterface>`, Spring queries its bean registry for all bean definitions matching `MyInterface.class`, instantiates them, orders them by `@Order` / `Ordered` if present, and passes the list.
- When the parameter has type `Map<String, MyInterface>`, Spring populates the map with the bean names as keys and the bean references as values.
</details>

<details>
<summary><strong>2. Explain why `@Transactional` does not work when one method calls another method within the same class (Self-Invocation).</strong></summary>

Spring manages transactions using dynamic proxies (either JDK Dynamic Proxies or CGLIB subclasses).
When an external caller invokes `orderService.placeOrder()`:
1. The call hits the **Proxy** first.
2. The proxy executes `TransactionInterceptor` (opens DB transaction).
3. The proxy forwards the call to the actual `OrderService` target.
If `placeOrder()` internally executes `this.settlePayment()`:
- `this` references the raw target object, **not the proxy**.
- The call completely bypasses the proxy and its `TransactionInterceptor`.
- Any annotations (`@Transactional`, `@Async`, `@Cacheable`) on `settlePayment()` are completely ignored.
</details>

<details>
<summary><strong>3. What is the difference between JDK Dynamic Proxy and CGLIB Proxy in Spring Boot?</strong></summary>

- **JDK Dynamic Proxy**: Built into the standard Java runtime (`java.lang.reflect.Proxy`). It can only proxy classes that implement an interface. It intercepts calls by implementing the interface dynamically.
- **CGLIB Proxy**: Generates bytecode at runtime by creating a subclass of the target class (`TargetClass$$EnhancerBySpringCGLIB`). It can proxy concrete classes without interfaces.
*Spring Boot 2.x and 3.x default to CGLIB proxies* (`spring.aop.proxy-target-class=true`) for consistent behavior across both interface-based and concrete beans.
</details>

<details>
<summary><strong>4. How does the Template Method pattern in `JdbcTemplate` prevent connection leaks?</strong></summary>

In traditional JDBC, developers had to write tedious `try-catch-finally` blocks:
```java
Connection conn = ds.getConnection();
try {
    PreparedStatement ps = conn.prepareStatement(sql);
    try {
        ResultSet rs = ps.executeQuery();
        // business logic
    } finally { rs.close(); }
} finally { conn.close(); }
```
If an exception occurred or a developer forgot `close()`, connections leaked.
`JdbcTemplate` implements the Template Method:
- Invariant steps (acquiring connection from `DataSourceUtils`, creating statements, handling SQLExceptions, and reliably closing resources in `finally`) are executed by `JdbcTemplate`.
- The variable step (extracting data from `ResultSet`) is delegated to a `RowMapper` or `ResultSetExtractor` callback.
</details>

<details>
<summary><strong>5. How does the Observer Pattern via Spring Events differ from a message queue like RabbitMQ or Kafka?</strong></summary>

- **Spring Events (`ApplicationEventPublisher`)**: In-process, JVM-local observer pattern. Events are dispatched in-memory. By default, they execute synchronously on the caller thread. If the JVM crashes, unhandled events are lost.
- **RabbitMQ / Kafka**: Distributed messaging systems. Events are serialized over the network, persisted to disk, and consumed across independent microservice instances with consumer group load balancing and durable re-delivery.
</details>

<details>
<summary><strong>6. How can you order the execution of multiple Strategy beans or Event Listeners in Spring?</strong></summary>

By using the `@Order` annotation or implementing the `Ordered` interface:
```java
@Component
@Order(1) // Executes first
public class HighPriorityValidationStrategy implements ValidationStrategy {}

@Component
@Order(2) // Executes second
public class StandardValidationStrategy implements ValidationStrategy {}
```
When injected into a `List<ValidationStrategy>`, Spring guarantees the beans appear in sorted order according to their `@Order` value (lowest value has highest priority).
</details>

<details>
<summary><strong>7. What GoF pattern is `HandlerInterceptor` in Spring Web MVC an example of?</strong></summary>

`HandlerInterceptor` is an implementation of the **Chain of Responsibility** pattern.
An incoming HTTP request passes sequentially through a chain of interceptors:
- `preHandle()`: Can inspect or mutate the request; returning `false` aborts the chain immediately.
- `postHandle()`: Executes after controller execution but before view rendering.
- `afterCompletion()`: Executes after the complete request lifecycle, ideal for cleanup.
</details>

<details>
<summary><strong>8. How does the Builder pattern fit into modern Spring Boot APIs?</strong></summary>

Modern Spring Boot favors fluent builder interfaces over large telescopic constructors:
- `RestClient.builder().baseUrl("...").defaultHeader("...").build()`
- `SecurityFilterChain` DSL: `http.csrf(...).authorizeHttpRequests(...).build()`
- Java 21 Records combined with Lombok or custom static builders for complex immutable configuration objects.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Implement Strategy Pattern for Document Export</strong></summary>

**Objective**: Write a strategy interface and two implementations (`PdfExporter`, `CsvExporter`) selectable by a file format enum.

**Solution**:
```java
public enum ExportFormat { PDF, CSV }

public interface DocumentExporter {
    ExportFormat getFormat();
    byte[] export(ReportData data);
}

@Component
public class PdfExporter implements DocumentExporter {
    @Override public ExportFormat getFormat() { return ExportFormat.PDF; }
    @Override public byte[] export(ReportData data) { return "PDF_BYTES".getBytes(); }
}

@Component
public class CsvExporter implements DocumentExporter {
    @Override public ExportFormat getFormat() { return ExportFormat.CSV; }
    @Override public byte[] export(ReportData data) { return "CSV_BYTES".getBytes(); }
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Self-Invocation Fix using Self-Injection</strong></summary>

**Objective**: Refactor an `@Async` method being called from within the same class so the proxy intercepts it correctly.

**Solution**:
```java
@Service
public class OrderService {

    private OrderService self;

    @Autowired
    public void setSelf(@Lazy OrderService self) {
        this.self = self;
    }

    public void processOrder(Long id) {
        // Calling via self proxy reference enables @Async
        self.sendAsyncReceipt(id);
    }

    @Async
    public void sendAsyncReceipt(Long id) {
        // Runs on a background thread!
    }
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Implement Decorator Pattern via `ClientHttpRequestInterceptor`</strong></summary>

**Objective**: Create a decorator that automatically injects a correlation ID header and logs outbound HTTP latency on every outgoing `RestClient` call.

**Solution**:
```java
@Component
public class CorrelationIdInterceptor implements ClientHttpRequestInterceptor {

    @Override
    public ClientHttpResponse intercept(HttpRequest request, byte[] body, ClientHttpRequestExecution execution) throws IOException {
        String correlationId = MDC.get("correlationId");
        if (correlationId != null) {
            request.getHeaders().add("X-Correlation-ID", correlationId);
        }
        return execution.execute(request, body);
    }
}
```
Register with `RestClient.builder().requestInterceptor(interceptor)`.
</details>

<details>
<summary><strong>Exercise 4 (Medium): Build a Pipeline with Chain of Responsibility Pattern</strong></summary>

**Objective**: Implement a sequential fraud check pipeline where any validator can reject an order.

**Solution**:
```java
public interface FraudCheckStep {
    void validate(OrderContext context);
}

@Service
public class FraudCheckPipeline {
    private final List<FraudCheckStep> steps;

    public FraudCheckPipeline(List<FraudCheckStep> steps) {
        this.steps = steps; // Injected in @Order sequence
    }

    public void execute(OrderContext context) {
        for (FraudCheckStep step : steps) {
            step.validate(context);
        }
    }
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Implement Custom Caching Annotation via AOP Proxy</strong></summary>

**Objective**: Build a custom `@Memoize` annotation and Aspect that caches method returns in a local `ConcurrentHashMap`.

**Solution**:
```java
@Target(ElementType.METHOD)
@Retention(RetentionPolicy.RUNTIME)
public @interface Memoize {
    String key();
}

@Aspect
@Component
public class MemoizeAspect {

    private final Map<String, Object> cache = new ConcurrentHashMap<>();

    @Around("@annotation(memoize)")
    public Object applyCache(ProceedingJoinPoint joinPoint, Memoize memoize) throws Throwable {
        String cacheKey = memoize.key() + ":" + Arrays.toString(joinPoint.getArgs());
        if (cache.containsKey(cacheKey)) {
            return cache.get(cacheKey);
        }
        Object result = joinPoint.proceed();
        if (result != null) {
            cache.put(cacheKey, result);
        }
        return result;
    }
}
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Build a Dynamic Strategy Selector with Spring SpEL Expressions</strong></summary>

**Objective**: Create a strategy router that evaluates Spring Expression Language (SpEL) to select the appropriate parser for arbitrary customer payloads.

**Solution**:
```java
public interface DynamicRuleStrategy {
    String getCondition(); // SpEL expression, e.g., "#payload.country == 'US'"
    void execute(Payload payload);
}

@Service
public class DynamicRuleEngine {
    private final List<DynamicRuleStrategy> rules;
    private final ExpressionParser parser = new SpelExpressionParser();

    public DynamicRuleEngine(List<DynamicRuleStrategy> rules) {
        this.rules = rules;
    }

    public void process(Payload payload) {
        StandardEvaluationContext context = new StandardEvaluationContext();
        context.setVariable("payload", payload);

        for (DynamicRuleStrategy rule : rules) {
            Boolean match = parser.parseExpression(rule.getCondition()).getValue(context, Boolean.class);
            if (Boolean.TRUE.equals(match)) {
                rule.execute(payload);
                return;
            }
        }
        throw new IllegalStateException("No rule matched payload: " + payload);
    }
}
```
</details>

---

## 12. Summary & Cheat Sheet

```
+----------------------------------------------------------------------------------------------------+
| CLASSIC GOF DESIGN PATTERNS IN SPRING BOOT CHEAT SHEET                                             |
+----------------------------------------------------------------------------------------------------+
| Pattern             | Spring Implementation                     | Primary Purpose                  |
+---------------------+-------------------------------------------+----------------------------------+
| Strategy            | Map<Key, Bean> / List<Bean> injection     | Dynamic algorithm selection (OCP)|
+---------------------+-------------------------------------------+----------------------------------+
| Template Method     | JdbcTemplate, TransactionTemplate         | Encapsulates resource invariant  |
|                     | RestClient, JmsTemplate                   | lifecycles around callbacks      |
+---------------------+-------------------------------------------+----------------------------------+
| Proxy               | Spring AOP (@Transactional, @Async)       | Non-invasive cross-cutting logic |
+---------------------+-------------------------------------------+----------------------------------+
| Observer            | ApplicationEventPublisher, @EventListener | Decoupled event broadcasting     |
+---------------------+-------------------------------------------+----------------------------------+
| Factory             | @Bean, FactoryBean<T>, BeanFactory        | Encapsulates complex creation    |
+---------------------+-------------------------------------------+----------------------------------+
| Chain of Resp.      | HandlerInterceptor, SecurityFilterChain   | Ordered sequential filtering     |
+---------------------+-------------------------------------------+----------------------------------+
| Decorator           | ClientHttpRequestInterceptor              | Wraps/augments component I/O     |
+----------------------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Design Patterns: Elements of Reusable Object-Oriented Software (GoF Original)](https://www.oreilly.com/library/view/design-patterns-elements/0201633612/)
- [Spring Framework Core Documentation: The IoC Container](https://docs.spring.io/spring-framework/reference/core/beans.html)
- [Spring AOP Concepts and Proxies](https://docs.spring.io/spring-framework/reference/core/aop.html)
- [Refactoring to Patterns (Joshua Kerievsky)](https://www.industriallogic.com/books/refactoring-to-patterns/)
- [Effective Java (Joshua Bloch, 3rd Edition)](https://www.oreilly.com/library/view/effective-java-3rd/9780134686097/)
