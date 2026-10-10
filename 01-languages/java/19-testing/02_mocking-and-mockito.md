# Mocking and Mockito

A **mock** is a stand-in object you can program (return canned values, throw errors) and interrogate (was this method called, with what?). **Mockito** is the standard Java library for creating them. Mocks let you test a class **in isolation** from collaborators that are slow, nondeterministic, or have side effects: a payment gateway, an email service, a remote API.

Mocking is also the most overused tool in Java testing. Used well, it isolates boundaries. Used everywhere, it produces tests that verify the mocks rather than the code, and break whenever you refactor. This note covers the mechanics *and* the judgment.

**Prerequisites:** [Unit Testing Fundamentals](00_unit-testing-fundamentals.md#3-test-doubles), [JUnit](01_junit-5.md), [Interfaces](../04-oop/09_interfaces.md).

---

## 1. Setup

```xml
<dependency>
  <groupId>org.mockito</groupId><artifactId>mockito-core</artifactId><scope>test</scope>
</dependency>
<dependency>
  <groupId>org.mockito</groupId><artifactId>mockito-junit-jupiter</artifactId><scope>test</scope>   <!-- MockitoExtension -->
</dependency>
```

(Versions via your BOM. Spring Boot's dependency management includes Mockito.)

```java
@ExtendWith(MockitoExtension.class)         // creates @Mock fields, validates stubbing
class OrderServiceTest {
    @Mock PaymentGateway gateway;
    @Mock OrderRepository orders;
    ...
}
```

Without the extension you can create mocks manually: `PaymentGateway gateway = Mockito.mock(PaymentGateway.class);`.

---

## 2. A realistic example

```java
public class OrderService {
    private final OrderRepository orders;
    private final PaymentGateway gateway;
    private final Clock clock;

    public OrderService(OrderRepository orders, PaymentGateway gateway, Clock clock) { ... }

    public Order place(String customerId, BigDecimal amount) {
        PaymentResult result = gateway.charge(customerId, amount);
        if (!result.successful()) throw new PaymentDeclinedException(result.reason());
        Order order = new Order(customerId, amount, OrderStatus.PAID, result.transactionId(), clock.instant());
        orders.save(order);
        return order;
    }
}
```

```java
@ExtendWith(MockitoExtension.class)
class OrderServiceTest {

    @Mock PaymentGateway gateway;
    @Mock OrderRepository orders;
    @Captor ArgumentCaptor<Order> orderCaptor;

    private final Clock clock = Clock.fixed(Instant.parse("2026-03-10T10:00:00Z"), ZoneOffset.UTC);
    private OrderService service;

    @BeforeEach
    void setUp() { service = new OrderService(orders, gateway, clock); }

    @Test
    void savesPaidOrderWhenPaymentSucceeds() {
        when(gateway.charge("cust-1", new BigDecimal("50.00")))
                .thenReturn(PaymentResult.success("tx-9"));                 // STUB: canned answer

        Order placed = service.place("cust-1", new BigDecimal("50.00"));

        assertThat(placed.status()).isEqualTo(OrderStatus.PAID);            // state assertion
        verify(orders).save(orderCaptor.capture());                         // MOCK: the save side effect
        assertThat(orderCaptor.getValue().transactionId()).isEqualTo("tx-9");
        assertThat(orderCaptor.getValue().createdAt()).isEqualTo(Instant.parse("2026-03-10T10:00:00Z"));
    }

    @Test
    void doesNotSaveAnythingWhenPaymentIsDeclined() {
        when(gateway.charge(any(), any())).thenReturn(PaymentResult.declined("insufficient funds"));

        assertThatThrownBy(() -> service.place("cust-1", new BigDecimal("50.00")))
                .isInstanceOf(PaymentDeclinedException.class)
                .hasMessageContaining("insufficient funds");

        verifyNoInteractions(orders);                                       // nothing may be persisted
    }
}
```

This shows the sensible split: the **gateway is stubbed** (an input), the **repository is verified** because "an order was saved" is the observable side effect, and time is a fixed `Clock`.

---

## 3. Stubbing

```java
when(repo.findById(1L)).thenReturn(Optional.of(user));              // return a value
when(repo.findById(2L)).thenReturn(Optional.empty());
when(gateway.charge(any(), any())).thenThrow(new GatewayTimeoutException());

when(counter.next()).thenReturn(1).thenReturn(2).thenThrow(new IllegalStateException());   // successive calls

when(mapper.map(any())).thenAnswer(inv -> "mapped:" + inv.getArgument(0));                 // compute from arguments

// void methods can't use when(...): use the do*/when form
doThrow(new MailException("down")).when(mailer).send(any());
doNothing().when(audit).record(any());
```

### Argument matchers

```java
when(repo.find(eq(1L), anyString())).thenReturn(...);       // any String
when(repo.search(argThat(q -> q.length() > 3))).thenReturn(...);
verify(mailer).send(startsWith("Hello"));
```

**The matcher rule:** in one call, use matchers for **all** arguments or for **none**. This is wrong and throws `InvalidUseOfMatchersException`:

```java
when(repo.find(1L, anyString()))        // WRONG: mixes a raw value with a matcher
when(repo.find(eq(1L), anyString()))    // right
```

### Default behavior of an unstubbed mock

A mock returns "empty" defaults: `0`/`false`, an empty `Optional`, empty collections and streams, and **`null` for other objects**. That `null` is the usual source of surprise `NullPointerException`s ("I forgot to stub this").

### BDD style (optional, reads as Given/When/Then)

```java
given(gateway.charge("c", amount)).willReturn(PaymentResult.success("t"));
...
then(orders).should().save(any());
```

---

## 4. Verification

```java
verify(orders).save(order);                         // exactly once
verify(orders, times(2)).save(any());
verify(orders, never()).delete(any());
verify(mailer, atLeastOnce()).send(any());
verifyNoInteractions(orders);                       // the mock was never touched
verifyNoMoreInteractions(gateway);                  // nothing happened beyond what I verified (use rarely)

InOrder inOrder = inOrder(gateway, orders);         // verify call ORDER (only when order is the requirement)
inOrder.verify(gateway).charge(any(), any());
inOrder.verify(orders).save(any());
```

### Capturing arguments

When the code builds an object internally and passes it to a collaborator, capture it and assert on its contents:

```java
@Captor ArgumentCaptor<Order> orderCaptor;
verify(orders).save(orderCaptor.capture());
assertThat(orderCaptor.getValue().status()).isEqualTo(OrderStatus.PAID);
```

### Verify less

Every `verify` couples the test to an implementation detail. Verify the interactions that **are the behavior** (an email was sent, a payment was requested, nothing was saved on failure). Don't verify queries, getters, or internal helper calls. `verifyNoMoreInteractions` on everything makes tests shatter on harmless refactoring.

---

## 5. Strictness

`MockitoExtension` runs in **strict stubs** mode, which keeps tests honest:

- **`UnnecessaryStubbingException`**: you stubbed something the test never used. Remove the stub (the test or the code changed), or you're testing something other than you think.
- **`PotentialStubbingProblem`**: the code called a stubbed method with *different arguments* than the stub expected, which is often a typo in the test or a real bug.

Relax with `lenient().when(...)` only for genuinely shared setup stubs, and sparingly.

---

## 6. Spies, static mocks, and other power tools

### Spy (partial mock)

A spy wraps a **real** object, calls real methods, and lets you stub some:

```java
@Spy List<String> list = new ArrayList<>();
doReturn(100).when(list).size();         // use doReturn: when(list.size()) would call the real method first
```

Needing a spy usually signals that a class does too much. Prefer splitting it.

### Static and constructor mocking

```java
try (MockedStatic<LegacyUtil> util = mockStatic(LegacyUtil.class)) {      // only inside this block
    util.when(() -> LegacyUtil.fetch("x")).thenReturn("stubbed");
    ...
}
```

`mockStatic` and `mockConstruction` exist for **code you can't change** (legacy statics, `new` inside code you don't own). In your own code, prefer **injecting** the dependency (a `Clock` instead of mocking `Instant.now()`, a factory instead of `new`).

### Mockito 5 notes

- Mockito 5 uses the **inline mock maker by default**, so you can mock `final` classes/methods and `static` methods without extra configuration. It requires Java 11+.
- Inline mocking works by attaching an instrumentation agent. On **JDK 21+** that dynamic self-attachment prints a warning (JEP 451: "dynamic agent loading"), and a future JDK is expected to disallow it by default. The robust fix is to **register Mockito as a `-javaagent`** in the test JVM. Mockito's documentation shows the Maven and Gradle snippets (a pattern like: resolve the `mockito-core` JAR into a property/configuration and pass `-javaagent:<path>` through surefire's `argLine` or Gradle's `jvmArgs`). Check the current guide for your Mockito and JDK versions.

---

## 7. Mocks vs fakes: choose deliberately

```java
// A FAKE: a real, simple implementation. No stubbing or verification needed
class InMemoryOrderRepository implements OrderRepository {
    private final Map<Long, Order> store = new HashMap<>();
    private long nextId = 1;

    @Override public Order save(Order o) { Order saved = o.withId(nextId++); store.put(saved.id(), saved); return saved; }
    @Override public Optional<Order> findById(long id) { return Optional.ofNullable(store.get(id)); }
    @Override public List<Order> findByCustomer(String c) { return store.values().stream().filter(o -> o.customerId().equals(c)).toList(); }
}
```

| | Mock (Mockito) | Fake |
|---|---|---|
| Setup | Per test: stubbing | Written once, reused |
| Verifies | Interactions | Resulting **state** |
| Resilience to refactoring | Low (tied to calls) | High |
| Best for | One-off outbound side effects, error injection | Repositories, caches, queues, clocks |

A good rule: **fake the stateful things (repositories), mock/stub the boundaries with side effects or failures (gateways), and use real objects for everything in-process.** Back fakes with a **contract test** run against both the fake and the real implementation so they can't drift ([Testing Patterns](04_testing-patterns.md#2-fakes-and-contract-tests)).

---

## 8. What not to mock

| Don't mock | Why | Instead |
|---|---|---|
| **The class under test** | Then you test the mock | Real object |
| **Value objects, records, DTOs, collections, `String`** | Cheap to create for real. Mocks add noise | Real instances (use builders) |
| **Types you don't own** (JDBC `Connection`, `HttpClient`, a vendor SDK) | Your mock encodes guesses about its behavior, and tests pass while reality fails | Wrap it behind your own small interface, mock *that*, and test the wrapper with an integration test ([Integration Testing](03_integration-testing.md)) |
| **Everything in a chain** (`RETURNS_DEEP_STUBS`) | A sign of Law-of-Demeter violations | Redesign the API |
| **Pure functions and logic** | Nothing to isolate | Call them |

Rule of thumb: **mock at architectural boundaries** (ports to the outside world), not between every pair of classes. Too many mocks mean the test mirrors the implementation line by line.

### Mocks in Spring tests

For Spring slice and integration tests, `@MockitoBean` (Spring Framework 6.2+) replaces a bean in the application context with a Mockito mock. It supersedes the older `@MockBean`, which Spring Boot deprecated in 3.4. Check your Boot version. It's a heavier tool than constructor injection in plain unit tests, so prefer plain unit tests where they suffice ([Integration Testing](03_integration-testing.md#3-spring-boot-tests)).

---

## 9. Alternatives for specific boundaries

- **HTTP services:** a fake HTTP server (WireMock, OkHttp `MockWebServer`) exercises your real client code, serialization, and timeouts.
- **Databases/brokers:** real ones in containers (Testcontainers) beat mocking JDBC.
- **Time:** an injected `Clock` ([Date-Time Best Practices](../10-date-and-time/05_date-time-best-practices.md#4-make-time-injectable-clock)).
- **Kotlin:** MockK is the idiomatic equivalent.

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Mocking everything, verifying every call | Mock boundaries only, verify behavior-level side effects |
| Mixing raw values and matchers in one call | All matchers or none |
| `when(spy.method())` calling the real method | `doReturn(...).when(spy).method()` |
| Forgetting to stub, then hitting `NullPointerException` | Remember unstubbed objects return `null`. Stub or use real objects |
| Unused stubs (strict-mode failures) | Remove them, or find why the code path isn't hit |
| Mocking types you don't own | Wrap, mock the wrapper, integration-test the wrapper |
| Using `mockStatic` in new code | Inject the dependency instead |
| Verifying getters and internal helpers | Verify only meaningful outward interactions |
| Mocking value objects/collections | Create real ones |
| Tests that pass although the real integration is broken | Add integration tests for the boundary |
| Dynamic agent warnings on JDK 21+ | Configure Mockito as a `-javaagent` |

### Debugging Mockito errors

| Message | Meaning / fix |
|---|---|
| `Wanted but not invoked: ...` | The code never made that call (or made it on another mock/with different arguments). Compare with the "However, there were other interactions" part |
| `Argument(s) are different! Wanted: ... Actual invocation has different arguments` | Equal-ness mismatch (`equals` missing, `BigDecimal` scale, mutated object). Use a captor and inspect |
| `InvalidUseOfMatchersException` | Mixed matchers and raw values, or a matcher used outside a stubbing/verification call |
| `UnfinishedStubbingException` / `UnfinishedVerificationException` | A previous `when(...)` or `verify(...)` wasn't completed (often nested mock calls inside a stubbing) |
| `UnnecessaryStubbingException` / `PotentialStubbingProblem` | Strict stubs: unused stub, or stub called with other arguments |
| `NullPointerException` in code under test | An unstubbed mock returned `null` |
| `MockitoException: Cannot mock/spy class ...` | Final/native/primitive types or an environment (agent) issue. Check the Mockito version, and the JDK/agent setup |

---

## Quick Summary

- **Mocks** replace collaborators so a unit can be tested in isolation. **Stub** inputs (`when(...).thenReturn(...)`), **verify** only outward side effects (`verify(...)`, `ArgumentCaptor`).
- Use `@ExtendWith(MockitoExtension.class)` with `@Mock`/`@Captor`. Strict stubs flag unused or mismatched stubbing.
- Matcher rule: **all matchers or none**. Unstubbed methods return empty/default values or `null`.
- Prefer **fakes** (in-memory implementations) for stateful collaborators, and **real objects** in-process. Mock at **boundaries** you own, and wrap third-party types.
- Spies, `mockStatic`, and deep stubs are smells. **Inject** `Clock`/factories instead.
- Mockito 5 is inline-by-default (final and static mocking). On JDK 21+, register it as a `-javaagent`.
- Over-mocking makes tests mirror the implementation and break on refactoring. Add integration tests where mocks end.

**Next:** [Integration Testing](03_integration-testing.md)