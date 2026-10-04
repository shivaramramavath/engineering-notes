# Generic Patterns

The mechanics of generics (type parameters, bounds, wildcards, erasure) combine into a handful of **recurring designs**. This file collects the ones you will meet in the JDK and frameworks, and the ones worth writing yourself, along with guidance on keeping generic APIs simple.

## 1. Generic utility methods

Small, stateless methods that work on any type. Use wildcards for flexibility ([02](./02_wildcards-and-pecs.md)) and bounds when you need behavior ([01](./01_bounded-types.md)).

```java
public final class Lists {
    private Lists() { }

    // Producer-extends input, consumer-super function: PECS applied
    public static <T, R> List<R> map(Collection<? extends T> input, Function<? super T, ? extends R> fn) {
        List<R> out = new ArrayList<>(input.size());
        for (T item : input) out.add(fn.apply(item));
        return out;
    }

    public static <T> List<T> filter(Collection<? extends T> input, Predicate<? super T> keep) {
        List<T> out = new ArrayList<>();
        for (T item : input) if (keep.test(item)) out.add(item);
        return out;
    }

    public static <T extends Comparable<? super T>> T max(Collection<? extends T> items) {
        Iterator<? extends T> it = items.iterator();
        T best = it.next();
        while (it.hasNext()) { T x = it.next(); if (x.compareTo(best) > 0) best = x; }
        return best;
    }
}

List<Integer> lengths = Lists.map(List.of("a", "bb", "ccc"), String::length);    // [1, 2, 3]
```

In real code prefer streams or existing helpers, but this is exactly how those APIs are typed.

## 2. Type-safe heterogeneous container (type token)

A map whose **values have different types**, with the key carrying the type. The classic example from *Effective Java*:

```java
public class Favorites {
    private final Map<Class<?>, Object> map = new HashMap<>();

    public <T> void put(Class<T> type, T instance) {
        map.put(Objects.requireNonNull(type), type.cast(instance));   // runtime check protects against raw-type misuse
    }

    public <T> T get(Class<T> type) {
        return type.cast(map.get(type));
    }
}

Favorites f = new Favorites();
f.put(String.class, "Java");
f.put(Integer.class, 0xCAFE);
String s = f.get(String.class);          // no cast needed, type-checked
int i = f.get(Integer.class);
// f.put(Integer.class, "oops");          // compile error
```

The `Class<T>` object is a **type token**. This pattern appears in dependency-injection containers (`context.getBean(UserService.class)`), service registries, event buses and configuration holders. Limitation: it cannot distinguish `List<String>` from `List<Integer>` (erasure): use a **super type token** for that ([03](./03_type-erasure.md#workarounds)).

### Event bus built on it

```java
public class EventBus {
    private final Map<Class<?>, List<Consumer<Object>>> handlers = new HashMap<>();

    @SuppressWarnings("unchecked")
    public <E> void subscribe(Class<E> type, Consumer<? super E> handler) {
        handlers.computeIfAbsent(type, k -> new ArrayList<>())
                .add(event -> handler.accept((E) event));        // safe: we only dispatch events of this type
    }

    public void publish(Object event) {
        handlers.getOrDefault(event.getClass(), List.of()).forEach(h -> h.accept(event));
    }
}

bus.subscribe(OrderPlaced.class, e -> System.out.println("order " + e.id()));
bus.publish(new OrderPlaced(42));
```

The unchecked cast is **encapsulated** and justified in one place; callers see a fully typed API.

## 3. Generic repository / service interfaces

One interface shape for many entity types ([24-design-patterns/04-architecture/02_repository-and-dao.md](../24-design-patterns/04-architecture/02_repository-and-dao.md)):

```java
public interface Repository<T, ID> {
    Optional<T> findById(ID id);
    List<T> findAll();
    T save(T entity);
    void deleteById(ID id);
}

public class InMemoryRepository<T, ID> implements Repository<T, ID> {
    private final Map<ID, T> store = new ConcurrentHashMap<>();
    private final Function<? super T, ? extends ID> idOf;

    public InMemoryRepository(Function<? super T, ? extends ID> idOf) { this.idOf = idOf; }

    @Override public Optional<T> findById(ID id) { return Optional.ofNullable(store.get(id)); }
    @Override public List<T> findAll() { return List.copyOf(store.values()); }
    @Override public T save(T entity) { store.put(idOf.apply(entity), entity); return entity; }
    @Override public void deleteById(ID id) { store.remove(id); }
}

Repository<User, Long> users = new InMemoryRepository<>(User::id);
```

Frameworks (Spring Data's `JpaRepository<T, ID>`) use the same shape. Share behavior through a generic base, specialize in subinterfaces (`UserRepository extends Repository<User, Long>`).

## 4. Self-typed (recursive) builders

Fluent builders in a class hierarchy lose their type after calling a parent method, unless the builder is parameterized by **its own type** (`B extends Builder<B>`).

```java
public abstract class Notification {
    private final String to;

    protected Notification(Builder<?> b) { this.to = b.to; }

    public abstract static class Builder<B extends Builder<B>> {
        private String to;
        public B to(String to) { this.to = to; return self(); }
        protected abstract B self();                          // subclasses return `this` typed as themselves
    }
}

public class Email extends Notification {
    private final String subject;

    private Email(Builder b) { super(b); this.subject = b.subject; }

    public static class Builder extends Notification.Builder<Builder> {
        private String subject;
        public Builder subject(String s) { this.subject = s; return this; }
        @Override protected Builder self() { return this; }
        public Email build() { return new Email(this); }
    }
}

Email mail = new Email.Builder()
        .to("ada@example.com")          // returns Email.Builder, not the parent type...
        .subject("Hello")               // ...so subclass methods are still available
        .build();
```

This is the **Curiously Recurring Template Pattern** (the same idea as `Enum<E extends Enum<E>>`). Use it sparingly: it is powerful but harder to read. See [24-design-patterns/01-creational/03_builder.md](../24-design-patterns/01-creational/03_builder.md).

## 5. Result / Either types

Return success or failure as a **value**, with no exception, for expected outcomes:

```java
public sealed interface Result<T> permits Result.Ok, Result.Err {
    record Ok<T>(T value) implements Result<T> { }
    record Err<T>(String message) implements Result<T> { }

    default <R> Result<R> map(Function<? super T, ? extends R> fn) {
        return switch (this) {
            case Ok<T> ok -> new Ok<>(fn.apply(ok.value()));
            case Err<T> err -> new Err<>(err.message());
        };
    }

    default T orElse(T fallback) {
        return this instanceof Ok<T> ok ? ok.value() : fallback;
    }
}

static Result<Integer> parse(String s) {
    try { return new Result.Ok<>(Integer.parseInt(s)); }
    catch (NumberFormatException e) { return new Result.Err<>("not a number: " + s); }
}

int n = parse("42").map(x -> x * 2).orElse(0);     // 84
```

Compare with `Optional<T>` (value or nothing) and exceptions ([06-exceptions-and-debugging/05_exception-handling-patterns.md](../06-exceptions-and-debugging/05_exception-handling-patterns.md), [12-modern-java](../12-modern-java/README.md)).

## 6. Pairs, tuples and small value types

```java
public record Pair<A, B>(A first, B second) {
    public static <A, B> Pair<A, B> of(A a, B b) { return new Pair<>(a, b); }
    public <C> Pair<A, C> withSecond(C c) { return new Pair<>(first, c); }
}

Pair<String, Integer> p = Pair.of("age", 36);
```

Handy for returning two values from a method ([methods](../02-methods/00_methods.md)). For anything used across a codebase, prefer a **named** record (`record Range(int start, int end)`) over `Pair<Integer, Integer>`: names document intent.

## 7. Generic functional pipelines

Functional interfaces are generic by design (`Function<T, R>`, `Predicate<T>`, `Supplier<T>`); wildcards make composition flexible ([09-functional-java](../09-functional-java/README.md)).

```java
public interface Validator<T> {
    Optional<String> validate(T value);                      // empty = valid, otherwise an error message

    default Validator<T> and(Validator<? super T> other) {   // a Validator<Object> can also validate Strings
        return v -> validate(v).or(() -> other.validate(v));
    }
}

Validator<String> notBlank = s -> s.isBlank() ? Optional.of("blank") : Optional.empty();
Validator<Object> notNull = o -> o == null ? Optional.of("null") : Optional.empty();
Validator<String> both = notBlank.and(notNull);              // accepted thanks to `? super T`
```

## 8. Generic caches and registries

```java
public class Cache<K, V> {
    private final Map<K, V> store = new ConcurrentHashMap<>();
    private final Function<? super K, ? extends V> loader;

    public Cache(Function<? super K, ? extends V> loader) { this.loader = loader; }

    public V get(K key) { return store.computeIfAbsent(key, loader); }
}

Cache<Long, User> users = new Cache<>(id -> repository.load(id));
```

See [25-real-world-patterns/00_caching.md](../25-real-world-patterns/00_caching.md): in practice, use a library such as Caffeine.

## 9. Generic visitor (typed results)

Let each operation choose its own return type:

```java
interface Shape { <R> R accept(ShapeVisitor<R> visitor); }
interface ShapeVisitor<R> { R visitCircle(Circle c); R visitRect(Rect r); }

class Circle implements Shape {
    final double r;
    Circle(double r) { this.r = r; }
    public <R> R accept(ShapeVisitor<R> v) { return v.visitCircle(this); }
}

double area = shape.accept(new ShapeVisitor<Double>() { ... });
String label = shape.accept(new ShapeVisitor<String>() { ... });
```

With sealed hierarchies and pattern-matching `switch`, many visitors are no longer needed ([12-modern-java](../12-modern-java/README.md)).

## 10. Factories with `Supplier` and `Class`

```java
public static <T> T newInstance(Class<? extends T> type) throws ReflectiveOperationException {
    return type.getDeclaredConstructor().newInstance();
}

public static <T> List<T> repeat(int n, Supplier<? extends T> factory) {
    return Stream.generate(factory).limit(n).collect(Collectors.toList());
}
```

Prefer `Supplier<T>` over `Class<T>` when you can: no reflection, no checked exceptions ([03](./03_type-erasure.md#workarounds)).

## API design guidance

| Guideline | Why |
|-----------|-----|
| **Prefer generic types and methods to casts** in your own APIs | Safer for callers |
| Use **PECS wildcards on parameters** to maximize flexibility | `Collection<? extends E>` accepts more arguments |
| **Do not return wildcard types** | Callers cannot use them comfortably |
| Keep **type parameter lists short** (1-3) | `Foo<A, B, C, D, E>` is unreadable |
| Use **bounds only when you need the methods** | Over-bounding limits users |
| Provide **static factory methods** with inference (`Pair.of(...)`) | Avoids spelling out type arguments |
| **Encapsulate unchecked casts** in one well-commented place | Users see a type-safe API |
| Prefer **lists to arrays** in generic code | Erasure and covariance problems |
| Document **what a type parameter means** (Javadoc `@param <T>`) | Clarity |
| Prefer **concrete types** when generics add no value | Simpler code |
| Avoid generics of generics of generics | `Map<String, List<Map<Long, Set<X>>>>`: introduce a type |

### Naming complex types

```java
// Hard to read
Map<String, Map<LocalDate, List<Transaction>>> data;

// Easier: give a name to the concept
record Ledger(Map<LocalDate, List<Transaction>> byDay) { }
Map<String, Ledger> ledgers;
```

## Where generics are **not** the answer

| Situation | Better |
|-----------|--------|
| The class really works with one concrete type | A normal class |
| You need different behavior per type | Polymorphism or an interface, not a type parameter |
| A type parameter only appears once, in a parameter | A wildcard (or just the bound's type) |
| Primitive-heavy numeric code | `int[]`, `IntStream`; boxing costs add up |
| You want to switch on `T` | Rethink the design ("generics" means *independent* of the type) |

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Over-engineering with generics | Unreadable signatures, nobody can use the API | Simplify: fewer parameters, concrete types |
| Missing `? super`/`? extends` on parameters | Callers need awkward casts or copies | Apply PECS |
| Wildcards in return types | Awkward for callers | Return concrete or type-parameter types |
| Unchecked casts scattered through code | Heap pollution risk | Isolate in one tested place |
| `Class<T>` tokens used for parameterized types | `List<String>` indistinguishable from `List<Integer>` | Super type tokens |
| Self-typed builders where a simple builder suffices | Complexity | Plain builders |
| Raw types in generic utilities | Unchecked warnings | Parameterize everything |
| `Pair<Integer, Integer>` everywhere | Unclear meaning | Named records |
| Using generics to avoid thinking about the design | Wrong abstractions | Start concrete, generalize when duplication appears |
| Checking `instanceof T`-style via casts without `Class<T>` | Always passes (erased) | `type.isInstance(x)` |

## Key takeaways

- Common generic designs: utility methods, type-token containers, repositories, self-typed builders, result types, pairs, validators, caches, visitors
- Apply **PECS** to parameters; return concrete types
- Encapsulate unavoidable unchecked casts in one place behind a type-safe API
- Keep generic signatures short and readable; name complex types
- Use generics where code is **independent of the type**; use polymorphism where behavior depends on it

**Next:** [08-collections](../08-collections/README.md)
