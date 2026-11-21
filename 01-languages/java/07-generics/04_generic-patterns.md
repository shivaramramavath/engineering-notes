# Advanced Generic Design Patterns

> Architectural patterns leveraging the Java type system: Typesafe Heterogeneous Containers, Curiously Recurring Template Pattern, Phantom Types, and Type-Safe State Machines.

| Metadata | Value |
|---|---|
| **Level** | Advanced |
| **Prerequisites** | [00_generic-classes-and-methods.md](00_generic-classes-and-methods.md), [01_bounded-types.md](01_bounded-types.md), [02_wildcards-and-pecs.md](02_wildcards-and-pecs.md), [03_type-erasure.md](03_type-erasure.md) |
| **Time to Read** | 25 minutes |
| **Status** | Active |
| **Java Version** | Java 5+ |

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

Standard generic collections parameterize the **entire container**: a `Map<K, V>` restricts all keys to type `K` and all values to type `V`. However, real-world systems frequently need containers where different entries can have different types, while still guaranteeing compile-time type safety for each individual entry:
- Application configuration contexts (storing strings, ints, custom settings)
- Dependency injection registries
- Type-safe session bags
- Strongly typed state machines

Without advanced generic design patterns, developers often revert to `Map<String, Object>`, losing type safety and introducing risky runtime casting.

By combining Java generics with runtime class tokens (`Class<T>`), self-referential type bounds, and phantom types, we can construct sophisticated architectures that enforce strict invariants at compile time without runtime overhead.

---

## 2. Core Concepts

### 1. Typesafe Heterogeneous Container (THC)
First detailed in Joshua Bloch's *Effective Java* (Item 33), a Typesafe Heterogeneous Container parameterizes the **key** rather than the container. The key is typically a `Class<T>` type token. This allows values of different types to be stored safely in the same map without casting.

```
Standard Map:  Map<String, Object>  ---> map.get("db.port") -> requires (Integer) cast
THC Pattern:   Container             ---> container.get(Integer.class) -> returns Integer! Type-safe!
```

### 2. Curiously Recurring Template Pattern (CRTP)
CRTP is a technique where a class `Sub` extends a generic base class parameterized by `Sub` itself:
```java
public abstract class Base<T extends Base<T>> {
    public abstract T self();
}
```
This enables fluent builders, polymorphic method chaining, and self-referential operations without losing concrete subclass typing.

### 3. Phantom Types
A **phantom type** is a type parameter that never appears in any instance field or method signature of a class. It exists purely as a compile-time marker to enforce protocol states and prevent invalid operations before the code compiles:
```java
public class HttpRequest<State> { ... } // State is a phantom marker: Unauthenticated, Authenticated
```

### 4. Recursive Type Bounds
Used when elements must interact with other elements of their own subtype (e.g., `Comparable<? super T>`). This pattern ensures type homogeneity in comparison trees and graph nodes.

---

## 3. How It Works Internally

### Typesafe Heterogeneous Container Memory Model
Inside a Typesafe Heterogeneous Container:
1. The underlying backing storage is typically `Map<Class<?>, Object>`.
2. When an object is added (`put(Class<T> type, T instance)`), the container stores `type.cast(instance)` or `instance` as `Object`.
3. When retrieved (`get(Class<T> type)`), the container calls `type.cast(rawObject)`. Because the entry was keyed by `Class<T>`, this cast is guaranteed to succeed.

```
+-------------------------------------------------------+
|  Typesafe Heterogeneous Container                     |
|                                                       |
|  Backing Map: Map<Class<?>, Object>                   |
|  +--------------------+----------------------------+  |
|  | Key (Class Token)  | Value (Object reference)   |  |
|  +--------------------+----------------------------+  |
|  | String.class       | "Production"               |  |
|  | Integer.class      | 8080                       |  |
|  | DatabaseConfig.class| DatabaseConfig instance   |  |
|  +--------------------+----------------------------+  |
|                                                       |
|  get(Integer.class) -> returns Integer 8080 (No cast!)|
+-------------------------------------------------------+
```

### Dynamic Cast Safety (`Class.cast`)
`Class.cast(Object obj)` is a runtime method that performs an unchecked cast dynamically while throwing a `ClassCastException` if the type does not match. In the THC pattern, calling `type.cast(instance)` during `put` ensures that malicious raw-type callers cannot poison the container.

---

## 4. Syntax & Usage

### 1. Typesafe Heterogeneous Container Implementation
```java
import java.util.HashMap;
import java.util.Map;
import java.util.Objects;

public class TypeSafeContext {
    private final Map<Class<?>, Object> entries = new HashMap<>();

    public <T> void put(Class<T> type, T instance) {
        Objects.requireNonNull(type, "Type token must not be null");
        // type.cast ensures runtime safety even if raw types are passed
        entries.put(type, type.cast(instance));
    }

    public <T> T get(Class<T> type) {
        Objects.requireNonNull(type, "Type token must not be null");
        return type.cast(entries.get(type));
    }
}
```

### 2. Multi-Key Heterogeneous Container (Composite Key)
```java
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

public class NamedContext {
    private record Key<T>(String name, Class<T> type) {}

    private final Map<Key<?>, Object> store = new ConcurrentHashMap<>();

    public <T> void put(String name, Class<T> type, T value) {
        store.put(new Key<>(name, type), type.cast(value));
    }

    public <T> T get(String name, Class<T> type) {
        return type.cast(store.get(new Key<>(name, type)));
    }
}
```

---

## 5. Practical Examples

### Example 1: Fluent Hierarchical Builder via CRTP (Basic to Intermediate)
```java
// Abstract base builder using self-bounding generic parameter
public abstract class QueryBuilder<T extends QueryBuilder<T>> {
    protected String table;
    protected String whereClause;

    @SuppressWarnings("unchecked")
    protected T self() {
        return (T) this;
    }

    public T from(String table) {
        this.table = table;
        return self();
    }

    public T where(String whereClause) {
        this.whereClause = whereClause;
        return self();
    }
}

// Concrete subclass extends base with itself as type parameter
public class SelectQueryBuilder extends QueryBuilder<SelectQueryBuilder> {
    private String columns = "*";

    public SelectQueryBuilder select(String columns) {
        this.columns = columns;
        return self();
    }

    public String build() {
        return "SELECT " + columns + " FROM " + table + (whereClause != null ? " WHERE " + whereClause : "");
    }

    public static void main(String[] args) {
        String sql = new SelectQueryBuilder()
            .from("users")
            .where("active = true")
            .select("id, username, email")
            .build();
        System.out.println("Generated SQL: " + sql);
    }
}
```

### Example 2: Compile-Time State Machine with Phantom Types (Real-World)
```java
// State marker interfaces
public sealed interface OrderState permits Draft, Validated, Paid {}
public final class Draft implements OrderState {}
public final class Validated implements OrderState {}
public final class Paid implements OrderState {}

// Order class with phantom type parameter 'S'
public final class Order<S extends OrderState> {
    private final String orderId;
    private final double amount;

    private Order(String orderId, double amount) {
        this.orderId = orderId;
        this.amount = amount;
    }

    // Factory creates order in Draft state
    public static Order<Draft> create(String orderId, double amount) {
        return new Order<>(orderId, amount);
    }

    // Validates Draft -> Validated
    public Order<Validated> validate(Order<Draft> order) {
        System.out.println("Validating order " + orderId);
        return new Order<>(order.orderId, order.amount);
    }

    // Pays Validated -> Paid
    public static Order<Paid> pay(Order<Validated> order) {
        System.out.println("Processing payment for order " + order.orderId);
        return new Order<>(order.orderId, order.amount);
    }

    public static void main(String[] args) {
        Order<Draft> draft = Order.create("ORD-99", 250.0);
        Order<Validated> validated = draft.validate(draft);
        Order<Paid> paid = Order.pay(validated);
        System.out.println("Order successfully processed: " + paid.orderId);

        // Attempting to pay an unvalidated draft fails at COMPILE TIME!
        // Order.pay(draft); // COMPILE ERROR: type mismatch!
    }
}
```

### Example 3: Edge Case - Non-Reifiable Types in Typesafe Containers
```java
import java.util.List;

public class THCWithGenericsEdgeCase {
    // Problem: List<String>.class does NOT exist due to type erasure!
    // Storing List<String> in TypeSafeContext using List.class causes collisions with List<Integer>!

    public static void main(String[] args) {
        TypeSafeContext ctx = new TypeSafeContext();
        // ctx.put(List<String>.class, List.of("a")); // COMPILE ERROR!

        // Workaround: Use Super Type Token Key or dedicated container wrapper
    }
}
```

### Example 4: Failure Case - Heap Pollution via Malicious Raw Class Token
```java
public class PoisonedContainerDemo {
    @SuppressWarnings({"unchecked", "rawtypes"})
    public static void attack(TypeSafeContext ctx) {
        Class rawClass = String.class;
        // If TypeSafeContext did NOT call type.cast(instance), this would insert Integer into String slot
        try {
            ctx.put(rawClass, Integer.valueOf(999));
        } catch (ClassCastException e) {
            System.out.println("Attack prevented at put-time: " + e.getMessage());
        }
    }

    public static void main(String[] args) {
        TypeSafeContext ctx = new TypeSafeContext();
        attack(ctx);
    }
}
```

---

## 6. Best Practices

1. **Always Use `type.cast()` in Typesafe Containers**: Defend against raw type heap pollution by actively calling `type.cast(instance)` inside your container's `put` method.
2. **Combine CRTP with `self()` Method**: When writing fluent builders with inheritance, declare a `protected abstract T self();` method to avoid unchecked casts across the codebase.
3. **Use Phantom Types for Critical Protocol Transitions**: In banking or medical domain logic, represent transaction phases (Unverified, Verified, Authorized, Executed) as phantom types so invalid steps are rejected by the compiler.
4. **Document Type Token Limitations**: Inform API consumers that `Class<T>` tokens cannot represent parameterized types (e.g., `List<String>.class`), and provide a `TypeReference<T>` alternative if collections are needed.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why Wrong | Fix |
|---|---|---|
| Omitting `type.cast` inside `put` | Raw type callers can corrupt the container, causing delayed `ClassCastException` on retrieval. | Always call `type.cast(value)` before saving. |
| Attempting `ctx.put(List<String>.class, ...)` | Non-reifiable; Java grammar does not support `.class` on parameterized types. | Use Super Type Token (`TypeReference<List<String>>`) or wrapper class. |
| Forgetting self-bounding upper bound in CRTP | Child builders lose fluent chaining when invoking inherited parent methods. | Declare `<T extends Builder<T>>`. |
| Creating phantom state classes with state data | Phantom markers should be empty marker types; storing state violates separation. | Keep phantom types stateless (`sealed interface` / `record`). |
| Incurring reflection overhead in tight loops | Using `Class.cast` or reflection in nanosecond hot paths adds minor overhead. | Cache lookups or use monomorphic call sites where performance is critical. |
| Over-engineering simple objects with CRTP | Adds cognitive complexity to simple domain classes without inheritance. | Use standard concrete builders unless hierarchical chaining is strictly needed. |

---

## 8. Performance & Security Notes

### Performance
- **`Class.cast()` JIT Optimization**: Modern JVMs (HotSpot C2 / GraalVM) recognize `Class.cast()` as an intrinsic candidate. When the class token is constant or monomorphic, the cast is compiled into an ordinary null-check and branch test with zero reflection overhead.

### Security
- **Defensive Type Token Validation**: Never allow untrusted caller-supplied class names to be dynamically resolved via `Class.forName()` without a strict allowlist; this prevents arbitrary class loading or initialization attacks.

---

## 9. Comparison / When to Use What

| Pattern | Complexity | Type Safety Guarantee | Primary Application |
|---|---|---|---|
| **Typesafe Heterogeneous Container** | Low | Strong at runtime key check | DI context, configuration bag, session store |
| **Curiously Recurring Template Pattern** | Medium | Compile-time method chaining | Multi-level fluent builders, AST nodes |
| **Phantom Types** | High | Compile-time state machine | Protocol transitions, safety-critical workflows |
| **Super Type Token** | Medium | Runtime reflection over metadata | JSON/XML serialization libraries (Jackson, Gson) |

---

## 10. Interview Questions

<details>
<summary>1. What is a Typesafe Heterogeneous Container and how does it differ from a standard `Map<K, V>`?</summary>
In a standard `Map<K, V>`, all keys share type `K` and all values share type `V`. In a Typesafe Heterogeneous Container, the key is parameterized with the type of the value (typically `Class<T>`), allowing values of differing types to reside in the same container while guaranteeing type safety on retrieval without explicit casts.
</details>

<details>
<summary>2. Why does `TypeSafeContext` call `type.cast(instance)` during `put`?</summary>
To ensure dynamic type safety. A caller could cast a raw `Class` token and pass an invalid object. Without `type.cast(instance)`, the bad object would be stored silently, causing a mysterious `ClassCastException` much later when a legitimate caller invokes `get()`.
</details>

<details>
<summary>3. What is the fundamental limitation of the standard Typesafe Heterogeneous Container pattern?</summary>
It cannot support non-reifiable parameterized types. You cannot store a `List<String>` and a `List<Integer>` under distinct keys because both share the same class literal: `List.class`.
</details>

<details>
<summary>4. How does the Curiously Recurring Template Pattern (CRTP) solve the problem of builder inheritance?</summary>
Without CRTP, calling an inherited setter returns a reference to the parent builder type, breaking the fluent chain for child-specific setters. With `Builder<T extends Builder<T>>`, the parent method returns `(T) this`, preserving the concrete child type across all chained calls.
</details>

<details>
<summary>5. What are Phantom Types and what problem do they solve?</summary>
Phantom types are type parameters declared on a class that do not correspond to any fields or runtime data. They are used exclusively by the compiler to verify states and protocols (e.g., ensuring a database connection is `Connected` before `execute()` can be called).
</details>

<details>
<summary>6. How do modern libraries like Jackson handle the non-reifiable limitation of type tokens?</summary>
They use Super Type Tokens (`TypeReference<T>`). By instantiating an anonymous subclass (`new TypeReference<List<String>>() {}`), the generic type argument is preserved in the class-file `Signature` attribute and retrieved via `getClass().getGenericSuperclass()`.
</details>

<details>
<summary>7. Can you enforce sealed states with phantom types in modern Java?</summary>
Yes. Java 17+ `sealed interface` can define a closed hierarchy of allowed phantom states, ensuring no unauthorized state types can be introduced by third parties.
</details>

<details>
<summary>8. Does `Class.cast()` incur significant reflection performance overhead?</summary>
No. In modern HotSpot JIT compilation, `Class.cast()` is an intrinsic method optimized into a direct bytecode type check.
</details>

---

## 11. Practice Exercises

### Easy
1. Implement a basic `TypeSafeContainer` supporting `put(Class<T>, T)` and `get(Class<T>)`.
2. Add a `has(Class<T>)` and `remove(Class<T>)` method to the container.
3. Write a small builder using CRTP that chains setters for `Person` and `Employee`.

### Medium
4. Extend `TypeSafeContainer` to support bounded type tokens: only accept classes that implement `java.io.Serializable`.
5. Implement a compile-time door lock state machine (`Locked`, `Unlocked`, `Open`) using Phantom Types.
6. Build a type-safe generic configuration manager that reads environment variables and maps them to requested types (`Integer`, `Boolean`, `String`).

### Hard
7. Implement an advanced Heterogeneous Container that natively supports parameterized types using a custom `TypeToken<T>` key.
8. Design a multi-state order workflow (Created -> Quoted -> Approved -> Fulfilled) where transitions consume previous state objects and return new state instances.
9. Implement a full CRTP-based abstract syntax tree (AST) expression evaluator with complete polymorphic visitor dispatch.

<details>
<summary>Solutions</summary>

```java
// Easy 1: TypeSafeContainer
public class TypeSafeContainer {
    private final java.util.Map<Class<?>, Object> map = new java.util.HashMap<>();

    public <T> void put(Class<T> type, T instance) {
        map.put(java.util.Objects.requireNonNull(type), type.cast(instance));
    }

    public <T> T get(Class<T> type) {
        return type.cast(map.get(type));
    }
}

// Medium 5: Door Phantom Types
public sealed interface DoorState permits Locked, Unlocked, Open {}
public final class Locked implements DoorState {}
public final class Unlocked implements DoorState {}
public final class Open implements DoorState {}

public record Door<S extends DoorState>(String id) {
    public static Door<Locked> create(String id) { return new Door<>(id); }
    public static Door<Unlocked> unlock(Door<Locked> d) { return new Door<>(d.id()); }
    public static Door<Open> open(Door<Unlocked> d) { return new Door<>(d.id()); }
    public static Door<Unlocked> close(Door<Open> d) { return new Door<>(d.id()); }
    public static Door<Locked> lock(Door<Unlocked> d) { return new Door<>(d.id()); }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Typesafe Heterogeneous Container (THC)**: Keyed by `Class<T>`, stores mixed types with complete safety.
- **Dynamic Type Checking**: Always use `type.cast(val)` in THC `put` methods.
- **CRTP**: `class Concrete extends Base<Concrete>` allows fluent chaining across class hierarchies.
- **Phantom Types**: Zero runtime cost; compiler enforces state machine rules via type markers.
- **Super Type Token**: Captures non-reifiable generic types via anonymous subclasses (`new TypeRef<List<T>>() {}`).

---

## 13. Further Reading
- Bloch, Joshua. *Effective Java (3rd Edition)*: Item 33 - Consider typesafe heterogeneous containers.
- Gafter, Neal. *Super Type Tokens* (nealgrafter.blogspot.com).
- Ghosh, Debasish. *Functional and Reactive Domain Modeling* (Phantom Types and State Verification).
