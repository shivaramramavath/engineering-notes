# IntelliJ IDEA: Architecture, Debugging, and Engineering Workflows

> IntelliJ IDEA is the enterprise-standard Integrated Development Environment (IDE) for Java, providing AST-driven static analysis, incremental compilation, deep JDWP debugging, and native integration with build automation systems.

| Metadata | Value |
| :--- | :--- |
| **Level** | Beginner to Intermediate |
| **Prerequisites** | [00_jdk-installation.md](file:///E:/Learning/engineering-notes/01-languages/java/00-setup/00_jdk-installation.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Java LTS Baseline** | Java 25 LTS (IntelliJ IDEA Project Model & JDWP Debugger) |

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
Modern enterprise software systems comprise thousands of source files, complex dependency graphs, and multi-module architectures. Navigating, compiling, refactoring, and debugging such systems via raw text editors and command-line scripts imposes massive friction. Renaming a method across 40 modules manually risks compile breakage and runtime reflection failures.

### Historical Context
- **The Early Era (1995–2001):** Java developers used text editors (Emacs, Notepad, Vi) or simple visual tools (Symantec Cafe, JBuilder).
- **The Eclipse & NetBeans Era (2001–2010):** Eclipse introduced incremental compilers and workspace concepts, democratizing open-source Java tooling.
- **The Modern Standard:** JetBrains developed IntelliJ IDEA around a deep semantic model of code (the **Program Structure Interface - PSI**). Rather than treating code as raw text, IntelliJ parses everything into an active, continuously indexed AST, enabling instant type-safe refactoring, real-time error highlighting, and intelligent code completions.

### Real-World Relevance
In production engineering, mastering IntelliJ IDEA translates to significantly higher velocity. Features like conditional breakpoints, evaluate expression, smart step into, memory histograms, and structural search allow senior engineers to pinpoint race conditions and memory leaks in minutes rather than hours.

---

## 2. Core Concepts

### The IntelliJ Project Hierarchy

```
+---------------------------------------------------------------------------------+
|                                 Project                                         |
|  Global configuration, Project SDK (e.g., JDK 25), Language Level (e.g., 25)    |
|                                                                                 |
|  +---------------------------------------------------------------------------+  |
|  |                            Module A (Core Domain)                         |  |
|  |   Content Roots:                                                          |  |
|  |   ├── src/main/java       [Source Root - Compiled to target/classes]       |  |
|  |   ├── src/main/resources  [Resource Root - Copied to classpath]          |  |
|  |   └── src/test/java       [Test Root - Only available during test runs]   |  |
|  +---------------------------------------------------------------------------+  |
|                                                                                 |
|  +---------------------------------------------------------------------------+  |
|  |                            Module B (Web API)                             |  |
|  |   Dependencies: Depends on Module A, Jackson, Netty                       |  |
|  +---------------------------------------------------------------------------+  |
+---------------------------------------------------------------------------------+
```

- **Project:** The top-level workspace encapsulation. Defines the overarching Project SDK and default bytecode language level.
- **Module:** A functional discrete sub-unit within a project that can be compiled, tested, and debugged independently. In Maven/Gradle projects, each POM or subproject maps directly to an IntelliJ Module.
- **Content Roots:** The physical directory containing module files. Folders are marked with distinct semantic roles:
  - **Sources (`Blue`):** Production code compiled into the output directory.
  - **Tests (`Green`):** Unit and integration test code, compiled separately and never packaged into production JARs.
  - **Resources (`Yellow`):** Non-code configuration files (`application.properties`, XML schemas) placed directly onto the runtime classpath.
  - **Excluded (`Red`):** Build artifacts (`target/`, `build/`, `.gradle/`) excluded from indexing to save RAM and CPU.

### Essential Productivity Keystrokes (Windows / POSIX)
| Shortcut | Action | Mechanical Purpose |
| :--- | :--- | :--- |
| `Shift + Shift` | **Search Everywhere** | Finds classes, files, symbols, settings, and git branches. |
| `Ctrl + N` / `Cmd + O` | **Go to Class** | Instantly navigates to any class in project or dependencies. |
| `Alt + Enter` | **Show Context Actions** | Resolves compilation errors, imports missing types, offers quick-fixes. |
| `Ctrl + Alt + L` | **Reformat Code** | Formats current buffer to match team style guidelines. |
| `Shift + F6` | **Rename Refactor** | Renames class/method/variable across all usages safely. |
| `Ctrl + B` / `Cmd + B` | **Go to Declaration / Usages** | Jumps to method definition or finds all call sites. |

---

## 3. How It Works Internally

### PSI (Program Structure Interface) and Background Indexing
Unlike traditional editors that use Language Server Protocol (LSP) or regex parsing, IntelliJ parses your Java code into a rich, in-memory abstract syntax tree known as the **PSI (Program Structure Interface)**.

```
Source File (OrderService.java)
       │
       ▼
Lexer -> Token Stream -> Parser -> PSI Tree (AST)
                                       │
                ┌──────────────────────┴──────────────────────┐
                ▼                                             ▼
     [ File-Based Indices ]                        [ Stub Indices ]
   Fast textual & symbol lookup               Lightweight class signatures
   (names, word occurrences)                  (method names, annotations, types)
```

- **Stubs:** When indexing external libraries (such as `java.base.jmod` or Maven JARs), IntelliJ creates binary **Stub Trees** containing only class signatures, method names, and annotations without method bodies. This allows the IDE to index a 100,000-class library in seconds with minimal RAM consumption.
- **Index Caches:** When you open a project, the "Indexing..." progress bar builds these indices. Once indexed, searches across millions of lines of code return in milliseconds.

### The Debugger Architecture: JDWP Transport
When you click **Debug** on a Java class, IntelliJ executes the JVM with the Java Debug Wire Protocol (JDWP) agent attached:

```
[ IntelliJ IDEA Process ] <==== JDWP Socket IPC ====> [ Target Application JVM ]
  UI: Breakpoint Manager,                               -agentlib:jdwp=transport=
  Threads View, Variable Inspector                      dt_socket,server=y,suspend=y
```
1. IntelliJ launches the JVM with:  
   `-agentlib:jdwp=transport=dt_socket,server=y,suspend=y,address=localhost:5005`
2. The target JVM pauses before executing `main` until IntelliJ's debugger establishes a TCP socket connection.
3. When execution hits a line with a breakpoint, the target JVM triggers a safepoint pause for that thread and transmits an event over JDWP. IntelliJ receives the packet, queries local variables, and updates the UI.

---

## 4. Syntax & Usage

### Configuring JVM Memory for IntelliJ (`idea64.exe.vmoptions`)
For large codebases, the default heap allocated to IntelliJ (often 2048 MB) can cause garbage collection pauses. You can customize the IDE VM options via **Help $\to$ Edit Custom VM Options...**:

```properties
# Allocate 4 GB to 8 GB of RAM depending on host hardware
-Xms2048m
-Xmx4096m
-XX:ReservedCodeCacheSize=512m

# Utilize modern generational ZGC for sub-millisecond IDE UI pauses
-XX:+UseZGC
-XX:+ZGenerational
```

### Build Delegation: IntelliJ vs. Build Tool
In **Settings $\to$ Build, Execution, Deployment $\to$ Build Tools $\to$ Maven / Gradle**:
- **Delegate IDE build/run actions to Maven/Gradle:** Ensures 100% build parity between your IDE and CI/CD pipelines (processes annotation processors and resource filtering identically).
- **Build and run using IntelliJ IDEA:** Significantly faster incremental compilation for rapid local test loops, bypassing Gradle/Maven daemon startup overhead.

---

## 5. Practical Examples

### Example 1: Basic Debugging with Conditional Breakpoints
- **Goal:** Set a conditional breakpoint that only pauses execution when a specific item is processed in a 100-iteration loop.
```java
public class DebugTarget {
    static int counter = 0;

    public static void process(String item) {
        counter++;
        if ("target-item".equals(item)) {
            // Conditional breakpoint: Set condition in IntelliJ: "target-item".equals(item)
            System.out.println("Hit target at counter=" + counter);
        }
    }

    public static void main(String[] args) {
        for (int i = 0; i < 100; i++) {
            process(i == 42 ? "target-item" : "item-" + i);
        }
    }
}
```
- **Console Output:**
```text
Hit target at counter=43
```
- **IntelliJ Debugger Action:** Right-click the breakpoint red dot on the `if` line, and enter `"target-item".equals(item)` in the **Condition** field. The debugger skips iterations 0 through 41 and pauses execution on iteration 42.

---

### Example 2: Enterprise Hot-Swap & Reload Changed Classes
- **Goal:** Modify a running application during a live debug session without restarting the JVM process.
```java
public class HotSwapDemo {
    public static String getGreeting() {
        return "Original Greeting"; // Change to "Live Hot-Swapped Greeting!" while paused
    }

    public static void main(String[] args) throws Exception {
        while (true) {
            System.out.println(getGreeting());
            Thread.sleep(2000);
        }
    }
}
```
- **Action:**
  1. Launch `HotSwapDemo` in **Debug** mode.
  2. While the loop is running, edit `getGreeting()` to return `"Live Hot-Swapped Greeting!"`.
  3. Press **Ctrl+Shift+F9** (Recompile) or **Run $\to$ Reload Changed Classes**.
  4. The debugger pushes the recompiled method bytecode to the target JVM via JDI `VirtualMachine.redefineClasses()`, instantly updating the live output without dropping connection or state.

---

### Example 3: Edge Case — Memory View & Leak Investigation
- **Goal:** Locate an object leak in memory by tracking instance counts during a breakpoint pause.
```java
import java.util.ArrayList;
import java.util.List;

public class LeakDemo {
    static class LeakingEntity {
        byte[] payload = new byte[1024 * 1024]; // 1 MB
    }

    static final List<LeakingEntity> REGISTRY = new ArrayList<>();

    public static void main(String[] args) throws Exception {
        for (int i = 0; i < 20; i++) {
            REGISTRY.add(new LeakingEntity());
            Thread.sleep(100);
        }
        System.out.println("Done allocating 20 MB."); // Put breakpoint here
    }
}
```
- **Action:**
  1. Place a breakpoint on the `println` line.
  2. Open the **Memory View** tab in the Debugger window.
  3. Type `LeakingEntity`. Observe: **Count = 20 (Total Size = 20 MB)**.
  4. Right-click the class and select **Show Instances** to inspect which static collection (`REGISTRY`) is preventing garbage collection.

---

### Example 4: Failure Case — Annotation Processor Desynchronization
- **Goal:** Diagnose compile failure when using Lombok, MapStruct, or custom annotation processors inside IntelliJ.
```java
// Scenario: Code uses @Builder or @Getter, but IntelliJ flags red errors:
// "Cannot resolve method 'builder()'"
public class Entity {
    private String id;
    // Generated by processor at compile time
}
```
- **Root Cause & Fix:** IntelliJ disables annotation processing by default for security and speed.
- **Remedy:** Go to **Settings $\to$ Build, Execution, Deployment $\to$ Compiler $\to$ Annotation Processors** and check **Enable annotation processing**. Then execute **Build $\to$ Rebuild Project**.

---

### Example 5: High-Performance / Modern Idiom — Structural Search and Replace (SSR)
- **Goal:** Automate large-scale codebase modernization by searching for legacy Java patterns and replacing them with modern idioms.
- **Search Template:**
```java
$List$.stream().filter($Predicate$).collect(java.util.stream.Collectors.toList())
```
- **Replacement Template (Java 16+ / Java 25):**
```java
$List$.stream().filter($Predicate$).toList()
```
- **Explanation:** IntelliJ's Structural Search operates on the PSI AST rather than raw text regex, guaranteeing that only syntactically valid stream pipelines are matched and refactored across thousands of files simultaneously.

---

## 6. Best Practices

1. **Commit `.gitignore` Excluding `.idea/` Workspace Files:** Always exclude `.idea/workspace.xml`, `.idea/shelf/`, and `*.iml` from git. Only commit shared project settings if your team explicitly standardizes on them.
2. **Use Line Breakpoints Over Method Breakpoints:** Method breakpoints (placing a breakpoint on the method signature line) force the JVM to disable JIT compilation and enter slow interpretive method-entry callbacks, drastically degrading debug performance. Always set breakpoints on executable code lines inside the method.
3. **Configure File Encodings to UTF-8:** Go to **Settings $\to$ Editor $\to$ File Encodings** and ensure **Global Encoding**, **Project Encoding**, and **Default encoding for properties files** are all explicitly set to `UTF-8` with **Transparent native-to-ascii conversion** enabled.
4. **Use Smart Step Into (`Shift + F7`):** When debugging a line containing nested method calls (e.g., `service.process(repo.find(id), calc.tax(rate))`), use Smart Step Into to jump directly into `calc.tax()` rather than stepping through each wrapper method sequentially.
5. **Mark Generated Directories Correctly:** If using code-generation plugins (Protobuf, OpenAPI, jOOQ), ensure the generated source folder is marked as **Generated Sources Root** so IntelliJ indices symbols cleanly.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Project SDK vs Language Level Desync** | Setting Project SDK to Java 25 but Language Level to 17 blocks modern syntax. | Synchronize Language Level with target Java version in Project Structure (`Ctrl+Alt+Shift+S`). |
| **Setting Method Breakpoints in Loops** | Causes target JVM to slow down by 100x due to interpreter method-entry hooks. | Use line breakpoints with conditional expressions instead. |
| **Editing Files Outside Source Roots** | Files outside marked source roots are treated as plain text; completions and refactorings fail. | Ensure parent folder is marked as **Sources Root** (`Blue`). |
| **Committing `.idea/workspace.xml`** | Contains personal local path states and window coordinates, causing perpetual git merge conflicts. | Add `.idea/workspace.xml` to top-level `.gitignore`. |
| **Outdated Indexes Causing Ghost Errors** | Crashed IDE shutdowns can corrupt local disk caches, showing false red compile errors. | Run **File $\to$ Invalidate Caches / Restart...**. |
| **Running Out of IDE Heap Memory** | Causes UI stuttering and long background GC freezes on large monorepos. | Increase `-Xmx` in `idea64.exe.vmoptions` to 4096m or higher. |

---

## 8. Performance & Security Notes

### Managing IDE Indexing Overhead
In multi-gigabyte repositories, indexing can consume significant CPU cycles:
- Explicitly mark build directories (`target`, `build`, `node_modules`, `.cache`) as **Excluded**.
- Disable unused bundled plugins (such as Android or Kotlin plugins if working purely on Java backend services) under **Settings $\to$ Plugins** to conserve 500+ MB of IDE heap memory.

### Remote Debugging Security Invariants
When enabling JDWP remote debugging on staging or remote server pods:
```bash
-agentlib:jdwp=transport=dt_socket,server=y,suspend=n,address=*:5005
```
**CRITICAL:** JDWP has **zero authentication or encryption**. Binding `address=*:5005` on a publicly accessible IP allows any attacker to attach a remote debugger, execute arbitrary bytecode, and compromise the host. Always bind to `localhost` (`address=127.0.0.1:5005`) and access via an encrypted SSH tunnel:
```bash
ssh -L 5005:localhost:5005 user@remote-server
```

---

## 9. Comparison / When to Use What

| Feature / IDE | IntelliJ IDEA Ultimate | IntelliJ IDEA Community | VS Code (with Java Pack) | Eclipse IDE |
| :--- | :--- | :--- | :--- | :--- |
| **Licensing** | Commercial Paid | Free / Open Source | Free / Open Source | Free / Open Source |
| **Java SE & Debugger** | Full (Advanced) | Full (Advanced) | Good (via Red Hat LSP) | Full (Incremental Compiler) |
| **Spring / Web / JPA** | Deep native tooling | Syntax-only | Extension-dependent | Good via Spring Tools 4 |
| **Memory Profiling** | Built-in JFR & Async Profiler | External tools needed | External tools needed | Eclipse Memory Analyzer (MAT) |
| **Resource Usage** | High (2–4 GB RAM) | High (1.5–3 GB RAM) | Moderate (800 MB–1.5 GB) | Moderate (1–2 GB RAM) |
| **Ideal Scenario** | Professional enterprise backends | General Java SE development | Polyglot / Lightweight editing | Legacy corporate environments |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the Program Structure Interface (PSI) in IntelliJ IDEA?</strong></summary>

**Answer:**  
The Program Structure Interface (PSI) is IntelliJ IDEA’s proprietary AST and semantic code model. Unlike simple syntax trees, the PSI represents code with complete syntactic, structural, and semantic fidelity: it models classes, methods, expressions, type hierarchies, and references. IntelliJ builds and caches PSI trees during background indexing, allowing it to perform instant reference resolution, type checking, inspections, and multi-file semantic refactoring.
</details>

<details>
<summary><strong>2. How does the Java Debug Wire Protocol (JDWP) work between IntelliJ and the target JVM?</strong></summary>

**Answer:**  
JDWP is a standardized packet-based binary protocol defining communication between a debugger process (the front-end) and a running JVM process (the back-end). The target JVM loads the JDWP agent library (`libjdwp.so` or `jdwp.dll`) via `-agentlib:jdwp`. The front-end connects via a TCP socket or shared memory. Through JDWP commands, IntelliJ requests event notifications (breakpoint hits, exceptions, thread states), inspects stack frames, queries object fields, and evaluates expressions dynamically.
</details>

<details>
<summary><strong>3. Why do method breakpoints incur a massive performance penalty compared to line breakpoints?</strong></summary>

**Answer:**  
Line breakpoints work by having the JVM insert an instruction breakpoint or patching a single bytecode offset. Method breakpoints, however, hook into the JVM’s method entry and exit notification mechanisms (`MethodEntry` and `MethodExit` in JVMTI). Because these events fire on every method call across all threads, the JVM is forced to disable JIT compilation and switch to interpreted mode or execute debug callbacks on every call, slowing execution down by orders of magnitude.
</details>

<details>
<summary><strong>4. How does IntelliJ’s "HotSwap" mechanism reload classes during debugging, and what are its limitations?</strong></summary>

**Answer:**  
HotSwap leverages the JVM Tool Interface (JVMTI) function `RedefineClasses`. When a class is recompiled, IntelliJ sends the updated bytecode to the JVM over JDWP. Standard HotSpot HotSwap allows modifying method bodies, but **cannot** add, remove, or rename methods, fields, constructors, or superclasses, nor can it change method signatures. For schema and signature changes, advanced tools like DCEVM or JRebel are required.
</details>

<details>
<summary><strong>5. What is the difference between Project SDK and Project Language Level in IntelliJ?</strong></summary>

**Answer:**  
The **Project SDK** defines the physical JDK installation (including compiler binaries, headers, and standard libraries) used to build and run the project (e.g., Oracle JDK 25). The **Project Language Level** defines the language syntax and feature set accepted by the editor and compiler (e.g., setting Language Level to 17 enforces Java 17 syntax and flags Java 21+ features as compile errors, even when compiling with a JDK 25 SDK).
</details>

<details>
<summary><strong>6. How does "Delegate IDE build/run actions to Gradle/Maven" prevent "works in IDE, fails in CI" defects?</strong></summary>

**Answer:**  
IntelliJ has its own fast, internal incremental compiler. However, complex builds often have Maven/Gradle build plugins that perform resource filtering, bytecode enhancement, annotation processing, or code generation. By delegating build actions to Gradle/Maven, IntelliJ executes the exact build lifecycle tasks specified in the build scripts, guaranteeing complete parity between local IDE runs and headless CI runners.
</details>

<details>
<summary><strong>7. What are Stub Indices in IntelliJ, and why are they critical for IDE performance?</strong></summary>

**Answer:**  
Stub Indices are compact, binary representations of external class signatures (class names, method signatures, field names, annotations) stripped of method bodies. By generating Stubs for large external dependency JARs rather than parsing full ASTs into memory, IntelliJ saves massive amounts of heap memory and allows near-instant symbol search across third-party libraries.
</details>

<details>
<summary><strong>8. What is the purpose of IntelliJ’s "Evaluate Expression" feature during debugging?</strong></summary>

**Answer:**  
While paused at a breakpoint, Evaluate Expression (`Alt + F8`) compiles arbitrary Java code snippets in the context of the current stack frame using the IDE’s in-memory compiler, transmits the compiled bytecode to the target JVM via JDWP, executes it on the paused thread, and serializes the result back to the UI. This allows developers to test hypothesis, inspect private fields, or invoke state queries without altering source code.
</details>

---

## 11. Practice Exercises

### Exercise 1: Breakpoint Condition Formulation (Easy)
- **Task:** Given a collection of 10,000 `Transaction` objects where each transaction has an `id` and an `amount`, write the exact Boolean expression string to enter into IntelliJ’s Breakpoint Condition dialog to pause only when a transaction with `amount > 1_000_000.0` and `status == Status.FAILED` is encountered.
<details>
<summary>Solution</summary>

```java
// Enter into Breakpoint Condition dialog:
tx.getAmount() > 1_000_000.0 && tx.getStatus() == Status.FAILED
```
</details>

### Exercise 2: Live Debug State Modification (Medium)
- **Task:** Write a small test program that simulates an authentication loop. Set a breakpoint on the failure branch and use IntelliJ’s **Set Value** (`F2`) or **Evaluate Expression** to mutate the local boolean variable `isAuthenticated` to `true` while paused, verifying that execution routes into the authorized branch.
<details>
<summary>Solution</summary>

```java
public class AuthDebugExercise {
    public static void main(String[] args) {
        boolean isAuthenticated = false; // Set breakpoint here

        // In debugger: select 'isAuthenticated' in Variables pane, press F2, change to true
        if (isAuthenticated) {
            System.out.println("ACCESS GRANTED: State successfully mutated in debugger!");
        } else {
            System.out.println("ACCESS DENIED: Default flow executed.");
        }
    }
}
```
</details>

### Exercise 3: Remote Debug Configuration Simulation (Hard)
- **Task:** Write a Java server program that launches a long-running background worker thread. Then, formulate the exact command-line arguments to launch the JVM in server-listen mode on port `5005`, and describe the step-by-step procedure to connect an IntelliJ Remote JVM Debug configuration to it.
<details>
<summary>Solution</summary>

```java
public class ServerWorker {
    public static void main(String[] args) throws Exception {
        System.out.println("Server Worker running on PID: " + ProcessHandle.current().pid());
        while (true) {
            Thread.sleep(1000);
        }
    }
}
```
- **Launch Command:**
```bash
java -agentlib:jdwp=transport=dt_socket,server=y,suspend=n,address=127.0.0.1:5005 ServerWorker
```
- **IntelliJ Configuration Procedure:**
  1. Go to **Run $\to$ Edit Configurations...**.
  2. Click `+` and select **Remote JVM Debug**.
  3. Set Host: `localhost`, Port: `5005`, Mode: `Attach to remote JVM`.
  4. Select the matching project module for classpath search.
  5. Click **Debug**. IntelliJ attaches to the running JVM over port 5005 and hits any configured breakpoints immediately.
</details>

---

## 12. Summary / Cheat Sheet

- **Core Shortcuts:**
  - `Shift + Shift`: Search Everywhere.
  - `Alt + Enter`: Quick-fix / Intentions.
  - `Ctrl + N`: Search Class.
  - `Ctrl + Alt + L`: Format code.
  - `Shift + F6`: Rename refactor.
  - `Alt + F8`: Evaluate Expression (in Debugger).
- **Project Structure:** Project $\to$ Modules $\to$ Content Roots (`src/main/java` = Sources, `src/test/java` = Tests).
- **Debugging Wisdom:** Never use method breakpoints in loops; use conditional line breakpoints.
- **Parity:** Delegate builds to Maven/Gradle under Settings to ensure 100% CI parity.

---

## 13. Further Reading

- [IntelliJ IDEA Official Documentation](https://www.jetbrains.com/help/idea/)
- [Java Debug Wire Protocol (JDWP) Specification](https://docs.oracle.com/en/java/javase/25/docs/specs/jdwp/jdwp-spec.html)
- [IntelliJ Architectural Overview: The Program Structure Interface (PSI)](https://plugins.jetbrains.com/docs/intellij/psi.html)
- [JVM Tool Interface (JVMTI) Reference](https://docs.oracle.com/en/java/javase/25/docs/specs/jvmti.html)

---

| [← Previous: 03_jshell.md](file:///E:/Learning/engineering-notes/01-languages/java/00-setup/03_jshell.md) | [Section Index](file:///E:/Learning/engineering-notes/01-languages/java/00-setup/README.md) | [Next: 05_first-project-with-maven-and-gradle.md](file:///E:/Learning/engineering-notes/01-languages/java/00-setup/05_first-project-with-maven-and-gradle.md) → |
