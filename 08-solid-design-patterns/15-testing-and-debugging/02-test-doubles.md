# Test Doubles

A **test double** is a stand-in for a real dependency during a test, like a stunt double in a film. You use one when the real thing is slow, unpredictable, hard to set up, or has side effects (databases, networks, email, clocks, payment providers).

Test doubles depend on the seams described in [testable design](./01-testable-design.md): the code must accept its dependencies from outside, normally through [dependency injection](../03-patterns-intro/02-dependency-injection.md).

## The five kinds

The commonly used vocabulary (popularized by Gerard Meszaros and Martin Fowler):

| Double | What it does | Used to |
|---|---|---|
| **Dummy** | Passed around but never actually used | Fill a required parameter |
| **Stub** | Returns canned answers | Control **indirect inputs** |
| **Spy** | A stub that also records how it was called | Observe calls afterwards |
| **Mock** | Pre-programmed with expectations, fails if they're not met | Verify **interactions** |
| **Fake** | A real, simplified working implementation | Replace heavy infrastructure (in-memory database) |

Informally many people say "mock" for all of them. The distinction matters because it tells you what the test is checking.

## Examples

Given:

```java
public interface PaymentGateway {
    PaymentResult charge(String customerId, long amountCents);
}

public interface Notifier {
    void notify(String to, String message);
}

public class CheckoutService {
    private final PaymentGateway payments;
    private final Notifier notifier;

    public CheckoutService(PaymentGateway payments, Notifier notifier) {
        this.payments = payments;
        this.notifier = notifier;
    }

    public boolean checkout(String customerId, String email, long amountCents) {
        PaymentResult result = payments.charge(customerId, amountCents);
        if (!result.successful()) return false;
        notifier.notify(email, "Payment received");
        return true;
    }
}
```

### Dummy

```java
Notifier unused = (to, msg) -> { throw new AssertionError("should not be called"); };
```

Only there to satisfy the constructor.

### Stub: controlling inputs

```java
PaymentGateway declining = (customer, amount) -> PaymentResult.declined();
var service = new CheckoutService(declining, unused);

assertFalse(service.checkout("c1", "a@example.com", 1000));
```

### Spy: recording calls

```java
class RecordingNotifier implements Notifier {
    final List<String> messages = new ArrayList<>();
    public void notify(String to, String message) { messages.add(to + ": " + message); }
}

var notifier = new RecordingNotifier();
var service = new CheckoutService((c, a) -> PaymentResult.success(), notifier);

service.checkout("c1", "a@example.com", 1000);

assertEquals(List.of("a@example.com: Payment received"), notifier.messages);
```

### Mock (with Mockito)

```java
PaymentGateway payments = mock(PaymentGateway.class);
Notifier notifier = mock(Notifier.class);
when(payments.charge("c1", 1000)).thenReturn(PaymentResult.success());

new CheckoutService(payments, notifier).checkout("c1", "a@example.com", 1000);

verify(notifier).notify("a@example.com", "Payment received");     // verify the interaction
verifyNoMoreInteractions(notifier);
```

`when(...).thenReturn(...)` makes the mock behave as a stub; `verify(...)` makes it a mock in the strict sense.

### Fake: a working simplification

```java
public class InMemoryOrderRepository implements OrderRepository {
    private final Map<OrderId, Order> store = new HashMap<>();

    public void save(Order o)              { store.put(o.id(), o); }
    public Optional<Order> findById(OrderId id) { return Optional.ofNullable(store.get(id)); }
}
```

A fake behaves like the real thing (save then find returns the object), so tests can focus on outcomes.

## State verification vs interaction verification

| | State-based | Interaction-based |
|---|---|---|
| Checks | The **resulting state or return value** | That specific **calls were made** |
| Typical doubles | Stubs, fakes | Mocks, spies |
| Coupling to implementation | Lower | Higher |
| Survives refactoring | Usually | Often breaks |

```java
// State: what is the outcome?
service.cancel(orderId);
assertEquals(CANCELLED, orders.findById(orderId).get().status());

// Interaction: how was it done?
verify(orders).save(argThat(o -> o.status() == CANCELLED));
```

**Prefer state-based checks** with stubs and fakes. Use interaction checks when the call *is* the observable behavior (an email must be sent, a payment must be requested) and no result shows it.

## Choosing a double

```text
Need to feed in a particular answer?              → stub
Need to check a side effect happened?             → spy or mock
Need realistic behavior across several calls?     → fake (in-memory repository)
Just need to fill a parameter?                    → dummy
```

For repositories, an in-memory **fake** is usually better than a mock: tests read naturally ("save, then find") and don't break when the service changes how many times it calls the repository.

## Rules of thumb

### Don't mock types you don't own

Mocking a third-party class (an HTTP client, a cloud SDK) tests your *assumptions* about it, and breaks when the library changes. Wrap it in your own interface (an [Adapter](../05-structural-patterns/01-adapter.md)), mock your interface in unit tests, and test the adapter itself with an integration test.

```text
Service ──► PaymentGateway (yours) ◄── StripeAdapter ──► Stripe SDK
   ▲               ▲                         ▲
 unit test     mocked here           integration test here
```

### Don't mock value objects or simple data

Create real instances. Mocking `Money` or `Order` adds noise and hides real behavior.

### Don't mock the class under test

If you need to stub part of the class you're testing, the class has too many responsibilities. Split it.

### Mock only direct collaborators

Stubbing a chain (`when(a.getB().getC())`) signals a Demeter problem in the design.

### Keep the amount of setup small

If a test needs ten stubbed calls, the unit has too many dependencies or knows too much.

## Keeping doubles honest: contract tests

A fake or stub can drift from the real implementation. A **contract test** runs the same checks against every implementation of an interface:

```java
abstract class OrderRepositoryContract {
    abstract OrderRepository create();

    @Test void savedOrderCanBeFound() {
        var repo = create();
        var order = anOrder();
        repo.save(order);
        assertEquals(Optional.of(order), repo.findById(order.id()));
    }
}

class InMemoryOrderRepositoryTest extends OrderRepositoryContract {
    OrderRepository create() { return new InMemoryOrderRepository(); }
}

class JpaOrderRepositoryTest extends OrderRepositoryContract {     // against a real database
    OrderRepository create() { return new JpaOrderRepository(testEntityManager()); }
}
```

If both pass, the fake is a trustworthy substitute. This is also how you check [Liskov substitution](../02-solid-principles/03-liskov-substitution.md) for implementations.

## Mockito basics

```java
@ExtendWith(MockitoExtension.class)
class CheckoutServiceTest {

    @Mock PaymentGateway payments;
    @Mock Notifier notifier;
    @InjectMocks CheckoutService service;

    @Test
    void sendsConfirmationAfterSuccessfulPayment() {
        when(payments.charge("c1", 1000)).thenReturn(PaymentResult.success());

        service.checkout("c1", "a@example.com", 1000);

        verify(notifier).notify("a@example.com", "Payment received");
    }
}
```

| API | Purpose |
|---|---|
| `mock(Type.class)` / `@Mock` | Create a double |
| `when(...).thenReturn(...)` / `thenThrow(...)` | Stub a response |
| `verify(mock).method(...)` | Check a call happened |
| `verify(mock, never())` | Check a call did *not* happen |
| `ArgumentCaptor` | Capture and inspect arguments passed |
| `spy(realObject)` | Partial mock of a real object (use sparingly) |

## Common mistakes

- **Over-mocking**: every collaborator mocked and every call verified, so the test is a copy of the implementation.
- **Mocking everything instead of using fakes** for repositories.
- **Stubbing what you then don't use**, adding clutter. (Mockito's strict mode reports unused stubs.)
- **Asserting the mock's own behavior** (`when(x).thenReturn(5)` followed by `assertEquals(5, x.get())`), which tests nothing.
- **Mocking third-party types directly**, tying tests to library details.
- **Verifying call order or counts** that don't matter to behavior.
- **A fake that diverges from reality**, giving false confidence. Use contract tests.
- **Mocking static methods or constructors** to work around design problems, instead of introducing a seam.
- **Tests that pass with mocks but fail in production**, because nothing tests the real integration.

## Quick Summary

- Dummy, stub, spy, mock and fake are different tools; know which you're using.
- Prefer state-based checks, with stubs and fakes; use interaction checks only when a call is the behavior.
- Don't mock what you don't own; wrap it in your own interface.
- Use contract tests to keep fakes honest, and integration tests for adapters.
- Heavy mocking is a design smell as much as a test smell.

## Related

[Testable design](./01-testable-design.md) · [Repository](../07-application-patterns/01-repository.md) · [Adapter](../05-structural-patterns/01-adapter.md) · [Dependency injection](../03-patterns-intro/02-dependency-injection.md)

## Next

[03-refactoring-to-patterns.md](./03-refactoring-to-patterns.md)
