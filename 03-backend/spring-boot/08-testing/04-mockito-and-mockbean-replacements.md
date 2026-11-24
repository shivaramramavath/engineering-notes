# 04. Mockito in Spring Boot: From @MockBean to @MockitoBean

| Metadata | Value |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | `01-testing-fundamentals-in-spring.md`, `03-spring-boot-test-and-slices.md`, Mockito Basics |
| **Estimated Reading Time** | 35 minutes |
| **Status** | Complete |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | OpenJDK 21, Spring Boot 3.4.1, Spring Framework 6.2 |

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
In Spring Boot integration tests, replacing a specific real bean with a mock double inside the running `ApplicationContext` is a common requirement (e.g., mocking an external Payment Gateway while keeping the real Database and Service layers active).

For years, developers relied on Spring Boot's `@MockBean` and `@SpyBean`. However, `@MockBean` had severe architectural drawbacks:
- It was defined exclusively in Spring Boot rather than core Spring Framework, creating inconsistent behavior between pure Spring and Spring Boot tests.
- It frequently **broke Spring's ApplicationContext caching**: adding `@MockBean` to one test class altered the context configuration, forcing Spring to boot a brand-new application context and bloating CI build times.
- Lifecycle management and reset semantics (`MockReset.AFTER`) were fragile and prone to state leakage between test methods.

In **Spring Framework 6.2 and Spring Boot 3.4+**, the team introduced **first-class Bean Overriding infrastructure** (`@TestBean`, `@MockitoBean`, and `@MockitoSpyBean`) into core Spring, fully deprecating legacy `@MockBean`.

### Historical Evolution
- **Spring Boot 1.4 - 3.3**: Introduced `@MockBean` and `@SpyBean` in `org.springframework.boot.test.mock.mockito`. Highly popular but notorious for context cache fragmentation.
- **Spring Framework 6.2 / Spring Boot 3.4+**: Officially introduced the `org.springframework.test.context.bean.override` SPI. Legacy `@MockBean` and `@SpyBean` were officially deprecated in favor of `@MockitoBean` and `@MockitoSpyBean`.
- **Spring Boot 4.x**: Completely removes legacy `@MockBean` in favor of standardized `@MockitoBean`.

### Real-World Relevance
Understanding modern bean overriding semantics and migrating from `@MockBean` to `@MockitoBean` is crucial for maintaining fast, high-performance integration test suites in Spring Boot 3.4+.

---

## 2. Core Concepts

### Architectural Contracts
- **`@MockitoBean`**: Core annotation in `org.springframework.test.context.bean.override.mockito`. Replaces an existing bean definition in the `ApplicationContext` with a Mockito mock.
- **`@MockitoSpyBean`**: Wraps an existing bean definition with a Mockito spy, delegating to the real implementation unless explicitly stubbed.
- **`@TestBean`**: Modern alternative that overrides a bean with the return value of a static factory method defined in the test class.
- **Bean Overriding SPI**: The extensible infrastructure in core Spring (`BeanOverrideHandler`, `BeanOverrideProcessor`) governing how test doubles substitute production beans.
- **`MockReset`**: Strategy defining when Mockito resets mock invocation state:
  - `AFTER` (Default): Resets the mock after each test method runs.
  - `BEFORE`: Resets the mock prior to test method execution.
  - `NONE`: Never resets the mock automatically.

### The Mental Model
```
Standard ApplicationContext Graph:
[ OrderController ] ──► [ OrderService ] ──► [ PaymentGateway (Real HTTP) ]

Test Class with @MockitoBean:
[ OrderController ] ──► [ OrderService ] ──► [ PaymentGateway (Mockito Mock) ]
                                                      │
                                                      └── Injected via BeanOverrideHandler
                                                          Context Cache Key remains stable!
```

### Invariants
1. A `@MockitoBean` replaces the target bean across the **entire** `ApplicationContext`; any other bean in the context that injects that dependency will receive the mock.
2. If multiple beans match the type, specify `name = "targetBeanName"` to disambiguate which bean definition is overridden.
3. Overriding a bean modifies the context configuration signature; grouping tests with identical bean overrides is necessary to maintain context cache hits.

---

## 3. How It Works Internally

### Bean Overriding Pipeline in Spring 6.2+
```
Test Class Execution
   │
   ├── 1. BeanOverrideContextCustomizerFactory scans test class for @MockitoBean / @TestBean
   │
   ├── 2. Registers BeanOverrideBeanFactoryPostProcessor in the ApplicationContext:
   │      ├─ Locates target BeanDefinition (by type or name)
   │      ├─ Swaps BeanDefinition implementation with Mockito factory bean
   │      └─ Injects mock instance into DI graph before singletons are created
   │
   ├── 3. Registers MockitoResetTestExecutionListener:
   │      ├─ Tracks all created @MockitoBean instances
   │      └─ Invokes Mockito.reset(mock) after each @Test method runs
   │
   └── 4. Injects the created mock instance directly into the test class field
```

---

## 4. Syntax & Usage

### 1. Migrating from `@MockBean` to `@MockitoBean`
```java
// BEFORE (Spring Boot 1.4 - 3.3, DEPRECATED):
import org.springframework.boot.test.mock.mockito.MockBean;
// ...
@MockBean
private EmailService emailService;

// AFTER (Spring Boot 3.4+, MODERN STANDARD):
import org.springframework.test.context.bean.override.mockito.MockitoBean;
// ...
@MockitoBean
private EmailService emailService;
```

### 2. Disambiguating Multiple Candidates by Name
```java
package com.example.service;

import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.test.context.bean.override.mockito.MockitoBean;

import static org.mockito.BDDMockito.given;

@SpringBootTest
class DisambiguatedMockTest {

    @MockitoBean(name = "primaryNotificationService") // Overrides specific named bean
    private NotificationService notificationService;

    @Autowired
    private OrderProcessor orderProcessor;

    @Test
    void shouldProcessOrderWithMockedNotification() {
        given(notificationService.send(any())).willReturn(true);
        orderProcessor.completeOrder(101L);
    }
}
```

---

## 5. Practical Examples

### Example 1: Full `@SpringBootTest` with `@MockitoBean`
Testing a business workflow where external credit card processing is mocked while database, services, and transactions remain real.

```java
package com.example.service;

import com.example.domain.Order;
import com.example.external.StripePaymentClient;
import com.example.repository.OrderRepository;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.test.context.bean.override.mockito.MockitoBean;
import org.springframework.transaction.annotation.Transactional;

import java.math.BigDecimal;

import static org.assertj.core.api.Assertions.assertThat;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.BDDMockito.given;
import static org.mockito.BDDMockito.then;

@SpringBootTest
@Transactional
class CheckoutServiceIntegrationTest {

    @Autowired
    private CheckoutService checkoutService;

    @Autowired
    private OrderRepository orderRepository;

    @MockitoBean // Real HTTP Stripe client is replaced by this mock
    private StripePaymentClient stripeClient;

    @Test
    void shouldPersistOrderAndInvokeStripeClient() {
        given(stripeClient.charge(any(BigDecimal.class)))
            .willReturn("ch_test_998877");

        Order order = checkoutService.createAndPayOrder("CUST-1", new BigDecimal("150.00"));

        assertThat(order.getId()).isNotNull();
        assertThat(order.getTransactionReference()).isEqualTo("ch_test_998877");
        then(stripeClient).should().charge(new BigDecimal("150.00"));
        assertThat(orderRepository.findById(order.getId())).isPresent();
    }
}
```

### Example 2: Spying Real Beans via `@MockitoSpyBean`
Using `@MockitoSpyBean` to wrap a real auditing service, verifying that audit events were logged while letting real audit logic persist records.

```java
package com.example.service;

import com.example.audit.AuditService;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.test.context.bean.override.mockito.MockitoSpyBean;

import static org.mockito.Mockito.verify;

@SpringBootTest
class UserRegistrationSpyTest {

    @Autowired
    private UserRegistrationService registrationService;

    @MockitoSpyBean // Real AuditService is wrapped in a Mockito spy
    private AuditService auditService;

    @Test
    void shouldInvokeAuditServiceDuringRegistration() {
        registrationService.register("alice", "alice@example.com");

        // Verify the spy was called while real audit record was inserted
        verify(auditService).recordEvent("USER_REGISTERED", "alice");
    }
}
```

### Example 3: Edge Case – Using `@TestBean` as a Clean Alternative to Mockito
In Spring 6.2+, `@TestBean` allows supplying a lightweight in-memory fake without introducing Mockito bytecode proxies.

```java
package com.example.service;

import com.example.external.WeatherClient;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.test.context.bean.override.convention.TestBean;

import static org.assertj.core.api.Assertions.assertThat;

@SpringBootTest
class WeatherAlertServiceTest {

    @Autowired
    private WeatherAlertService alertService;

    // Overrides WeatherClient bean using the static factory method below
    @TestBean
    private WeatherClient weatherClient;

    // Convention: static method named "{fieldName}TestOverride" or matching type
    static WeatherClient weatherClient() {
        return city -> "SUNNY_25C"; // Pure lambda fake! Zero Mockito overhead!
    }

    @Test
    void shouldTriggerNoAlertsForSunnyWeather() {
        boolean alert = alertService.hasSevereWeatherAlert("London");
        assertThat(alert).isFalse();
    }
}
```

### Example 4: Failure Anti-Pattern & Production Fix
**Anti-Pattern**: Using `@MockitoBean` on a pure unit test where Spring is not even needed, slowing down test execution unnecessarily.

```java
// SLOW & UNNECESSARY: Boots Spring simply to inject a mock!
@SpringBootTest
class TaxCalculatorTest {
    @MockitoBean
    private TaxRates rates; // Why boot Spring for this?!
    @Autowired
    private TaxCalculator calculator;
}
```

**Production Fix**:
Use pure Mockito `@ExtendWith(MockitoExtension.class)`.

```java
// FAST & PURE: No Spring context!
@ExtendWith(MockitoExtension.class)
class TaxCalculatorTest {
    @Mock
    private TaxRates rates;
    @InjectMocks
    private TaxCalculator calculator;
}
```

### Example 5: High-Performance Test Context Preservation with Shared Mocks
Standardizing `@MockitoBean` definitions across an abstract base class to prevent context cache thrashing.

```java
package com.example.testing.base;

import com.example.external.EmailSender;
import com.example.external.SmsGateway;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.test.context.bean.override.mockito.MockitoBean;

@SpringBootTest
public abstract class BaseIntegrationTest {

    // Declaring mocks in the base class guarantees all subclasses share the SAME context cache key!
    @MockitoBean
    protected EmailSender emailSender;

    @MockitoBean
    protected SmsGateway smsGateway;
}
```

---

## 6. Best Practices

1. **Migrate Immediately to `@MockitoBean`**: Replace all occurrences of deprecated `org.springframework.boot.test.mock.mockito.MockBean` with `org.springframework.test.context.bean.override.mockito.MockitoBean`.
2. **Centralize `@MockitoBean` in Base Classes**: Consolidate common mocked external clients into a shared abstract test class to maximize Spring TestContext caching hits.
3. **Prefer `@TestBean` for Simple Fakes**: If a dependency only needs static or lambda-based responses, use Spring 6.2's `@TestBean` to eliminate Mockito proxy overhead.
4. **Disambiguate Using `name`**: When multiple beans implement an interface, specify `@MockitoBean(name = "beanName")` to prevent `NoUniqueBeanDefinitionException`.
5. **Verify Meaningful Side-Effects**: Use Mockito verification on mocks only to confirm external contract interactions (e.g., verifying an email was sent).
6. **Keep `@MockitoBean` for Integration Boundaries**: Never use `@MockitoBean` for internal domain services in unit tests; use pure unit tests with constructor injection instead.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Still using deprecated `@MockBean` | Deprecated in Spring Boot 3.4; removed in Spring Boot 4.0. | Migrate imports to `org.springframework.test.context.bean.override.mockito.MockitoBean`. |
| Context cache fragmentation | Test A mocks Service 1, Test B mocks Service 2; Spring creates two heavy contexts. | Consolidate common mocks in an abstract base test class. |
| Inadvertent state leakage across tests | Mock stubs configured in Test Method 1 bleed into Test Method 2. | Rely on default `MockReset.AFTER` or clean stubs in `@BeforeEach`. |
| Ambiguous bean overriding | Multiple beans match the mocked type; Spring cannot decide which bean to override. | Specify `@MockitoBean(name = "targetBean")`. |
| Over-mocking internal services | Mocking everything in an integration test reduces it to a trivial unit test. | Mock only external network boundaries (payment APIs, emails, third parties). |
| Using `@MockitoBean` on final classes | Byte Buddy subclassing fails on final classes in older JDK configurations without inline mocks. | Ensure mockito-subclass or mockito-inline is enabled. |

---

## 8. Performance & Security Notes

- **Context Cache Signature Impact**: Every unique combination of `@MockitoBean` declarations changes the `MergedContextConfiguration` cache key. A suite of 20 test classes with different `@MockitoBean` definitions will boot 20 distinct `ApplicationContext` instances, causing multi-minute build delays.
- **Byte Buddy Reflection Cost**: Generating dynamic mocks for Spring beans adds a tiny CPU cost during context startup (< 10ms per mock).
- **Virtual Threads Integration**: Mockito 5.14+ is fully compatible with Java 21 Virtual Threads; mock invocations do not pin carrier threads.

---

## 9. Comparison / When to Use What

| Annotation | Origin | Scope | Best Use Case |
|---|---|---|---|
| **`@Mock`** | Mockito | Pure Unit Tests (No Spring) | Domain unit tests with constructor injection |
| **`@MockBean`** | Spring Boot (Legacy) | Spring ApplicationContext | Deprecated; migrate to `@MockitoBean` |
| **`@MockitoBean`** | Spring Framework 6.2+ | Spring ApplicationContext | Replacing Spring beans in `@SpringBootTest` or slices |
| **`@MockitoSpyBean`**| Spring Framework 6.2+ | Spring ApplicationContext | Wrapping a real Spring bean to verify interactions |
| **`@TestBean`** | Spring Framework 6.2+ | Spring ApplicationContext | Replacing a bean with a static lambda fake |

---

## 10. Interview Questions

<details>
<summary>1. Why was `@MockBean` deprecated in favor of `@MockitoBean` in Spring Boot 3.4?</summary>

**Answer:**
`@MockBean` was a Spring Boot-specific feature that existed outside core Spring Framework. It had complicated lifecycle interactions, fragile mock resetting semantics, and frequently caused context cache fragmentation. Spring Framework 6.2 introduced a standardized, first-class Bean Overriding SPI (`@TestBean`, `@MockitoBean`, `@MockitoSpyBean`) directly into the core TestContext Framework, unifying bean overriding across all Spring environments.
</details>

<details>
<summary>2. How does `@MockitoBean` replace a bean definition in the Spring IoC container?</summary>

**Answer:**
During context initialization, a `BeanOverrideBeanFactoryPostProcessor` scans the test class for `@MockitoBean` annotations. It locates the matching `BeanDefinition` in the `BeanFactory` (by type or name), alters its target definition to be a Mockito mock factory, and registers a test execution listener to handle automated mock resets (`MockReset.AFTER`) between test methods.
</details>

<details>
<summary>3. Why does having different `@MockitoBean` definitions across test classes cause slow test runs?</summary>

**Answer:**
Spring's TestContext Framework uses the complete set of bean definitions and overrides to compute the `MergedContextConfiguration` cache key. If Test Class A overrides `EmailService`, while Test Class B overrides `SmsService`, their cache keys differ. Spring cannot reuse the cached context from Class A and is forced to boot a fresh `ApplicationContext` from scratch for Class B.
</details>

<details>
<summary>4. What is the difference between `@MockitoBean` and `@MockitoSpyBean`?</summary>

**Answer:**
- `@MockitoBean`: Replaces the real bean definition entirely with a hollow Mockito mock. None of the original bean's code will execute unless explicitly stubbed.
- `@MockitoSpyBean`: Retains the real bean instance but wraps it in a Mockito spy proxy. Real production code executes by default, while allowing developers to stub specific methods or verify interactions.
</details>

<details>
<summary>5. What is the `@TestBean` annotation introduced in Spring Framework 6.2?</summary>

**Answer:**
`@TestBean` is a non-Mockito alternative for bean overriding. It replaces an existing bean with the object returned by a static factory method defined in the test class (by convention matching `{fieldName}` or `{fieldName}TestOverride`). It is ideal for supplying lightweight lambda-based fakes or static test doubles without the bytecode proxy overhead of Mockito.
</details>

<details>
<summary>6. How does `MockReset` manage mock lifecycle between test methods?</summary>

**Answer:**
`MockReset` specifies when Mockito wipes recorded invocations and stubs on a mock bean. The default is `MockReset.AFTER`, which cleans the mock after each test method completes. This ensures stubs and verifications from one test method do not pollute subsequent test methods sharing the same cached context.
</details>

<details>
<summary>7. What happens if multiple beans in the context match the type annotated with `@MockitoBean`?</summary>

**Answer:**
Spring throws a `NoUniqueBeanDefinitionException` during context startup. To resolve this ambiguity, you must provide the bean name explicitly in the annotation: `@MockitoBean(name = "primaryNotificationService")`.
</details>

<details>
<summary>8. Can you use `@MockitoBean` inside a `@WebMvcTest` slice?</summary>

**Answer:**
Yes. In fact, it is the primary way to supply mocked service dependencies to controllers in slice tests, since `@WebMvcTest` excludes `@Service` and `@Repository` beans from component scanning.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Basic `@MockitoBean` in Slice Test (Easy)</summary>

**Task**: Write a `@WebMvcTest` for `GreetingController` using `@MockitoBean` to mock `GreetingService.greet()` and verify the returned HTTP response.

**Solution**:
```java
@WebMvcTest(GreetingController.class)
class GreetingControllerTest {

    @Autowired
    private MockMvc mockMvc;

    @MockitoBean
    private GreetingService greetingService;

    @Test
    void shouldReturnGreeting() throws Exception {
        given(greetingService.greet("World")).willReturn("Hello, World!");

        mockMvc.perform(get("/api/greet?name=World"))
            .andExpect(status().isOk())
            .andExpect(content().string("Hello, World!"));
    }
}
```
</details>

<details>
<summary>Exercise 2: Named Bean Disambiguation (Easy)</summary>

**Task**: Override a specific bean named `primaryEmailSender` among multiple `EmailSender` beans using `@MockitoBean`.

**Solution**:
```java
@SpringBootTest
class MultiBeanOverrideTest {

    @MockitoBean(name = "primaryEmailSender")
    private EmailSender emailSender;

    @Test
    void contextLoadsWithMock() {
        assertThat(emailSender).isNotNull();
    }
}
```
</details>

<details>
<summary>Exercise 3: Spying Real Service Invocations (Medium)</summary>

**Task**: Use `@MockitoSpyBean` on `PricingService` in a `@SpringBootTest` to verify that `calculateVat()` is invoked during checkout without mocking its return value.

**Solution**:
```java
@SpringBootTest
class CheckoutSpyTest {

    @Autowired
    private CheckoutService checkoutService;

    @MockitoSpyBean
    private PricingService pricingService;

    @Test
    void shouldInvokeRealPricingService() {
        checkoutService.processOrder(100.0);
        verify(pricingService).calculateVat(100.0);
    }
}
```
</details>

<details>
<summary>Exercise 4: Using `@TestBean` Factory Method (Medium)</summary>

**Task**: Replace a `TimeService` bean with a static fake using Spring 6.2's `@TestBean` returning a fixed timestamp `Instant.parse("2026-11-24T00:00:00Z")`.

**Solution**:
```java
@SpringBootTest
class FixedTimeTest {

    @TestBean
    private TimeService timeService;

    static TimeService timeService() {
        return () -> Instant.parse("2026-11-24T00:00:00Z");
    }

    @Test
    void shouldReturnFixedTime() {
        assertThat(timeService.now()).isEqualTo("2026-11-24T00:00:00Z");
    }
}
```
</details>

<details>
<summary>Exercise 5: Base Integration Test for Cache Preservation (Hard)</summary>

**Task**: Create an abstract integration test base class that declares `@MockitoBean` for two external clients (`SmsService` and `PaymentGateway`) and write two subclasses verifying that the context starts only once.

**Solution**:
```java
@SpringBootTest
public abstract class SharedMocksBaseTest {
    @MockitoBean
    protected SmsService smsService;

    @MockitoBean
    protected PaymentGateway paymentGateway;
}

class PaymentFlowTest extends SharedMocksBaseTest {
    @Test void testPayment() {}
}

class SmsFlowTest extends SharedMocksBaseTest {
    @Test void testSms() {}
}
// Both classes share identical MergedContextConfiguration, ensuring 100% cache hits!
```
</details>

<details>
<summary>Exercise 6: MockReset Verification (Hard)</summary>

**Task**: Write a test class demonstrating that a `@MockitoBean` stub configured in one test method does not bleed into a second test method due to default `MockReset.AFTER`.

**Solution**:
```java
@SpringBootTest
class MockResetIsolationTest {

    @MockitoBean
    private AuthService authService;

    @Test
    void testMethodOne() {
        given(authService.authenticate("alice")).willReturn(true);
        assertThat(authService.authenticate("alice")).isTrue();
    }

    @Test
    void testMethodTwo() {
        // Mock was automatically reset by MockReset.AFTER; defaults to false!
        assertThat(authService.authenticate("alice")).isFalse();
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **`@MockitoBean` replaces `@MockBean`**: Modern Spring Boot 3.4+ / Spring 6.2 standard.
- **`@MockitoSpyBean` replaces `@SpyBean`**: Wraps a real bean in a Mockito spy.
- **`@TestBean` for Fakes**: Non-Mockito alternative using static factory methods.
- **Cache Preservation**: Consolidate `@MockitoBean` declarations in shared base classes to avoid context cache thrashing.
- **Automatic Reset**: Mocks are automatically reset after each test method via `MockReset.AFTER`.

---

## 13. Further Reading
- [Spring Framework 6.2 Reference: Bean Overriding](https://docs.spring.io/spring-framework/reference/testing/testcontext-framework/bean-overriding.html)
- [Spring Boot 3.4 Release Notes: Bean Overriding](https://github.com/spring-projects/spring-boot/wiki/Spring-Boot-3.4-Release-Notes#bean-overriding-support)
- [Mockito Official Guide](https://javadoc.io/doc/org.mockito/mockito-core/latest/org/mockito/Mockito.html)
- [Spring TestContext Framework: Context Caching](https://docs.spring.io/spring-framework/reference/testing/testcontext-framework/ctx-management/caching.html)
- [AssertJ Fluent Assertions](https://assertj.github.io/doc/)
