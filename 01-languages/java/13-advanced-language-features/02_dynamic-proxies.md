# Dynamic Proxies

A **dynamic proxy** is an object created *at runtime* that implements one or more interfaces and sends **every method call** to a single handler you write. You never write the implementing class: the JDK generates it.

That one hook ("run my code for every call") is the foundation of **AOP-style features**: transactions, logging, caching, security checks, retries, lazy loading, and "declarative" interfaces like Spring Data repositories where you write only an interface and the framework supplies the implementation.

```text
caller ──► proxy (implements Service) ──► InvocationHandler.invoke(...) ──► real target
                                              │
                                              └── before / after / instead of the call
```

**Prerequisites:** [Reflection](01_reflection.md), [Annotations](00_annotations.md), [Interfaces](../04-oop/09_interfaces.md), [Proxy pattern](../24-design-patterns/02-structural/03_proxy.md).

---

## 1. The API

```java
Object proxy = Proxy.newProxyInstance(
        ClassLoader loader,              // usually the interface's class loader
        Class<?>[] interfaces,           // what the proxy implements
        InvocationHandler handler);      // where every call goes

public interface InvocationHandler {
    Object invoke(Object proxy, Method method, Object[] args) throws Throwable;
}
```

- `proxy`: the proxy object itself (rarely needed; calling methods on it re-enters the handler).
- `method`: the `Method` that was invoked, from the **interface**.
- `args`: the arguments, **`null` when the method has no parameters**, with primitives boxed.
- The return value becomes the result of the call.

---

## 2. First example: a timing proxy

```java
interface UserService {
    String findName(long id);
    void delete(long id);
}

class UserServiceImpl implements UserService {
    public String findName(long id) { return "user-" + id; }
    public void delete(long id) { /* ... */ }
}

static <T> T timed(T target, Class<T> iface) {
    return iface.cast(Proxy.newProxyInstance(
        iface.getClassLoader(),
        new Class<?>[] { iface },
        (proxy, method, args) -> {
            long start = System.nanoTime();
            try {
                return method.invoke(target, args);                    // delegate to the real object
            } catch (InvocationTargetException e) {
                throw e.getCause();                                    // rethrow the REAL exception
            } finally {
                System.out.printf("%s took %d µs%n", method.getName(), (System.nanoTime() - start) / 1_000);
            }
        }));
}

UserService service = timed(new UserServiceImpl(), UserService.class);
service.findName(7);          // prints "findName took 12 µs"
```

The caller sees an ordinary `UserService`. The timing behavior was added **without touching `UserServiceImpl`**, and the same `timed` method works for any interface. This is the [Decorator](../24-design-patterns/02-structural/01_decorator.md) / [Proxy](../24-design-patterns/02-structural/03_proxy.md) idea, written once instead of once per interface.

---

## 3. Rules and behavior to know

### Interfaces only

JDK proxies can only implement **interfaces**, not classes. The generated class extends `java.lang.reflect.Proxy` and implements your interfaces. Proxying a *class* needs bytecode-generation libraries (section 6).

### Which calls reach the handler

- All methods of the proxied interfaces.
- Three `Object` methods: **`equals`, `hashCode`, `toString`**. Your handler must handle them sensibly, since `method.invoke(target, args)` is a reasonable default. Other `Object` methods (`getClass`, `wait`, `notify`) are not intercepted.

### Return values

- The returned object must be assignable to the method's return type, or the caller gets a `ClassCastException`.
- Returning `null` for a **primitive** return type causes a `NullPointerException` in the caller. Return `0`, `false`, etc. instead.
- `void` methods: return `null`.

### Exceptions

- If the handler throws a **checked exception that the interface method doesn't declare**, the caller receives an `UndeclaredThrowableException` wrapping it. Runtime exceptions and declared checked exceptions pass through unchanged.
- When delegating with `method.invoke`, unwrap `InvocationTargetException` as shown above. Otherwise callers see the wrapper instead of the real exception.

### Default methods

An interface `default` method also goes to the handler. To run the interface's own implementation, use `InvocationHandler.invokeDefault(proxy, method, args)` (Java 16+).

### Inspecting

```java
Proxy.isProxyClass(service.getClass());           // true
Proxy.getInvocationHandler(service);              // the handler
service.getClass().getName();                     // e.g. jdk.proxy2.$Proxy12   ← what you'll see in stack traces
```

---

## 4. A complete example: annotation-driven proxy

This ties the three notes together: an **annotation** declares what to read, **reflection** inspects it, and a **proxy** supplies the implementation of an interface you never implemented.

```java
@Retention(RetentionPolicy.RUNTIME)
@Target(ElementType.METHOD)
@interface Key {
    String value();
    String defaultValue() default "";
}

interface AppConfig {
    @Key("server.port")                       int port();
    @Key(value = "server.host", defaultValue = "localhost") String host();
    @Key(value = "debug", defaultValue = "false")           boolean debug();
}

static <T> T bind(Class<T> type, Properties props) {
    return type.cast(Proxy.newProxyInstance(
        type.getClassLoader(), new Class<?>[] { type },
        (proxy, method, args) -> {

            if (method.getDeclaringClass() == Object.class) {              // equals / hashCode / toString
                return switch (method.getName()) {
                    case "toString" -> type.getSimpleName() + "@proxy";
                    case "hashCode" -> System.identityHashCode(proxy);
                    default         -> proxy == args[0];                   // equals
                };
            }

            Key key = method.getAnnotation(Key.class);
            if (key == null) throw new UnsupportedOperationException("Missing @Key on " + method);

            String raw = props.getProperty(key.value(), key.defaultValue());
            Class<?> returnType = method.getReturnType();

            if (returnType == int.class)     return Integer.parseInt(raw);
            if (returnType == boolean.class) return Boolean.parseBoolean(raw);
            return raw;                                                    // String
        }));
}

Properties props = new Properties();
props.setProperty("server.port", "8080");

AppConfig config = bind(AppConfig.class, props);
config.port();     // 8080
config.host();     // "localhost" (default)
config.debug();    // false
```

There is no `AppConfig` implementation class anywhere in the source. This is the same mechanism behind Spring Data repositories, Retrofit and Feign HTTP clients, and MyBatis mappers: **you declare an annotated interface, and a proxy supplies the behavior**.

---

## 5. Where proxies are used

| Use | Example |
|---|---|
| Cross-cutting concerns (AOP) | Transactions, security checks, logging, metrics, caching wrapped around a service |
| Declarative interfaces | Query/HTTP/mapper interfaces implemented by the framework |
| Lazy loading | An ORM returns a proxy that loads the real data on first access |
| Remote proxies | A local interface whose calls travel over the network |
| Mocking and testing | Test doubles for interfaces (Mockito generates its mocks with bytecode libraries instead) |
| Validation, retries, rate limiting | Wrap a client interface with policy ([Retry and Backoff](../25-real-world-patterns/02_retry-and-backoff.md)) |

Dependency-injection containers create proxies around your beans for exactly these reasons ([Dependency Injection](../24-design-patterns/04-architecture/00_dependency-injection.md), [From JDBC to ORM](../16-jdbc-and-databases/07_from-jdbc-to-orm.md)).

---

## 6. Limits and alternatives

| Limitation | Consequence / alternative |
|---|---|
| Interfaces only | For classes, frameworks generate **subclasses** at runtime with ByteBuddy or CGLIB (used by Hibernate, Mockito, and Spring depending on configuration). Final classes and methods can't be proxied this way |
| Reflection cost | Each call goes through the handler and `Method.invoke`. The overhead is small for typical service calls, but not for tight loops. Measure ([JMH](../20-performance/01_benchmarking-with-jmh.md)) |
| Runtime magic | Stack traces gain `$Proxy` and framework frames. Errors appear at runtime, not compile time |
| Native-image/AOT builds | Proxy classes must be declared in the tool's configuration (or avoided via compile-time code generation) |
| Hard to see | Code doesn't show the behavior being added. Keep handlers small, and document the cross-cutting rules |

Compile-time alternatives (annotation processors that generate decorators, AspectJ weaving) trade runtime flexibility for earlier errors, faster startup, and easier debugging.

---

## 7. The self-invocation trap

A proxy only intercepts calls that **go through the proxy**. When the target calls its own method with `this.other()`, that call goes straight to the real object and **bypasses the proxy**:

```java
class OrderService implements Orders {
    public void placeOrder(Order o) {
        validate(o);
        this.audit(o);               // direct call: NOT intercepted by the proxy
    }
    public void audit(Order o) { ... }   // e.g. expected to be @Transactional / timed / secured
}
```

This is a famous surprise in Spring AOP: an annotated method called from another method **in the same class** silently doesn't get its transaction, cache, or security behavior. Fixes: move the method into another bean and call it through that bean's proxy, restructure so the logic doesn't depend on being intercepted, or use compile-time/load-time weaving (AspectJ), which modifies the target class itself.

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Trying to proxy a concrete class with `Proxy` | Use an interface, or ByteBuddy/CGLIB-style subclassing |
| Not unwrapping `InvocationTargetException` | `throw e.getCause()` |
| Returning `null` for a primitive return type | Return the right default (`0`, `false`) |
| Forgetting `equals`/`hashCode`/`toString` also hit the handler | Handle them (delegate to the target) |
| Throwing an undeclared checked exception from the handler | It becomes `UndeclaredThrowableException`. Throw declared or unchecked ones |
| Calling `method.invoke(proxy, args)` inside the handler | Infinite recursion. Invoke on the **target** |
| Assuming a proxy intercepts calls made inside the target (`this.x()`) | It doesn't. Call through another proxied bean |
| `ClassCastException` casting the proxy to the implementation class | Cast only to the interfaces it was created with |
| Creating a new proxy per call in hot code | Create once and reuse |
| Doing heavy work or blocking I/O in a handler that wraps every method | Keep handlers light, or apply them selectively (by annotation) |

### Debugging

- Stack trace full of `$Proxy` and `Method.invoke`: look for the first frame in your own code *below* them, and for `getCause()` chains.
- `UndeclaredThrowableException` → find the wrapped cause and either declare it on the interface or convert it to an unchecked exception.
- Behavior "not applied" → is the call going through the proxy? Check for self-invocation, `new`-created instances, and `private`/`final`/`static` methods that proxies can't intercept.
- Unsure what you have → `Proxy.isProxyClass(obj.getClass())` and `obj.getClass().getName()`.
- Infinite recursion/`StackOverflowError` in a handler → you invoked a method on `proxy` instead of `target`, or `toString()` calls the proxy in a log statement.

---

## Quick Summary

- `Proxy.newProxyInstance(loader, interfaces, handler)` creates an object implementing the interfaces, with **every call routed to `InvocationHandler.invoke`**.
- JDK proxies are **interface-only**. Class proxies come from ByteBuddy/CGLIB-style subclassing.
- In the handler, delegate with `method.invoke(target, args)`, **unwrap `InvocationTargetException`**, handle `equals`/`hashCode`/`toString`, and don't return `null` for primitives.
- Annotations + reflection + proxies = declarative frameworks: an annotated interface with no implementation class.
- Calls made from inside the target (`this.method()`) **bypass** the proxy: the self-invocation trap.
- Costs: runtime magic, harder stack traces, small overhead, and extra configuration for native images.

**Next module:** [Concurrency](../14-concurrency/00_threads-and-lifecycle.md)