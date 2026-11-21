# Syntax and Program Structure in Java

> Java programs are organized into modular compilation units containing package declarations, imports, type definitions, and entry points, bridging traditional object-oriented structure with modern compact source files and module imports.

| Metadata | Value |
| :--- | :--- |
| **Level** | Beginner |
| **Prerequisites** | [00-setup/02_how-java-works.md](file:///E:/Learning/engineering-notes/01-languages/java/00-setup/02_how-java-works.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Java LTS Baseline** | Java 25 LTS (with Java 8, 11, 17, 21 comparison) |

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
A programming language requires unambiguous lexical and grammatical rules to govern how source code is written, parsed, and executed. Programs must establish strict boundaries for namespaces (to avoid symbol collisions across global libraries), declare explicit visibility rules (to enforce encapsulation), and define well-known entry points for runtime execution.

### Historical Context & Modern Evolution
- **Java 1.0 to 21:** Java strictly required that every executable program reside inside a declared `class`, and that every executable entry point match the signature `public static void main(String[] args)`. Furthermore, each file could only contain one `public` top-level class whose name exactly matched the filename on disk.
- **Java 23 (JEP 467):** Introduced **Markdown Documentation Comments** (`///`), replacing HTML tags inside Javadoc comments with standard CommonMark syntax.
- **Java 25 (JEP 511 & 512):** Delivered two major milestones:
  - **Module Import Declarations (JEP 511):** Allows importing an entire module's public packages at once via `import module java.base;`, ending import lists of 30+ separate package statements.
  - **Compact Source Files & Instance Main Methods (JEP 512):** Enables zero-ceremony programming: files can omit the `public class` wrapper and `String[] args`, executing directly via `void main()`.

### Real-World Relevance
Understanding program structure—from enterprise multi-package domain models to single-file automation scripts—enables developers to write clean, maintainable, and idiomatic Java that scales seamlessly from small microservices to massive enterprise codebases.

---

## 2. Core Concepts

### The Anatomy of a Compilation Unit (`.java` file)
A canonical Java source file follows a rigid ordering of declarations:

```
+---------------------------------------------------------------------------------+
|  1. Package Declaration (Optional, at most one)                                 |
|     package com.enterprise.billing;                                            |
|                                                                                 |
|  2. Import Declarations (Zero or more)                                          |
|     import java.util.List;                                                      |
|     import static java.lang.Math.PI;                                            |
|     import module java.base;                     // Java 25 Module Import       |
|                                                                                 |
|  3. Type Declarations (Class, Interface, Record, Enum)                          |
|     public class InvoiceProcessor {                                             |
|         // Fields, Constructors, Methods                                        |
|         public static void main(String[] args) { ... }                          |
|     }                                                                           |
|                                                                                 |
|  4. Package-Private Non-Public Types (Zero or more)                             |
|     class InternalHelper { ... }                                                |
+---------------------------------------------------------------------------------+
```

### Key Elements Explained
1. **Package Declaration:** Maps directly to the directory hierarchy on disk. Prevents naming conflicts across different organizations (e.g., `com.google` vs. `org.apache`).
2. **Import Statements:** Informs the compiler where to find types referenced in the source code without typing fully qualified names (FQN) like `java.util.concurrent.ConcurrentHashMap`.
3. **Class/Record/Interface Header:** Defines the type name, access modifier (`public` or package-private default), and type parameters (generics).
4. **The `main` Method Entry Point:**
   - **Canonical (Java 1.0–25):** `public static void main(String[] args)`
   - **Compact / Instance (Java 25+):** `void main()` inside an undeclared implicit class.
5. **Comments:**
   - Single-line: `// ...`
   - Multi-line block: `/* ... */`
   - Traditional Javadoc: `/** ... */`
   - Modern Markdown Javadoc: `/// ...` (Java 23+)

---

## 3. How It Works Internally

### Source Parsing to Bytecode Entry Point Dispatch
When `javac` compiles a source file, it transforms text tokens into an Abstract Syntax Tree (AST), performs type attribution and flow checks, and emits bytecode:

```
Source Code (App.java)
       │
       ▼
Lexical Analysis (Scanner) ──► Tokens (KEYWORDS, IDENTIFIERS, LITERALS, SEPARATORS)
       │
       ▼
Syntactic Analysis (Parser) ──► Abstract Syntax Tree (JCTree)
       │
       ▼
Semantic Analysis (Enter / MemberEnter / Attr / Flow) ──► Symbol Table Resolution
       │
       ▼
Code Generation (Gen) ──► Generates App.class Bytecode
```

### The Launcher Dispatch Logic for `main`
When the `java` launcher boots the JVM, it searches for the entry point according to a prioritized resolution protocol:
1. `public static void main(String[] args)` (Standard entry point)
2. `public static void main()` (Parameterless static main)
3. `void main(String[] args)` (Instance main with args)
4. `void main()` (Parameterless instance main)

If an instance `main()` method is selected, the launcher invokes the zero-argument default constructor of the class via `invokespecial <init>()` to instantiate an object, and then invokes `main()` on that instance.

---

## 4. Syntax & Usage

### Naming Conventions (Standard Java Style)
- **Classes, Records, Interfaces, Enums:** `UpperCamelCase` (PascalCase), e.g., `OrderService`, `PaymentStatus`, `UserRepository`.
- **Methods and Variables:** `lowerCamelCase`, e.g., `calculateTotal()`, `userEmail`, `isAvailable`.
- **Constants (`static final`):** `UPPER_SNAKE_CASE`, e.g., `MAX_RETRY_COUNT`, `DEFAULT_TIMEOUT_MS`.
- **Packages:** Strict lowercase dot-separated, e.g., `com.enterprise.order.service`.

### Lexical Rules and Identifiers
- An identifier must start with a letter (`a-z`, `A-Z`), currency character (`$`), or underscore (`_`). Subsequent characters may include digits (`0-9`).
- Identifiers cannot match reserved keywords (`class`, `public`, `return`, `switch`, `while`, etc.).
- A single underscore (`_`) is a reserved keyword in modern Java (used as an unnamed variable or pattern matcher since Java 21) and cannot be used as an identifier name.

---

## 5. Practical Examples

### Example 1: Classic Canonical Class Structure
- **Goal:** Demonstrate an enterprise Java class with package declaration, imports, fields, methods, and entry point.
```java
package com.learning.fundamentals;

import java.time.LocalDate;
import java.util.List;

public class CanonicalProgram {
    private static final String APP_NAME = "Enterprise Core";

    public static void main(String[] args) {
        System.out.println("Starting: " + APP_NAME);
        System.out.println("Current Date: " + LocalDate.now());
        
        List<String> modules = List.of("Auth", "Billing", "Reports");
        for (String mod : modules) {
            System.out.println("Loaded module: " + mod);
        }
    }
}
```
- **Console Output:**
```text
Starting: Enterprise Core
Current Date: 2026-11-21
Loaded module: Auth
Loaded module: Billing
Loaded module: Reports
```
- **Explanation:** Demonstrates canonical package grouping, standard library imports, `static final` constant definition, and iteration inside `main`.

---

### Example 2: Modern Java 25 Compact Source File
- **Goal:** Leverage Java 25 Compact Source Files (JEP 512), Module Imports (JEP 511), and Markdown Javadoc (JEP 467) for zero-boilerplate scripts.
```java
// CompactMain.java
import module java.base;

/// # System Verification Script
/// Computes system metrics without explicit class declaration or main ceremony.
void main() {
    IO.println("Java 25 Instance Main Method & Module Import Verified!");
    List<String> list = List.of("Alpha", "Beta", "Gamma");
    IO.println("List: " + list);
    IO.println("Available Cores: " + Runtime.getRuntime().availableProcessors());
}
```
- **Execution & Output:**
```bash
$ java CompactMain.java
```
```text
Java 25 Instance Main Method & Module Import Verified!
List: [Alpha, Beta, Gamma]
Available Cores: 16
```
- **Explanation:** In Java 25, the compiler automatically synthesizes an implicit top-level class and instantiates it to run `void main()`. `import module java.base;` imports all public packages of `java.base` at once, and `IO.println` is automatically available.

---

### Example 3: Static Imports for Fluent Code
- **Goal:** Simplify code readability when frequently calling static utility methods.
```java
package com.learning.fundamentals;

import static java.lang.Math.PI;
import static java.lang.Math.pow;
import static java.lang.Math.sqrt;

public class StaticImportDemo {
    public static void main(String[] args) {
        double radius = 5.0;
        double area = PI * pow(radius, 2);
        double hypotenuse = sqrt(pow(3.0, 2) + pow(4.0, 2));

        System.out.printf("Circle Area: %.2f%n", area);
        System.out.printf("Hypotenuse:  %.2f%n", hypotenuse);
    }
}
```
- **Console Output:**
```text
Circle Area: 78.54
Hypotenuse:  5.00
```
- **Explanation:** `import static` imports static members and methods directly into scope, avoiding repetitive `Math.` prefixes.

---

### Example 4: Failure Case — Identifier and Filename Mismatches
- **Goal:** Demonstrate common compile-time errors arising from structural violations.
```java
// File name: MyService.java
public class DifferentName { // ERROR: Class is public, must be named MyService
    public static void main(String[] args) {
        int 1stPlace = 1;     // ERROR: Identifiers cannot start with a digit
        int _ = 42;           // ERROR: As of Java 9, '_' is a keyword, not a valid identifier
    }
}
```
- **Compiler Error (`javac MyService.java`):**
```text
MyService.java:2: error: class DifferentName is public, should be declared in a file named DifferentName.java
MyService.java:4: error: <identifier> expected
        int 1stPlace = 1;
           ^
MyService.java:5: error: as of release 9, '_' is a keyword, and may not be used as an identifier
        int _ = 42;
            ^
```
- **Remedy:** Ensure the public class name strictly matches the `.java` filename, avoid leading digits in identifiers, and use meaningful variable names instead of lone underscores.

---

### Example 5: High-Performance / Modern Idiom — Single-Source Executable Scripts (Shebang)
- **Goal:** Run Java source code directly as an executable UNIX shell script using standard shebang syntax.
```bash
#!/usr/bin/env java --source 25

void main() {
    System.out.println("Executed directly as a POSIX script!");
    System.out.println("Current PID: " + ProcessHandle.current().pid());
}
```
- **Execution:**
```bash
$ chmod +x script.java
$ ./script.java
# Output: Executed directly as a POSIX script! Current PID: 12450
```
- **Explanation:** Introduced in Java 11 (JEP 330), the `java` launcher ignores the first line if it begins with `#!`, allowing Java to serve as a high-performance alternative to Bash and Python scripts for systems automation.

---

## 6. Best Practices

1. **One Public Class per File:** Maintain strict adherence to the one-public-class-per-file rule. Package-private helper classes in the same file should only be used for small, tightly coupled internal implementations.
2. **Avoid Wildcard Imports (`import java.util.*`):** Explicitly import the classes you use. Wildcard imports lead to compilation errors when two packages introduce identically named classes (e.g., `java.util.Date` vs. `java.sql.Date` or `java.util.List` vs. `java.awt.List`).
3. **Use Markdown Javadoc (`///`):** For new projects targeting Java 23+, adopt the `///` Markdown syntax for Javadoc. It is significantly more readable in plain text than legacy HTML `<p>` and `<ul>` tags.
4. **Group Imports Logically:** Organize imports into standard groups separated by a blank line: standard Java (`java.*`, `javax.*`), third-party libraries, organizational domain packages, and static imports.
5. **Use Meaningful Identifiers:** Never use abbreviations like `usr` or single letters (except `i`, `j` for loop indices). Prefer clear names like `activeUserCount` and `orderRepository`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Class name differs from file name** | The Java compiler requires the public class name to match the file base name. | Rename file to `<ClassName>.java` or make class package-private. |
| **Using `_` as a variable name** | Since Java 9, `_` is a reserved keyword; in Java 21+ it represents unnamed patterns. | Use descriptive names like `unused` or `ignored`. |
| **Forgetting `package` statement** | Classes without a package live in the "unnamed package" and cannot be imported by packaged classes. | Always declare an explicit package in production code. |
| **Accidental semicolon after class header** | `public class App; { ... }` causes syntax error. | Do not place semicolons after class, method, or loop headers. |
| **Confusing `static import` with package import** | `import java.lang.Math.PI;` fails; static members require `import static`. | Use `import static <Class>.<member>;`. |
| **Overusing `public` on helper classes** | Violates encapsulation by exposing internal classes to external consumers. | Keep internal helpers package-private (omit modifier) or `private static`. |

---

## 8. Performance & Security Notes

### Package-Private Encapsulation as a Security Boundary
Omitting an access modifier makes a class or method **package-private** (accessible only to classes in the exact same package). This is a powerful architectural pattern: keep domain services and repositories package-private, exposing only an interface or facade as `public`. This prevents consumer code in other modules from bypassing validation logic or accessing implementation internals.

### Compilation Performance and Import Scanning
Import statements have **zero runtime overhead**. During compilation, `javac` uses imports to resolve short names to fully qualified names (FQN) in its internal symbol table. Once compiled into `.class` bytecode, all class references are fully qualified (e.g., `Ljava/util/List;`). Wildcard imports (`import java.util.*`) slightly increase compiler symbol lookup time, but have zero effect on bytecode size or runtime JVM performance.

---

## 9. Comparison / When to Use What

| Feature / Pattern | Java 8 | Java 11 / 17 | Java 21 | Java 25 (Current LTS) |
| :--- | :--- | :--- | :--- | :--- |
| **Entry Point** | `public static void main(String[] args)` required | Same | Instance `main` in preview | **`void main()` Final (JEP 512)** |
| **Class Wrapper** | Mandatory `class` wrapper | Mandatory `class` wrapper | Undeclared class in preview | **Implicit / Compact Class Final** |
| **Single-File Launch** | Manual `javac` + `java` | `java App.java` (JEP 330) | Supported | Supported |
| **Module Imports** | None (Individual packages only) | JPMS `module-info` | Individual packages | **`import module java.base;` (JEP 511)** |
| **Documentation Syntax** | HTML Javadoc `/** ... */` | HTML Javadoc | HTML Javadoc | **Markdown Javadoc `///` (JEP 467)** |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why must a public class in Java match the filename on disk?</strong></summary>

**Answer:**  
This requirement allows the Java compiler and JVM ClassLoader to locate class definitions efficiently without scanning the contents of every file in the directory. By enforcing that `public class OrderService` must reside in `OrderService.java` inside a directory path matching its package (`com/enterprise/OrderService.java`), the compiler can map fully qualified class names directly to filesystem paths in $O(1)$ time.
</details>

<details>
<summary><strong>2. What is the difference between single-type imports, on-demand imports, and static imports?</strong></summary>

**Answer:**  
- **Single-type import (`import java.util.List;`):** Imports a specific type into the compilation unit namespace.
- **On-demand / Wildcard import (`import java.util.*;`):** Allows any public type within that package to be resolved on demand if referenced.
- **Static import (`import static java.lang.Math.PI;`):** Imports static fields or methods of a class directly into scope, allowing them to be invoked without the qualifying class name.
</details>

<details>
<summary><strong>3. How does Java 25 Compact Source Files (JEP 512) change the entry point contract?</strong></summary>

**Answer:**  
JEP 512 relaxes the classic entry point requirement. Developers can declare an instance main method: `void main()` or `void main(String[] args)` without `public` or `static`. Furthermore, the entire `class` declaration can be omitted. The compiler automatically synthesizes an implicit top-level class residing in the unnamed package, instantiates it via its default constructor, and executes `main()`.
</details>

<details>
<summary><strong>4. What is the unnamed package, and why should it be avoided in production?</strong></summary>

**Answer:**  
When a source file omits a `package` declaration, the compiler places its types into the "unnamed package". Types in the unnamed package cannot be imported by any class residing in a named package. Consequently, code in the unnamed package cannot be consumed as a reusable library or module, restricting its use to quick exploratory prototypes.
</details>

<details>
<summary><strong>5. What is the significance of the single underscore (`_`) in modern Java?</strong></summary>

**Answer:**  
In Java 1.0 to 8, `_` was a legal identifier. In Java 9, it became a reserved keyword. In Java 21+ (JEP 443), it was designated as the syntax for **Unnamed Variables and Patterns**, allowing developers to explicitly mark variables that are unused (e.g., `try (var _ = acquireLock())` or `catch (NumberFormatException _)`).
</details>

<details>
<summary><strong>6. How do Java 25 Module Imports (`import module java.base;`) work?</strong></summary>

**Answer:**  
Introduced in JEP 511, `import module <ModuleName>;` imports all public top-level classes and interfaces from all packages exported by that module. For example, `import module java.base;` automatically imports `java.util.*`, `java.io.*`, `java.nio.file.*`, `java.util.concurrent.*`, eliminating large blocks of package import declarations in application classes.
</details>

<details>
<summary><strong>7. What happens if two imported packages contain a class with the same name?</strong></summary>

**Answer:**  
If a file has wildcard imports for two packages that both define the same class name (e.g., `import java.util.*;` and `import java.sql.*;`, both defining `Date`), referencing `Date` directly causes a compile-time error: `reference to Date is ambiguous`. The developer must either explicitly import the intended type via single-type import (`import java.util.Date;`) or reference it via its Fully Qualified Name (`java.sql.Date`).
</details>

<details>
<summary><strong>8. What are Markdown documentation comments (`///`), and how do they differ from `/** */`?</strong></summary>

**Answer:**  
Introduced in Java 23 (JEP 467), `///` allows writing Javadoc documentation using standard Markdown syntax (headers `#`, lists `-`, code blocks ` ``` `, links `[text](url)`) instead of embedding HTML tags (`<p>`, `<code>`, `<ul>`, `<li>`) inside `/** ... */`. The Javadoc tool parses `///` lines and renders clean HTML documentation while keeping raw source files easily readable.
</details>

---

## 11. Practice Exercises

### Exercise 1: Identifying Lexical and Syntactic Errors (Easy)
- **Task:** Review the following code snippet and identify all three compilation errors:
```java
package com.demo;
import java.util.List;

Public Class TestApp {
    public static void Main(String args) {
        System.out.println("Hello World")
    }
}
```
<details>
<summary>Solution</summary>

1. `Public Class`: Keywords in Java are case-sensitive; must be lowercase `public class`.
2. `void Main(String args)`: Entry point method name must be lowercase `main`, and parameter must be an array `String[] args`.
3. Missing semicolon: The statement `System.out.println("Hello World")` must terminate with a semicolon `;`.
</details>

### Exercise 2: Refactoring to Static Imports (Medium)
- **Task:** Refactor the following math-heavy method to use static imports for all `Math` methods and constants:
```java
public class Geometry {
    public static double sphereVolume(double r) {
        return (4.0 / 3.0) * java.lang.Math.PI * java.lang.Math.pow(r, 3);
    }
}
```
<details>
<summary>Solution</summary>

```java
package com.demo;

import static java.lang.Math.PI;
import static java.lang.Math.pow;

public class Geometry {
    public static double sphereVolume(double r) {
        return (4.0 / 3.0) * PI * pow(r, 3);
    }
}
```
</details>

### Exercise 3: Package Hierarchy Validator (Hard)
- **Task:** Write a standalone Java utility that inspects a directory of `.java` source files, reads the `package` line from each file, and verifies that the file's physical directory structure on disk strictly matches its declared package namespace.
<details>
<summary>Solution</summary>

```java
import java.io.BufferedReader;
import java.io.IOException;
import java.nio.file.*;
import java.util.regex.*;

public class PackageStructureValidator {
    private static final Pattern PACKAGE_PATTERN = Pattern.compile("^\\s*package\\s+([a-zA-Z0-9_.]+);");

    public static void main(String[] args) throws IOException {
        Path root = Path.of("src/main/java");
        if (!Files.exists(root)) {
            System.out.println("Source root src/main/java not found.");
            return;
        }

        try (var stream = Files.walk(root)) {
            stream.filter(p -> p.toString().endsWith(".java")).forEach(file -> {
                try (BufferedReader reader = Files.newBufferedReader(file)) {
                    String line;
                    while ((line = reader.readLine()) != null) {
                        Matcher matcher = PACKAGE_PATTERN.matcher(line);
                        if (matcher.find()) {
                            String pkg = matcher.group(1);
                            String expectedSubPath = pkg.replace('.', '/');
                            String actualParent = file.getParent().toString().replace('\\', '/');

                            if (!actualParent.endsWith(expectedSubPath)) {
                                System.err.printf("MISMATCH in %s: Declared package '%s' does not match path '%s'%n",
                                    file.getFileName(), pkg, actualParent);
                            } else {
                                System.out.printf("OK: %s -> %s%n", file.getFileName(), pkg);
                            }
                            break;
                        }
                    }
                } catch (IOException e) {
                    System.err.println("Error reading " + file + ": " + e.getMessage());
                }
            });
        }
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **File Ordering:** `package` $\to$ `import` $\to$ `class` / `record` / `interface`.
- **Main Method Signatures (Java 25):**
  - Canonical: `public static void main(String[] args)`
  - Modern Compact: `void main()` (no class wrapper required).
- **Imports:**
  - `import package.Type;`: Single class.
  - `import static package.Class.member;`: Static methods/fields.
  - `import module java.base;`: Entire module's exported packages (Java 25).
- **Naming Conventions:** Types in `PascalCase`, methods/variables in `camelCase`, constants in `UPPER_SNAKE_CASE`.
- **Modern Docs:** Use `///` Markdown Javadoc (Java 23+).

---

## 13. Further Reading

- [The Java Language Specification (Java SE 25 Edition) - Chapter 7: Packages and Compilation Units](https://docs.oracle.com/javase/specs/jls/se25/html/jls-7.html)
- [JEP 511: Module Import Declarations](https://openjdk.org/jeps/511)
- [JEP 512: Compact Source Files and Instance Main Methods](https://openjdk.org/jeps/512)
- [JEP 467: Markdown Documentation Comments](https://openjdk.org/jeps/467)

---

| [Section Index](file:///E:/Learning/engineering-notes/01-languages/java/01-fundamentals/README.md) | [Next: 01_variables-and-data-types.md](file:///E:/Learning/engineering-notes/01-languages/java/01-fundamentals/01_variables-and-data-types.md) → |
