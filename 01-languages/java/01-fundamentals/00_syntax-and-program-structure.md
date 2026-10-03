# Syntax and Program Structure

A Java program is a set of **classes**. Execution starts at a `main` method. The compiler checks syntax and types before anything runs.

```java
package com.example;                       // 1. package (optional)

import java.util.List;                     // 2. imports (optional)

public class Greeter {                     // 3. class declaration
    public static void main(String[] args) {   // 4. entry point
        String name = "Java";              // 5. statements, each ends with ;
        System.out.println("Hello, " + name + "!");
    }
}
```

## Basic rules

| Rule | Example / note |
|------|----------------|
| Java is **case-sensitive** | `Main` and `main` are different names |
| Statements end with `;` | `int x = 5;` |
| Blocks use `{ }` | Class bodies, methods, loops, `if` |
| Whitespace and line breaks are ignored | Use them for readability only |
| One `public` top-level class per file, file name must match | `public class Greeter` lives in `Greeter.java` |
| A file may contain several non-public top-level classes | Avoid; one class per file is the norm |
| Execution starts in `main` | Only for classes you run directly |

## The `main` method

```java
public static void main(String[] args) { ... }
```

| Part | Meaning |
|------|---------|
| `public` | The JVM must be able to call it from outside |
| `static` | Runs without creating an object ([04-oop/02_static.md](../04-oop/02_static.md)) |
| `void` | Returns nothing |
| `String[] args` | Command-line arguments ([09_console-input-output.md](./09_console-input-output.md)) |

`String... args` and `final String[] args` also work.

### Modern shortcut (Java 25+)

Java 25 finalized **instance main methods** and **compact source files**, so small programs and scripts need no class wrapper:

```java
// Hello.java
void main() {
    IO.println("Hello!");
}
```

```bash
java Hello.java
```

Real projects still use the full class form above, and it works on every Java version, so learn it first.

## Comments

```java
// single-line comment

/* multi-line
   comment */

/**
 * Javadoc comment: documents the next class or method.
 * @param name the name to greet
 * @return the greeting
 */
String greet(String name) { return "Hello, " + name; }
```

Comment the **why**, not the what. `i++; // increment i` adds nothing.

## Statements, expressions, blocks

```java
int x = 5;                  // declaration statement
x = x + 2;                  // expression statement (assignment)
System.out.println(x);      // expression statement (method call)

if (x > 3) {                // block: groups statements, creates a scope
    int y = x * 2;
}                           // y no longer exists here
```

- An **expression** produces a value: `x + 2`, `a > b`, `list.size()`
- A **statement** performs an action; many are expressions followed by `;`

## Identifiers and naming conventions

Identifiers (names) may contain letters, digits, `_` and `$`, and must not start with a digit or be a keyword. Conventions matter more than the rules:

| Kind | Convention | Example |
|------|------------|---------|
| Class, interface, enum, record | `PascalCase`, noun | `OrderService`, `Comparable` |
| Method | `camelCase`, verb | `calculateTotal()` |
| Variable, parameter, field | `camelCase`, noun | `totalPrice` |
| Constant (`static final`) | `UPPER_SNAKE_CASE` | `MAX_RETRIES` |
| Package | lowercase, no underscores | `com.example.orders` |
| Type parameter | single capital letter | `T`, `E`, `K`, `V` |

Prefer descriptive names (`customerCount`) over short ones (`cc`). Single letters are fine for short loop counters (`i`, `j`).

## Keywords

Reserved words cannot be used as names: `class`, `public`, `static`, `void`, `if`, `else`, `for`, `while`, `return`, `new`, `this`, `super`, `try`, `catch`, `final`, `int`, `boolean`, and about 40 more. `true`, `false` and `null` are reserved literals.

`var`, `record`, `sealed`, `yield` and `permits` are **contextual** keywords, reserved only in specific positions. `goto` and `const` are reserved but unused.

## Types of errors

| Kind | When | Example | Message |
|------|------|---------|---------|
| **Syntax error** | Compile time | Missing `;` or `}` | `';' expected` |
| **Type / semantic error** | Compile time | `int x = "hello";` | `incompatible types` |
| **Runtime exception** | While running | `args[0]` with no arguments | `ArrayIndexOutOfBoundsException` |
| **Logic error** | While running, silently wrong | `average = sum / count` using int division | Wrong output, no message |

Compile errors are the easiest: the compiler tells you the line. Logic errors are the hardest, which is why [debugging](../06-exceptions-and-debugging/06_stack-traces-and-debugging.md) and tests matter.

## Common compiler messages

| Message | Usual cause |
|---------|-------------|
| `cannot find symbol` | Typo, missing import, variable out of scope |
| `';' expected` | Missing semicolon (often reported on the line after the real problem) |
| `class X is public, should be declared in a file named X.java` | File name differs from class name |
| `incompatible types: possible lossy conversion from double to int` | Needs a cast ([type casting](./03_type-casting-and-conversion.md)) |
| `variable x might not have been initialized` | Local variable used before assignment |
| `missing return statement` | Some path through a non-`void` method returns nothing |
| `unreachable statement` | Code after `return`, `break` or an infinite loop |
| `reached end of file while parsing` | Unbalanced braces |

Read the **first** error first; later errors are often caused by it.

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| `Main` vs `main`, `String` vs `string` | `cannot find symbol` | Java is case-sensitive |
| Wrong `main` signature | `Error: Main method not found` | `public static void main(String[] args)` |
| `System.out.println` typos (`system`, `Println`) | `cannot find symbol` | Exact case: `System.out.println` |
| Missing closing brace | `reached end of file while parsing` | Let the IDE format and match braces |
| Using `'text'` for strings | `unclosed character literal` | Strings use `"..."`, chars use `'a'` |
| Public class name differs from file name | Compile error | Rename one |

## Key takeaways

- A program is classes; `public static void main(String[] args)` is the classic entry point
- Java is case-sensitive; statements end with `;`; blocks use `{ }` and create scopes
- Follow naming conventions: `PascalCase` types, `camelCase` members, `UPPER_SNAKE` constants
- Read compiler errors top-down; fix the first one first

**Next:** [Variables and Data Types](./01_variables-and-data-types.md)
