# Pass-by-Value Semantics and Memory Evaluation in Java

> Java is strictly and unconditionally a pass-by-value language: calling a method copies primitive values directly, and for reference types, copies the memory address pointer by value into the callee's stack frame slot.

| Metadata | Value |
| :--- | :--- |
| **Level** | Beginner to Intermediate |
| **Prerequisites** | [00_methods.md](file:///E:/Learning/engineering-notes/01-languages/java/02-methods/00_methods.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Java LTS Baseline** | Java 25 LTS (JVM Evaluation Strategy) |

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

### Problem Statement
When a method receives an argument, how are mutations handled? If a method reassigns a parameter variable or modifies an internal field, does that change reflect in the caller's scope? Confusion over parameter passing semantics is one of the most common sources of software defects and technical interview failures in computer engineering.

### Historical Context & Evaluation Strategies
Across programming languages, different parameter evaluation strategies exist:
- **Pass-by-Reference (C++, Pascal):** The callee receives an alias to the caller's actual variable memory location. Reassigning the parameter variable replaces the caller's variable directly.
- **Pass-by-Pointer (C):** Memory addresses are passed explicitly via pointer syntax (`&x`, `*x`).
- **Pass-by-Value (Java, C):** The caller evaluates the argument expression, and an independent **copy** of the evaluated value is pushed onto the callee's stack frame.
- **Java's Canonical Axiom:** Java has **zero** pass-by-reference mechanics. Everything in Java is passed strictly by value. When passing an object, Java passes a copy of the reference pointer by value.

### Real-World Relevance
Understanding pass-by-value semantics enables engineers to reason accurately about mutability, prevent accidental state pollution across microservice layers, and design defensive APIs that protect domain invariants.

---

## 2. Core Concepts

### The Two Modes of Pass-by-Value

```
+---------------------------------------------------------------------------------+
|                           1. Primitive Arguments by Value                       |
|                                                                                 |
|   Caller Stack Frame:                Callee Stack Frame:                        |
|   [ int x = 42 ] ───── Copy Bits ──► [ int n = 42 ]                             |
|                                                                                 |
|   • Modifying 'n' (e.g. n = 99) alters Callee slot ONLY.                       |
|   • Caller's 'x' remains 42 unconditionally.                                    |
+---------------------------------------------------------------------------------+
|                           2. Reference Arguments by Value                       |
|                                                                                 |
|   Caller Stack Frame:                Callee Stack Frame:                        |
|   [ p = 0x7FFF0040 ] ── Copy Ptr ──► [ q = 0x7FFF0040 ]                         |
|           │                                  │                                  |
|           └──────────────────┬───────────────┘                                  |
|                              ▼                                                  |
|                     [ Heap Object: Person ]                                     |
|                     ┌─────────────────────┐                                     |
|                     │ name = "Original"   │                                     |
|                     └─────────────────────┘                                     |
|                                                                                 |
|   • Action A (Mutation): q.name = "Mutated" -> Changes SHARED heap object!     |
|   • Action B (Reassignment): q = new Person("Other") -> Reassigns local pointer |
|     slot in Callee frame ONLY; Caller's 'p' still points to original object!   |
+---------------------------------------------------------------------------------+
```

### The Reference Passing Rules
1. **Mutating the Object:** If a method invokes mutating methods on an argument (e.g., `list.add("item")` or `person.setName("Bob")`), the changes are visible to the caller because both reference copies point to the exact same heap memory address.
2. **Reassigning the Reference:** If a method reassigns the parameter variable itself (e.g., `person = new Person(...)` or `person = null`), it merely reassigns the local reference slot in the callee's stack frame. The caller's variable continues pointing to the original object.

---

## 3. How It Works Internally

### Stack Frame Trace: Why `swap(a, b)` Fails in Java
Consider the classic attempt to write a swap function in Java:

```java
public static void swap(Person a, Person b) {
    Person temp = a;
    a = b;
    b = temp;
}
```

Let's trace the physical JVM stack frames during execution:

```
Step 1: In main(), p1 points to Person("Alice"), p2 points to Person("Bob")
  main() Frame:
  [Slot 1: p1 -> 0x1000 ("Alice")]
  [Slot 2: p2 -> 0x2000 ("Bob")]

Step 2: main() calls swap(p1, p2).
  The JVM copies the pointer values onto the operand stack and creates swap() Frame:
  swap() Frame:
  [Slot 0: a -> 0x1000 ("Alice")]  (Independent copy of pointer)
  [Slot 1: b -> 0x2000 ("Bob")]    (Independent copy of pointer)
  [Slot 2: temp -> null]

Step 3: swap() executes: temp = a; a = b; b = temp;
  swap() Frame now holds:
  [Slot 0: a -> 0x2000 ("Bob")]
  [Slot 1: b -> 0x1000 ("Alice")]
  [Slot 2: temp -> 0x1000 ("Alice")]

Step 4: swap() returns. Its stack frame is popped and destroyed.
  main() resumes:
  main() Frame STILL holds:
  [Slot 1: p1 -> 0x1000 ("Alice")]  <-- COMPLETELY UNCHANGED!
  [Slot 2: p2 -> 0x2000 ("Bob")]    <-- COMPLETELY UNCHANGED!
```

Because `a` and `b` were independent pointer slots on the `swap()` stack frame, swapping their values had zero impact on `Slot 1` and `Slot 2` in `main()`.

### Bytecode Execution Breakdown
When `main` invokes `modify(obj)`, `javac` emits:
```text
aload_1                  // Push reference address of 'obj' onto operand stack
invokestatic  #7         // Method modify:(LPerson;)V
```
The JVM copies the top of the operand stack (the 32-bit compressed reference pointer) directly into the first local variable slot of the `modify` stack frame. No object cloning occurs; only the 4-byte pointer is copied.

---

## 4. Syntax & Usage

### Demonstrating the Immutability of Primitives
```java
public class PrimitiveScope {
    public static void increment(int value) {
        value = value + 1; // Modifies local stack slot only
    }

    public static void main(String[] args) {
        int count = 10;
        increment(count);
        System.out.println("Count remains: " + count); // 10
    }
}
```

### Passing Reference Copies
```java
public class ReferenceScope {
    public static void clearList(List<String> list) {
        list.clear(); // Mutates the underlying heap object!
    }

    public static void replaceList(List<String> list) {
        list = new ArrayList<>(); // Reassigns local pointer; caller unaffected!
    }
}
```

---

## 5. Practical Examples

### Example 1: Reference Reassignment vs. State Mutation
- **Goal:** Conclusively verify that reassigning a reference variable inside a method does not affect the caller, while mutating an internal field does.
```java
public class PassByValueDemo {
    static class Person {
        String name;
        Person(String name) { this.name = name; }
    }

    public static void tryReassign(Person p) {
        p = new Person("Reassigned Name"); // Reassigns local pointer copy
    }

    public static void mutateState(Person p) {
        p.name = "Mutated Name"; // Modifies the shared heap object!
    }

    public static void main(String[] args) {
        Person p = new Person("Original Name");

        tryReassign(p);
        System.out.println("After tryReassign: " + p.name); // Still Original Name!

        mutateState(p);
        System.out.println("After mutateState: " + p.name); // Mutated Name!
    }
}
```
- **Console Output:**
```text
After tryReassign: Original Name
After mutateState: Mutated Name
```
- **Explanation:** `tryReassign` changes only where the parameter `p` points inside the method frame. `mutateState` accesses the shared heap object via the copied pointer and mutates its internal `name` field.

---

### Example 2: The Generic Swap Problem & The Container Solution
- **Goal:** Demonstrate how to achieve swapping in Java by mutating an enclosing container or array.
```java
import java.util.Arrays;

public class SwapSolutionDemo {
    // Array Container Solution:
    public static <T> void swapInArray(T[] array, int i, int j) {
        T temp = array[i];
        array[i] = array[j];
        array[j] = temp;
    }

    // Wrapper Class Solution:
    static class RefHolder<T> {
        T value;
        RefHolder(T value) { this.value = value; }
    }

    public static <T> void swapHolders(RefHolder<T> h1, RefHolder<T> h2) {
        T temp = h1.value;
        h1.value = h2.value;
        h2.value = temp;
    }

    public static void main(String[] args) {
        String[] names = { "Alice", "Bob" };
        swapInArray(names, 0, 1);
        System.out.println("Swapped Array: " + Arrays.toString(names)); // [Bob, Alice]

        RefHolder<String> r1 = new RefHolder<>("Alice");
        RefHolder<String> r2 = new RefHolder<>("Bob");
        swapHolders(r1, r2);
        System.out.println("Swapped Holders: r1=" + r1.value + ", r2=" + r2.value);
    }
}
```
- **Console Output:**
```text
Swapped Array: [Bob, Alice]
Swapped Holders: r1=Bob, r2=Alice
```
- **Explanation:** To alter what a caller sees, the callee must mutate an object that the caller continues to hold (such as an array or a wrapper container).

---

### Example 3: Edge Case — Mutating Primitive Wrapper Arguments
- **Goal:** Show that passing an `Integer` behaves like a primitive because wrapper classes are strictly immutable.
```java
public class BoxedPassByValueDemo {
    public static void tryToModify(Integer num) {
        num = num + 1; // Unboxes to int, adds 1, boxes to a NEW Integer object!
    }

    public static void main(String[] args) {
        Integer score = 100;
        tryToModify(score);
        System.out.println("Score remains: " + score); // 100
    }
}
```
- **Console Output:**
```text
Score remains: 100
```
- **Explanation:** Because `Integer` has no setter methods and its internal value is `final`, any operation like `num++` produces a brand new `Integer` object and reassigns the local parameter variable, leaving the caller's `score` unchanged.

---

### Example 4: Failure Case — Accidental Shared State Mutation
- **Goal:** Demonstrate an insidious enterprise bug where passing a collection into a helper method inadvertently mutates the caller's operational cache.
```java
import java.util.ArrayList;
import java.util.List;

public class AccidentalMutationBug {
    public static void filterActiveUsers(List<String> users) {
        // BUG: Mutating the caller's list directly instead of returning a new list!
        users.removeIf(u -> u.startsWith("guest_"));
    }

    public static void main(String[] args) {
        List<String> originalRegistry = new ArrayList<>(List.of("admin", "guest_1", "user_42"));

        filterActiveUsers(originalRegistry);

        System.out.println("Original list was polluted: " + originalRegistry);
    }
}
```
- **Console Output:**
```text
Original list was polluted: [admin, user_42]
```
- **Remedy:** Helper methods should treat collection arguments as read-only, returning a newly allocated filtered list (`users.stream().filter(...).toList()`) rather than mutating caller collections in place.

---

### Example 5: High-Performance / Modern Idiom — Immutable Java Records
- **Goal:** Use modern Java records to guarantee zero side-effects when passing domain models across methods.
```java
public class ImmutableRecordDemo {
    // Records are shallowly immutable; all components are private final
    record UserAccount(String id, double balance) {
        // Return a new record instance with updated state
        public UserAccount withDeposit(double amount) {
            return new UserAccount(this.id, this.balance + amount);
        }
    }

    public static void processTransaction(UserAccount account) {
        UserAccount updated = account.withDeposit(50.0);
        System.out.println("Inside method: " + updated);
    }

    public static void main(String[] args) {
        UserAccount user = new UserAccount("USR-101", 100.0);
        processTransaction(user);
        System.out.println("Caller state preserved: " + user);
    }
}
```
- **Console Output:**
```text
Inside method: UserAccount[id=USR-101, balance=150.0]
Caller state preserved: UserAccount[id=USR-101, balance=100.0]
```
- **Explanation:** By using immutable records and "wither" methods, callee methods can compute state transitions without any risk of polluting the caller's references.

---

## 6. Best Practices

1. **Mark Parameters `final` if You Want to Enforce Non-Reassignment:**
   ```java
   public void process(final Customer customer) {
       // customer = new Customer(); // COMPILE ERROR!
   }
   ```
2. **Treat Arguments as Read-Only:** Never mutate an incoming collection, map, or domain model unless the method's explicit, documented purpose is in-place mutation (e.g., `Collections.sort()`).
3. **Return New Collections from Transformation Methods:** Use Streams (`.toList()`) to return new collections rather than mutating input arguments.
4. **Use Records for Pure Data Transfer Objects:** Java records eliminate mutable setters, making pass-by-value completely safe from accidental side effects.
5. **Use Return Values to Communicate State Changes:** Rather than trying to update parameters via wrappers, return the updated state directly: `user = updateUser(user);`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Attempting to Write `swap(a, b)`** | Reassigning parameters modifies local stack slots only; caller references are unaffected. | Return an array, pair record, or swap elements inside a container. |
| **Assuming Setting `param = null` Clears Caller Object** | Only sets the local stack pointer to null; caller's variable still points to the heap object. | Manage lifecycle explicitly or return null to caller. |
| **Modifying Shared Collections In-Place** | Caller's collection is mutated, causing unexpected side effects in other subsystems. | Return a new filtered/mapped collection. |
| **Thinking Objects Are Passed by Reference** | Leads to incorrect assumptions that reassigning an object alters the caller's variable. | Remember: Java passes a copy of the reference pointer. |
| **Confusing Wrapper Immutability with Primitives** | Passing `Integer` or `String` feels like passing an object, but they cannot be mutated in place. | Treat wrappers and Strings as immutable values. |
| **Defensive Copy Omission in Constructors** | Storing incoming raw arrays directly into fields allows callers to mutate internal state later. | Clone the array: `this.data = data.clone();`. |

---

## 8. Performance & Security Notes

### Zero Overhead of Passing References
Passing an object reference in Java is exceptionally fast:
- Regardless of whether the heap object is a 20-byte `Point` or a 2-gigabyte in-memory database cache, only a **single 4-byte (or 8-byte) reference pointer** is copied onto the callee's stack frame.
- Passing objects never clones heap memory.

### Security Invariants: Defending Against Reference Hijacking
If a public method accepts a mutable object (like a `java.util.Date` or a mutable collection `List<String>`) and stores it directly into a private field, the caller retains the original reference and can mutate internal security-sensitive state at any time from another thread! Always create a **defensive copy** upon ingestion:
```java
public SecurityContext(List<Role> roles) {
    this.roles = List.copyOf(roles); // Creates an unmodifiable defensive copy!
}
```

---

## 9. Comparison / When to Use What

| Language / Evaluation Model | Syntax Example | How Arguments Are Passed | Can Callee Reassign Caller's Variable? |
| :--- | :--- | :--- | :---: |
| **Java (All Types)** | `void foo(Person p)` | **Pass-by-Value** (Value or Pointer copy) | **NO** |
| **C++ (Pass-by-Reference)** | `void foo(Person& p)` | Pass-by-Reference (True variable alias) | **YES** |
| **C++ / C (Pass-by-Pointer)** | `void foo(Person* p)` | Pass-by-Value (Pointer copied explicitly) | **NO** (unless `Person**`) |
| **Python** | `def foo(p):` | Pass-by-Object-Reference (Similar to Java) | **NO** |
| **C# (`ref` / `out`)** | `void foo(ref Person p)` | Pass-by-Reference (Explicit reference alias) | **YES** |

---

## 10. Interview Questions

<details>
<summary><strong>1. "Is Java pass-by-value or pass-by-reference?" What is the definitive answer?</strong></summary>

**Answer:**  
Java is **strictly pass-by-value**, always and without exception. When an argument is passed to a method, the JVM copies the value of the variable into the parameter slot. For primitive types, the value copied is the literal binary data (`int`, `double`, etc.). For reference types, the value copied is the **memory address (reference pointer)** pointing to the object on the heap, NOT the object itself.
</details>

<details>
<summary><strong>2. Why is it impossible to write a method in Java that swaps two object references?</strong></summary>

**Answer:**  
Because method arguments are copied by value into the callee's stack frame. In a hypothetical `swap(Person a, Person b)` method, the variables `a` and `b` are local to `swap`'s stack frame. Executing `Person temp = a; a = b; b = temp;` swaps only the local references inside `swap`. When the method returns and its frame is destroyed, the caller's original reference variables in `main` remain completely unchanged.
</details>

<details>
<summary><strong>3. If Java is pass-by-value, why does calling `list.add("hello")` modify the caller's list?</strong></summary>

**Answer:**  
Because the value that was passed to the method was a **copy of the reference pointer**. Both the caller's variable and the callee's parameter variable hold copies of the exact same memory address pointing to the same `ArrayList` instance on the heap. When `add()` is called, it dereferences that pointer and modifies the internal state of that shared heap object.
</details>

<details>
<summary><strong>4. What is the effect of setting a parameter to `null` inside a method (`p = null;`)?</strong></summary>

**Answer:**  
It has zero effect on the caller. Setting `p = null` simply writes `0x0` into the parameter's local slot within the callee's stack frame. The caller's variable continues to hold the original memory address pointing to the heap object.
</details>

<details>
<summary><strong>5. What does the `final` keyword do when placed on a method parameter (`public void foo(final Person p)`)?</strong></summary>

**Answer:**  
The `final` keyword on a parameter enforces compile-time re-assignment protection: it prevents the method body from reassigning the parameter variable to another object or to null (`p = new Person();` will fail to compile). However, it does **not** make the object immutable; code inside the method can still call mutating methods on `p` (e.g., `p.setName("Bob")`).
</details>

<details>
<summary><strong>6. How does String passing demonstrate pass-by-value immutability?</strong></summary>

**Answer:**  
`String` is an object, so its reference is passed by value. However, `String` is completely immutable: methods like `toUpperCase()` or `replace()` do not mutate the existing string object; they allocate a brand new `String` on the heap and return its reference. Therefore, passing a `String` to a method guarantees that the caller's string can never be modified.
</details>

<details>
<summary><strong>7. How can you simulate pass-by-reference in Java when you need to return multiple updated values?</strong></summary>

**Answer:**  
Since Java cannot pass references by reference, developers simulate it by:
1. Returning an immutable **record** or composite object containing the multiple results (idiomatic modern Java).
2. Passing a mutable wrapper object or single-element array (`int[] resultHolder = new int[1];`).
3. Passing an existing container collection that the callee populates.
</details>

<details>
<summary><strong>8. Does passing a large 10-million-element `int[]` array copy all 10 million elements?</strong></summary>

**Answer:**  
No. In Java, an array is an object. When an array is passed as an argument, only the **reference pointer** to the array's heap header (4 bytes with Compressed OOPs) is copied onto the callee's stack frame. The 10 million elements remain in place on the heap, incurring zero memory copying overhead.
</details>

---

## 11. Practice Exercises

### Exercise 1: Predicting Mutation vs Reassignment Outputs (Easy)
- **Task:** What will the following program print?
```java
public class Mystery {
    static class Data { int val = 10; }

    static void change(Data d, int x) {
        x = 99;
        d.val = 50;
        d = new Data();
        d.val = 100;
    }

    public static void main(String[] args) {
        Data myData = new Data();
        int myX = 5;
        change(myData, myX);
        System.out.println("val=" + myData.val + ", x=" + myX);
    }
}
```
<details>
<summary>Solution</summary>

**Output:** `val=50, x=5`
- `myX` is a primitive, so its copy `x` is modified to `99` with zero effect on `myX` (remains 5).
- `d.val = 50;` mutates the shared heap object pointed to by `myData`, setting `val` to 50.
- `d = new Data();` reassigns the local parameter pointer `d` to a new object, so `d.val = 100;` mutates the new temporary object, leaving `myData` pointing to the first object with `val = 50`.
</details>

### Exercise 2: Safe In-Place Primitive Swap via Single Array (Medium)
- **Task:** Implement a method `void swapPrimitives(int[] holder)` that swaps the first two elements of a 2-element integer array, verifying that the caller's values are visibly exchanged.
<details>
<summary>Solution</summary>

```java
public class PrimitiveSwapExercise {
    public static void swapPrimitives(int[] holder) {
        if (holder == null || holder.length < 2) return;
        int temp = holder[0];
        holder[0] = holder[1];
        holder[1] = temp;
    }

    public static void main(String[] args) {
        int[] pair = { 10, 20 };
        swapPrimitives(pair);
        System.out.println("Pair after swap: " + java.util.Arrays.toString(pair)); // [20, 10]
    }
}
```
</details>

### Exercise 3: Defensive Immutability Barrier (Hard)
- **Task:** Given a mutable `CustomerProfile` class with a `List<String> phoneNumbers`, implement a constructor and getter that prevent external callers from mutating internal phone numbers using pass-by-value defensive copying.
<details>
<summary>Solution</summary>

```java
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public final class CustomerProfile {
    private final String id;
    private final List<String> phoneNumbers;

    public CustomerProfile(String id, List<String> phoneNumbers) {
        this.id = id;
        // Defensive copy on ingestion to isolate from caller's list
        this.phoneNumbers = (phoneNumbers != null) 
            ? new ArrayList<>(phoneNumbers) 
            : new ArrayList<>();
    }

    public String getId() {
        return id;
    }

    public List<String> getPhoneNumbers() {
        // Return unmodifiable view to prevent caller from mutating internal state
        return Collections.unmodifiableList(phoneNumbers);
    }

    public static void main(String[] args) {
        List<String> externalList = new ArrayList<>();
        externalList.add("555-0100");

        CustomerProfile profile = new CustomerProfile("CUST-1", externalList);

        // Attempt 1: Mutating external list after passing to constructor:
        externalList.add("555-9999");
        System.out.println("Profile phones protected from external add: " + 
            (profile.getPhoneNumbers().size() == 1)); // true

        // Attempt 2: Mutating via getter:
        try {
            profile.getPhoneNumbers().add("555-8888");
        } catch (UnsupportedOperationException e) {
            System.out.println("Profile phones protected from getter modification: true");
        }
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Core Law:** Java is strictly **pass-by-value** in all cases.
- **Primitives:** The literal bits are copied; mutating parameter cannot affect caller.
- **Objects:** The **memory address pointer** is copied by value:
  - *Mutating object fields:* Affects shared heap object (caller sees changes).
  - *Reassigning parameter pointer:* Reassigns local stack slot only (caller unaffected).
- **Swap Method:** Direct `swap(a, b)` is impossible in Java without an array or wrapper container.
- **Defensive Copying:** Always clone mutable arrays and wrap collections (`List.copyOf()`) to prevent callers from corrupting internal state.

---

## 13. Further Reading

- [The Java Language Specification (Java SE 25 Edition) - Chapter 15.12.4.2: Evaluate Arguments](https://docs.oracle.com/javase/specs/jls/se25/html/jls-15.html#jls-15.12.4.2)
- [The Java Programming Language (4th Edition) by Ken Arnold and James Gosling - Section 2.6: Method Invocation](https://www.pearson.com/)
- [Effective Java (3rd Edition) - Item 50: Make defensive copies when needed](https://www.oreilly.com/library/view/effective-java-3rd/9780134686097/)

---

| [← Previous: 00_methods.md](file:///E:/Learning/engineering-notes/01-languages/java/02-methods/00_methods.md) | [Section Index](file:///E:/Learning/engineering-notes/01-languages/java/02-methods/README.md) | [Next: 02_overloading-and-varargs.md](file:///E:/Learning/engineering-notes/01-languages/java/02-methods/02_overloading-and-varargs.md) → |
