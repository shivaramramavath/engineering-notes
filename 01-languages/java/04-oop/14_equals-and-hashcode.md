# The Equals and HashCode Contract in Java

> An authoritative guide to equivalence relations, the hashcode contract, hash table bucket mechanics, and avoiding common collection corruption bugs.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [00_classes-and-objects.md](./00_classes-and-objects.md), [13_object-class.md](./13_object-class.md) |
| **Time to Read** | 25 minutes |
| **Status** | Active |
| **Java Version** | Java 8+ (Java 25 LTS verified) |

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
9. [Comparison: Manual vs Objects.hash vs Records](#9-comparison-manual-vs-objectshash-vs-records)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

In Java, determining whether two objects represent the "same" logical entity is governed by the contract of `equals(Object)` and `hashCode()`.

By default, `java.lang.Object` implements:
- `equals()` as **reference equality** (`this == obj`): two references are equal only if they point to the exact same memory address.
- `hashCode()` as a native memory-derived 31-bit integer.

For value objects (such as `String`, `Integer`, `LocalDate`, or a domain `Customer`), callers expect **logical equivalence** based on field values. Whenever you override `equals()`, you **must** also override `hashCode()`; violating this fundamental rule breaks every hash-based collection in the Java ecosystem (`HashMap`, `HashSet`, `ConcurrentHashMap`).

---

## 2. Core Concepts

### 2.1 The Equivalence Relation Contract (JLS & Effective Java Item 10)
An `equals()` method must implement an equivalence relation. For any non-null reference values `x`, `y`, `z`:
1. **Reflexive:** `x.equals(x)` must return `true`.
2. **Symmetric:** `x.equals(y)` must return `true` if and only if `y.equals(x)` returns `true`.
3. **Transitive:** If `x.equals(y)` returns `true` and `y.equals(z)` returns `true`, then `x.equals(z)` must return `true`.
4. **Consistent:** Multiple invocations of `x.equals(y)` consistently return `true` or consistently return `false`, provided no information used in equals comparisons is modified.
5. **Non-nullity:** For any non-null reference `x`, `x.equals(null)` must return `false`.

### 2.2 The HashCode Contract (Effective Java Item 11)
1. **Consistency:** Whenever invoked on the same object more than once during an application execution, `hashCode()` must consistently return the same integer, provided no equals fields are modified.
2. **Equal Objects Must Have Equal Hash Codes:**
   $$\text{If } a.\text{equals}(b) == \text{true} \implies a.\text{hashCode}() == b.\text{hashCode}()$$
3. **Unequal Objects May Share Hash Codes:** If $a.\text{equals}(b) == \text{false}$, their hash codes do *not* have to be distinct (although distinct hash codes improve hash table performance).

---

## 3. How It Works Internally

### 3.1 HashMap Bucket Lookup Pipeline
When you execute `map.put(key, value)` or `map.get(key)`:
```
1. Compute 32-bit hash: h = key.hashCode()
2. Spread hash bits:    h = h ^ (h >>> 16)
3. Calculate bucket:    index = (n - 1) & h  (where n is bucket array length)
4. Scan bucket chain:
   - First compare cached hash: if (node.hash == h)
   - Then compare identity:     if (node.key == key || key.equals(node.key))
```
```
Why Violating the Contract Breaks HashMap:
Object A and Object B have identical fields: a.equals(b) is TRUE.
BUT hashCode() is NOT overridden:
- a.hashCode() = 129048 (Maps to Bucket 4)
- b.hashCode() = 894312 (Maps to Bucket 11)

Result:
map.put(a, "data");
map.get(b); // Looks in Bucket 11 -> RETURNS NULL! Object is lost!
```

### 3.2 Why the Number 31 in Hashing?
The standard Java hashcode formula computes:
$$h = 31 \times h + \text{field.hashCode}()$$
Why 31?
1. **It is an odd prime:** Multiplying by an even number shifts bits left (multiplying by 2 shifts by 1), losing entropy from the high bits on overflow.
2. **Compiler optimization:** Modern compilers replace $31 \times i$ with a bit shift and subtraction:
   $$31 \times i = (i \ll 5) - i$$
   Executing in a single CPU clock cycle!

---

## 4. Syntax & Usage

### 4.1 Canonical Recipe for `equals()` and `hashCode()`
```java
import java.util.Objects;

public final class CustomerId {
    private final String systemPrefix;
    private final long sequenceNumber;

    public CustomerId(String systemPrefix, long sequenceNumber) {
        this.systemPrefix = systemPrefix;
        this.sequenceNumber = sequenceNumber;
    }

    @Override
    public boolean equals(Object o) {
        // Step 1: Check reference identity (Fast path)
        if (this == o) return true;

        // Step 2: Check for null and exact class matching
        if (o == null || getClass() != o.getClass()) return false;

        // Step 3: Cast to target type
        CustomerId that = (CustomerId) o;

        // Step 4: Compare significant fields
        return sequenceNumber == that.sequenceNumber &&
               Objects.equals(systemPrefix, that.systemPrefix);
    }

    @Override
    public int hashCode() {
        return Objects.hash(systemPrefix, sequenceNumber);
    }
}
```

---

## 5. Practical Examples

### Example 1: High-Performance Manual `hashCode()` Calculation
```java
public final class GeoCoordinate {
    private final double latitude;
    private final double longitude;

    public GeoCoordinate(double latitude, double longitude) {
        this.latitude = latitude;
        this.longitude = longitude;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (!(o instanceof GeoCoordinate that)) return false;
        // Use Double.compare to handle NaN and -0.0 vs +0.0 properly
        return Double.compare(that.latitude, latitude) == 0 &&
               Double.compare(that.longitude, longitude) == 0;
    }

    @Override
    public int hashCode() {
        // High-performance manual calculation avoids Objects.hash() array allocation
        int result = 1;
        result = 31 * result + Double.hashCode(latitude);
        result = 31 * result + Double.hashCode(longitude);
        return result;
    }

    public static void main(String[] args) {
        GeoCoordinate c1 = new GeoCoordinate(37.7749, -122.4194);
        GeoCoordinate c2 = new GeoCoordinate(37.7749, -122.4194);
        System.out.println("c1.equals(c2): " + c1.equals(c2));
        System.out.println("c1.hashCode() == c2.hashCode(): " + (c1.hashCode() == c2.hashCode()));
    }
}
```
**Output:**
```text
c1.equals(c2): true
c1.hashCode() == c2.hashCode(): true
```

### Example 2: The Inheritance Symmetry Trap
```java
class Point {
    private final int x, y;
    public Point(int x, int y) { this.x = x; this.y = y; }

    @Override
    public boolean equals(Object o) {
        if (!(o instanceof Point p)) return false;
        return p.x == x && p.y == y;
    }
}

class ColorPoint extends Point {
    private final String color;
    public ColorPoint(int x, int y, String color) {
        super(x, y);
        this.color = color;
    }

    // BROKEN: Violates Symmetry!
    @Override
    public boolean equals(Object o) {
        if (!(o instanceof ColorPoint cp)) return false;
        return super.equals(o) && cp.color.equals(color);
    }
}

public class EqualsSymmetryTrap {
    public static void main(String[] args) {
        Point p = new Point(1, 2);
        ColorPoint cp = new ColorPoint(1, 2, "RED");

        System.out.println("p.equals(cp):  " + p.equals(cp));  // true! (Point ignores color)
        System.out.println("cp.equals(p):  " + cp.equals(p));  // false! (ColorPoint demands ColorPoint)
        // VIOLATES SYMMETRY! p.equals(cp) != cp.equals(p)!
    }
}
```
**Output:**
```text
p.equals(cp):  true
cp.equals(p):  false
```
*Resolution: Favor composition over inheritance (Item 18). Add `Point` as a component of `ColorPoint`.*

### Example 3: Edge Case – Mutating Hash Keys Inside a HashSet
```java
import java.util.HashSet;
import java.util.Set;

public class MutableKeyCorruptionDemo {
    static class MutableAccount {
        String id;
        MutableAccount(String id) { this.id = id; }
        @Override
        public boolean equals(Object o) {
            return o instanceof MutableAccount other && this.id.equals(other.id);
        }
        @Override
        public int hashCode() { return id.hashCode(); }
    }

    public static void main(String[] args) {
        Set<MutableAccount> accounts = new HashSet<>();
        MutableAccount acc = new MutableAccount("ACC_100");
        accounts.add(acc);

        System.out.println("Contains before mutation: " + accounts.contains(acc)); // true

        // FATAL MISTAKE: Mutating field used in hashCode!
        acc.id = "ACC_999";

        // Now hashCode has changed, but the object remains in the OLD bucket!
        System.out.println("Contains after mutation:  " + accounts.contains(acc)); // false!
        System.out.println("Set size:                  " + accounts.size());         // 1 (Zombie element!)
    }
}
```
**Output:**
```text
Contains before mutation: true
Contains after mutation:  false
Set size:                  1
```

### Example 4: Failure Case – Overloading Instead of Overriding `equals`
```java
public class OverloadEqualsTrap {
    static class BrokenItem {
        private final int id;
        BrokenItem(int id) { this.id = id; }

        // FATAL FLAW: Parameter is BrokenItem, NOT Object!
        // This is an OVERLOAD, not an override!
        public boolean equals(BrokenItem other) {
            return other != null && this.id == other.id;
        }
    }

    public static void main(String[] args) {
        BrokenItem a = new BrokenItem(5);
        BrokenItem b = new BrokenItem(5);
        Object objB = b;

        System.out.println("Direct call: " + a.equals(b));       // true (calls overload)
        System.out.println("Via Object:  " + a.equals(objB));    // false! (calls Object.equals ==)
    }
}
```
**Output:**
```text
Direct call: true
Via Object:  false
```

### Example 5: Optimized / Idiomatic – Modern Java Records Handle Both Automatically
```java
// Java 16+ Records generate fully contract-compliant equals() and hashCode() automatically!
public record OrderKey(String tenantId, long orderNumber) {}

public class RecordEqualsDemo {
    public static void main(String[] args) {
        OrderKey k1 = new OrderKey("US_EAST", 1024L);
        OrderKey k2 = new OrderKey("US_EAST", 1024L);

        System.out.println("k1.equals(k2): " + k1.equals(k2)); // true
        System.out.println("Hash match:    " + (k1.hashCode() == k2.hashCode())); // true
    }
}
```
**Output:**
```text
k1.equals(k2): true
Hash match:    true
```

---

## 6. Best Practices

1. **Always Override `hashCode` When Overriding `equals`:** Never violate the contract. Equal objects must produce identical hash codes.
2. **Never Use Mutable Fields in `hashCode()` or `equals()`:** If an object's fields change while it is stored in a hash map, its hash code changes and it becomes unretrievable.
3. **Use `Objects.equals(a, b)`:** Handles null checks automatically without throwing `NullPointerException`.
4. **Compare Floating-Point with `Double.compare()`:** Never use `==` for `float` or `double` in equals; `Double.compare()` properly handles `Double.NaN` and `-0.0` vs `+0.0`.
5. **Annotate with `@Override`:** Ensures you are overriding `boolean equals(Object o)` and not accidentally creating an overload `boolean equals(MyClass o)`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Is Wrong | Correct Idiom |
| :--- | :--- | :--- |
| Overriding `equals` but not `hashCode` | Equal objects map to different hash buckets; `HashMap.get()` returns `null`. | Always override both in tandem. |
| Parameter is `MyType` instead of `Object` | Overloads `equals`; collections call `Object.equals(Object)` and bypass your logic. | Signature MUST be `equals(Object o)`. |
| Modifying keys stored in HashSet/Map | Modifies hash code while key resides in old bucket; element becomes permanently lost. | Use strictly immutable objects as keys. |
| `instanceof` with inheritance | Violates symmetry if subclass overrides equals; violates transitivity if subtyping adds fields. | Use composition or exact `getClass()` matching. |
| Comparing array contents with `.equals()` | Array `.equals()` performs reference identity (`==`). | Use `Arrays.equals(arr1, arr2)`. |

---

## 8. Performance & Security Notes

### 8.1 Hash Collision Attacks (HashDoS)
If an attacker knows an application's hash function, they can craft thousands of request keys that produce identical hash codes, degrading a standard hash table from $O(1)$ to $O(N)$ and causing 100% CPU starvation.
- In Java 8+, `HashMap` mitigates this by transforming long collision bucket chains (treeifying threshold = 8) from linked lists into balanced Red-Black trees (`TreeNode`), ensuring worst-case lookup performance is bounded to $O(\log N)$ provided keys implement `Comparable`.

---

## 9. Comparison: Manual vs Objects.hash vs Records

| Approach | Readability | Performance | GC Allocation | Handles Null? |
| :--- | :--- | :--- | :--- | :--- |
| **Manual Calculation** | Verbose | Fastest | Zero allocation | Manual guards |
| **`Objects.hash(...)`** | High | Slightly slower | Allocates `Object[]` varargs array | ✅ Yes |
| **Java `record`** | Native / Zero lines | Optimal | Zero allocation | ✅ Yes |

---

## 10. Interview Questions

<details>
<summary><strong>1. What happens if you override equals() but forget to override hashCode()?</strong></summary>

The class inherits `Object.hashCode()`, which assigns hash codes based on memory addresses. Two logically equal objects (`a.equals(b)` is true) will produce completely different hash codes. When stored in a `HashMap` or `HashSet`, they will map to different buckets. Calling `map.get(new Key(...))` will look in the wrong bucket and return `null`, even though an equal key exists in the map.
</details>

<details>
<summary><strong>2. Can two unequal objects produce the same hashCode()?</strong></summary>

Yes. This is called a **hash collision**. The `hashCode()` contract only requires that equal objects have equal hash codes. Because there are $2^{32}$ possible integers and an infinite number of possible objects, collisions are inevitable (Pigeonhole Principle). Hash tables handle collisions using bucket chains or red-black trees.
</details>

<details>
<summary><strong>3. Why does equals() use getClass() != o.getClass() vs !(o instanceof MyClass)?</strong></summary>

- `instanceof` allows comparing subclasses to superclasses, but can violate the symmetry or transitivity contracts if subclasses add new value fields (Effective Java Item 10).
- `getClass() == o.getClass()` guarantees that objects are only equal if they share the exact same concrete class, preserving symmetry and transitivity across inheritance hierarchies, although it disallows subtyping equivalence.
</details>

<details>
<summary><strong>4. Why should fields used in equals() and hashCode() be immutable?</strong></summary>

If a field used in `hashCode()` changes while the object is a key in a `HashMap`, its hashcode changes. However, the object remains in the bucket determined by its original hashcode. Calling `map.get(key)` will calculate the new hashcode, search a different bucket, and fail to find the object, creating a memory leak and corrupted state.
</details>

<details>
<summary><strong>5. How does Java 8 improve HashMap performance during heavy hash collisions?</strong></summary>

In Java 7 and earlier, all collisions in a bucket were stored in a linked list with $O(N)$ search time. In Java 8+, once a bucket reaches 8 elements (`TREEIFY_THRESHOLD`) and the table has at least 64 buckets, `HashMap` converts the linked list into a balanced Red-Black tree (`TreeNode`), improving worst-case search, insertion, and deletion complexity from $O(N)$ to $O(\log N)$.
</details>

<details>
<summary><strong>6. Why is 31 used as the multiplier in the hashCode algorithm?</strong></summary>

31 is an odd prime number. Multiplying by an even number shifts bits and discards information on integer overflow. Furthermore, $31 \times i = (i \ll 5) - i$, which allows modern JIT compilers to replace the multiplication with a fast bit shift and subtraction on the CPU.
</details>

<details>
<summary><strong>7. How should float and double fields be compared in equals()?</strong></summary>

Use `Float.compare(f1, f2) == 0` and `Double.compare(d1, d2) == 0`. Using `==` fails for `NaN` (because `NaN == NaN` is false) and incorrectly equates `+0.0` and `-0.0` (which have different bit layouts in IEEE 754).
</details>

<details>
<summary><strong>8. Does a record automatically implement equals and hashCode?</strong></summary>

Yes. The Java compiler automatically generates contract-compliant `equals()` and `hashCode()` implementations for all Java records based on all declared record components.
</details>

---

## 11. Practice Exercises

### Easy
1. **User Identity:** Implement `equals()` and `hashCode()` for a `User` class with `id` and `email`.
2. **Book Record Equality:** Verify `equals()` behavior on a Java `record Book(String isbn, String title)`.
3. **Null-Safe Comparison:** Test `Objects.equals(a, b)` across permutations of null and non-null values.

### Medium
4. **Complex Entity with Arrays:** Implement `equals()` and `hashCode()` for an object containing primitive fields, String fields, and a `byte[]` payload using `Arrays.equals()`.
5. **Cached HashCode for Immutable Objects:** Implement an immutable `TransactionKey` that caches its `hashCode` after the first calculation.
6. **Symmetry Violation Test:** Write a unit test asserting that an inheritance hierarchy violates the symmetry property of `equals()`.

### Hard
7. **Hash Collision Generator:** Write a program generating two distinct strings with identical Java `hashCode()` values and inspect their placement in a `HashMap`.
8. **HashMap Treeification Benchmark:** Measure lookup latency as bucket size expands from 1 to 1,000 elements with and without `Comparable` keys.
9. **Zero-Allocation Hash Function:** Implement a custom 64-bit MurmurHash3 or xxHash function in Java for high-speed network packet deduplication.

<details>
<summary><strong>Exercise Solutions</strong></summary>

```java
// Solution to Exercise 1: User Identity
public final class User {
    private final long id;
    private final String email;

    public User(long id, String email) {
        this.id = id;
        this.email = email;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (!(o instanceof User user)) return false;
        return id == user.id && java.util.Objects.equals(email, user.email);
    }

    @Override
    public int hashCode() {
        return java.util.Objects.hash(id, email);
    }
}
```
</details>

---

## 12. Summary & Cheat Sheet

```
┌────────────────────────────────────────────────────────────────────────┐
│                   EQUALS & HASHCODE CHEAT SHEET                        │
├────────────────────────────────────────────────────────────────────────┤
│ Contract:           If a.equals(b) == true -> a.hashCode() == b.hash() │
│ Cardinal Rule:      Always override both in tandem                     │
│ Method Signature:   public boolean equals(Object o) (NEVER MyClass)    │
│ Standard Recipe:    1. if (this == o) return true                      │
│                     2. if (!(o instanceof Target t)) return false      │
│                     3. Compare fields with Objects.equals / ==         │
│ Float/Double:       Use Float.compare() and Double.compare()           │
│ Arrays:             Use Arrays.equals() and Arrays.hashCode()          │
│ Collections:        Keys MUST be immutable to prevent loss in buckets  │
│ Java 16+ Records:   Auto-generate contract-compliant equals/hashCode   │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 13. Further Reading
- Java Language Specification (JLS) Section 4.3.2: The Class `Object`
- *Effective Java (3rd Edition)*, Joshua Bloch, Item 10: "Obey the general contract when overriding equals".
- *Effective Java (3rd Edition)*, Joshua Bloch, Item 11: "Always override hashCode when you override equals".
