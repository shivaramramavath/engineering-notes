# Comparable and Comparator

> Ordering contracts in Java: natural vs explicit ordering, comparator composition, reverse and null-safe comparators, and sorting invariants.

| Metadata | Value |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | [00_collection-hierarchy.md](00_collection-hierarchy.md), [07-generics/01_bounded-types.md](../07-generics/01_bounded-types.md) |
| **Time to Read** | 24 minutes |
| **Status** | Active |
| **Java Version** | Java 1.2+ (Functional combinators Java 8+) |

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

Sorting, searching, and maintaining sorted data structures (`TreeSet`, `TreeMap`, `PriorityQueue`) require comparing two objects to determine their relative order. Java separates comparison into two distinct philosophies:
1. **Natural Ordering (`Comparable<T>`)**: Defined **internally** by the class itself. Represents the single canonical way instances of that class should be ordered (e.g., numbers ordered by value, strings ordered lexicographically).
2. **Explicit / Custom Ordering (`Comparator<T>`)**: Defined **externally** by a separate strategy object. Allows ordering instances by multiple different criteria (e.g., sorting users by age, by sign-up date, or by last name), without modifying the original class.

Java 8 transformed `Comparator` from a verbose anonymous inner class into a fluent functional interface with powerful combinators (`comparing`, `thenComparing`, `reversed`, `nullsFirst`).

---

## 2. Core Concepts

### `Comparable<T>` Contract
Declared as:
```java
public interface Comparable<T> {
    int compareTo(T o);
}
```
**Return value rules**:
- Negative integer (conventionally `-1`): `this < o`
- Zero (`0`): `this` equals `o` in ordering
- Positive integer (conventionally `1`): `this > o`

**Mathematical Invariants**:
1. **Antisymmetry**: `sgn(x.compareTo(y)) == -sgn(y.compareTo(x))`
2. **Transitivity**: `(x.compareTo(y) > 0 && y.compareTo(z) > 0)` implies `x.compareTo(z) > 0`
3. **Congruence**: `x.compareTo(y) == 0` implies `sgn(x.compareTo(z)) == sgn(y.compareTo(z))`
4. **Consistency with Equals** (strongly recommended): `(x.compareTo(y) == 0) == (x.equals(y))`

### `Comparator<T>` Contract
Declared as:
```java
@FunctionalInterface
public interface Comparator<T> {
    int compare(T o1, T o2);
}
```
`Comparator` is a functional interface; any lambda `(a, b) -> Integer.compare(a.val, b.val)` is a valid comparator.

---

## 3. How It Works Internally

### Comparator Combinators (Method References and Chaining)
Under the hood, Java 8 `Comparator.comparing` builds a chained function pipeline using lambda closures:
```java
Comparator<User> comp = Comparator
    .comparing(User::getLastName)
    .thenComparing(User::getFirstName)
    .thenComparingInt(User::getAge);
```
In bytecode:
- `comparing` creates a `Comparator` that applies the key extractor `User::getLastName` and delegates to natural ordering.
- `thenComparing` creates a composite comparator that first calls the initial comparator; if the result is `0`, it invokes the secondary comparator.

```
Input: User("Alice", "Smith", 25), User("Bob", "Smith", 30)

Step 1: Compare last names -> "Smith".compareTo("Smith") == 0
Step 2: Tie-breaker triggered! Compare first names -> "Alice".compareTo("Bob") < 0
Result: -1 (Alice precedes Bob)
```

---

## 4. Syntax & Usage

### 1. Implementing `Comparable` on a Domain Record
```java
public record Employee(int id, String name, double salary) implements Comparable<Employee> {
    @Override
    public int compareTo(Employee other) {
        // Use Integer.compare to avoid integer subtraction overflow!
        return Integer.compare(this.id, other.id);
    }
}
```

### 2. Modern Fluent Comparator Composition
```java
import java.util.Comparator;

public class ComparatorChainingDemo {
    public static final Comparator<Employee> BY_SALARY_DESC_THEN_NAME = Comparator
        .comparingDouble(Employee::salary)
        .reversed() // Descending salary
        .thenComparing(Employee::name); // Ascending name tie-breaker
}
```

### 3. Null-Safe Comparators
```java
import java.util.Comparator;

public class NullSafeDemo {
    // Sorts nulls to the beginning, non-nulls by name
    public static final Comparator<Employee> NULL_SAFE_NAME = Comparator
        .comparing(Employee::name, Comparator.nullsFirst(String::compareToIgnoreCase));
}
```

---

## 5. Practical Examples

### Example 1: Multi-Column Sorting (Basic)
```java
import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;

public class MultiColumnSort {
    record Product(String category, String name, double price) {}

    public static void main(String[] args) {
        List<Product> products = new ArrayList<>(List.of(
            new Product("Electronics", "Laptop", 1200.0),
            new Product("Books", "Java Guide", 45.0),
            new Product("Electronics", "Phone", 800.0),
            new Product("Books", "Algorithms", 60.0)
        ));

        // Sort by Category ascending, then by Price descending
        products.sort(Comparator
            .comparing(Product::category)
            .thenComparing(Comparator.comparingDouble(Product::price).reversed()));

        products.forEach(p -> System.out.println(p.category() + " | " + p.name() + " | $" + p.price()));
    }
}
```

### Example 2: Custom Sorting in `PriorityQueue` (Real-World)
```java
import java.util.Comparator;
import java.util.PriorityQueue;

public class TaskScheduler {
    record Task(String id, int priority, long submittedAt) {}

    public static void main(String[] args) {
        // Higher priority first; if equal, earlier submission first (FIFO on ties)
        Comparator<Task> taskComparator = Comparator
            .comparingInt(Task::priority).reversed()
            .thenComparingLong(Task::submittedAt);

        PriorityQueue<Task> queue = new PriorityQueue<>(taskComparator);
        queue.offer(new Task("Cleanup", 1, 1000L));
        queue.offer(new Task("Backup", 5, 1050L));
        queue.offer(new Task("SecurityScan", 5, 1020L));

        while (!queue.isEmpty()) {
            Task t = queue.poll();
            System.out.println("Processing: " + t.id() + " (Priority " + t.priority() + ")");
        }
        // Processing SecurityScan (5) -> Backup (5) -> Cleanup (1)
    }
}
```

### Example 3: Edge Case - Integer Overflow with Subtraction in `compareTo`
```java
public class SubtractionOverflowHazard {
    public static void main(String[] args) {
        int x = -2_000_000_000;
        int y = 2_000_000_000;

        // DANGEROUS BAD PRACTICE:
        int diff = x - y; // UNDERFLOWS to positive integer 294,967,296!
        System.out.println("Faulty diff: " + diff); // Positive! Indicates x > y, which is WRONG!

        // CORRECT FIX:
        int correct = Integer.compare(x, y);
        System.out.println("Correct comparison: " + correct); // -1 (Correct: x < y)
    }
}
```

### Example 4: Failure Case - Violating Transitivity and TimSort Crash
```java
import java.util.Arrays;
import java.util.Comparator;

public class TimSortViolationDemo {
    public static void main(String[] args) {
        // Inconsistent comparator violating transitivity
        Comparator<Integer> buggy = (a, b) -> (a.equals(b)) ? 0 : (a > b ? 1 : 1); // Never returns -1!

        Integer[] arr = new Integer[40];
        Arrays.fill(arr, 1);
        arr[0] = 2;

        try {
            Arrays.sort(arr, buggy);
        } catch (IllegalArgumentException e) {
            System.out.println("TimSort detected broken contract: " + e.getMessage());
            // "Comparison method violates its general contract!"
        }
    }
}
```

---

## 6. Best Practices

1. **Never Use Subtraction for Integer/Long Comparison**: Always use `Integer.compare(a, b)` or `Double.compare(a, b)` to prevent numerical overflow bugs.
2. **Make `compareTo` Consistent with `equals`**: If `compareTo` returns 0, `equals` should return `true`. Inconsistencies cause subtle bugs in `TreeSet` and `TreeMap`.
3. **Prefer `Comparator.comparing` Combinators**: Fluent combinators are readable, avoid boxing when using primitive specializations (`comparingInt`), and reduce boilerplate.
4. **Handle Nulls Explicitly**: Use `Comparator.nullsFirst` or `Comparator.nullsLast` when sorting data sets that may contain null values.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why Wrong | Fix |
|---|---|---|
| Using subtraction `return a.id - b.id;` | Causes integer overflow/underflow, inverting the comparison result. | Use `Integer.compare(a.id, b.id)`. |
| Violating transitivity | Triggers `IllegalArgumentException: Comparison method violates its general contract` in TimSort. | Strictly obey mathematical ordering axioms. |
| Inconsistency with `equals` | `TreeSet` drops distinct items if `compareTo` returns 0. | Ensure all identity fields are compared. |
| Boxing overhead with `Comparator.comparing(User::getAge)` | Auto-boxes primitive `int` to `Integer` for every comparison ($O(N \log N)$ allocations). | Use `Comparator.comparingInt(User::getAge)`. |
| Unhandled `NullPointerException` during sorting | Calling `compareTo` on `null` crashes the sort algorithm. | Wrap in `Comparator.nullsFirst()`. |
| Reversing with `-compare(a, b)` | If `compare()` returns `Integer.MIN_VALUE`, `-Integer.MIN_VALUE` remains negative! | Use `comparator.reversed()`. |

---

## 8. Performance & Security Notes

### Performance
- **Primitive Specializations**: Always use `comparingInt()`, `comparingLong()`, and `comparingDouble()` to avoid boxing millions of primitive fields during sorts.
- **TimSort Invariant Checking**: Java's sorting algorithm (`TimSort`) actively checks for contract violations (transitivity and antisymmetry) and throws `IllegalArgumentException` rather than looping indefinitely or corrupting memory.

### Security
- **Predictable Sorting Invariants**: Do not use non-deterministic properties (such as `System.currentTimeMillis()` or random values) inside comparators. Non-deterministic comparators violate sorting contracts and can crash application services.

---

## 9. Comparison / When to Use What

| Feature | `Comparable<T>` | `Comparator<T>` |
|---|---|---|
| **Package** | `java.lang` | `java.util` |
| **Location** | Implemented by the target class itself | Implemented in separate class/lambda |
| **Number of Orders** | Exactly one (natural order) | Unlimited custom strategies |
| **Source Code Modification** | Requires modifying class source | Works on classes you cannot modify |
| **Invocation** | `itemA.compareTo(itemB)` | `comparator.compare(itemA, itemB)` |
| **Primary Method** | `int compareTo(T o)` | `int compare(T o1, T o2)` |

---

## 10. Interview Questions

<details>
<summary>1. What is the fundamental difference between `Comparable` and `Comparator`?</summary>
`Comparable` defines the natural, default ordering of an object implemented within the class itself via `compareTo()`. `Comparator` defines external, alternative sorting strategies implemented outside the class via `compare()`.
</details>

<details>
<summary>2. Why does `a - b` comparison cause bugs?</summary>
If `a` is a large negative integer and `b` is a large positive integer, `a - b` underflows, producing a positive integer and incorrectly reporting that `a > b`.
</details>

<details>
<summary>3. What error occurs if a comparator violates transitivity during `Collections.sort`?</summary>
`IllegalArgumentException: Comparison method violates its general contract!` thrown by TimSort.
</details>

<details>
<summary>4. What does "consistent with equals" mean?</summary>
An ordering is consistent with equals if `x.compareTo(y) == 0` is true if and only if `x.equals(y)` is true. If violated, collections like `TreeSet` and `TreeMap` will exhibit unexpected behavior because they use comparison rather than `equals` to determine key uniqueness.
</details>

<details>
<summary>5. How do you sort a list in reverse natural order?</summary>
`list.sort(Comparator.reverseOrder());`.
</details>

<details>
<summary>6. Why is `Comparator.comparingInt(Person::getAge)` preferred over `Comparator.comparing(Person::getAge)`?</summary>
`comparingInt` uses an `ToIntFunction`, preventing boxing overhead from primitive `int` to `Integer` during every comparison step.
</details>

<details>
<summary>7. How does `Comparator.nullsFirst` handle comparisons when both elements are null?</summary>
It considers two `null` elements equal (`returns 0`). If one is null and the other is not, `null` is considered smaller.
</details>

<details>
<summary>8. Can you change the sorting order of a `TreeSet` after creation?</summary>
No. The comparator is set at construction time. To re-sort with a different order, you must create a new `TreeSet` with the new comparator and add the elements.
</details>

---

## 11. Practice Exercises

### Easy
1. Implement `Comparable<Book>` on a `Book` record to sort naturally by publication year.
2. Sort a list of strings by their string length ascending using a lambda comparator.
3. Sort a list of names case-insensitively using `String.CASE_INSENSITIVE_ORDER`.

### Medium
4. Build a multi-level comparator for `Student` (GPA descending, then Graduation Year ascending, then Name ascending).
5. Sort a list containing `null` values such that all nulls appear at the end, and non-null strings are sorted alphabetically.
6. Write a custom comparator that sorts even numbers before odd numbers, and sorts numbers within each group in ascending order.

### Hard
7. Implement a natural sort comparator ("Human Sort") that correctly compares alphanumeric strings (e.g., "file2.txt" before "file10.txt").
8. Implement an in-place version of TimSort contract validator that verifies if a given `Comparator<T>` obeys transitivity on a sample set.
9. Construct a composite dynamic comparator builder that parses sort expressions from string inputs (e.g., `"name:asc,age:desc"`).

<details>
<summary>Solutions</summary>

```java
// Easy 2: Sort by length
List<String> words = new ArrayList<>(List.of("banana", "apple", "pear", "fig"));
words.sort(Comparator.comparingInt(String::length));

// Medium 4: Student multi-level
record Student(double gpa, int gradYear, String name) {}
Comparator<Student> comp = Comparator
    .comparingDouble(Student::gpa).reversed()
    .thenComparingInt(Student::gradYear)
    .thenComparing(Student::name);
```
</details>

---


### Deep-Dive: Bytecode Execution of Chained Comparators
When chaining comparators with `thenComparing`, Java avoids virtual call overhead through specialized lambda function objects:
```java
default Comparator<T> thenComparing(Comparator<? super T> other) {
    Objects.requireNonNull(other);
    return (Comparator<T> & Serializable) (c1, c2) -> {
        int res = compare(c1, c2);
        return (res != 0) ? res : other.compare(c1, c2);
    };
}
```
If the primary comparison returns non-zero (`res != 0`), the second comparator is never evaluated (short-circuiting), keeping CPU cycles minimal.

### Detailed Exercise Solutions (All 9 Problems)
<details>
<summary>View Complete Solutions for All Exercises</summary>

```java
// Easy 1: Book natural order
record Book(String title, int year) implements Comparable<Book> {
    public int compareTo(Book o) { return Integer.compare(this.year, o.year); }
}

// Easy 2: Sort by string length
List<String> words = new ArrayList<>(List.of("banana", "fig", "apple", "watermelon"));
words.sort(Comparator.comparingInt(String::length));

// Easy 3: Case-insensitive sort
words.sort(String.CASE_INSENSITIVE_ORDER);

// Medium 4: Multi-level Student
record Student(double gpa, int gradYear, String name) {}
Comparator<Student> studentComp = Comparator
    .comparingDouble(Student::gpa).reversed()
    .thenComparingInt(Student::gradYear)
    .thenComparing(Student::name);

// Medium 5: Nulls at end, non-null sorted
Comparator<String> nullsLastComp = Comparator.nullsLast(String::compareTo);

// Medium 6: Evens before odds
Comparator<Integer> evenFirstComp = (a, b) -> {
    boolean aEven = (a % 2 == 0);
    boolean bEven = (b % 2 == 0);
    if (aEven != bEven) return aEven ? -1 : 1;
    return Integer.compare(a, b);
};

// Hard 7: Natural Alphanumeric Sort (Human Sort)
public class AlphanumericComparator implements Comparator<String> {
    public int compare(String s1, String s2) {
        int i = 0, j = 0;
        while (i < s1.length() && j < s2.length()) {
            char c1 = s1.charAt(i), c2 = s2.charAt(j);
            if (Character.isDigit(c1) && Character.isDigit(c2)) {
                int start1 = i, start2 = j;
                while (i < s1.length() && Character.isDigit(s1.charAt(i))) i++;
                while (j < s2.length() && Character.isDigit(s2.charAt(j))) j++;
                int num1 = Integer.parseInt(s1.substring(start1, i));
                int num2 = Integer.parseInt(s2.substring(start2, j));
                if (num1 != num2) return Integer.compare(num1, num2);
            } else {
                if (c1 != c2) return Character.compare(c1, c2);
                i++; j++;
            }
        }
        return Integer.compare(s1.length(), s2.length());
    }
}

// Hard 8: TimSort contract validator
public static <T> boolean validateTransitivity(Comparator<T> comp, List<T> samples) {
    for (T a : samples) {
        for (T b : samples) {
            for (T c : samples) {
                int ab = comp.compare(a, b);
                int bc = comp.compare(b, c);
                int ac = comp.compare(a, c);
                if (ab > 0 && bc > 0 && ac <= 0) return false;
            }
        }
    }
    return true;
}

// Hard 9: Dynamic string sort expression parser
public static Comparator<Map<String, Comparable>> parseSort(String expression) {
    Comparator<Map<String, Comparable>> comp = null;
    for (String part : expression.split(",")) {
        String[] tokens = part.split(":");
        String field = tokens[0];
        boolean asc = tokens.length <= 1 || tokens[1].equalsIgnoreCase("asc");
        Comparator<Map<String, Comparable>> fieldComp = (m1, m2) -> {
            Comparable v1 = m1.get(field);
            Comparable v2 = m2.get(field);
            return asc ? v1.compareTo(v2) : v2.compareTo(v1);
        };
        comp = (comp == null) ? fieldComp : comp.thenComparing(fieldComp);
    }
    return comp;
}
```
</details>


## 12. Summary / Cheat Sheet

- **Comparable**: `compareTo(T o)`, natural ordering inside class.
- **Comparator**: `compare(T o1, T o2)`, custom strategies outside class.
- **Rules**: Negative (`<`), Zero (`==`), Positive (`>`).
- **Safety**: Always use `Integer.compare(a, b)`, never subtraction.
- **Combinators**: `Comparator.comparing()`, `thenComparing()`, `reversed()`, `nullsFirst()`.
- **Primitives**: Use `comparingInt()`, `comparingDouble()` to avoid boxing penalties.

---

## 13. Further Reading
- Bloch, Joshua. *Effective Java (3rd Edition)*: Item 14 (Consider implementing Comparable).
- Oracle Java Documentation: *The Comparator Interface*.
- OpenJDK: *TimSort implementation analysis*.
