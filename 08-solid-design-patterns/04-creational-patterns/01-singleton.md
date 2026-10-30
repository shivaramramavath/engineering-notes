# Singleton

**Intent:** ensure a class has exactly one instance, and provide a global point of access to it.

It is the most famous pattern and the most misused. Understand it mainly so you can recognize it, implement it safely when truly needed, and know the better alternatives.

## The problem

Some things should exist once in a process: a configuration registry, a connection pool, a hardware gateway. Creating several would waste resources or cause inconsistency.

## Structure

```text
Singleton
 ├─ private constructor        (nobody else can call new)
 ├─ private static instance
 └─ public static getInstance() (returns the one instance)
```

## Implementations in Java

### 1. Enum singleton (simplest and safest)

```java
public enum AppConfig {
    INSTANCE;

    private final Map<String, String> values = new ConcurrentHashMap<>();

    public String get(String key) { return values.get(key); }
    public void set(String key, String value) { values.put(key, value); }
}

AppConfig.INSTANCE.get("env");
```

The JVM guarantees one instance, and it is safe against reflection and serialization attacks. Limitation: it can't extend another class (it can implement interfaces).

### 2. Initialization-on-demand holder (lazy, thread-safe)

```java
public final class ConnectionRegistry {
    private ConnectionRegistry() {}

    private static class Holder {
        static final ConnectionRegistry INSTANCE = new ConnectionRegistry();
    }

    public static ConnectionRegistry getInstance() { return Holder.INSTANCE; }
}
```

The inner class loads, and creates the instance, only when `getInstance()` is first called. Class loading is thread-safe, so no explicit locking is needed.

### 3. Double-checked locking (only if you need it)

```java
public final class Cache {
    private static volatile Cache instance;
    private Cache() {}

    public static Cache getInstance() {
        if (instance == null) {
            synchronized (Cache.class) {
                if (instance == null) {
                    instance = new Cache();
                }
            }
        }
        return instance;
    }
}
```

The `volatile` keyword is **required**. Without it, another thread may see a partially constructed object. Prefer the holder idiom; it's simpler and equally lazy.

### Not thread-safe (don't do this)

```java
public static Cache getInstance() {
    if (instance == null) instance = new Cache();   // two threads can both create one
    return instance;
}
```

## Why it's often a problem

| Issue | Consequence |
|---|---|
| Global mutable state | Any code can change it; behavior depends on hidden state |
| Hidden dependencies | Class signatures don't show that they use the singleton |
| Hard to test | Tests share state and can't swap in a fake |
| Violates SRP | The class manages its own lifecycle *and* its business logic |
| Tight coupling | Callers depend on the concrete class via `getInstance()` |
| Concurrency | Shared mutable state needs careful synchronization |
| Class loaders / multiple JVMs | "One instance" is really "one per class loader" |

## The better alternative: single instance, not Singleton

Often what you need is **one instance**, not a class that *enforces* it. Let a DI container or composition root create one and pass it where needed.

```java
// Plain class: no static, no private constructor
public class ConnectionPool { /* ... */ }

// Composition root creates one and shares it
ConnectionPool pool = new ConnectionPool(config);
OrderRepository orders = new JdbcOrderRepository(pool);
UserRepository users = new JdbcUserRepository(pool);
```

Now `ConnectionPool` is easy to test and replace, and "only one" is a wiring decision, not a code constraint. In Spring, singleton scope is the default, giving the benefit without the pattern's drawbacks. See [dependency injection](../03-patterns-intro/02-dependency-injection.md).

## When it is still reasonable

- A truly process-wide, effectively immutable resource, such as a read-only configuration loaded at startup.
- Low-level infrastructure where DI isn't available.
- Stateless utilities where one instance is merely an optimization (though a plain static method or a normal instance often works).

If you use one, keep it **stateless or immutable**, and hide it behind an interface where possible.

## Common mistakes

- **Using it as a convenient global variable.**
- **Lazy initialization without thread safety.**
- **Forgetting `volatile`** in double-checked locking.
- **Mutable singleton state shared across threads** with no synchronization.
- **Calling `getInstance()` deep inside business logic**, hiding the dependency. Inject it instead.
- **Assuming serialization or reflection can't create a second instance.** Enum singletons avoid this.

## Debugging

| Symptom | Likely cause |
|---|---|
| Tests pass alone but fail together | Shared singleton state leaking between tests |
| Two instances observed | Different class loaders, or unsafe lazy initialization |
| Stale or wrong configuration | Singleton initialized too early or with defaults |

## Quick Summary

- Singleton restricts a class to one instance with global access.
- Safest Java forms: enum singleton or the holder idiom; double-checked locking needs `volatile`.
- The pattern's main cost is hidden global state and poor testability.
- Prefer a single instance managed by DI over a class that enforces its own uniqueness.

## Related

[Factory Method](./02-factory-method.md) (a factory may return a shared instance) · [Dependency injection](../03-patterns-intro/02-dependency-injection.md)

## Next

[02-factory-method.md](./02-factory-method.md)
