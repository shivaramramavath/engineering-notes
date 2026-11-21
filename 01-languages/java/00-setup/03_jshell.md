# JShell: Interactive Java REPL and Prototyping Engine

> JShell (Java Shell) is the official interactive Read-Eval-Print Loop (REPL) and programmatic evaluation API (`jdk.jshell`) introduced in Java 9, enabling instant expression evaluation, API discovery, and zero-ceremony code prototyping without writing class wrappers or compilation harnesses.

| Metadata | Value |
| :--- | :--- |
| **Level** | Beginner |
| **Prerequisites** | [00_jdk-installation.md](file:///E:/Learning/engineering-notes/01-languages/java/00-setup/00_jdk-installation.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Java LTS Baseline** | Java 25 LTS (HotSpot JShell CLI & `jdk.jshell` API) |

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
Historically, evaluating a simple Java expression—such as testing a regular expression, checking date arithmetic, or observing the behavior of a stream pipeline—required significant ceremony. A developer had to launch an IDE, create a project or scratch file, declare a package, declare a `public class`, declare a `public static void main(String[] args)` method, compile the file, and execute the process. This heavy cognitive overhead discouraged rapid experimentation and placed Java at a disadvantage compared to REPL-driven languages like Python, Ruby, and Node.js.

### Historical Context & JEP 222
To solve this, OpenJDK delivered **JEP 222 (jshell: The Java Shell)** in Java 9. JShell introduced both:
1. A rich, terminal-based REPL tool (`jshell`) bundled directly in the JDK `bin/` directory.
2. A public, modular programmatic evaluation API (`jdk.jshell`) allowing IDEs, notebooks, and tools to execute arbitrary Java code snippets dynamically in an isolated JVM.

### Real-World Relevance
In modern engineering workflows, JShell is widely used for:
- Testing stream transformations and lambda pipelines before embedding them in production code.
- Interactively debugging third-party libraries by importing external JARs via `/env --class-path`.
- Validating regex patterns, time-zone offsets, and numeric edge cases.
- Prototyping modern language features (such as Java 25 Stream Gatherers, Records, and Pattern Matching) without scaffold overhead.

---

## 2. Core Concepts

### Snippet Types
JShell classifies user inputs into distinct structural units called **Snippets**:
1. **Expressions:** Inputs that yield a value (e.g., `2 + 2` or `Math.sin(1.5)`). JShell automatically captures the value into a synthetic scratch variable (`$1`, `$2`, `$3`).
2. **Statements:** Control flow or void operations (e.g., `System.out.println("Hello");` or `for (int i = 0; i < 3; i++) { ... }`). Semicolons are optional for single-line expressions, but required within multi-statement blocks.
3. **Declarations:** Variables, methods, classes, records, and interfaces. Declarations are registered in the global session scope.
4. **Commands:** Control directives starting with a slash (`/`), such as `/vars`, `/methods`, `/list`, `/open`, and `/exit`.

### Key Features of JShell
- **Scratch Variables:** Whenever an expression is evaluated without explicit assignment, JShell assigns the result to an incrementing variable (`$1`, `$2`, etc.), allowing immediate reuse in subsequent snippets.
- **Forward Referencing:** JShell allows you to declare a method that calls another method or references a class that has not yet been defined. The method remains in an "unresolved" state until the missing component is provided.
- **Dynamic Redefinition:** You can redefine an existing variable type, method signature, or class declaration without restarting the session. JShell automatically re-evaluates all dependent snippets.
- **Auto-Imports:** By default, JShell pre-loads standard packages: `java.base` (including `java.lang`, `java.io`, `java.util`, `java.util.concurrent`, `java.util.stream`).

---

## 3. How It Works Internally

### JShell Architecture: Dual-Process IPC Model
JShell does not compile and run code inside the shell's own process. Instead, it operates a robust **dual-process architecture** connected via socket IPC and the Java Debug Interface (JDI):

```
+-----------------------------------------------------------------------------------+
|                              JShell Process (Host CLI)                            |
|                                                                                   |
|   [ Terminal Input ]                                                              |
|          │                                                                        |
|          ▼                                                                        |
|   [ Snippet Parser & AST Analyzer ]                                               |
|          │                                                                        |
|          ▼                                                                        |
|   [ Wrap Engine: Synthesizes synthetic class around snippet ]                     |
|          │                                                                        |
|          ▼                                                                        |
|   [ In-Memory Compiler (javax.tools.JavaCompiler) -> Produces bytecode ]          |
|          │                                                                        |
+──────────┼────────────────────────────────────────────────────────────────────────+
           │ Socket IPC / JDI (Java Debug Interface) Transport
           ▼
+-----------------------------------------------------------------------------------+
|                         Remote Target JVM (Worker Engine)                         |
|                                                                                   |
|   [ Target ClassLoader loads synthesized class bytecode ]                         |
|          │                                                                        |
|          ▼                                                                        |
|   [ Invokes synthesized method via Reflection ]                                   |
|          │                                                                        |
|          ▼                                                                        |
|   [ Captures returned object or uncaught exception ]                              |
|          │                                                                        |
|          ▼                                                                        |
|   [ Serializes result value back to Host CLI ] ───► Prints to User Terminal       |
+-----------------------------------------------------------------------------------+
```

### Mechanical Breakdown
1. **Source Wrapping:** When you type `int x = 42;`, JShell wraps this snippet into a synthetic top-level class:
   ```java
   public class REPL.$1 {
       public static int x = 42;
   }
   ```
2. **In-Memory Compilation:** JShell invokes the Java Compiler API (`javax.tools.ToolProvider.getSystemJavaCompiler()`) to generate bytecode entirely in RAM.
3. **Remote Class Loading:** The bytecode is pushed over a local socket connection to the subordinate target JVM process.
4. **State Isolation:** Because code executes inside the subordinate JVM, an infinite loop or `System.exit()` does not crash the host JShell CLI. The host catches the exit event or handles thread interruption via `Ctrl+C`.

---

## 4. Syntax & Usage

### Launching JShell
```bash
# Standard interactive launch
$ jshell

# Launch with verbose output (prints types and evaluation details)
$ jshell -v

# Launch enabling Java 25 preview features
$ jshell --enable-preview

# Launch with custom external dependencies on the classpath
$ jshell --class-path /path/to/gson.jar:/path/to/guava.jar

# Pass memory tuning flags to the remote worker JVM
$ jshell -R-Xmx512m -R-XX:+UseZGC
```

### Essential JShell Commands
| Command | Action / Purpose |
| :--- | :--- |
| `/vars` | List all active variables, types, and values in the current session. |
| `/methods` | List all declared methods and their signatures. |
| `/types` | List all active classes, records, interfaces, and enums. |
| `/list` | Display the chronological history of all successfully executed snippets. |
| `/open <file>` | Load and execute a `.java` or `.jsh` script file into the session. |
| `/save <file>` | Save all active snippets from the current session into a script file. |
| `/edit <id>` | Open a visual editor (or configured CLI editor) to modify a snippet. |
| `/env --class-path <path>` | Dynamically add external JARs or directories to the runtime classpath. |
| `/reset` | Clear all variables, types, and methods, resetting the worker JVM state. |
| `/exit` | Terminate the JShell session and return to the OS shell. |

---

## 5. Practical Examples

### Example 1: Basic Expressions & Scratch Variables
- **Goal:** Launch JShell, evaluate mathematical expressions, and use synthetic scratch variables (`$1`, `$2`) to build composite calculations.
```text
$ jshell
|  Welcome to JShell -- Version 25
|  For an introduction type: /help intro

jshell> 15 * 4
$1 ==> 60

jshell> $1 + 100
$2 ==> 160

jshell> Math.sqrt($2)
$3 ==> 12.649110640673518

jshell> String greeting = "Hello, Java " + Runtime.version().feature()
greeting ==> "Hello, Java 25"
```
- **Explanation:** Every bare expression automatically creates an incrementing scratch identifier (`$1`, `$2`, `$3`), enabling interactive chaining without redundant variable declarations.

---

### Example 2: Forward Referencing & Method Redefinition
- **Goal:** Demonstrate declaring a method that references a non-existent method, observing JShell's unresolved state, and resolving it dynamically.
```text
jshell> double calculateTotal(double base) {
   ...>     return base + calculateTax(base);
   ...> }
|  created method calculateTotal(double), however, it cannot be invoked until method calculateTax(double) is declared

jshell> calculateTotal(100.0)
|  attempted to call method calculateTotal(double) which cannot be invoked until method calculateTax(double) is declared

jshell> double calculateTax(double base) {
   ...>     return base * 0.15;
   ...> }
|  created method calculateTax(double)

jshell> calculateTotal(100.0)
$7 ==> 115.0
```
- **Explanation:** Forward referencing decouples declaration order from execution readiness. As soon as `calculateTax` is defined, `calculateTotal` transitions from unresolved to callable without recompiling callers.

---

### Example 3: External Dependency Exploration via Classpath
- **Goal:** Dynamically import an external JSON library or utility JAR during an active JShell session without restarting.
```text
jshell> /env --class-path C:/libs/fast-json.jar
|  Setting new options and restoring state.

jshell> import com.example.json.JsonParser;

jshell> JsonParser.parse("{\"status\":\"active\"}")
$9 ==> {status=active}
```
- **Explanation:** `/env --class-path` re-configures the ClassLoader of the remote JVM worker, permitting live exploration of external SDKs, database drivers, or corporate domain models.

---

### Example 4: Session Persistence with `/save` and `/open`
- **Goal:** Prototype a data structure interactively, save it to a script file, reset the session, and restore it instantly.
```text
jshell> record User(String id, String email) {}
|  created record User

jshell> var users = List.of(new User("u1", "a@test.com"), new User("u2", "b@test.com"));
users ==> [User[id=u1, email=a@test.com], User[id=u2, email=b@test.com]]

jshell> /save C:/temp/session.jsh

jshell> /reset
|  Resetting state.

jshell> /vars
|  (empty)

jshell> /open C:/temp/session.jsh

jshell> users.getFirst()
$3 ==> User[id=u1, email=a@test.com]
```
- **Explanation:** `/save` dumps the active session's snippet definitions to disk in valid `.jsh` format. `/open` executes the file line-by-line, restoring all types and variables.

---

### Example 5: High-Performance / Modern Idiom — Programmatic JShell API
- **Goal:** Embed the `jdk.jshell` API directly inside a Java application to dynamically evaluate user expressions at runtime.
```java
import jdk.jshell.JShell;
import jdk.jshell.SnippetEvent;
import java.util.List;

public class ProgrammaticJShell {
    public static void main(String[] args) {
        try (JShell jshell = JShell.create()) {
            List<SnippetEvent> events = jshell.eval("int x = 42;");
            for (SnippetEvent e : events) {
                System.out.println("Status: " + e.status() + ", Value: " + e.value());
            }

            events = jshell.eval("x * 2;");
            for (SnippetEvent e : events) {
                System.out.println("Result: " + e.value());
            }
        }
    }
}
```
- **Console Output:**
```text
Status: VALID, Value: 42
Result: 84
```
- **Explanation:** Modern Java tools, debuggers, educational platforms, and interactive notebook kernels (such as Jupyter Java kernels) leverage `jdk.jshell.JShell` to execute arbitrary code safely with structured programmatic events.

---

## 6. Best Practices

1. **Use Tab Completion Extensively:** Pressing `<Tab>` in JShell provides rich code-completion for method names, packages, and overload signatures. Pressing `<Shift>+<Tab>` displays detailed Javadoc documentation for the method at the cursor.
2. **Leverage `/edit` for Multi-Line Blocks:** Avoid typing large 50-line algorithms directly in the raw terminal prompt. Use `/edit <method-name>` to launch an editor window where you can format and refine code comfortably.
3. **Pass `-R` Flags for Heavy Prototyping:** If you are testing memory-intensive workloads or custom GC algorithms in JShell, pass flags directly to the remote worker: `jshell -R-Xmx4g -R-XX:+UseZGC`.
4. **Use `/reset` Between Experiments:** Residual variables and type redefinitions can leak state and cause subtle side effects. Run `/reset` before beginning an unrelated prototyping task.
5. **Save Scratch Work Regularly:** Create a habits of running `/save myscript.jsh` before closing the terminal so useful snippets are never lost.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Forgetting Semicolons in Multi-line Loops** | Semicolons are optional for top-level single expressions, but required within control flow bodies. | Include standard semicolons inside `for`, `while`, and `if` blocks. |
| **Expecting Package Declarations to Work** | JShell executes inside a synthetic default package; declaring `package com.foo;` is illegal. | Do not use `package` statements in JShell; use imports directly. |
| **Confusing `/vars` with Local Variables** | `/vars` only tracks top-level session variables; variables declared inside a method or loop are local and invisible to `/vars`. | Inspect variables by returning them or assigning them to top-level variables. |
| **Calling Unresolved Methods** | Invoking a forward-referenced method before defining its dependencies causes an immediate runtime exception. | Define all prerequisite methods before executing the caller. |
| **Using `System.exit(0)` to Close JShell** | While the host CLI survives because of the dual-process architecture, the worker process terminates abruptly, dropping session state. | Use `/exit` to cleanly close the session. |
| **Classpath Confusion with `-cp` vs `/env`** | Setting `-cp` only works at launch; updating classpath after launch requires `/env --class-path`. | Use `/env --class-path` for dynamic mid-session additions. |

---

## 8. Performance & Security Notes

### Performance Overhead
JShell is designed for exploratory prototyping, not production throughput:
- Each snippet undergoes dynamic code generation, in-memory compilation via `javac`, and IPC transmission over local sockets.
- HotSpot does not optimize short-lived JShell snippets as aggressively as pre-compiled classes running in a long-lived JVM.
- Do not run high-frequency performance benchmarks inside JShell; use JMH in a standalone build instead.

### Security Considerations
The programmatic `jdk.jshell` API provides arbitrary code execution capabilities. Never expose an unrestricted `JShell` evaluation endpoint over an unauthenticated HTTP or network API, as users can execute arbitrary system commands via `ProcessBuilder` or access host filesystems.

---

## 9. Comparison / When to Use What

| Tool / Approach | Startup Ceremony | Execution Feedback | Persistence | Ideal Use Case |
| :--- | :--- | :--- | :--- | :--- |
| **JShell (JDK REPL)** | **Zero** (Instant terminal launch) | **Immediate** (Prints value on Enter) | Ephemeral (Saved via `/save`) | Quick API experiments, regex testing, learning syntax |
| **Single-File `java App.java`** | Very Low (Requires `void main()`) | Full program run | Permanent (`.java` on disk) | Scripting, CLI utilities, automation hooks |
| **IDE Scratch File** | Low (Requires IDE open) | Fast (IDE runner) | Saved in IDE workspace | Multi-class prototyping, refactoring experiments |
| **JUnit 5 Scratch Test** | Moderate (Requires test harness) | Structured assertions | Integrated with git repo | Regression tests, verified algorithmic assertions |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the fundamental architecture of JShell, and why does it use a dual-process model?</strong></summary>

**Answer:**  
JShell uses a dual-process architecture comprising a **Host CLI process** and a **Target Worker JVM process** communicating over socket IPC and the Java Debug Interface (JDI). The Host CLI handles user input, syntax validation, wrapping snippets into synthetic class constructs, and compiling them via `javax.tools.JavaCompiler`. The compiled bytecode is transmitted to the Target JVM for execution. This separation guarantees fault isolation: if user code triggers an infinite loop, throws an OutOfMemoryError, or calls `System.exit()`, the host CLI remains responsive and can interrupt or restart the worker without losing shell state.
</details>

<details>
<summary><strong>2. What is "Forward Referencing" in JShell, and how does the compiler facilitate it?</strong></summary>

**Answer:**  
Forward referencing allows a user to define a method, variable, or class that references another identifier that has not yet been declared. The JShell compiler wraps the snippet and registers it with status `UNRESOLVED`. The snippet cannot be invoked yet, but it exists in the symbol table. As soon as the missing dependency is declared in a subsequent snippet, JShell's dependency graph updates the status to `VALID`, enabling execution without manual re-declaration.
</details>

<details>
<summary><strong>3. How does JShell handle variable and method redefinition under the hood?</strong></summary>

**Answer:**  
When an existing variable or method name is re-declared (even with a different type or signature), JShell creates a new synthetic wrapper class (e.g., `REPL.$2`) and recompiles the snippet. It updates its internal symbol table so that future snippet references resolve to the new wrapper class. It also checks all dependent snippets: if a dependent snippet becomes incompatible with the new type, its status transitions back to `UNRESOLVED` with diagnostic warnings.
</details>

<details>
<summary><strong>4. How do synthetic scratch variables (`$1`, `$2`, etc.) work in JShell?</strong></summary>

**Answer:**  
When JShell evaluates an input categorized as an expression rather than a statement or declaration, it synthesizes an implicit variable declaration:
`$N = <expression>;` where `N` is an auto-incrementing integer. The variable is given the inferred type of the expression. It is added to the session's active variable registry, making it accessible to subsequent snippets just like a manually declared variable.
</details>

<details>
<summary><strong>5. Can JShell evaluate preview language features, and how do you configure it?</strong></summary>

**Answer:**  
Yes. JShell fully supports preview features by passing the standard compiler flags at launch:
`jshell --enable-preview`. This configures both the in-memory `JavaCompiler` and the target worker JVM to permit preview syntax, allowing developers to test experimental features (such as Structured Concurrency) without build configuration.
</details>

<details>
<summary><strong>6. How can an external library JAR be loaded into an active JShell session without restarting?</strong></summary>

**Answer:**  
By using the slash command `/env --class-path <path-to-jar>`. JShell resets the execution engine ClassLoader, updates the classpath search paths, and replays all active snippets from the current session history into the newly configured environment.
</details>

<details>
<summary><strong>7. What are the key classes in the `jdk.jshell` programmatic API?</strong></summary>

**Answer:**  
The programmatic API is centered on:
- `jdk.jshell.JShell`: The primary controller and session instance (created via `JShell.create()`).
- `jdk.jshell.Snippet`: Represents a piece of source code (expression, statement, declaration).
- `jdk.jshell.SnippetEvent`: Emitted upon snippet evaluation, providing status (`VALID`, `REJECTED`), prior status, exception details, and the resulting string value.
</details>

<details>
<summary><strong>8. Why are package declarations disallowed in JShell snippets?</strong></summary>

**Answer:**  
JShell executes all user snippets inside a unified synthetic package managed internally by the runtime. Allowing arbitrary package declarations across snippets would fragment the evaluation context into isolated namespaces, preventing subsequent snippets from accessing previously defined variables and methods without verbose fully qualified names.
</details>

---

## 11. Practice Exercises

### Exercise 1: Interactive Expression Prototyping (Easy)
- **Task:** Using JShell, calculate the number of days between today and January 1, 2030, using the modern `java.time` API.
<details>
<summary>Solution</summary>

```text
jshell> import java.time.LocalDate;
jshell> import java.time.temporal.ChronoUnit;

jshell> LocalDate today = LocalDate.now();
today ==> 2026-11-21

jshell> LocalDate future = LocalDate.of(2030, 1, 1);
future ==> 2030-01-01

jshell> ChronoUnit.DAYS.between(today, future)
$3 ==> 1137
```
</details>

### Exercise 2: Method Redefinition and Dependency Tracking (Medium)
- **Task:** In JShell, define a method `double discount(double p)` that applies a 10% discount. Then define `double finalPrice(double p)` that calls `discount(p)`. Verify `finalPrice(100.0)` returns `90.0`. Then, redefine `discount(double p)` to apply a 20% discount and verify that calling `finalPrice(100.0)` automatically reflects the new 20% logic without redefining `finalPrice`.
<details>
<summary>Solution</summary>

```text
jshell> double discount(double p) { return p * 0.90; }
|  created method discount(double)

jshell> double finalPrice(double p) { return discount(p); }
|  created method finalPrice(double)

jshell> finalPrice(100.0)
$3 ==> 90.0

jshell> double discount(double p) { return p * 0.80; }
|  modified method discount(double)

jshell> finalPrice(100.0)
$5 ==> 80.0
```
</details>

### Exercise 3: Programmatic JShell Evaluation Engine (Hard)
- **Task:** Write a standalone Java program that utilizes the `jdk.jshell.JShell` API to accept a mathematical formula string (e.g., `"(12 * 5) + (100 / 4)"`), evaluates it dynamically, and returns the computed result as a `double`.
<details>
<summary>Solution</summary>

```java
import jdk.jshell.JShell;
import jdk.jshell.SnippetEvent;
import java.util.List;

public class MathEvaluator {
    public static double evaluateFormula(String formula) {
        try (JShell jshell = JShell.create()) {
            List<SnippetEvent> events = jshell.eval(formula + ";");
            for (SnippetEvent e : events) {
                if (e.status() == jdk.jshell.Snippet.Status.VALID) {
                    return Double.parseDouble(e.value());
                }
            }
        } catch (Exception ex) {
            System.err.println("Evaluation failed: " + ex.getMessage());
        }
        return Double.NaN;
    }

    public static void main(String[] args) {
        String expr = "(12.0 * 5) + (100.0 / 4)";
        double result = evaluateFormula(expr);
        System.out.printf("Formula: %s = %.2f%n", expr, result);
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Launch:** Run `jshell` from your terminal; use `--enable-preview` to test bleeding-edge features.
- **Scratch Variables:** Unassigned expressions become `$1`, `$2`, `$3` automatically.
- **Forward References:** Methods can reference symbols before they are defined.
- **Key Commands:**
  - `/vars`: View active variables.
  - `/methods`: View active methods.
  - `/types`: View classes, records, and interfaces.
  - `/open <file>`: Load script.
  - `/save <file>`: Save session.
  - `/reset`: Clear state.
  - `/exit`: Quit JShell.
- **Programmatic API:** Use `jdk.jshell.JShell` to embed live Java evaluation into custom tooling.

---

## 13. Further Reading

- [JEP 222: jshell: The Java Shell (Read-Eval-Print Loop)](https://openjdk.org/jeps/222)
- [Official Oracle JShell User Guide](https://docs.oracle.com/en/java/javase/25/jshell/introduction-jshell.html)
- [Module jdk.jshell API Specification](https://docs.oracle.com/en/java/javase/25/docs/api/jdk.jshell/jdk/jshell/package-summary.html)

---

| [← Previous: 02_how-java-works.md](file:///E:/Learning/engineering-notes/01-languages/java/00-setup/02_how-java-works.md) | [Section Index](file:///E:/Learning/engineering-notes/01-languages/java/00-setup/README.md) | [Next: 04_intellij-idea.md](file:///E:/Learning/engineering-notes/01-languages/java/00-setup/04_intellij-idea.md) → |
