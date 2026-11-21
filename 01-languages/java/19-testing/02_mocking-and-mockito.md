# Mocking and Mockito: Architecture, Verification, and Best Practices

> Mocking mechanics in Java: Mockito Architecture, Byte Buddy Inline Mocking, Strict Stubs, ArgumentCaptor, Static Method Mocking, and Avoiding Mock Overuse.

| Metadata | Details |
|---|---|
| **Level** | Core / Intermediate |
| **Prerequisites** | Java Interfaces, Dynamic Proxies, Unit Testing Fundamentals |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **JDK Baseline** | Java 17 / 21 / 25 LTS (Mockito 5.x) |

---

## Table of Contents
1. [1. What & Why](#1-what--why)
2. [2. Core Concepts](#2-core-concepts)
3. [3. How It Works Internally: Byte Buddy & Inline Mock Maker](#3-how-it-works-internally-byte-buddy--inline-mock-maker)
4. [4. Syntax & Usage](#4-syntax--usage)
5. [5. Practical Examples](#5-practical-examples)
6. [6. Best Practices](#6-best-practices)
7. [7. Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [8. Performance & Security Notes](#8-performance--security-notes)
9. [9. Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [10. Interview Questions](#10-interview-questions)
11. [11. Practice Exercises](#11-practice-exercises)
12. [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [13. Further Reading](#13-further-reading)

---

## 1. What & Why
In unit testing, isolating the System Under Test (SUT) from external collaborators—such as payment gateways, third-party REST clients, or complex message brokers—is essential for fast, deterministic verification.

**Mockito** is the industry-standard mocking framework for Java. It allows developers to create lightweight proxy objects (test doubles) that simulate collaborator behavior without implementing concrete classes. However, mocking is a double-edged sword: over-mocking creates brittle tests tightly coupled to private implementation details, while outdated stubbing hides real bugs. Modern Mockito (version 5+) introduced the **Inline Mock Maker** by default, native **Java 21 Record** and **final class** mocking, and **Strict Stubbing** to eliminate test rot.

---

## 2. Core Concepts
- **Mock vs Spy**:
  - *Mock*: A completely synthetic object generated via bytecode. All methods return default values (`null`, `0`, `false`) unless explicitly stubbed.
  - *Spy*: A partial mock wrapping a real instance. Real methods execute by default unless explicitly overridden with canned behavior.
- **Stubbing**: Pre-programming method return values (`when(...).thenReturn(...)` or `doReturn(...).when(...)`).
- **Verification**: Inspecting that collaborating methods were invoked with specific parameters and frequencies (`verify(mock, times(1)).doAction()`).
- **Argument Matchers**: Flexible argument constraints (`any()`, `eq()`, `argThat()`). If matchers are used for one parameter, all parameters in that method call must use matchers.
- **ArgumentCaptor**: Captures actual argument values passed to a mock during execution, allowing deep post-execution assertions.
- **Strictness (`Strictness.STRICT_STUBS`)**: Enforces clean testing by throwing errors if stubs are declared but never called (unnecessary stubbing) or if arguments mismatch slightly.
- **Mocking Static Methods (`mockStatic`)**: Scoped mocking of static utility methods within a try-with-resources block.

---

## 3. How It Works Internally: Byte Buddy & Inline Mock Maker

Mockito generates and instruments mock objects dynamically at runtime using Rafael Winterhalter's Byte Buddy library:

```
[ Mockito.mock(PaymentClient.class) ]
                  |
                  v
+-------------------------------------------------------+
|             Byte Buddy Bytecode Engine                |
|  - Injects java.lang.instrument agent                 |
|  - Redefines PaymentClient class bytecode in memory   |
|  - Intercepts method entries and redirects to handler |
+-------------------------------------------------------+
                  |
                  v
+-------------------------------------------------------+
|              MockHandler / InvocationContainer        |
|  - Records every method invocation (Arguments, Thread)|
|  - Matches against stubbing rules in LinkedHashSet    |
|  - Returns canned value or records invocation history |
+-------------------------------------------------------+
```

### Subclass Mocking vs Inline Mocking (Mockito 5+):
- **Subclass Mock Maker (Legacy)**: Generated a dynamic subclass at runtime (`PaymentClient$MockitoMock$1234`). Failed on `final` classes, `final` methods, and Java Records because final types cannot be subclassed.
- **Inline Mock Maker (Modern Default)**: Uses JVM bytecode instrumentation (`ClassFileTransformer`) to modify the original class bytecode directly in memory. It transparently mocks `final` classes, `final` methods, Java 21 Records, and static methods without subclassing restrictions.

---

## 4. Syntax & Usage

### 4.1 Production Mockito Test with JUnit 5 Extension & Strict Stubs
```java
package com.example.testing;

import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.extension.ExtendWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.InjectMocks;
import org.mockito.Mock;
import org.mockito.junit.jupiter.MockitoExtension;
import java.math.BigDecimal;
import java.util.Optional;
import static org.assertj.core.api.Assertions.*;
import static org.mockito.Mockito.*;

@ExtendWith(MockitoExtension.class) // Enables @Mock injection and Strict Stubbing automatically!
@DisplayName("OrderProcessingService Collaboration Tests")
class OrderProcessingServiceTest {

    @Mock
    private PaymentGateway paymentGateway;

    @Mock
    private OrderRepository orderRepository;

    @Mock
    private NotificationClient notificationClient;

    @InjectMocks
    private OrderProcessingService orderService;

    @Captor
    private ArgumentCaptor<PaymentRequest> paymentCaptor;

    @Test
    @DisplayName("Process order: charges payment gateway and sends confirmation")
    void processOrder_successfulPayment_updatesOrderAndNotifies() {
        // Arrange
        Order order = new Order("ORD-101", "cust@example.com", new BigDecimal("120.00"));
        when(orderRepository.findById("ORD-101")).thenReturn(Optional.of(order));
        when(paymentGateway.charge(any(PaymentRequest.class)))
                .thenReturn(new PaymentResult(true, "TX-9988"));

        // Act
        orderService.processOrder("ORD-101");

        // Assert: Verify payment gateway received correct payload via ArgumentCaptor
        verify(paymentGateway, times(1)).charge(paymentCaptor.capture());
        PaymentRequest capturedRequest = paymentCaptor.getValue();
        assertThat(capturedRequest.orderId()).isEqualTo("ORD-101");
        assertThat(capturedRequest.amount()).isEqualByComparingTo(new BigDecimal("120.00"));

        // Verify notification was sent
        verify(notificationClient, times(1))
                .sendReceipt(eq("cust@example.com"), eq("TX-9988"));

        // Verify no other unexpected interactions occurred on notification client
        verifyNoMoreInteractions(notificationClient);
    }
}
```

---

## 5. Practical Examples

### Example 1: Mocking Static Methods with Scoped try-with-resources
```java
package com.example.testing;

import org.junit.jupiter.api.Test;
import org.mockito.MockedStatic;
import java.time.Instant;
import static org.assertj.core.api.Assertions.*;
import static org.mockito.Mockito.*;

class AuditLoggerTest {

    @Test
    void logEvent_recordsFixedTimestamp() {
        Instant fixedNow = Instant.parse("2026-11-20T12:00:00Z");

        // Mocking static Instant.now() strictly within the try block
        try (MockedStatic<Instant> mockedInstant = mockStatic(Instant.class, CALLS_REAL_METHODS)) {
            mockedInstant.when(Instant::now).thenReturn(fixedNow);

            AuditRecord record = AuditLogger.createRecord("USER_LOGOUT");

            assertThat(record.timestamp()).isEqualTo(fixedNow);
        }
        // Outside the try block, Instant.now() is automatically restored to real system clock!
    }
}
```

### Example 2: Spy vs Mock in Partial Execution
```java
package com.example.testing;

import org.junit.jupiter.api.Test;
import java.util.ArrayList;
import java.util.List;
import static org.assertj.core.api.Assertions.*;
import static org.mockito.Mockito.*;

class SpyDemoTest {
    @Test
    void spy_executesRealMethods_unlessStubbed() {
        List<String> realList = new ArrayList<>();
        List<String> spyList = spy(realList);

        // Real method executes: element added to underlying list!
        spyList.add("First");
        spyList.add("Second");

        assertThat(spyList).hasSize(2);

        // Override size() with canned stub
        doReturn(100).when(spyList).size();

        assertThat(spyList).hasSize(100);
        assertThat(spyList.get(0)).isEqualTo("First"); // Real element remains accessible!
    }
}
```

### Example 3: Edge Case - doThrow for Void Methods
```java
package com.example.testing;

import org.junit.jupiter.api.Test;
import static org.assertj.core.api.Assertions.*;
import static org.mockito.Mockito.*;

class EmailNotificationTest {
    @Test
    void sendEmail_networkFailure_handlesGracefully() {
        EmailSender sender = mock(EmailSender.class);
        // when(sender.send(...)) does not compile for void methods! Use doThrow:
        doThrow(new NetworkException("Connection refused"))
                .when(sender).send(anyString(), anyString());

        NotificationService service = new NotificationService(sender);

        assertThatThrownBy(() -> service.notifyUser("user@example.com", "Hello"))
                .isInstanceOf(ServiceException.class)
                .hasCauseInstanceOf(NetworkException.class);
    }
}
```

---

## 6. Best Practices
1. **Always Use `MockitoExtension`**: Leverage `@ExtendWith(MockitoExtension.class)` to enable strict stubbing and automatic cleanup of mocks.
2. **Do Not Mock What You Do Not Own**: Avoid mocking third-party libraries (e.g., Apache HttpClient, Jackson); mock your own domain gateway interfaces that wrap external dependencies.
3. **Never Mock Data Objects or Java 21 Records**: Never mock value objects, DTOs, Collections, or Records; instantiate real instances directly.
4. **Prefer ArgumentCaptor over Complex Matchers**: Use `ArgumentCaptor` to capture objects and perform clear, expressive assertions using AssertJ.
5. **Use Strict Stubbing**: Eliminate unnecessary stubs; unused stubs lead to test rot and mask subtle bugs when production code paths change.
6. **Limit Mocks per Test Class**: If a test requires more than 3 or 4 mocks, the production class violates the Single Responsibility Principle and should be refactored.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Correction |
|---|---|---|
| Mixing matchers and raw values in one call | Mockito throws `InvalidUseOfMatchersException` | Wrap literal values in `eq(value)` if matchers are used |
| Mocking value objects / Collections | Adds brittle mocking code for simple data structures | Instantiate real Collections and Records directly |
| Mocking the Class Under Test (SUT) | Obscures the real behavior being tested | Only mock external collaborators, never the SUT |
| Using `when(spy.get(0))` on empty spy | Real method executes during stubbing, throwing `IndexOutOfBoundsException` | Use `doReturn(val).when(spy).get(0)` for spies |
| Forgetting to close `MockedStatic` | Leaves static method mocked globally, breaking subsequent tests | Always enclose `mockStatic` in try-with-resources |
| Overusing `verifyNoMoreInteractions` | Makes test fragile against minor internal refactoring | Verify only interactions essential to the business contract |
| Ignoring Unnecessary Stubbing warnings | Dead code accumulates in test suite | Remove unused `when(...)` stubs flagged by strictness |
| Mocking methods that are never called | Hides dead code or altered business logic | Use `Strictness.STRICT_STUBS` |

---

## 8. Performance & Security Notes
- **Inline Mock Maker Overhead**: The inline mock maker instruments class bytecode via the JVM Attach API. Mockito 5 optimizes this by attaching lazily, keeping startup overhead under 100 ms.
- **Memory Retention in Mocks**: Mock instances retain all invocation arguments in internal collections to satisfy potential `verify()` calls. For long-running test suites or large byte arrays, avoid holding mock references in static fields.
- **Deep Stubs Anti-Pattern**: Avoid `RETURNS_DEEP_STUBS` (e.g., `when(a.getB().getC().getD()).thenReturn(...)`). It violates the Law of Demeter and indicates severe coupling.

---

## 9. Comparison / When to Use What

| Mockito Feature | Behavior | Execution | Best Use Case |
|---|---|---|---|
| **Standard Mock (`mock()`)** | Canned defaults | Synthetic Bytecode | External collaborators (Gateway, Repository) |
| **Spy (`spy()`)** | Real methods default | Real instance wrapped | Legacy code, partial method overrides |
| **ArgumentCaptor** | Captures runtime args | Post-execution | Deep inspection of request DTOs sent to collaborators |
| **mockStatic** | Scoped static intercept | Bytecode instrumentation | Testing legacy code with static utilities (Clock, UUID) |

---

## 10. Interview Questions

<details>
<summary>1. How does Mockito 5's Inline Mock Maker differ from the legacy Subclass Mock Maker?</summary>
The legacy Subclass Mock Maker used CGLIB or Byte Buddy to generate a dynamic subclass extending the target class. This approach fundamentally could not mock <code>final</code> classes, <code>final</code> methods, or Java Records because the JVM prohibits subclassing final types.<br>
Mockito 5 uses the Inline Mock Maker by default, which leverages the JVM Instrumentation API (<code>ClassFileTransformer</code>) to modify the target class's bytecode directly in memory. This allows seamless mocking of final classes, records, and static methods.
</details>

<details>
<summary>2. Why does Mockito throw an InvalidUseOfMatchersException when mixing matchers with raw literals?</summary>
Mockito's argument matcher mechanism relies on a static thread-local stack. When an argument matcher like <code>any()</code> is evaluated, it pushes a matcher onto the stack and returns a dummy value (e.g., null, 0). If a method takes three arguments and you write <code>method(any(), eq("b"), "c")</code>, the third argument does not push a matcher onto the stack, creating a count mismatch. Mockito requires either <i>all</i> arguments to use matchers or <i>none</i>.
</details>

<details>
<summary>3. What is Strict Stubbing in Mockito and why is it enabled by default in MockitoExtension?</summary>
Strict Stubbing enforces testing hygiene:
1. It detects and fails the test if any declared stub is never invoked (Unnecessary Stubbing).<br>
2. It detects argument mismatches: if code calls a stubbed method with arguments slightly different from what was configured, Mockito throws an exception pointing to the exact mismatch.<br>
This prevents "test rot" and ensures tests stay synchronized with evolving production code.
</details>

<details>
<summary>4. What is the difference between when(spy.method()).thenReturn() and doReturn().when(spy).method()?</summary>
With <code>when(spy.method()).thenReturn()</code>, the real method is actually invoked once during the stubbing declaration itself before Mockito can register the stub! If the real method has side effects (e.g., database writes or throwing an exception), the test fails during setup.<br>
With <code>doReturn().when(spy).method()</code>, Mockito intercepts the call before it reaches the real method, safely registering the stub without executing the underlying code.
</details>

<details>
<summary>5. When should you choose an in-memory Fake over a Mockito mock?</summary>
An in-memory Fake should be preferred when:
- Testing complex domain business workflows where verifying multiple mock interactions makes the test brittle.
- Simulating a database repository or stateful cache where the SUT performs multiple reads, writes, and deletions.
- Fakes provide higher fidelity and allow refactoring internal collaborator method calls without breaking tests.
</details>

<details>
<summary>6. Why is mocking static methods generally considered a code smell?</summary>
Reliance on static methods indicates procedural design, tight coupling to global state, and violation of Dependency Injection. While <code>mockStatic()</code> is useful for refactoring legacy code or intercepting system clocks, modern code should inject collaborators (like <code>java.time.Clock</code>) via constructors instead of invoking static methods directly.
</details>

<details>
<summary>7. What does verifyNoInteractions(mock) do?</summary>
<code>verifyNoInteractions(mock)</code> asserts that no methods were ever invoked on the specified mock object throughout the entire test execution. It is ideal for verifying negative branches (e.g., verifying that a payment gateway was never contacted if input validation failed).
</details>

<details>
<summary>8. How does ArgumentCaptor help prevent over-specification in tests?</summary>
Instead of writing complex, brittle custom argument matchers inside <code>verify()</code>, <code>ArgumentCaptor</code> captures the actual object passed to the collaborator. The test can then use standard AssertJ assertions to verify only the relevant properties of the captured object, keeping the test clean and readable.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Write a test capturing an argument and asserting its fields with AssertJ.</summary>

```java
@Test
void publishEvent_validPayload_sendsToMessageBroker() {
    MessageBroker broker = mock(MessageBroker.class);
    EventPublisher publisher = new EventPublisher(broker);
    ArgumentCaptor<EventMessage> captor = ArgumentCaptor.forClass(EventMessage.class);

    publisher.publishUserCreated("user-123");

    verify(broker).send(captor.capture());
    EventMessage message = captor.getValue();
    assertThat(message.topic()).isEqualTo("user-events");
    assertThat(message.payload()).contains("user-123");
}
```
</details>

<details>
<summary>Exercise 2: Write a test verifying that no interactions occur on a payment gateway when order is free.</summary>

```java
@Test
void checkout_zeroDollarOrder_skipsPaymentGateway() {
    PaymentGateway gateway = mock(PaymentGateway.class);
    CheckoutService service = new CheckoutService(gateway);

    service.checkout(new Order("ORD-FREE", BigDecimal.ZERO));

    verifyNoInteractions(gateway);
}
```
</details>

<details>
<summary>Exercise 3: Demonstrate how to stub a method to throw an exception on first call and succeed on second.</summary>

```java
@Test
void retryLogic_recoversAfterFirstFailure() {
    RemoteService remote = mock(RemoteService.class);
    when(remote.fetchData())
            .thenThrow(new RuntimeException("Transient network hiccup"))
            .thenReturn("Success Payload");

    ResilientClient client = new ResilientClient(remote);
    String result = client.fetchWithRetry();

    assertThat(result).isEqualTo("Success Payload");
    verify(remote, times(2)).fetchData();
}
```
</details>

---

## 12. Summary / Cheat Sheet
- **Mock vs Spy**: Mock = synthetic default stub; Spy = real instance with selective overrides.
- **Inline Mocking**: Mockito 5 instruments bytecode directly; mocks `final` classes, records, and static methods.
- **Strict Stubbing**: Enabled via `@ExtendWith(MockitoExtension.class)`; eliminates unused stubs.
- **Argument Capturing**: Use `ArgumentCaptor<T>` to perform AssertJ assertions on collaborator inputs.
- **Spies Rule**: Always use `doReturn(val).when(spy).method()` to avoid executing real code during stubbing.
- **Mock Overuse**: Never mock value objects, Collections, or Java Records; mock only domain service boundaries.

---

## 13. Further Reading
- Mockito Official Documentation and Release Notes
- Martin Fowler: Mocks Aren't Stubs
- Byte Buddy: Runtime Bytecode Generation for the Java Virtual Machine
- *Effective Java* (3rd Edition) by Joshua Bloch
