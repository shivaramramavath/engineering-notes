# 02. Pure Unit Testing Without Spring (JUnit 5 & Mockito)

| Metadata | Value |
|---|---|
| **Level** | Beginner |
| **Prerequisites** | Java OOP, Core Dependency Injection concepts |
| **Estimated Reading Time** | 35 minutes |
| **Status** | Complete |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | OpenJDK 21, JUnit 5.11, Mockito 5.14, AssertJ 3.26 |

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
A common anti-pattern in modern Spring Boot development is launching the entire Spring container (`@SpringBootTest` or test slices) simply to test a single domain service class. Booting an `ApplicationContext` takes anywhere from 1.5 to 5 seconds per test class. When a project accumulates hundreds of such tests, build times stretch into tens of minutes.

Furthermore, relying on the container masks architectural flaws:
- If a class cannot be instantiated or tested without Spring, it suffers from **tight coupling**, hidden dependencies (e.g., field injection with `@Autowired`), and poor cohesion.
- Pure unit tests force clean object-oriented architecture: by designing classes with **constructor injection**, services can be instantiated directly with plain Java `new` and tested with **JUnit 5**, **Mockito**, and **AssertJ** in less than 5 milliseconds.

### Historical Evolution
- **Java 1.4 / JUnit 3**: Required extending `TestCase`, prefixing methods with `test*`, and using primitive assertion libraries.
- **JUnit 4 & Mockito 1.x**: Introduced `@Test`, `@RunWith(MockitoJUnitRunner.class)`, and `Mockito.when()`. Field injection was rampant.
- **JUnit 5 (Jupiter) & Mockito 5.x**: Decoupled engine from API, introduced `@ExtendWith(MockitoExtension.class)`, modernized assertion fluency via AssertJ, embraced lambda expressions, and made constructor injection the de-facto standard.

### Real-World Relevance
Pure unit tests form the vast foundation of the Testing Pyramid (~70%). In continuous integration environments, thousands of pure unit tests can execute in under 10 seconds, providing immediate feedback during code refactorings.

---

## 2. Core Concepts

### Architectural Contracts
- **Unit Under Test (SUT)**: The specific class being tested (System Under Test). Should be tested in complete isolation from all real external dependencies (databases, network APIs, file systems).
- **Test Double**: A generic term for any object that stands in for a real dependency during testing:
  - *Dummy*: Passed around but never actually used (e.g., filling a parameter list).
  - *Stub*: Provides predefined answers to method calls made during the test.
  - *Mock*: A stub with pre-programmed expectations that can verify whether specific methods were invoked.
  - *Spy*: A real object wrapper that records method interactions while delegating to real method implementations.
  - *Fake*: A working implementation with a shortcut (e.g., an in-memory `ConcurrentHashMap` repository).
- **`@ExtendWith(MockitoExtension.class)`**: JUnit 5 extension that initializes fields annotated with `@Mock`, `@Spy`, and `@InjectMocks` before each test method runs.
- **BDDMockito**: A syntax layer over Mockito providing behavior-driven language (`given()`, `when()`, `then()`) matching Arrange-Act-Assert (AAA) conventions.
- **AssertJ**: Fluent assertion library providing readable, expressive assertions (`assertThat(result).isNotNull().hasSize(3)`).

### The Mental Model
```
[ JUnit 5 Test Runner ]
         │
         ├── 1. Initializes Mocks via MockitoExtension
         │      - Mock: OrderRepository
         │      - Mock: PaymentGateway
         │
         ├── 2. Instantiates SUT via Constructor Injection:
         │      OrderService service = new OrderService(mockRepo, mockGateway);
         │
         ├── 3. Arrange (Given):
         │      given(mockRepo.findById(101L)).willReturn(Optional.of(validOrder));
         │
         ├── 4. Act (When):
         │      OrderResult result = service.processOrder(101L);
         │
         └── 5. Assert (Then):
                assertThat(result.isSuccess()).isTrue();
                then(mockGateway).should().charge(any());
```

### Invariants
1. A pure unit test must **never** load a Spring `ApplicationContext` or use `@Autowired`.
2. A unit test must never touch the network, file system, or a real database.
3. Tests must be independent: no test method may depend on the execution order or shared mutable state of another test method.

---

## 3. How It Works Internally

### The Mockito Dynamic Proxy Mechanism
When you create a mock (`mock(OrderRepository.class)` or `@Mock`):
1. **Bytecode Generation**: Mockito uses **Byte Buddy** to generate a dynamic subclass or proxy of the target class at runtime.
2. **Method Interception**: Every method invocation on the mock is intercepted by a Mockito `MethodInterceptor`.
3. **Invocation Recording**: Mockito records the method name, arguments, and invocation count in an internal `InvocationContainer`.
4. **Stub Lookup**: When a stubbed method is called, Mockito matches the invocation against configured rules (`when(...).thenReturn(...)`). If no stub matches, it returns a safe default (e.g., `null`, `0`, `false`, or empty collections).
5. **Verification**: When `verify(mock).save(...)` executes, Mockito inspects the recorded invocation history. If the method was not invoked with matching arguments, it throws an `AssertionError`.

---

## 4. Syntax & Usage

### 1. Minimal Maven Dependencies (`pom.xml`)
The `spring-boot-starter-test` artifact automatically bundles JUnit 5, Mockito, and AssertJ:
```xml
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-starter-test</artifactId>
    <scope>test</scope>
</dependency>
```

### 2. Standard JUnit 5 & Mockito Skeleton
```java
package com.example.service;

import com.example.repository.CustomerRepository;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.extension.ExtendWith;
import org.mockito.InjectMocks;
import org.mockito.Mock;
import org.mockito.junit.jupiter.MockitoExtension;

import java.util.Optional;

import static org.assertj.core.api.Assertions.assertThat;
import static org.assertj.core.api.Assertions.assertThatThrownBy;
import static org.mockito.BDDMockito.given;
import static org.mockito.BDDMockito.then;
import static org.mockito.Mockito.never;

@ExtendWith(MockitoExtension.class)
class CustomerServiceTest {

    @Mock
    private CustomerRepository customerRepository;

    @InjectMocks
    private CustomerService customerService;

    @Test
    @DisplayName("Should return customer profile when valid ID is provided")
    void shouldReturnCustomerProfileWhenIdExists() {
        // Arrange
        Customer customer = new Customer(1L, "Alice", "alice@example.com");
        given(customerRepository.findById(1L)).willReturn(Optional.of(customer));

        // Act
        CustomerDTO result = customerService.getCustomerById(1L);

        // Assert
        assertThat(result).isNotNull();
        assertThat(result.email()).isEqualTo("alice@example.com");
        then(customerRepository).should().findById(1L);
    }
}
```

---

## 5. Practical Examples

### Example 1: Constructor-Based Instantiation (Zero Framework Overhead)
Demonstrating clean instantiation without even `@InjectMocks`, guaranteeing maximum transparency.

```java
package com.example.service;

import com.example.domain.Account;
import com.example.exception.InsufficientFundsException;
import com.example.repository.AccountRepository;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.extension.ExtendWith;
import org.mockito.Mock;
import org.mockito.junit.jupiter.MockitoExtension;

import java.math.BigDecimal;
import java.util.Optional;

import static org.assertj.core.api.Assertions.assertThat;
import static org.assertj.core.api.Assertions.assertThatThrownBy;
import static org.mockito.BDDMockito.given;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.never;

@ExtendWith(MockitoExtension.class)
class BankTransferServiceTest {

    @Mock
    private AccountRepository accountRepository;

    private BankTransferService transferService;

    @BeforeEach
    void setUp() {
        // Explicit constructor injection: fast, clear, and refactoring-safe
        this.transferService = new BankTransferService(accountRepository);
    }

    @Test
    void shouldTransferFundsBetweenValidAccounts() {
        Account source = new Account("ACC-1", new BigDecimal("500.00"));
        Account target = new Account("ACC-2", new BigDecimal("100.00"));

        given(accountRepository.findById("ACC-1")).willReturn(Optional.of(source));
        given(accountRepository.findById("ACC-2")).willReturn(Optional.of(target));

        transferService.transfer("ACC-1", "ACC-2", new BigDecimal("200.00"));

        assertThat(source.getBalance()).isEqualByComparingTo("300.00");
        assertThat(target.getBalance()).isEqualByComparingTo("300.00");
        verify(accountRepository).save(source);
        verify(accountRepository).save(target);
    }

    @Test
    void shouldThrowExceptionAndNeverSaveWhenBalanceIsInsufficient() {
        Account source = new Account("ACC-1", new BigDecimal("50.00"));
        given(accountRepository.findById("ACC-1")).willReturn(Optional.of(source));

        assertThatThrownBy(() -> transferService.transfer("ACC-1", "ACC-2", new BigDecimal("200.00")))
            .isInstanceOf(InsufficientFundsException.class)
            .hasMessageContaining("Insufficient funds");

        verify(accountRepository, never()).save(source);
    }
}
```

### Example 2: Verifying Invocations with `ArgumentCaptor`
Capturing objects passed to dependency mocks to assert on internal transformation state.

```java
package com.example.service;

import com.example.domain.AuditRecord;
import com.example.event.AuditLogger;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.extension.ExtendWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.jupiter.MockitoExtension;

import static org.assertj.core.api.Assertions.assertThat;
import static org.mockito.Mockito.verify;

@ExtendWith(MockitoExtension.class)
class AuditServiceTest {

    @Mock
    private AuditLogger auditLogger;

    @Captor
    private ArgumentCaptor<AuditRecord> recordCaptor;

    @Test
    void shouldCaptureAndVerifyLoggedAuditEvent() {
        AuditService service = new AuditService(auditLogger);

        service.recordUserLogin("john_doe", "192.168.1.50");

        verify(auditLogger).log(recordCaptor.capture());

        AuditRecord captured = recordCaptor.getValue();
        assertThat(captured.getUsername()).isEqualTo("john_doe");
        assertThat(captured.getIpAddress()).isEqualTo("192.168.1.50");
        assertThat(captured.getTimestamp()).isNotNull();
    }
}
```

### Example 3: Edge Case – Partial Mocks Using `@Spy`
Using a Mockito spy to test a legacy class where one internal method must be stubbed while testing real logic on other methods.

```java
package com.example.service;

import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.extension.ExtendWith;
import org.mockito.Spy;
import org.mockito.junit.jupiter.MockitoExtension;

import static org.assertj.core.api.Assertions.assertThat;
import static org.mockito.Mockito.doReturn;

@ExtendWith(MockitoExtension.class)
class LegacyReportGeneratorTest {

    @Spy
    private LegacyReportGenerator reportGenerator;

    @Test
    void shouldCalculateReportWithMockedExternalData() {
        // Stub ONLY the expensive network call, execute real calculation logic
        doReturn("MOCK_EXTERNAL_RAW_CSV").when(reportGenerator).fetchRawDataOverNetwork();

        String summary = reportGenerator.generateSummary();

        assertThat(summary).contains("PROCESSED: MOCK_EXTERNAL_RAW_CSV");
    }
}
```

### Example 4: Failure Anti-Pattern & Production Fix
**Anti-Pattern**: Using `@SpringBootTest` with `@MockBean` to test a simple utility service, slowing down test execution by 3,000%.

```java
// SEVERE ANTI-PATTERN: Takes 3,500ms to test a simple calculation!
@SpringBootTest
class CalculatorServiceTest {
    @Autowired
    private CalculatorService service; // Unnecessary Spring IoC!

    @Test
    void testAdd() {
        assertThat(service.add(2, 3)).isEqualTo(5);
    }
}
```

**Production Fix**:
Instantiate the service directly with plain Java `new`. Executes in under 1ms.

```java
// FAST & PURE: Takes 1ms!
class CalculatorServiceTest {
    private final CalculatorService service = new CalculatorService();

    @Test
    void testAdd() {
        assertThat(service.add(2, 3)).isEqualTo(5);
    }
}
```

### Example 5: High-Performance Parameterized Testing with AssertJ
Testing edge cases cleanly with JUnit 5 `@ParameterizedTest` and `@CsvSource`.

```java
package com.example.domain;

import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.CsvSource;

import static org.assertj.core.api.Assertions.assertThat;

class PasswordPolicyTest {

    private final PasswordPolicyValidator validator = new PasswordPolicyValidator();

    @ParameterizedTest(name = "Password ''{0}'' should be valid: {1}")
    @CsvSource({
        "Short1!, false",
        "NoDigitsSpecial!, false",
        "ValidP@ssw0rd123, true",
        "'', false",
        "   , false",
        "P@ssw0rdWithSuperLongLengthAndHighEntropy2026!, true"
    })
    void shouldValidatePasswordComplexity(String password, boolean expectedValidity) {
        boolean result = validator.isValid(password);
        assertThat(result).isEqualTo(expectedValidity);
    }
}
```

---

## 6. Best Practices

1. **Mandate Constructor Injection**: Never use field injection (`@Autowired` on private fields). Constructor injection makes unit testing trivial because dependencies can be passed via `new`.
2. **Follow Arrange-Act-Assert (AAA)**: Clearly structure tests into three distinct blocks: Arrange (setup stubs), Act (execute SUT), and Assert (verify results).
3. **Prefer BDDMockito Syntax**: Use `given(...).willReturn(...)` and `then(...).should()` instead of legacy `when(...).thenReturn()`.
4. **Assert with AssertJ**: Prefer `assertThat(actual).isEqualTo(expected)` over JUnit's `assertEquals(expected, actual)` for better readability and failure diagnostics.
5. **Verify Interactions Sparingly**: Do not verify every single mock method call; verify only side-effects that represent actual business contract fulfillment (e.g., saving an entity or sending an email).
6. **Use `@ParameterizedTest` for Edge Cases**: Replace multiple repetitive test methods with parameterized tests to verify boundaries comprehensively.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Using `@SpringBootTest` for unit tests | Boots the whole container, slowing build execution by orders of magnitude. | Use `@ExtendWith(MockitoExtension.class)` or plain Java `new`. |
| Field injection with `@Autowired` | Private fields cannot be injected without reflection or Spring containers. | Refactor classes to constructor injection. |
| Stubbing unnecessary methods | Mockito throws `UnnecessaryStubbingException` under strict stubbing mode. | Only stub methods that the SUT will actually invoke. |
| Mocking value objects / DTOs | Mocking simple records or DTOs adds unnecessary complexity and mock overhead. | Instantiate real domain records and DTOs using constructors or builders. |
| Over-verifying internal calls | Verifying every getter call couples tests to implementation details, causing brittle tests. | Test outputs and public behavior, not private internal mechanics. |
| Forgetting `@ExtendWith(MockitoExtension.class)` | Mocks annotated with `@Mock` remain `null`, causing `NullPointerException`. | Add the extension or initialize mocks via `MockitoAnnotations.openMocks(this)`. |

---

## 8. Performance & Security Notes

- **Zero Container Startup Time**: Pure unit tests execute in the sub-millisecond range (< 2ms per test). A test suite of 2,000 pure unit tests completes in under 4 seconds on modern multi-core processors.
- **Strict Stubbing Mode**: Mockito 5 defaults to `Strictness.STRICT_STUBS`, which proactively warns against unused mock setup and catches subtle bugs where test data doesn't match production calls.
- **Virtual Threads Compatibility**: Pure unit tests execute cleanly in parallel on Java 21 without thread-safety concerns, provided tests avoid shared mutable static state.

---

## 9. Comparison / When to Use What

| Feature | Pure Unit Test | Spring Slice Test (`@WebMvcTest`) | Full `@SpringBootTest` |
|---|---|---|---|
| **Speed** | **< 5 ms** | 100 - 300 ms | 1,500 - 5,000 ms |
| **Spring IoC** | None | Partial Slice | Complete Container |
| **Dependencies** | 100% Mocked | Mocked via `@MockitoBean` | Real or Testcontainers |
| **Best For** | Domain logic, services, algorithms | Controller routing, serialization | End-to-end integration flows |

---

## 10. Interview Questions

<details>
<summary>1. Why does constructor injection make unit testing easier than field injection?</summary>

**Answer:**
Field injection (`@Autowired` on private fields) tightly couples the class to the Spring container because outside of Spring, private fields cannot be set without reflection. Constructor injection allows developers to instantiate the class with plain Java `new` in a unit test, passing mock objects directly into the constructor without requiring Spring, reflection, or any testing framework helpers.
</details>

<details>
<summary>2. What is the difference between a Mock and a Spy in Mockito?</summary>

**Answer:**
- **Mock**: A completely artificial dummy object created by bytecode generation. None of the real class's code is executed; all methods return default values (`null`, `0`, `false`) unless explicitly stubbed.
- **Spy**: A partial mock wrapped around a **real** object instance. By default, calling a method on a spy executes the actual production method logic unless that specific method has been stubbed.
</details>

<details>
<summary>3. What is Mockito's `Strictness.STRICT_STUBS` and why is it beneficial?</summary>

**Answer:**
`Strictness.STRICT_STUBS` is Mockito's default mode. It prevents test suite rot by:
1. Throwing an `UnnecessaryStubbingException` if you stub a method that is never invoked during the test.
2. Detecting argument mismatches where a method is called with different arguments than expected.
This keeps test code clean and eliminates dead stubbing code.
</details>

<details>
<summary>4. What is the purpose of `ArgumentCaptor`?</summary>

**Answer:**
`ArgumentCaptor` allows developers to capture and inspect the exact arguments passed to a mock dependency method during execution. It is especially useful when verifying that a service constructed an internal domain object or event payload correctly before passing it to a repository or message publisher.
</details>

<details>
<summary>5. Why should you avoid mocking data transfer objects (DTOs) and domain records?</summary>

**Answer:**
DTOs, entities, and Java records are state containers with zero business behavior. Mocking them is an anti-pattern because constructing them with real data is trivial, fast, and eliminates unnecessary Mockito proxy overhead. Mocks should be reserved for classes representing services, repositories, or external boundaries.
</details>

<details>
<summary>6. How does BDDMockito differ from standard Mockito?</summary>

**Answer:**
BDDMockito is a semantic wrapper around Mockito that aligns method names with Behavior-Driven Development (Given-When-Then) conventions. Instead of `when(mock.call()).thenReturn(val)`, it uses `given(mock.call()).willReturn(val)`. Instead of `verify(mock).call()`, it uses `then(mock).should().call()`. Functionally they are identical, but BDDMockito improves readability.
</details>

<details>
<summary>7. What is the difference between `@Mock` and `@InjectMocks`?</summary>

**Answer:**
- `@Mock`: Instructs Mockito to create a mock double of the annotated dependency.
- `@InjectMocks`: Instructs Mockito to create a real instance of the class under test and inject all available `@Mock` or `@Spy` fields into it (via constructor, setter, or field injection).
</details>

<details>
<summary>8. How do you test that a specific exception is thrown using AssertJ?</summary>

**Answer:**
Using `assertThatThrownBy()`:
```java
assertThatThrownBy(() -> service.process(-1))
    .isInstanceOf(IllegalArgumentException.class)
    .hasMessage("ID must be positive");
```
This is cleaner and more expressive than JUnit's `assertThrows`.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Basic Service Mocking (Easy)</summary>

**Task**: Write a pure unit test using `@ExtendWith(MockitoExtension.class)` that tests `UserService.findEmail(id)` by stubbing `UserRepository.findById(id)`.

**Solution**:
```java
@ExtendWith(MockitoExtension.class)
class UserServiceTest {

    @Mock
    private UserRepository userRepository;

    @InjectMocks
    private UserService userService;

    @Test
    void shouldReturnEmail() {
        given(userRepository.findById(10L))
            .willReturn(Optional.of(new User(10L, "test@example.com")));

        String email = userService.findEmail(10L);

        assertThat(email).isEqualTo("test@example.com");
    }
}
```
</details>

<details>
<summary>Exercise 2: AssertJ Exception Verification (Easy)</summary>

**Task**: Write an assertion using AssertJ verifying that calling `calculator.divide(10, 0)` throws `ArithmeticException` with message `"/ by zero"`.

**Solution**:
```java
@Test
void shouldThrowArithmeticExceptionOnZeroDivision() {
    Calculator calc = new Calculator();
    assertThatThrownBy(() -> calc.divide(10, 0))
        .isInstanceOf(ArithmeticException.class)
        .hasMessage("/ by zero");
}
```
</details>

<details>
<summary>Exercise 3: Parameterized Domain Test (Medium)</summary>

**Task**: Write a `@ParameterizedTest` verifying that a `TaxCalculator` correctly computes a 20% VAT across 3 different price inputs.

**Solution**:
```java
@ParameterizedTest
@CsvSource({
    "100.00, 20.00",
    "250.00, 50.00",
    "0.00, 0.00"
})
void shouldCalculateCorrectVat(BigDecimal gross, BigDecimal expectedTax) {
    TaxCalculator calculator = new TaxCalculator();
    assertThat(calculator.calculateVat(gross)).isEqualByComparingTo(expectedTax);
}
```
</details>

<details>
<summary>Exercise 4: Capturing Event Payloads with ArgumentCaptor (Medium)</summary>

**Task**: Test that `OrderService.placeOrder()` calls `EventPublisher.publish()` with an `OrderPlacedEvent` that contains the correct order ID and status `"PENDING"`.

**Solution**:
```java
@Test
void shouldPublishOrderPlacedEvent() {
    EventPublisher publisher = mock(EventPublisher.class);
    OrderService service = new OrderService(publisher);
    ArgumentCaptor<OrderPlacedEvent> captor = ArgumentCaptor.forClass(OrderPlacedEvent.class);

    service.placeOrder("ORD-99");

    verify(publisher).publish(captor.capture());
    assertThat(captor.getValue().orderId()).isEqualTo("ORD-99");
    assertThat(captor.getValue().status()).isEqualTo("PENDING");
}
```
</details>

<details>
<summary>Exercise 5: Verification of Never-Executed Interactions (Hard)</summary>

**Task**: Write a unit test ensuring that when a payment validation fails, the `PaymentGateway.charge()` method is never invoked and an error is logged.

**Solution**:
```java
@Test
void shouldNeverChargeGatewayWhenValidationFails() {
    PaymentValidator validator = mock(PaymentValidator.class);
    PaymentGateway gateway = mock(PaymentGateway.class);
    PaymentProcessor processor = new PaymentProcessor(validator, gateway);

    PaymentRequest invalidRequest = new PaymentRequest(-50.0);
    given(validator.isValid(invalidRequest)).willReturn(false);

    assertThatThrownBy(() -> processor.process(invalidRequest))
        .isInstanceOf(InvalidPaymentException.class);

    verify(gateway, never()).charge(any());
}
```
</details>

<details>
<summary>Exercise 6: Testing Complex Business State Machine (Hard)</summary>

**Task**: Implement a pure unit test verifying that an `Order` domain aggregate enforces state transitions: `CREATED -> PAID -> SHIPPED` and throws an `IllegalStateException` if attempting to ship an unpaid order.

**Solution**:
```java
class OrderAggregateTest {

    @Test
    void shouldTransitionToPaidSuccessfully() {
        Order order = new Order("ORD-1");
        order.markPaid();
        assertThat(order.getStatus()).isEqualTo(OrderStatus.PAID);
    }

    @Test
    void shouldDisallowShippingUnpaidOrder() {
        Order order = new Order("ORD-1");
        assertThatThrownBy(order::ship)
            .isInstanceOf(IllegalStateException.class)
            .hasMessage("Cannot ship an unpaid order");
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Constructor Injection**: Enables framework-free unit testing using plain Java `new`.
- **Pure Unit Tests are Fast**: Execute in < 5ms without Spring overhead.
- **BDDMockito**: Use `given(...).willReturn(...)` for Arrange and `then(...).should()` for Assert.
- **AssertJ**: Use fluent `assertThat()` assertions for clean, expressive diagnostics.
- **`ArgumentCaptor`**: Captures method parameters passed to mocks for deep assertion.

---

## 13. Further Reading
- [JUnit 5 User Guide](https://junit.org/junit5/docs/current/user-guide/)
- [Mockito Official Documentation](https://javadoc.io/doc/org.mockito/mockito-core/latest/org/mockito/Mockito.html)
- [AssertJ Fluent Assertions Guide](https://assertj.github.io/doc/)
- [Martin Fowler: Mocks Aren't Stubs](https://martinfowler.com/articles/mocksArentStubs.html)
- [Refactoring to Constructor Injection](https://refactoring.guru/replace-parameter-with-method-call)
