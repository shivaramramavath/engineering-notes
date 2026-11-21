# Conditionals: Branching, Modern Switch Expressions, and Pattern Matching

> Control flow in Java enables deterministic conditional execution through strict boolean-evaluated `if/else` ladders, ternary operators, and modern expression-oriented `switch` constructs featuring guarded pattern matching and sealed hierarchy exhaustiveness.

| Metadata | Value |
| :--- | :--- |
| **Level** | Beginner to Intermediate |
| **Prerequisites** | [02_operators.md](file:///E:/Learning/engineering-notes/01-languages/java/01-fundamentals/02_operators.md) |
| **Time to Read** | 30 minutes |
| **Status** | Complete |
| **Java LTS Baseline** | Java 25 LTS (with Pattern Matching Switch & Record Deconstruction) |

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
Imperative algorithms require branching logic to handle diverse runtime conditions: validating user inputs, processing domain events, routing requests, and managing state machines. Without structured, type-safe conditional branching, programs cannot make decisions based on dynamic state.

### Historical Context & The Evolution of Switch
- **Java 1.0 to 6:** Conditionals were limited to `if/else` and classic C-style `switch` statements restricted strictly to primitive integer types (`byte`, `short`, `char`, `int`) and `enum` constants. Classic `switch` suffered from hazardous fall-through behavior where missing a `break` statement silently executed subsequent cases.
- **Java 7:** Added support for `String` in `switch` statements via hashcode and equality desugaring.
- **Java 14 (JEP 361):** Revolutionized branching with **Switch Expressions**. Introduced arrow syntax (`->`), eliminated fall-through, enabled multi-value case labels, and allowed `switch` to yield values directly into variables.
- **Java 21 LTS (JEP 440 & 441):** Finalized **Pattern Matching for Switch** and **Record Patterns**. `switch` expanded from a simple value matching construct into an algebraic data type (ADT) decomposition engine with guarded `when` clauses and null handling.

### Real-World Relevance
Modern Java services leverage switch expressions and pattern matching to replace fragile `instanceof` casting chains and bulky Visitor design patterns, providing compile-time verified exhaustiveness across sealed domain models.

---

## 2. Core Concepts

### 1. The `if`, `else if`, and `else` Ladder
- Evaluates a condition that must evaluate strictly to a primitive `boolean` or boxed `Boolean`. Unlike C/JavaScript, numbers (`0`, `1`) and objects are **never** truthy or falsy.
- Evaluated sequentially from top to bottom; the first branch whose condition evaluates to `true` is executed, and all subsequent branches are skipped.

### 2. The Ternary Operator (`? :`)
- An inline expression returning one of two values: `condition ? trueExpression : falseExpression`.
- Produces a value that must be assigned, passed to a method, or returned.

### 3. Classic `switch` Statement vs. Modern `switch` Expression

```
+---------------------------------------------------------------------------------+
|                         Classic switch Statement (Java 1.0+)                    |
|  • Uses colon syntax: case CONSTANT:                                            |
|  • Hazard: Fall-through occurs unless explicit 'break' is written               |
|  • Statement only; cannot return a value directly                               |
+---------------------------------------------------------------------------------+
|                         Modern switch Expression (Java 14+ / 21+ / 25)          |
|  • Uses arrow syntax: case A, B, C -> ...                                       |
|  • Zero fall-through: Only the matched branch executes                          |
|  • Expression: Evaluates to a value; uses 'yield' for multi-line blocks         |
|  • Compiler enforces EXHAUSTIVENESS across all possible domain values           |
+---------------------------------------------------------------------------------+
```

### 4. Pattern Matching and Guarded Patterns (`when`)
- **Type Patterns:** `case String s -> ...` tests type and extracts variable `s` in one step.
- **Guarded Patterns:** `case Rectangle r when r.width() == r.height() -> ...` adds an arbitrary boolean condition (`when`) to refine matching.
- **Explicit Null Handling:** `case null -> ...` or `case null, default -> ...` allows graceful null handling without throwing `NullPointerException`.

---

## 3. How It Works Internally

### Bytecode Branching: `tableswitch` vs. `lookupswitch`
The JVM optimizes `switch` statements at compile time by selecting between two distinct bytecode instructions based on the density of case values:

```
                      Is case value space dense?
                                │
                 ┌──────────────┴──────────────┐
                 ▼ (Yes: consecutive numbers)  ▼ (No: sparse numbers or hashcodes)
          [ tableswitch ]                               [ lookupswitch ]
     Direct O(1) jump table                        Binary search O(log N)
     Indexed directly via table offset             Ordered array of (key, target) pairs
```

- **`tableswitch`:** When case labels are contiguous or nearly contiguous (e.g., `case 1:`, `case 2:`, `case 3:`), `javac` emits a `tableswitch`. The JVM treats the target variable as a direct array index into a jump table, executing in $O(1)$ constant time.
- **`lookupswitch`:** When case labels are sparse (e.g., `case 10:`, `case 5000:`, `case 99999:`), `javac` emits a `lookupswitch`. The JVM stores keys in a sorted table and performs an $O(\log N)$ binary search to determine the jump target.

### String Switch Desugaring
When switching on a `String`, how does the JVM match cases without native string instructions?
`javac` desugars the switch into a two-tier lookup:
1. First, it computes the string's `.hashCode()` and performs a `lookupswitch` on the resulting integer hash.
2. Because different strings can produce identical hash codes (hash collision), the generated bytecode executes `.equals()` inside the matched hash branch to verify identity before jumping to the actual case body!

---

## 4. Syntax & Usage

### Modern Switch Expression with Arrow Syntax
```java
DayOfWeek day = LocalDate.now().getDayOfWeek();

// Expression yielding a value directly to a variable
int estimatedWorkHours = switch (day) {
    case MONDAY, TUESDAY, WEDNESDAY, THURSDAY, FRIDAY -> 8;
    case SATURDAY -> 4;
    case SUNDAY -> {
        System.out.println("Rest day scheduled");
        yield 0; // 'yield' returns a value from a block
    }
};
```

### Pattern Matching Switch with Guarded Patterns (`when`)
```java
public static String formatObject(Object obj) {
    return switch (obj) {
        case null -> "Null Reference";
        case Integer i when i < 0 -> "Negative Integer: " + i;
        case Integer i -> "Positive Integer: " + i;
        case String s when s.isBlank() -> "Blank String";
        case String s -> "String of length " + s.length();
        default -> "Unknown Type: " + obj.getClass().getSimpleName();
    };
}
```

---

## 5. Practical Examples

### Example 1: Modern Switch Expression with Value Assignment
- **Goal:** Determine pricing tiers based on membership level using clean arrow syntax with zero fall-through.
```java
public class SwitchExpressionDemo {
    public enum Tier { BRONZE, SILVER, GOLD, PLATINUM }

    public static double calculateDiscount(Tier tier) {
        return switch (tier) {
            case BRONZE -> 0.05;
            case SILVER -> 0.10;
            case GOLD -> 0.20;
            case PLATINUM -> {
                System.out.println("VIP discount level applied");
                yield 0.30;
            }
        };
    }

    public static void main(String[] args) {
        System.out.println("Gold Discount:     " + calculateDiscount(Tier.GOLD));
        System.out.println("Platinum Discount: " + calculateDiscount(Tier.PLATINUM));
    }
}
```
- **Console Output:**
```text
Gold Discount:     0.2
VIP discount level applied
Platinum Discount: 0.3
```
- **Explanation:** Notice that when switching on an `enum`, the compiler verifies exhaustiveness: all enum constants are covered, so no `default` branch is required or needed.

---

### Example 2: Pattern Matching over Sealed Hierarchy
- **Goal:** Deconstruct domain objects using sealed interfaces and pattern matching with guarded `when` clauses.
```java
public class ConditionalsDemo {
    sealed interface Shape permits Circle, Rectangle {}
    record Circle(double radius) implements Shape {}
    record Rectangle(double width, double height) implements Shape {}

    public static double computeArea(Shape shape) {
        return switch (shape) {
            case Circle c -> Math.PI * c.radius() * c.radius();
            case Rectangle r when r.width() == r.height() -> {
                System.out.println("Processing a Square");
                yield r.width() * r.width();
            }
            case Rectangle r -> r.width() * r.height();
        };
    }

    public static void main(String[] args) {
        Shape c = new Circle(5.0);
        Shape sq = new Rectangle(4.0, 4.0);
        Shape rect = new Rectangle(3.0, 6.0);

        System.out.printf("Circle Area:    %.2f%n", computeArea(c));
        System.out.printf("Square Area:    %.2f%n", computeArea(sq));
        System.out.printf("Rectangle Area: %.2f%n", computeArea(rect));
    }
}
```
- **Console Output:**
```text
Circle Area:    78.54
Processing a Square
Square Area:    16.00
Rectangle Area: 18.00
```
- **Explanation:** The sealed hierarchy (`permits Circle, Rectangle`) guarantees that no other subclasses can exist. The compiler recognizes that `Circle` and `Rectangle` cover all possibilities, ensuring 100% compile-time exhaustiveness without a redundant `default` case.

---

### Example 3: Edge Case — Handling `null` in Switch Expressions
- **Goal:** Observe how modern pattern matching switch handles `null` explicitly without throwing `NullPointerException`.
```java
public class NullSwitchDemo {
    public static String inspect(String text) {
        return switch (text) {
            case null -> "Handled Null Safely";
            case "admin" -> "Superuser Access";
            case String s when s.startsWith("user_") -> "Standard Account: " + s;
            default -> "Guest Account";
        };
    }

    public static void main(String[] args) {
        System.out.println(inspect(null));
        System.out.println(inspect("admin"));
        System.out.println(inspect("user_42"));
        System.out.println(inspect("anonymous"));
    }
}
```
- **Console Output:**
```text
Handled Null Safely
Superuser Access
Standard Account: user_42
Guest Account
```
- **Explanation:** In classic Java, switching on a `null` variable immediately threw `NullPointerException` before checking any case. In modern Java, `case null` explicitly intercepts null references.

---

### Example 4: Failure Case — Accidental Fall-Through in Legacy Switch
- **Goal:** Demonstrate the catastrophic logic failure caused by missing `break` statements in legacy switch syntax.
```java
public class FallthroughBugDemo {
    public static void main(String[] args) {
        int status = 1; // 1 = PENDING, 2 = APPROVED, 3 = REJECTED

        System.out.println("--- Legacy Switch with Missing Break ---");
        switch (status) {
            case 1:
                System.out.println("Processing Pending Request...");
                // BUG: Missing break! Silently falls through to case 2!
            case 2:
                System.out.println("CRITICAL FAULT: Auto-Approving Request!");
                break;
            case 3:
                System.out.println("Rejecting Request");
                break;
        }
    }
}
```
- **Console Output:**
```text
--- Legacy Switch with Missing Break ---
Processing Pending Request...
CRITICAL FAULT: Auto-Approving Request!
```
- **Remedy:** Migrate to modern switch expressions using arrow syntax (`case 1 -> ...; case 2 -> ...;`), which eliminates fall-through entirely at the grammatical level.

---

### Example 5: High-Performance / Modern Idiom — Record Deconstruction Patterns
- **Goal:** Deconstruct record components directly within the `switch` header (Java 21+ / 25).
```java
public class RecordPatternDemo {
    record GeoPoint(double lat, double lon) {}
    record DeviceLocation(String deviceId, GeoPoint point) {}

    public static void processLocation(Object event) {
        switch (event) {
            // Nested record deconstruction pattern!
            case DeviceLocation(String id, GeoPoint(double lat, double lon)) when lat == 0.0 && lon == 0.0 ->
                System.out.printf("Device %s is at Null Island (0,0)%n", id);

            case DeviceLocation(String id, GeoPoint(double lat, double lon)) ->
                System.out.printf("Device %s located at (%.4f, %.4f)%n", id, lat, lon);

            default -> System.out.println("Non-location event received");
        }
    }

    public static void main(String[] args) {
        processLocation(new DeviceLocation("DEV-1", new GeoPoint(0.0, 0.0)));
        processLocation(new DeviceLocation("DEV-2", new GeoPoint(37.7749, -122.4194)));
    }
}
```
- **Console Output:**
```text
Device DEV-1 is at Null Island (0,0)
Device DEV-2 located at (37.7749, -122.4194)
```
- **Explanation:** Record patterns extract inner record fields (`deviceId`, `lat`, `lon`) directly in the `case` header, eliminating intermediate getter calls and temporary local variables.

---

## 6. Best Practices

1. **Always Prefer Modern Switch Expressions Over Statements:** Use arrow syntax (`case ->`) by default. Reserve colon syntax (`case :`) solely for legacy codebase maintenance.
2. **Leverage Sealed Types for 100% Exhaustiveness:** When modeling domain events or state hierarchies, use `sealed interface` combined with `record` implementations. This forces the compiler to detect missing cases if a new state is added in the future.
3. **Do Not Put Default Cases on Exhaustive Sealed Hierarchies:** Omit `default` when switching over an enum or sealed hierarchy. If you add a `default` case, adding a new enum constant or record will be silently swallowed by `default` instead of producing a helpful compile error.
4. **Order Specific Guarded Patterns Before General Patterns:** Because cases are evaluated sequentially, place guarded cases (`case Integer i when i > 0`) **before** unguarded catch-all cases (`case Integer i`). The compiler will flag an error if a preceding case dominates a subsequent one.
5. **Keep Case Bodies Small:** Avoid 50-line blocks inside a switch branch. Extract multi-line branch logic into private helper methods.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Missing `break` in Colon Syntax** | Execution silently falls through into the next case body. | Use modern arrow syntax (`->`) which forbids fall-through. |
| **Missing `default` in Non-Exhaustive Switch Expression** | Switch expressions returning a value must cover 100% of inputs. | Add `default -> ...` or cover all sealed subtypes. |
| **Dominated Case Patterns** | Placing `case Object o` before `case String s` makes `String` unreachable. | Order cases from most specific to most general. |
| **Unboxing NPE in Classic Switch** | Passing `null` to a classic `switch` throws `NullPointerException` before matching. | Add `case null -> ...` in modern switch or check before switch. |
| **Assuming Boolean Truthiness in `if`** | `if (count)` or `if (object)` fails compilation; only `boolean` is allowed. | Write explicit conditions: `if (count > 0)` or `if (obj != null)`. |
| **Forgetting `yield` in Multi-Line Switch Expression Blocks** | Blocks enclosed in `{}` in a switch expression must return a value. | Use `yield <value>;` to return from the block. |

---

## 8. Performance & Security Notes

### Branch Prediction & JIT Compilation
Modern CPUs use sophisticated branch predictors. When an `if/else` condition is consistently biased in one direction (e.g., $99\%$ of requests pass validation), the CPU speculatively executes the instructions in the pipeline. HotSpot C2 compiles the biased branch as inline assembly and moves the cold branch out of the instruction cache line.

### Switch Optimization Mechanics
- A `tableswitch` compiles to an indexed jump table ($O(1)$ assembly `jmp [table + index*8]`), executing almost instantly regardless of whether there are 5 or 500 cases.
- A `lookupswitch` executes a binary search ($O(\log N)$ comparisons).
- If your switch statement matches on integer codes, try to keep the integer range dense to allow the compiler to generate a `tableswitch`.

---

## 9. Comparison / When to Use What

| Technique | Readability | Fall-Through Risk | Exhaustiveness Check | Pattern Matching Support | Ideal Use Case |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **`if / else if` Ladder** | Moderate | None | None | Manual `instanceof` | Non-uniform conditions, range checks ($x > 10$) |
| **Ternary Operator (`? :`)** | High (for 1 line) | None | N/A | None | Inline default selection, simple binary assignments |
| **Classic `switch` (Colon)** | Poor (Verbose) | **High** | Enum only | None | Legacy codebases only (avoid in new code) |
| **Modern `switch` Expression** | **High** | **Zero** | **Strict Compile-Time** | **Full (Types, Records, `when`)** | Domain routing, state machines, algebraic types |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the fundamental difference between a statement and an expression in Java?</strong></summary>

**Answer:**  
A **statement** is a complete unit of execution that performs an action (e.g., assignment, method invocation, loop) but does not evaluate to a value on its own (e.g., `if (...) { ... }` or `for (...) { ... }`). An **expression** is a syntactic construct that evaluates to a concrete value (e.g., `2 + 2`, `obj.toString()`, `a ? b : c`). Modern switch expressions (`int result = switch(...)`) allow the switch construct to operate as an expression yielding a value.
</details>

<details>
<summary><strong>2. How does the JVM bytecode differ between `tableswitch` and `lookupswitch`?</strong></summary>

**Answer:**  
- **`tableswitch`** is generated when the case integer values are dense (consecutive or near-consecutive). It contains an array of direct jump offsets indexed by $(value - lowValue)$, providing $O(1)$ constant-time branch dispatch.
- **`lookupswitch`** is generated when the case values are sparse. It contains a sorted table of $(matchValue, targetOffset)$ pairs. The JVM executes a binary search over the table, requiring $O(\log N)$ comparisons to find the jump target.
</details>

<details>
<summary><strong>3. How does `switch` on `String` handle hash collisions?</strong></summary>

**Answer:**  
In Java 7+, switching on a `String` compiles into two steps:
1. The compiler emits a `lookupswitch` on the computed `String.hashCode()`.
2. Inside each hash target branch, the JVM executes `String.equals()` to verify that the string’s actual characters match the case label. If two different strings produce the same hashcode (hash collision), the generated bytecode contains sequential `.equals()` checks for each colliding case before branching to the user code.
</details>

<details>
<summary><strong>4. What is the role of `yield` in modern switch expressions?</strong></summary>

**Answer:**  
In a switch expression, single-line arrow branches (`case A -> 10;`) return values implicitly. However, if a case branch requires a multi-statement block enclosed in braces (`{ ... }`), the keyword `yield` is used to explicitly return the evaluated value from that block (analogous to `return` for methods). `yield` is a contextual keyword and does not interfere with existing variables named `yield`.
</details>

<details>
<summary><strong>5. What is "Dominated Case" compile-time error in pattern matching switch?</strong></summary>

**Answer:**  
A dominated case error occurs when a preceding case pattern matches a superset of a subsequent pattern, making the subsequent pattern completely unreachable. For example, placing `case CharSequence cs` before `case String s` or placing an unguarded `case Integer i` before a guarded `case Integer i when i > 0` causes the compiler to reject the program with: `this case label is dominated by a preceding case label`.
</details>

<details>
<summary><strong>6. How does modern switch handle `null` compared to classic switch?</strong></summary>

**Answer:**  
In classic switch statements, if the selector variable is `null`, the JVM immediately throws a `NullPointerException` upon entering the switch construct, regardless of whether a `default` case is present. In modern pattern matching switch (Java 21+ / 25), you can write an explicit `case null -> ...` or `case null, default -> ...` to handle null references gracefully without throwing an exception.
</details>

<details>
<summary><strong>7. Why should you avoid adding `default` to a switch expression over a sealed interface hierarchy?</strong></summary>

**Answer:**  
When switching over a sealed interface or an enum, the compiler can mathematically verify that all possible subtypes or enum constants are explicitly handled (exhaustiveness). If you add a `default` case, you disable this exhaustiveness safety net: if a developer later adds a new permitted subtype or enum constant, the compiler will silently route it to `default` rather than failing compilation and forcing the developer to handle the new case.
</details>

<details>
<summary><strong>8. What is a Guarded Pattern, and what keyword is used to declare it?</strong></summary>

**Answer:**  
A Guarded Pattern combines a type pattern with a secondary boolean expression to refine matching. In Java 21+ / 25, the contextual keyword **`when`** is used (e.g., `case Person p when p.age() >= 18 -> ...`). The case branch is only executed if the object matches the type pattern *and* the boolean expression following `when` evaluates to `true`.
</details>

---

## 11. Practice Exercises

### Exercise 1: Refactoring Classic Switch to Modern Switch Expression (Easy)
- **Task:** Refactor the following legacy switch statement into a concise modern switch expression:
```java
String role = "ADMIN";
int accessLevel;
switch (role) {
    case "ADMIN":
        accessLevel = 1;
        break;
    case "MANAGER":
        accessLevel = 2;
        break;
    case "USER":
    case "GUEST":
        accessLevel = 3;
        break;
    default:
        accessLevel = 0;
        break;
}
```
<details>
<summary>Solution</summary>

```java
String role = "ADMIN";
int accessLevel = switch (role) {
    case "ADMIN" -> 1;
    case "MANAGER" -> 2;
    case "USER", "GUEST" -> 3;
    default -> 0;
};
```
</details>

### Exercise 2: Guarded Pattern HTTP Status Formatter (Medium)
- **Task:** Write a method `String describeHttpStatus(int statusCode)` using a modern pattern matching switch expression with guarded `when` clauses to classify codes into:
  - $100–199$: "Informational (1xx)"
  - $200–299$: "Success (2xx)"
  - $300–399$: "Redirection (3xx)"
  - $400–499$: "Client Error (4xx)"
  - $500–599$: "Server Error (5xx)"
  - Any other value: "Invalid HTTP Status"
<details>
<summary>Solution</summary>

```java
public class HttpStatusMatcher {
    public static String describeHttpStatus(int statusCode) {
        return switch (statusCode) {
            case int c when c >= 100 && c <= 199 -> "Informational (1xx)";
            case int c when c >= 200 && c <= 299 -> "Success (2xx)";
            case int c when c >= 300 && c <= 399 -> "Redirection (3xx)";
            case int c when c >= 400 && c <= 499 -> "Client Error (4xx)";
            case int c when c >= 500 && c <= 599 -> "Server Error (5xx)";
            default -> "Invalid HTTP Status: " + statusCode;
        };
    }

    public static void main(String[] args) {
        System.out.println(describeHttpStatus(200)); // Success (2xx)
        System.out.println(describeHttpStatus(404)); // Client Error (4xx)
        System.out.println(describeHttpStatus(999)); // Invalid HTTP Status
    }
}
```
</details>

### Exercise 3: Sealed AST Evaluator with Record Patterns (Hard)
- **Task:** Implement an arithmetic expression tree evaluator using sealed interfaces, record deconstruction, and pattern matching switch for addition, multiplication, and numeric constants.
<details>
<summary>Solution</summary>

```java
public class ExpressionEvaluator {
    sealed interface Expr permits Constant, Add, Multiply {}
    record Constant(double value) implements Expr {}
    record Add(Expr left, Expr right) implements Expr {}
    record Multiply(Expr left, Expr right) implements Expr {}

    public static double eval(Expr expr) {
        return switch (expr) {
            case Constant(double v) -> v;
            case Add(Expr l, Expr r) -> eval(l) + eval(r);
            case Multiply(Expr l, Expr r) -> eval(l) * eval(r);
        };
    }

    public static void main(String[] args) {
        // Evaluate: (5 + 3) * 2 = 16.0
        Expr expression = new Multiply(
            new Add(new Constant(5), new Constant(3)),
            new Constant(2)
        );

        double result = eval(expression);
        System.out.println("Evaluated AST Result: " + result); // 16.0
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **`if / else`:** Strictly requires primitive or boxed `boolean`; no truthy/falsy evaluation.
- **Ternary Operator:** `condition ? expr1 : expr2`; always produces a typed value.
- **Switch Expressions (Java 14+):** Arrow syntax (`->`) eliminates fall-through; multiple labels (`case A, B ->`); yield values via `yield`.
- **Pattern Matching Switch (Java 21+ / 25):** Matches types directly (`case String s ->`); uses guarded `when` expressions; handles `case null`.
- **Bytecode:** Dense consecutive cases $\to$ `tableswitch` ($O(1)$); sparse cases $\to$ `lookupswitch` ($O(\log N)$).

---

## 13. Further Reading

- [The Java Language Specification (Java SE 25 Edition) - Chapter 14: Blocks, Statements, and Patterns](https://docs.oracle.com/javase/specs/jls/se25/html/jls-14.html)
- [JEP 361: Switch Expressions](https://openjdk.org/jeps/361)
- [JEP 441: Pattern Matching for switch](https://openjdk.org/jeps/441)
- [JEP 440: Record Patterns](https://openjdk.org/jeps/440)

---

| [← Previous: 04_wrapper-classes-and-autoboxing.md](file:///E:/Learning/engineering-notes/01-languages/java/01-fundamentals/04_wrapper-classes-and-autoboxing.md) | [Section Index](file:///E:/Learning/engineering-notes/01-languages/java/01-fundamentals/README.md) | [Next: 06_loops.md](file:///E:/Learning/engineering-notes/01-languages/java/01-fundamentals/06_loops.md) → |
