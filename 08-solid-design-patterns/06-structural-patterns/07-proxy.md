# Proxy

**Intent:** provide a surrogate or placeholder for another object to **control access** to it.

A proxy has the **same interface** as the real object (the *subject*), so clients can't tell the difference. The proxy decides whether, when and how the real object is used.

## The problem

You want to control access to an object without changing the object or the clients:

- It is expensive to create, and you want to delay it until first use.
- It lives on another machine, and clients shouldn't deal with the network.
- Some callers aren't allowed to use it.
- You want to cache results, log calls or count usage around it.

## Structure

```text
Client ──► Subject (interface)
              ▲
              ├─ RealSubject   (does the real work)
              └─ Proxy         holds RealSubject; same interface
                                 decides when/whether to delegate
```

## Types of proxies

| Type | Purpose | Example |
|---|---|---|
| **Virtual** | Delay creation of an expensive object | Lazy-loading a large image or a database entity |
| **Protection** | Check permissions before allowing access | Only admins may call `delete()` |
| **Remote** | Represent an object in another process or machine | RPC/REST client stub |
| **Caching** | Store results of expensive calls | Cache API responses |
| **Logging / smart reference** | Add bookkeeping around access | Audit, metrics, reference counting |

## Example: virtual proxy (lazy loading)

```java
public interface Image {
    void display();
}

public class HighResImage implements Image {
    private final byte[] pixels;

    public HighResImage(String path) {
        this.pixels = loadFromDisk(path);        // expensive
    }

    public void display() { /* render pixels */ }
}

public class LazyImageProxy implements Image {
    private final String path;
    private HighResImage real;                   // created on first use

    public LazyImageProxy(String path) { this.path = path; }

    @Override public void display() {
        if (real == null) real = new HighResImage(path);
        real.display();
    }
}

List<Image> gallery = List.of(new LazyImageProxy("a.png"), new LazyImageProxy("b.png"));
gallery.get(0).display();      // only now is a.png loaded; b.png is never loaded
```

The gallery builds instantly. Image data is loaded only for images actually displayed. (This version is not thread-safe; add synchronization if shared across threads.)

## Example: protection proxy

```java
public interface DocumentService {
    String read(String docId);
    void delete(String docId);
}

public class SecureDocumentService implements DocumentService {
    private final DocumentService target;
    private final CurrentUser user;

    public SecureDocumentService(DocumentService target, CurrentUser user) {
        this.target = target;
        this.user = user;
    }

    public String read(String docId) { return target.read(docId); }

    public void delete(String docId) {
        if (!user.hasRole("ADMIN")) throw new SecurityException("Not allowed");
        target.delete(docId);
    }
}
```

## Dynamic proxies in Java

Writing one proxy class per interface gets repetitive. Java's `java.lang.reflect.Proxy` creates a proxy at runtime for any **interface**, with one handler for all calls:

```java
@SuppressWarnings("unchecked")
public static <T> T withTiming(T target, Class<T> iface) {
    return (T) Proxy.newProxyInstance(
            iface.getClassLoader(),
            new Class<?>[] { iface },
            (proxy, method, args) -> {
                long start = System.nanoTime();
                try {
                    return method.invoke(target, args);
                } catch (InvocationTargetException e) {
                    throw e.getCause();                       // unwrap the real exception
                } finally {
                    System.out.println(method.getName() + " took "
                            + (System.nanoTime() - start) / 1_000_000 + " ms");
                }
            });
}

UserRepository repo = withTiming(new JdbcUserRepository(ds), UserRepository.class);
```

Limits: only interfaces can be proxied this way. For classes, libraries generate subclass-based proxies (for example CGLIB or ByteBuddy).

## Proxies in frameworks

Many frameworks rely on proxies behind the scenes. Spring uses them for transactions, caching, security and async execution. You annotate a method, and the framework wraps your bean in a proxy that adds the behavior. ORMs use proxies for lazy-loaded entities.

This explains a common surprise: **calling an annotated method from another method in the same class** bypasses the proxy, because the call goes straight to `this`, not through the wrapper. With proxy-based mechanisms such as Spring's default, annotations like `@Transactional` or `@Cacheable` then have no effect on that call.

## Proxy vs Decorator

| | Proxy | [Decorator](./04-decorator.md) |
|---|---|---|
| Purpose | Control **access** to the subject | Add **behavior** |
| Who creates the subject | Often the proxy itself (virtual, remote) | The client passes it in |
| Typical relationship | Client usually unaware a proxy is involved | Client composes decorators deliberately |

The code looks almost identical. Judge by intent.

## Benefits and costs

| Benefits | Costs |
|---|---|
| Add lazy loading, security, caching without changing the subject | Extra indirection and an extra class (unless dynamic) |
| Clients unchanged | Can add latency, and hide where time is spent |
| Open/Closed friendly | Behavior can surprise (self-invocation, proxy-vs-target identity) |

## Common mistakes

- **Leaking the real object** (returning `real` from a method), which lets callers bypass the proxy.
- **Self-invocation** bypassing a framework proxy, as described above.
- **Proxy changing semantics** (for example, caching results of non-idempotent calls).
- **Thread safety** of lazy initialization.
- **Comparing proxy and target by identity** (`==`, `getClass()`), which fails.
- **Exceptions wrapped** by reflection-based proxies, and not unwrapped, so callers see `InvocationTargetException`.
- **Stacking too many proxies**, making stack traces hard to read.

## Debugging

| Symptom | Likely cause |
|---|---|
| Annotation (`@Transactional`, `@Cacheable`) seems ignored | Call came from within the same class, bypassing the proxy |
| `ClassCastException` after injection | Dynamic proxy implements the interface but isn't an instance of the concrete class |
| Unexpected `InvocationTargetException` | Handler didn't unwrap the underlying cause |
| First call slow, later calls fast | Lazy proxy initializing on first access |

## Quick Summary

- A proxy stands in for another object, with the same interface, to control access.
- Common types: virtual, protection, remote, caching, logging.
- Java can generate proxies at runtime for interfaces, and frameworks use them heavily.
- Differs from Decorator by intent: access control vs added behavior.

## Related

[Decorator](./04-decorator.md) · [Adapter](./01-adapter.md) · [Facade](./05-facade.md)

## Next

Behavioral patterns: [../06-behavioral-patterns/00-overview.md](../06-behavioral-patterns/00-overview.md)
