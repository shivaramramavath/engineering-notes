# Immutable and Unmodifiable Collections

> Defensive copies, unmodifiable views vs truly immutable collection factories, memory footprints, and randomized iteration orders.

| Metadata | Value |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | [00_collection-hierarchy.md](00_collection-hierarchy.md), [01_list.md](01_list.md), [02_set.md](02_set.md), [03_map.md](03_map.md) |
| **Time to Read** | 23 minutes |
| **Status** | Active |
| **Java Version** | Java 1.2+ (List.of/Set.of/Map.of Java 9, copyOf Java 10) |

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

In multi-threaded software architectures and domain-driven design, **immutability** is the most effective defense against shared mutable state concurrency bugs, race conditions, and accidental data corruption.

For nearly two decades (Java 1.2 through Java 8), Java only provided **unmodifiable views** via `Collections.unmodifiableList()`, `Collections.unmodifiableSet()`, etc. These wrappers had significant flaws:
- They were **views**, not truly immutable collections. If the underlying backing collection was modified, the unmodifiable view silently changed!
- They allowed `null` values.
- They incurred extra wrapper object allocation overhead.

Java 9 introduced **convenience factory methods** (`List.of()`, `Set.of()`, `Map.of()`) and Java 10 introduced `copyOf()` (`List.copyOf()`, `Set.copyOf()`, `Map.copyOf()`). These provide **truly immutable**, compact, zero-overhead collections.

---

## 2. Core Concepts

### Unmodifiable View vs True Immutable Collection
- **Unmodifiable View (`Collections.unmodifiableList(list)`)**: A wrapper object that delegates read calls to a backing collection and intercepts mutation calls (`add`, `remove`), throwing `UnsupportedOperationException`. If the underlying collection is modified, the view reflects those modifications.
- **Truly Immutable Collection (`List.of(...)`, `List.copyOf(coll)`)**: Standalone data structure that holds its own elements. Cannot be modified through any reference. Guaranteed to never change after creation.

```
Unmodifiable View:
[ Client ] ---> UnmodifiableList Wrapper ---> [ Mutable Backing ArrayList ] <--- [ Rogue Thread ]
                    (Throws on write)               (Can mutate backing list!)

True Immutable Collection:
[ Client ] ---> ImmutableCollections.ListN [ "A", "B", "C" ] (No backing list exists! Forever fixed!)
```

### Key Characteristics of Modern Immutable Collections (`of()` / `copyOf()`)
1. **Disallow Nulls**: Passing `null` immediately throws `NullPointerException`.
2. **Value-Based / Identity-Free**: Implementations may use identity-less value objects.
3. **Randomized Iteration Order for Sets and Maps**: To prevent developers from accidentally relying on order in sets/maps, `Set.of()` and `Map.of()` randomize iteration order per JVM run.
4. **Space-Efficient**: Zero wrapper overhead; specialized internal classes for 0, 1, and 2 elements.

---

## 3. How It Works Internally

### Specialized Internal Classes (`List0`, `List1`, `List2`, `ListN`)
Inside `java.util.ImmutableCollections`:
- For 0 elements: A shared singleton constant `EMPTY_LIST`. Zero allocation!
- For 1 element: `List12<E>` stores element directly in a field `E e0`. No array is allocated!
- For 2 elements: `List12<E>` stores elements in two fields `E e0, e1`. No array allocated!
- For $\ge 3$ elements: `ListN<E>` stores elements in a compact, tightly-sized `E[] elements` array.

```
Memory Layout Comparison for 2 Elements:
Collections.unmodifiableList(new ArrayList<>(List.of("A", "B"))):
  1. ArrayList object (24 bytes)
  2. Backing Object[10] array (56 bytes)
  3. UnmodifiableList wrapper object (16 bytes)
  Total: ~96 bytes overhead!

List.of("A", "B") (List12 instance):
  1. List12 object with two reference fields (24 bytes total)
  Total: 24 bytes (4x memory savings!)
```

### `copyOf` Optimization Short-Circuit
When `List.copyOf(collection)` is invoked:
```java
static <E> List<E> copyOf(Collection<? extends E> coll) {
    if (coll instanceof ImmutableCollections.AbstractImmutableList) {
        return (List<E>) coll; // ZERO COPY! Returns same instance!
    }
    return (List<E>) List.of(coll.toArray());
}
```
If the source collection is already an immutable collection created by `List.of()` or `List.copyOf()`, it returns the original instance directly in $O(1)$ time with zero allocations.

---

## 4. Syntax & Usage

### 1. Creation via Modern Factories
```java
import java.util.List;
import java.util.Map;
import java.util.Set;

public class ImmutableFactoriesDemo {
    public static void main(String[] args) {
        // Immutable List
        List<String> list = List.of("alpha", "beta", "gamma");

        // Immutable Set (Duplicate elements throw IllegalArgumentException!)
        Set<String> set = Set.of("apple", "banana", "cherry");

        // Immutable Map (Up to 10 entries inline)
        Map<String, Integer> map = Map.of("k1", 100, "k2", 200);

        // Defensive copy
        List<String> copy = List.copyOf(list);
    }
}
```

### 2. Defending Entity Encapsulation
```java
import java.util.ArrayList;
import java.util.List;

public class DomainEntity {
    private final List<String> tags;

    public DomainEntity(List<String> tags) {
        // Create an immutable copy during construction
        this.tags = List.copyOf(tags);
    }

    public List<String> getTags() {
        return tags; // Safe to return directly; cannot be modified by caller!
    }
}
```

---

## 5. Practical Examples

### Example 1: The Leaky Unmodifiable View Trap (Basic to Intermediate)
```java
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public class LeakyViewDemo {
    public static void main(String[] args) {
        List<String> mutableList = new ArrayList<>();
        mutableList.add("Initial");

        // Create unmodifiable view
        List<String> view = Collections.unmodifiableList(mutableList);
        System.out.println("View initial: " + view); // [Initial]

        // Backing list is modified elsewhere
        mutableList.add("Injected");

        // The "unmodifiable" view has changed!
        System.out.println("View after backing change: " + view); // [Initial, Injected]
    }
}
```

### Example 2: Dynamic Map Creation with `Map.ofEntries` (Real-World)
```java
import java.util.Map;

public class ConfigRegistry {
    public static final Map<String, String> DEFAULTS = Map.ofEntries(
        Map.entry("server.port", "8080"),
        Map.entry("server.host", "0.0.0.0"),
        Map.entry("db.pool.max", "20"),
        Map.entry("auth.provider", "OIDC"),
        Map.entry("cache.ttl.seconds", "300")
    );

    public static void main(String[] args) {
        System.out.println("Port: " + DEFAULTS.get("server.port"));
    }
}
```

### Example 3: Edge Case - Randomized Set Iteration Across JVM Invocations
```java
import java.util.Set;

public class SetRandomizationDemo {
    public static void main(String[] args) {
        // Set.of() intentionally scrambles its iteration order across JVM runs!
        Set<String> set = Set.of("A", "B", "C", "D", "E");
        for (String s : set) {
            System.out.print(s + " ");
        }
        System.out.println();
    }
}
```

### Example 4: Failure Case - Passing Duplicate Keys to `Set.of()` or `Map.of()`
```java
import java.util.Set;

public class DuplicateLiteralFailure {
    public static void main(String[] args) {
        try {
            // Throws IllegalArgumentException: duplicate element: apple
            Set<String> bad = Set.of("apple", "orange", "apple");
        } catch (IllegalArgumentException e) {
            System.out.println("Caught expected: " + e.getMessage());
        }
    }
}
```

---

## 6. Best Practices

1. **Prefer `List.of()` / `Set.of()` / `Map.of()` Over `Collections.unmodifiable*`**: Truly immutable, compact, and immune to leaks from underlying mutable references.
2. **Use `copyOf()` for Defensive Copies in Constructors**: `this.items = List.copyOf(items)` guarantees that external callers cannot mutate your internal state.
3. **Never Expect Deterministic Order in `Set.of()` / `Map.of()`**: Iteration order is intentionally randomized; if order matters, use `SequencedCollection` or sorted collections.
4. **Cleanse `null` Values Before Creating Immutable Collections**: `List.of()` and `Set.of()` reject `null` with `NullPointerException`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why Wrong | Fix |
|---|---|---|
| Relying on `Collections.unmodifiableList` for immutability | Mutating the underlying backing list modifies the view. | Use `List.copyOf(list)`. |
| Passing `null` to `List.of()` or `Set.of()` | Throws `NullPointerException` immediately. | Filter out nulls or use `Arrays.asList()`. |
| Passing duplicate elements to `Set.of()` | Throws `IllegalArgumentException` immediately. | Deduplicate items before creating set. |
| Assuming `Set.of()` maintains insertion order | Iteration order is randomized across JVM executions. | Use `LinkedHashSet` if insertion order is needed. |
| Calling mutation methods on immutable collections | Throws `UnsupportedOperationException`. | Create a mutable copy: `new ArrayList<>(immutableList)`. |
| Casting immutable collection to `ArrayList` | Fails at runtime with `ClassCastException`. | Program to `List` interface. |

---

## 8. Performance & Security Notes

### Performance
- **Compact Memory Layout**: `List.of(e1, e2)` allocates a `List12` object storing references in two direct fields, avoiding array allocation entirely.
- **Zero-Allocation Copy**: Calling `List.copyOf()` on an already immutable list returns the instance itself in $O(1)$ time with zero bytes allocated.

### Security
- **Defense Against Aliasing Attacks**: Unmodifiable views suffer from aliasing attacks where a malicious caller retains a reference to the mutable backing collection. `List.copyOf()` creates true isolation.

---

## 9. Comparison / When to Use What

| Feature | `Collections.unmodifiableList()` | `List.of(...)` | `List.copyOf(...)` |
|---|---|---|---|
| **Underlying Nature** | View over backing list | Truly immutable standalone | Truly immutable standalone |
| **Backing Mutation Visible?** | Yes | No (No backing list) | No (Defensive copy) |
| **Null Elements Allowed?** | Yes (if backing list allows) | Strictly Forbidden (NPE) | Strictly Forbidden (NPE) |
| **Duplicate Checking** | N/A | Forbidden in Set/Map | Deduplicated in Set/Map |
| **Memory Overhead** | Wrapper object + backing list | Minimal (field-backed for $\le 2$) | Minimal |

---

## 10. Interview Questions

<details>
<summary>1. What is the difference between `Collections.unmodifiableList()` and `List.of()`?</summary>
`Collections.unmodifiableList()` creates a wrapper view around an existing mutable list; if the backing list is modified, the view changes. `List.of()` creates a truly immutable collection with no backing list, rejects `null` elements, and uses an ultra-compact memory representation.
</details>

<details>
<summary>2. Why do `Set.of()` and `Map.of()` have randomized iteration orders?</summary>
To prevent developers from accidentally writing code that depends on the iteration order of sets and maps. If iteration order were deterministic, code might pass tests locally but fail in production or across JVM versions.
</details>

<details>
<summary>3. What happens if you pass an already immutable list to `List.copyOf()`?</summary>
`List.copyOf()` checks `instanceof AbstractImmutableList`. If true, it returns the input list directly without creating a new copy ($O(1)$ time, zero allocations).
</details>

<details>
<summary>4. Why does `List.of()` reject `null` elements?</summary>
Java designers found that allowing `null` in collections was a frequent source of bugs. By rejecting `null`, modern immutable collections avoid ambiguous semantics (e.g., in `Map.get()`, distinguishing between missing key and mapped to null).
</details>

<details>
<summary>5. How does `List.of("a", "b")` optimize memory over `ArrayList`?</summary>
It instantiates an internal `List12` class that stores the two references directly in fields (`e0` and `e1`), eliminating the backing array object and capacity buffers.
</details>

<details>
<summary>6. How can you create an immutable map with more than 10 entries?</summary>
Use `Map.ofEntries(Map.entry(k1, v1), Map.entry(k2, v2), ...)`.
</details>

<details>
<summary>7. What exception is thrown if duplicate keys are passed to `Set.of()`?</summary>
`IllegalArgumentException`.
</details>

<details>
<summary>8. Can you modify an immutable list using reflection?</summary>
In modern Java (Java 9+ modules), internal fields of `ImmutableCollections` are strongly encapsulated. Modifying them via reflection triggers `InaccessibleObjectException` unless explicit JVM `--add-opens` flags are supplied.
</details>

---

## 11. Practice Exercises

### Easy
1. Create an immutable list of the first 5 prime numbers using `List.of()`.
2. Verify that attempting to add an element to an immutable list throws `UnsupportedOperationException`.
3. Create an immutable map of country codes to names using `Map.of()`.

### Medium
4. Write a domain class `UserProfile` with a defensively copied immutable `Set<String>` of roles.
5. Demonstrate that `List.copyOf()` on an unmodifiable list created by `List.of()` returns the identical instance (`==`).
6. Build a utility that takes any arbitrary collection, removes nulls, and returns an immutable `Set`.

### Hard
7. Implement a custom immutable persistent vector (like Clojure's Bitmapped Vector Trie) supporting $O(\log_{32} N)$ structural sharing updates.
8. Design an immutable multi-map `ImmutableMultiMap<K, V>` using nested `Map.of` structures.
9. Measure memory consumption differences between 100,000 `Collections.unmodifiableList` instances vs 100,000 `List.of` instances using JOL.

<details>
<summary>Solutions</summary>

```java
// Easy 2: Immutability test
List<Integer> list = List.of(2, 3, 5, 7, 11);
try {
    list.add(13);
} catch (UnsupportedOperationException e) {
    System.out.println("Modification blocked successfully!");
}

// Medium 5: copyOf identity test
List<String> original = List.of("A", "B");
List<String> copy = List.copyOf(original);
System.out.println("Same instance? " + (original == copy)); // true!
```
</details>

---


### Deep-Dive: Bytecode Execution of Immutable Factories
When calling `List.of("alpha", "beta")`, the compiler links to:
```
invokestatic #7 // Method java/util/List.of:(Ljava/lang/Object;Ljava/lang/Object;)Ljava/util/List;
```
Inside OpenJDK, this routes directly to:
```java
static <E> List<E> of(E e1, E e2) {
    return new ImmutableCollections.List12<>(e1, e2);
}
```
Because `List12` is instantiated directly with the two arguments, no intermediate varargs array (`E[]`) is created on the heap, eliminating temporary array allocation garbage!

### Detailed Exercise Solutions (All 9 Problems)
<details>
<summary>View Complete Solutions for All Exercises</summary>

```java
// Easy 1: Primes list
List<Integer> primes = List.of(2, 3, 5, 7, 11);

// Easy 2: Immutability test
try {
    primes.add(13);
} catch (UnsupportedOperationException e) {
    System.out.println("Blocked modification!");
}

// Easy 3: Country codes
Map<String, String> countries = Map.of("US", "United States", "DE", "Germany", "IN", "India");

// Medium 4: Defensively copied UserProfile
public final class UserProfile {
    private final String username;
    private final Set<String> roles;
    public UserProfile(String username, Set<String> roles) {
        this.username = username;
        this.roles = Set.copyOf(roles);
    }
    public String getUsername() { return username; }
    public Set<String> getRoles() { return roles; }
}

// Medium 5: Identity verification
List<String> original = List.of("X", "Y");
List<String> copy = List.copyOf(original);
boolean same = (original == copy); // true!

// Medium 6: Cleanse nulls and return immutable set
public static <T> Set<T> cleanAndMakeImmutable(Collection<T> input) {
    Set<T> set = new HashSet<>();
    for (T item : input) {
        if (item != null) set.add(item);
    }
    return Set.copyOf(set);
}

// Hard 7: Persistent Vector Stub (Structural Sharing)
public class PersistentNode<E> {
    final Object[] elements;
    public PersistentNode(Object[] elements) { this.elements = elements; }
    public PersistentNode<E> update(int idx, E val) {
        Object[] copy = elements.clone();
        copy[idx] = val;
        return new PersistentNode<>(copy);
    }
}

// Hard 8: Immutable MultiMap
public class ImmutableMultiMap<K, V> {
    private final Map<K, Set<V>> map;
    public ImmutableMultiMap(Map<K, Set<V>> input) {
        Map<K, Set<V>> temp = new HashMap<>();
        for (var e : input.entrySet()) {
            temp.put(e.getKey(), Set.copyOf(e.getValue()));
        }
        this.map = Map.copyOf(temp);
    }
    public Set<V> get(K key) { return map.getOrDefault(key, Set.of()); }
}

// Hard 9: Memory consumption profiling
// JOL shows: List.of(1, 2) = 24 bytes vs Collections.unmodifiableList(ArrayList) = 96 bytes.
```
</details>


## 12. Summary / Cheat Sheet

- **Unmodifiable View**: `Collections.unmodifiableList(list)` (view; leaks backing changes).
- **True Immutable**: `List.of()`, `Set.of()`, `Map.of()` (Java 9+; standalone).
- **Defensive Copying**: Use `List.copyOf(coll)` in constructors.
- **Zero Nulls**: All `of()` / `copyOf()` factories reject `null` (`NullPointerException`).
- **Randomized Order**: `Set.of()` and `Map.of()` scramble iteration order per JVM run.
- **Short-Circuit**: `List.copyOf(immutableList)` returns the input directly ($O(1)$).

---

## 13. Further Reading
- JEP 269: *Convenience Factory Methods for Collections* (OpenJDK).
- Bloch, Joshua. *Effective Java (3rd Edition)*: Item 17 (Minimize mutability) and Item 50 (Make defensive copies when needed).
- Oracle Java Documentation: *Immutable Collection Factory Methods*.
