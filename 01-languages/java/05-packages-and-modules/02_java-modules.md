# Java Platform Module System (JPMS): Architecture, Directives, and Modularity

> An authoritative guide to the Java Platform Module System (Project Jigsaw / JEP 261), covering `module-info.java`, strong encapsulation, service providers, and migration strategies.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate to Advanced |
| **Prerequisites** | [00_packages-and-imports.md](./00_packages-and-imports.md), [01_classpath-and-jars.md](./01_classpath-and-jars.md) |
| **Time to Read** | 25 minutes |
| **Status** | Active |
| **Java Version** | Java 9+ (Java 25 LTS verified) |

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
9. [Comparison: Classpath vs Modulepath](#9-comparison-classpath-vs-modulepath)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

Introduced in **Java 9 (Project Jigsaw / JEP 261)**, the **Java Platform Module System (JPMS)** is the most significant architectural evolution in Java's history.

Prior to Java 9:
- The JDK was a monolithic 200MB runtime file (`rt.jar`). Even a simple Hello World program required shipping the entire monolithic runtime including CORBA, Swing, and XML parsers.
- Any `public` class on the classpath was globally accessible to all other classes, destroying internal encapsulation.
- Dependency errors were detected lazily at runtime through `NoClassDefFoundError`.

JPMS solved these problems by introducing a higher-level aggregation above packages: the **Module**. A module is a uniquely named, self-describing collection of code and data that explicitly declares:
1. What other modules it **requires** (dependencies).
2. What packages it **exports** (public API contract).
3. What packages it **opens** for deep reflection.

---

## 2. Core Concepts

### 2.1 The `module-info.java` Descriptor
A module is defined by a `module-info.java` file placed at the root of the source hierarchy:
```java
module com.company.billing {
    // 1. Mandatory Dependencies
    requires java.sql;
    requires com.company.core;

    // 2. Transitive Dependency (Implied Readability)
    requires transitive java.logging;

    // 3. Exported Packages (Public API)
    exports com.company.billing.api;

    // 4. Qualified Export (Restricted to specific friend modules)
    exports com.company.billing.internal to com.company.billing.test;

    // 5. Open for Deep Reflection (e.g. Jackson / Hibernate)
    opens com.company.billing.model to com.fasterxml.jackson.databind;

    // 6. Service Provider SPI
    uses com.company.billing.spi.PaymentProvider;
    provides com.company.billing.spi.PaymentProvider
        with com.company.billing.provider.StripePaymentProvider;
}
```

### 2.2 The Three Types of Modules
1. **Explicit (Named) Modules:** Contains a `module-info.java` file. Placed on the modulepath. Enforces strict encapsulation.
2. **Automatic Modules:** Traditional JAR files without `module-info.java` placed on the **modulepath**. The JVM derives a module name from the JAR filename and exports all packages.
3. **The Unnamed Module:** All JARs and classes placed on the traditional **classpath**. Exports all packages and can read all other modules.

---

## 3. How It Works Internally

### 3.1 Module Resolution Graph and Readability
At JVM startup, the module system builds a **Module Resolution Graph** before executing `main()`:
- If module `A` requires module `B`, a directed edge $A \longrightarrow B$ is established.
- **Fail-Fast Startup:** If any required module is missing, or if circular dependencies occur across explicit modules, the JVM **refuses to start**, outputting a descriptive error immediately!
- **Accessibility Formula:** Type `T` in module `M` is accessible to code in module `N` if and only if:
  1. Module `N` **reads** module `M` (via `requires`).
  2. Package containing `T` is **exported** by `M`.
  3. Type `T` is declared `public`.

```
Module Readability Graph:
┌───────────────────────┐
│ com.app.presentation  │
└───────────┬───────────┘
            │ requires
┌───────────▼───────────┐
│   com.app.service     │
└───────────┬───────────┘
            │ requires transitive
┌───────────▼───────────┐
│    com.app.model      │ (Accessible to presentation because service
└───────────────────────┘  requires it transitively!)
```

### 3.2 Strong Encapsulation vs Reflection
In JPMS, `public` no longer means "accessible to the world":
- Calling `setAccessible(true)` on a private field of a class in a closed module throws `java.lang.reflect.InaccessibleObjectException`.
- To permit frameworks (Spring, Hibernate, Jackson) to inspect private fields, the module must explicitly declare `opens package.name to framework.module;`.

---

## 4. Syntax & Usage

### 4.1 Compiling and Running Modular Code
```bash
# Compile modular source tree
javac -d target/classes \
      --module-path lib \
      src/module-info.java src/com/app/*.java

# Run modular application
java --module-path target/classes:lib \
     --module com.app/com.app.Main
```

---

## 5. Practical Examples

### Example 1: Defining a Clean Two-Module Architecture
```java
// File: src/org.library/module-info.java
module org.library {
    exports org.library.api; // Public API
    // org.library.internal is NOT exported -> Completely hidden from callers!
}

// File: src/org.library/org/library/api/Calculator.java
package org.library.api;
import org.library.internal.MathEngine;

public class Calculator {
    public static int add(int a, int b) {
        return MathEngine.compute(a, b);
    }
}

// File: src/org.consumer/module-info.java
module org.consumer {
    requires org.library;
}

// File: src/org.consumer/org/consumer/Main.java
package org.consumer;
import org.library.api.Calculator;
// import org.library.internal.MathEngine; // COMPILE ERROR: Package not visible!

public class Main {
    public static void main(String[] args) {
        System.out.println("Result: " + Calculator.add(5, 10));
    }
}
```
**Output:**
```text
Result: 15
```

### Example 2: Real-World Service Provider Interface (`uses` and `provides`)
```java
// Module A: Defines SPI Interface
module com.storage.spi {
    exports com.storage.spi;
}

// Module B: Consumes SPI Interface via java.util.ServiceLoader
module com.app.client {
    requires com.storage.spi;
    uses com.storage.spi.StorageProvider; // Declares dependency on SPI implementations
}

// Module C: Implements SPI Interface
module com.storage.s3 {
    requires com.storage.spi;
    provides com.storage.spi.StorageProvider
        with com.storage.s3.S3StorageProvider; // Registers implementation
}
```
At runtime, the client discovers all registered providers dynamically:
```java
ServiceLoader<StorageProvider> loader = ServiceLoader.load(StorageProvider.class);
for (StorageProvider provider : loader) {
    provider.upload(data);
}
```

### Example 3: Edge Case – Qualified Exports (`exports ... to ...`)
```java
module com.engine.core {
    // Only com.engine.test and com.engine.admin are permitted to access diagnostics!
    exports com.engine.core.diagnostics to
        com.engine.test,
        com.engine.admin;
}
```
Any other module attempting to access `com.engine.core.diagnostics` will fail compilation, guaranteeing internal friend-assembly protection.

### Example 4: Failure Case – Reflection Blocked by Strong Encapsulation
```java
import java.lang.reflect.Field;

public class ReflectionBarrierDemo {
    public static void main(String[] args) throws Exception {
        // Attempting deep reflection into java.base internal classes:
        try {
            Field f = String.class.getDeclaredField("value");
            f.setAccessible(true); // THROWS InaccessibleObjectException in Java 17+!
        } catch (Exception ex) {
            System.out.println("Caught: " + ex.getClass().getSimpleName() + " - " + ex.getMessage());
        }
    }
}
```
**Output:**
```text
Caught: InaccessibleObjectException - Unable to make field private final byte[] java.lang.String.value accessible...
```

### Example 5: Optimized / Idiomatic – Open Modules for Microservices
```java
// An "open module" allows unrestricted runtime reflection on ALL its packages,
// ideal for Spring Boot / Jackson entities while preserving compile-time boundaries:
open module com.service.order {
    requires java.net.http;
    exports com.service.order.api;
}
```

---

## 6. Best Practices

1. **Use `requires transitive` for Return Types:** If an exported method returns a type from an external module (e.g. `java.sql.Connection`), mark that dependency `requires transitive java.sql;` so callers don't have to redundantly declare it.
2. **Open Specifically Rather Than Open Modules:** Instead of marking the entire module `open module`, use targeted directives: `opens com.app.model to com.fasterxml.jackson.databind;`.
3. **Decouple Modules with Services (`uses` / `provides`):** Use `ServiceLoader` to break direct compile-time coupling between modules.
4. **Never Export Internal Implementation Packages:** Expose only API interfaces; keep concrete engines, helpers, and drivers unexported.
5. **Use Automatic Modules During Gradual Migration:** Place third-party legacy JARs on the modulepath as automatic modules while modernizing internal modules.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Is Wrong | Correct Idiom |
| :--- | :--- | :--- |
| Split Packages | Two modules export the same package name; JVM refuses to start. | Ensure every package is owned by strictly one module. |
| Missing `opens` for JSON/JPA | Frameworks (Jackson/Hibernate) cannot read private fields via reflection. | Add `opens package.name to framework;`. |
| Cyclic dependencies in explicit modules | Module A requires B, and B requires A; compilation fails immediately. | Break cycles using interfaces or events. |
| Forgetting `requires transitive` | Callers using your API cannot see types returned by your methods. | Add `requires transitive`. |
| Mixing modulepath and classpath carelessly | Classes on classpath cannot access non-exported classes of modules. | Move entirely to modulepath or use unnamed module. |

---

## 8. Performance & Security Notes

### 8.1 Elimination of Illegal Reflection Access
In Java 16 and later, `--illegal-access=deny` became the permanent default. Deep reflection across module boundaries without explicit `opens` directives is completely prohibited by the JVM security architecture. This eliminates entire classes of vulnerabilities where libraries inspect or tamper with internal JVM memory structures.

---

## 9. Comparison: Classpath vs Modulepath

| Feature | Classpath (`-cp`) | Modulepath (`-p`) |
| :--- | :--- | :--- |
| **Descriptor** | None | `module-info.java` |
| **Encapsulation** | Public is world-accessible | Public is only accessible if exported |
| **Missing Dependencies**| Fails at runtime (`NoClassDefFound`)| Fails at startup (Fast fail) |
| **Split Packages** | Allowed (Silent shadowing hazard) | Prohibited (Startup error) |
| **Custom Runtimes** | Cannot use `jlink` | Supports `jlink` custom miniature runtimes |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the fundamental difference between exports and opens in module-info.java?</strong></summary>

- `exports <package>`: Grants **compile-time and runtime access** to the `public` and `protected` members of the package. Private members remain completely hidden and inaccessible to reflection.
- `opens <package>`: Grants **deep runtime reflective access** (including `private` members via `setAccessible(true)`), but does **not** grant compile-time access.
</details>

<details>
<summary><strong>2. What is a "Split Package" and why does JPMS forbid it?</strong></summary>

A Split Package occurs when classes belonging to the exact same package name reside in two different modules. JPMS strictly forbids this to guarantee deterministic class resolution and prevent class shadowing attacks. If two modules contain the same package, the JVM aborts startup with an error.
</details>

<details>
<summary><strong>3. What does the requires transitive directive do?</strong></summary>

`requires transitive <module>` establishes **implied readability**. Any module that depends on your module will automatically gain readability on the transitively required module as well, without needing to declare an explicit `requires` statement. It is essential when a module's exported methods return types defined in another module.
</details>

<details>
<summary><strong>4. What is an Automatic Module?</strong></summary>

An Automatic Module is a traditional JAR file (lacking `module-info.class`) placed on the **modulepath**. The JVM assigns it a module name derived from its filename (or `Automatic-Module-Name` manifest header), exports all its packages, and allows it to read all other modules. It acts as a bridge for migrating legacy libraries to JPMS.
</details>

<details>
<summary><strong>5. What is the Unnamed Module?</strong></summary>

The Unnamed Module contains all classes loaded from the traditional **classpath**. To preserve backward compatibility, the unnamed module can read all loaded modules and exports all of its own packages. However, named modules on the modulepath cannot read the unnamed module.
</details>

<details>
<summary><strong>6. How does ServiceLoader integrate with JPMS?</strong></summary>

JPMS natively integrates with `ServiceLoader` through the `uses <Interface>` and `provides <Interface> with <Implementation>` directives. This eliminates the need for legacy `META-INF/services/` text files and enforces compile-time verification of service contracts.
</details>

<details>
<summary><strong>7. What happens if a required module is missing at startup?</strong></summary>

Unlike the classpath which fails lazily during execution when an unreferenced class is encountered, JPMS validates the entire module resolution graph **eagerly at JVM startup**. If a required module is missing, the JVM immediately terminates with a clear `ResolutionException`.
</details>

<details>
<summary><strong>8. What is an "open module"?</strong></summary>

An open module (`open module com.app {}`) is an explicit module that exports its packages for compile-time access according to its `exports` statements, while automatically opening **all** of its packages for deep runtime reflection to all other modules.
</details>

---

## 11. Practice Exercises

### Easy
1. **Hello Module:** Create a minimal module with `module-info.java` exporting one package and execute it via `java --module-path`.
2. **Module Inspection:** Run `java --list-modules` from the terminal and inspect core platform modules.
3. **Describe Module:** Use `java --describe-module java.sql` to observe its exports and transitive dependencies.

### Medium
4. **Two-Module Assembly:** Build two interdependent modules where Module B requires Module A, verifying that non-exported packages cannot be imported.
5. **JSON Model Open Directive:** Configure a module with Jackson where entity model packages are selectively opened to `com.fasterxml.jackson.databind`.
6. **SPI ServiceLoader Implementation:** Build a modular plugin system using `uses` and `provides with`.

### Hard
7. **Modular Migration with Automatic Modules:** Place a legacy non-modular JAR on the modulepath and integrate it with an explicit module.
8. **Qualified Export Verification:** Verify that a qualified export (`exports to`) permits access to the friend module while blocking a third module.
9. **jlink Minimal Image Creation:** Package a modular application into a custom miniature standalone JRE using `jlink`.

<details>
<summary><strong>Exercise Solutions</strong></summary>

```java
// Solution to Exercise 1: Hello Module Descriptor
// File: src/module-info.java
module com.hello {
    exports com.hello;
}

// File: src/com/hello/Main.java
package com.hello;
public class Main {
    public static void main(String[] args) {
        System.out.println("Hello Modular Java!");
    }
}
```
</details>

---

## 12. Summary & Cheat Sheet

```
┌────────────────────────────────────────────────────────────────────────┐
│                          JPMS CHEAT SHEET                              │
├────────────────────────────────────────────────────────────────────────┤
│ Descriptor:         module-info.java (Root of source tree)             │
│ requires:           Declares mandatory dependency on another module    │
│ requires transitive:Implied readability for callers                    │
│ exports:            Exposes public API packages at compile & runtime   │
│ exports ... to:     Qualified export restricted to friend modules      │
│ opens ... to:       Opens package for deep reflection (Jackson, JPA)   │
│ uses / provides:    Declarative SPI ServiceLoader integration          │
│ Execution:          java --module-path target --module mod.name/Class  │
│ Split Packages:     STRICTLY FORBIDDEN (Fails startup)                 │
│ Inaccessible:       setAccessible(true) blocked unless package is open │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 13. Further Reading
- [JEP 261: Module System](https://openjdk.org/jeps/261)
- [JEP 200: The Modular JDK](https://openjdk.org/jeps/200)
- Java Language Specification (JLS) Section 7.7: Module Declarations
- *The Java Module System*, Nicolai Parlog (Manning Publications).
