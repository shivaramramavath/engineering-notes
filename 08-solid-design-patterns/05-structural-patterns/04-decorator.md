# Decorator

**Intent:** attach additional responsibilities to an object **dynamically**, as a flexible alternative to subclassing.

A decorator wraps an object, implements the **same interface**, adds its own behavior before or after delegating, and can itself be wrapped.

## The problem

You want to add optional behaviors (logging, caching, compression, validation, retries) to a class. With inheritance, every combination needs a subclass:

```text
Repository
 ├─ LoggingRepository
 ├─ CachingRepository
 ├─ LoggingCachingRepository
 ├─ CachingRetryingRepository
 └─ ... (grows with every combination)
```

Inheritance is also fixed at compile time. Decorators let you combine behaviors at runtime.

## Structure

```text
Component (interface)
  + operation()
      ▲
      ├─ ConcreteComponent        (the real work)
      └─ Decorator (abstract)     holds a Component; delegates
            ├─ DecoratorA         adds behavior A
            └─ DecoratorB         adds behavior B

Client ──► DecoratorB ──► DecoratorA ──► ConcreteComponent
```

## Example

```java
public interface UserRepository {
    Optional<User> findById(String id);
}

public class JdbcUserRepository implements UserRepository {
    public Optional<User> findById(String id) { /* SQL query */ }
}

public class LoggingUserRepository implements UserRepository {
    private final UserRepository delegate;
    private final Logger log;

    public LoggingUserRepository(UserRepository delegate, Logger log) {
        this.delegate = delegate;
        this.log = log;
    }

    @Override public Optional<User> findById(String id) {
        long start = System.nanoTime();
        try {
            return delegate.findById(id);
        } finally {
            log.info("findById({}) took {} ms", id, (System.nanoTime() - start) / 1_000_000);
        }
    }
}

public class CachingUserRepository implements UserRepository {
    private final UserRepository delegate;
    private final Map<String, User> cache = new ConcurrentHashMap<>();

    public CachingUserRepository(UserRepository delegate) { this.delegate = delegate; }

    @Override public Optional<User> findById(String id) {
        User cached = cache.get(id);
        if (cached != null) return Optional.of(cached);

        Optional<User> loaded = delegate.findById(id);
        loaded.ifPresent(u -> cache.put(id, u));
        return loaded;
    }
}
```

Compose at the composition root:

```java
UserRepository repo =
        new LoggingUserRepository(
                new CachingUserRepository(
                        new JdbcUserRepository(dataSource)),
                logger);
```

Clients only see `UserRepository`. Adding or removing a behavior is a wiring change, with no changes to existing classes. This is the [Open/Closed Principle](../02-solid-principles/02-open-closed.md) applied through composition.

## Order matters

```text
Logging → Caching → Jdbc    logs every call, including cache hits
Caching → Logging → Jdbc    logs only calls that miss the cache
```

The outermost decorator sees the request first. Think of it as layers of an onion.

## In the Java standard library

| Example | Detail |
|---|---|
| `java.io` streams | `new BufferedInputStream(new FileInputStream(f))` adds buffering to a file stream |
| `Collections.unmodifiableList(list)` | Wraps a list and blocks modification |
| `Collections.synchronizedList(list)` | Adds thread safety by wrapping |

## Decorator vs Proxy vs Adapter

| Pattern | Interface | Purpose | Who decides to wrap? |
|---|---|---|---|
| **Decorator** | Same | Add responsibilities | The client composes layers |
| [Proxy](./07-proxy.md) | Same | Control access to the object | Usually the proxy manages its subject itself |
| [Adapter](./01-adapter.md) | Different | Convert an interface | The integrator |

Technically they look alike; the question is what the wrapper is *for*.

## Decorator vs inheritance

| | Inheritance | Decorator |
|---|---|---|
| Bound | Compile time | Runtime |
| Combinations | A subclass per combination | Compose freely |
| Coupling | To the parent's internals | To the interface only |

## When to use it

- Cross-cutting behaviors that are optional and combinable: logging, metrics, caching, retries, authorization, compression, encryption.
- You can't or shouldn't modify the original class.
- Subclassing would cause a combinatorial explosion.

## Common mistakes

- **Wide interfaces:** a decorator must implement every method of the interface, which gets tedious. Keep interfaces small ([ISP](../02-solid-principles/04-interface-segregation.md)).
- **Depending on order** without documenting it.
- **Identity checks:** `decorated == original` is false, and `instanceof` checks against the concrete class fail.
- **Too many layers**, making stack traces and debugging hard to follow.
- **Behavior that changes the contract** (for example, swallowing exceptions). That breaks [LSP](../02-solid-principles/03-liskov-substitution.md).
- **Stateful decorators shared across threads** without synchronization.

## Debugging

- Deep decorator chains produce long stack traces. Name decorators clearly (`LoggingUserRepository`, not `Wrapper1`).
- Log the composed chain at startup to confirm the order is what you intended.

## Quick Summary

- A decorator wraps an object of the same interface to add behavior.
- Layers can be combined and reordered at runtime, replacing subclass explosions.
- Order determines which layer sees the call first.
- Keep interfaces small, and keep decorators faithful to the original contract.

## Related

[Proxy](./07-proxy.md) · [Strategy](../06-behavioral-patterns/01-strategy.md) (swaps the algorithm *inside*; decorator wraps *around*) · [Chain of Responsibility](../06-behavioral-patterns/06-chain-of-responsibility.md)

## Next

[05-facade.md](./05-facade.md)
