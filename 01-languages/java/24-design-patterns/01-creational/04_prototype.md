# Prototype Pattern in Modern Java

> Efficient object cloning and templating: the flaws of `Cloneable`, copy constructors, factory copy methods, serialization cloning, and deep copying strategies.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core Design Pattern / Creational |
| **Prerequisites** | [04_immutability](../23-design-and-clean-code/04_immutability.md), [04_oop](../../04-oop/README.md) |
| **Time to Read** | 35 minutes |
| **Status** | Verified Production Guide |
| **Java Baseline** | Java 17 LTS / Java 21 LTS / Java 25 LTS |

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
10. [Interview Questions & Answers](#10-interview-questions--answers)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### What is the Prototype Pattern?
The Prototype pattern is a creational design pattern that allows an application to create new objects by **cloning an existing prototypical instance**, rather than instantiating from scratch via constructors.

When is Prototype Useful?
- When object creation is computationally expensive (e.g. requires loading 10 MB configuration matrices, compiling complex ASTs, or making expensive database queries).
- When you need a pre-configured template instance that can be duplicated and slightly tweaked.
- When client code needs to duplicate an object without coupling to its concrete class.

### Why `java.lang.Cloneable` is Widely Considered Broken
Java standard library includes `Cloneable` and `Object.clone()`. However, Josh Bloch and Java language designers widely acknowledge that `Cloneable` is severely flawed:
- `Cloneable` has no methods! It is a marker interface that modifies the behavior of the protected `Object.clone()` method.
- `Object.clone()` bypasses constructors completely, violating normal object initialization.
- It performs a shallow copy by default, easily corrupting mutable fields.
- Modern Java replaces `Cloneable` with **Copy Constructors** and **Copy Factory Methods**.

---

## 2. Core Concepts

### Shallow Copy vs. Deep Copy
- **Shallow Copy**: Duplicates the object header and primitive fields, but copies object reference pointers directly. Both the prototype and the clone point to the identical internal collections or child objects.
- **Deep Copy**: Recursively duplicates the object AND all referenced child objects, creating a completely independent object graph.

### Copy Constructors and Static Copy Factories
- **Copy Constructor**: `public User(User source)`
- **Copy Factory**: `public static User newInstance(User source)`
These idioms respect constructors, work cleanly with `final` fields, do not throw checked `CloneNotSupportedException`, and allow polymorphic copies.

---

## 3. How It Works Internally

### Prototype Registry Architecture
```
[ Prototype Registry ]
  - "default_report" -> Template Report Instance (Pre-parsed layout)
  - "admin_policy"   -> Template Policy Instance (Pre-loaded rules)
           |
           | Client requests: registry.get("default_report").clone()
           v
[ Rapid In-Memory Duplication ] (Copy Constructor / Deep Copy)
           |
           v
[ Specialized Clone Returned to Client ]
```

---

## 4. Syntax & Usage

### 1. Copy Constructor Idiom
```java
public final class DocumentTemplate {
    private final String header;
    private final java.util.List<String> sections;

    public DocumentTemplate(String header, java.util.List<String> sections) {
        this.header = header;
        this.sections = new java.util.ArrayList<>(sections);
    }

    // The Copy Constructor
    public DocumentTemplate(DocumentTemplate source) {
        this.header = source.header;
        // Deep copy of mutable list
        this.sections = new java.util.ArrayList<>(source.sections);
    }
}
```

---

## 5. Practical Examples

### Example 1: In-Memory Prototype Registry with Custom Prototype Interface
**Goal:** Create a game engine unit spawner that clones pre-configured prototypical units without using `Cloneable`.

```java
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

// Custom Prototype Interface
public interface Prototype<T> {
    T copy();
}

// Concrete Prototype Class
public class MonsterUnit implements Prototype<MonsterUnit> {
    private final String species;
    private int health;
    private int attackPower;

    public MonsterUnit(String species, int health, int attackPower) {
        this.species = species;
        this.health = health;
        this.attackPower = attackPower;
    }

    // Copy Constructor
    public MonsterUnit(MonsterUnit source) {
        this.species = source.species;
        this.health = source.health;
        this.attackPower = source.attackPower;
    }

    @Override
    public MonsterUnit copy() {
        return new MonsterUnit(this);
    }

    public void setHealth(int h) { this.health = h; }
}

// Prototype Registry
public class SpawnerRegistry {
    private final Map<String, Prototype<MonsterUnit>> prototypes = new ConcurrentHashMap<>();

    public void register(String key, MonsterUnit prototype) {
        prototypes.put(key, prototype);
    }

    public MonsterUnit spawn(String key) {
        Prototype<MonsterUnit> proto = prototypes.get(key);
        if (proto == null) throw new IllegalArgumentException("Unknown unit: " + key);
        return proto.copy();
    }
}
```

### Example 2: Deep Copying via Modern JSON Serialization
**Goal:** Perform an automated deep copy of complex nested structures without manual field copying boilerplate.

```java
import com.fasterxml.jackson.databind.ObjectMapper;

public class DeepCloner {
    private static final ObjectMapper MAPPER = new ObjectMapper();

    public static <T> T deepCopy(T source, Class<T> clazz) {
        try {
            byte[] bytes = MAPPER.writeValueAsBytes(source);
            return MAPPER.readValue(bytes, clazz);
        } catch (Exception e) {
            throw new RuntimeException("Deep copy failed", e);
        }
    }
}
```

---

### Example 3: Thread-Safe Object Cloning with ByteArray Streams
**Goal:** Deep copy complex graphs safely using serialization when classes implement `Serializable`.

```java
import java.io.*;

public class SerializationCloner {
    @SuppressWarnings("unchecked")
    public static <T extends Serializable> T deepClone(T object) {
        try {
            ByteArrayOutputStream baos = new ByteArrayOutputStream();
            try (ObjectOutputStream oos = new ObjectOutputStream(baos)) {
                oos.writeObject(object);
            }
            try (ByteArrayInputStream bais = new ByteArrayInputStream(baos.toByteArray());
                 ObjectInputStream ois = new ObjectInputStream(bais)) {
                return (T) ois.readObject();
            }
        } catch (Exception e) {
            throw new IllegalArgumentException("Cloning failed", e);
        }
    }
}
```

### Example 4: Copy Factory Method Pattern (Effective Java Item 13)
**Goal:** Provide an explicit static factory method for copying without constructor proliferation.

```java
public record SecurityToken(String id, java.time.Instant expiry, byte[] signature) {
    public SecurityToken {
        signature = signature.clone(); // Invariant protection
    }

    // Copy Factory Method
    public static SecurityToken copyOf(SecurityToken source) {
        return new SecurityToken(source.id(), source.expiry(), source.signature());
    }
}
```

## 6. Best Practices

1. **Avoid `Cloneable` and `clone()`**: Use Copy Constructors (`new Class(source)`) or Copy Factory Methods (`Class.copyOf(source)`).
2. **Ensure Deep Copies for Mutable State**: Never share internal mutable lists or arrays between prototype and clone.
3. **Use a Prototype Registry**: Cache expensive pre-configured prototypes in a centralized registry map.
4. **Make Value Types Immutable**: If an object is deeply immutable (like a Java record), cloning is completely unnecessary—simply return `this`!
5. **Preserve `final` Fields**: Copy constructors allow target fields to remain `final`. `Object.clone()` cannot assign to final fields easily.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Secure Fix |
| :--- | :--- | :--- |
| **Relying on `super.clone()` for shallow copies** | Shared mutable references lead to spooky cross-instance mutations. | Deep copy all mutable fields. |
| **Using `Cloneable` with `final` fields** | `clone()` cannot assign to blank final fields. | Use copy constructors. |
| **Cloning immutable objects** | Wastes memory and CPU; immutable instances can be shared safely. | Return the same instance. |
| **Calling `clone()` on untrusted subclasses** | Subclass can override `clone()` to return malicious proxy. | Finalize class or use copy constructor. |

---

## 8. Performance & Security Notes

### In-Memory Cloning Speed
Creating objects via copy constructors takes mere nanoseconds. By cloning pre-initialized, heavily validated data matrices, applications bypass expensive disk I/O, parsing overhead, and network handshakes.

---

## 9. Comparison / When to Use What

| Technique | Modifies Final Fields | Constructor Invoked | Deep Copy Ease | Modern Recommendation |
| :--- | :--- | :--- | :--- | :--- |
| **`Object.clone()`** | No (Cannot assign) | No (Bypassed) | Manual / Hard | Deprecated in spirit |
| **Copy Constructor** | Yes (Normal init) | Yes | Easy & Explicit | Recommended standard |
| **Copy Factory Method**| Yes | Yes | Easy & Explicit | Recommended standard |
| **JSON Serialization** | Yes | Yes (Jackson) | Fully Automatic | Complex nested graphs |

---

## 10. Interview Questions & Answers

### Q1: Why is `java.lang.Cloneable` considered a broken design in Java?
<details>
<summary>View Answer</summary>
`Cloneable` is a marker interface that lacks any methods. Instead of declaring `clone()`, it changes the runtime behavior of the protected `Object.clone()` method. Furthermore, `clone()` creates an object without calling any constructor, bypassing normal invariant validation. It does not play well with `final` fields because final fields cannot be reassigned inside `clone()`. Effective Java Item 13 strongly recommends using Copy Constructors or Copy Factories instead.
</details>

### Q2: How does a Copy Constructor solve the problems of `Cloneable`?
<details>
<summary>View Answer</summary>
A copy constructor (`public MyClass(MyClass other)`) is a standard constructor. It does not rely on mysterious language mechanisms, honors class invariants, allows fields to be marked `final`, throws no checked exceptions, and requires no type casting.
</details>

### Q3: When should the Prototype pattern be preferred over Factory Method?
<details>
<summary>View Answer</summary>
When object instantiation involves high computational cost (e.g. database lookups, network configuration, or intensive calculations) that can be performed once and duplicated, or when the system needs to spawn objects whose state matches dynamic runtime configurations without creating parallel creator class hierarchies.
</details>

### Q4: If an object is completely immutable, how should Prototype\'s `copy()` method be implemented?
<details>
<summary>View Answer</summary>
If an object is deeply immutable, cloning is redundant because the instance can be shared safely across any number of threads. The `copy()` method should simply return `this`.
</details>

### Q5: What is the difference between a shallow copy and a deep copy?
<details>
<summary>View Answer</summary>
A shallow copy duplicates only the top-level object\'s field values; any reference fields still point to the exact same child objects in memory. A deep copy recursively duplicates the top-level object and all referenced child objects, ensuring zero shared mutable references.
</details>

### Q6: How does Java native serialization perform deep copying, and why is it dangerous?
<details>
<summary>View Answer</summary>
Historically developers used `ByteArrayOutputStream` and `ObjectInputStream` to deep copy objects. However, native Java serialization is notoriously slow and exposes the application to severe deserialization gadget chain vulnerabilities (RCE). JSON or copy constructors should always be used instead.
</details>

### Q7: Can a Copy Constructor accept an interface instead of a concrete class?
<details>
<summary>View Answer</summary>
Yes. This is called a **Conversion Constructor** (e.g. `new TreeSet(Collection c)`). It allows callers to convert one representation of data into another while copying.
</details>

### Q8: What is a Prototype Registry?
<details>
<summary>View Answer</summary>
A Prototype Registry is a catalog (usually a map) of pre-configured prototype instances. Clients query the registry by key or type to retrieve a fresh clone of the desired template.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a Copy Constructor with Deep List Copy
**Task:** Create an `OrderTemplate` class with a deep-copy constructor that clones an internal list of line items.
<details>
<summary>View Solution</summary>

```java
import java.util.ArrayList;
import java.util.List;

public class OrderTemplate {
    private final String customerId;
    private final List<String> items;

    public OrderTemplate(String customerId, List<String> items) {
        this.customerId = customerId;
        this.items = new ArrayList<>(items);
    }

    public OrderTemplate(OrderTemplate source) {
        this.customerId = source.customerId;
        this.items = new ArrayList<>(source.items); // Deep copy
    }

    public List<String> getItems() { return items; }
}
```
</details>

### Exercise 2: Prototype Interface Implementation
**Task:** Implement a generic `Copyable<T>` interface on a `UserProfile` class.
<details>
<summary>View Solution</summary>

```java
public interface Copyable<T> { T copy(); }

public class UserProfile implements Copyable<UserProfile> {
    private String bio;
    public UserProfile(String bio) { this.bio = bio; }
    public UserProfile(UserProfile other) { this.bio = other.bio; }
    @Override public UserProfile copy() { return new UserProfile(this); }
}
```
</details>

### Exercise 3: Registry-Based Spawner
**Task:** Create a registry that stores prototypes and clones them on demand.
<details>
<summary>View Solution</summary>

```java
import java.util.HashMap;
import java.util.Map;

public class ShapeRegistry {
    private final Map<String, Copyable<?>> cache = new HashMap<>();

    public void addPrototype(String key, Copyable<?> proto) {
        cache.put(key, proto);
    }

    public Object getClone(String key) {
        Copyable<?> proto = cache.get(key);
        return (proto != null) ? proto.copy() : null;
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Core Goal**: Create new objects by duplicating existing templates.
- **Avoid Cloneable**: Never implement `Cloneable`; use Copy Constructors instead.
- **Deep Copy**: Always duplicate mutable child collections to avoid cross-mutation.
- **Immutable Types**: If deeply immutable, return `this` rather than cloning.
- **Prototypes Registry**: Cache pre-configured templates in a central map.
- **Copy Constructor**: `public MyClass(MyClass source)`.

---

## 13. Further Reading
- Effective Java: [Item 13: Override clone judiciously (Prefer copy constructors)](https://www.oreilly.com/library/view/effective-java-3rd/9780134686097/)
- GoF Design Patterns: [Prototype Pattern](https://www.oreilly.com/library/view/design-patterns-elements/0201633612/)
- Refactoring Guru: [Prototype Design Pattern](https://refactoring.guru/design-patterns/prototype)
