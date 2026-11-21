# Module 00: Environment Setup, Toolchain, and Architecture

> Establish a robust, reproducible Java 25 LTS engineering environment: JDK installation, version management, compilation and JVM execution internals, JShell REPL prototyping, IntelliJ IDEA configuration, and automated build orchestration with Maven and Gradle.

| Metadata | Value |
| :--- | :--- |
| **Track Section** | 01-languages / java |
| **Module Number** | 00 |
| **Prerequisites** | None |
| **Baseline Runtime** | Oracle JDK 25 LTS / Eclipse Temurin 25 LTS |
| **Toolchains** | `javac`, `java`, `jshell`, `jcmd`, Maven 3.9+, Gradle 8.10+, IntelliJ IDEA |

---

## 1. Architectural Overview & Workflow

```
[ Install JDK 25 LTS ] ──► [ Version Management ] ──► [ JVM Execution Mechanics ]
  (javac, java, jcmd)         (SDKMAN! / Shims)          (Bytecode, ClassLoader, JIT)
           │
           ▼
[ Rapid Prototyping ]  ──► [ Enterprise IDE Setup ] ──► [ Build Automation ]
  (JShell / REPL)             (IntelliJ IDEA & JDWP)      (Maven & Gradle Wrappers)
```

This setup module provides the operational foundation for the entire Java curriculum. Every subsequent module assumes a properly installed Java 25 LTS development environment, validated build wrappers, and an understanding of the JVM compilation pipeline.

---

## 2. Topic Directory & Navigation

| # | Topic Document | Primary Engineering Focus | Target Lines |
|---|---|---|---|
| 00 | [00_jdk-installation.md](./00_jdk-installation.md) | JDK directory hierarchy, vendor distributions (Temurin, Corretto, Oracle), `JAVA_HOME` and `PATH` configuration. | 525 |
| 01 | [01_java-version-management.md](./01_java-version-management.md) | Managing parallel JDKs (SDKMAN!, Scoop), `.sdkmanrc` auto-switching, cross-compilation via `--release`. | 556 |
| 02 | [02_how-java-works.md](./02_how-java-works.md) | End-to-end execution pipeline: source to bytecode, bytecode verification, Tiered Compilation (Interpreter, C1, C2), and escape analysis. | 526 |
| 03 | [03_jshell.md](./03_jshell.md) | JShell interactive REPL, scratch variables (`$1`, `$2`), forward references, and programmatic `jdk.jshell` API evaluation. | 495 |
| 04 | [04_intellij-idea.md](./04_intellij-idea.md) | IntelliJ IDEA project model, PSI AST indexing, JDWP debugger, conditional breakpoints, hot-swap reload, and memory view. | 466 |
| 05 | [05_first-project-with-maven-and-gradle.md](./05_first-project-with-maven-and-gradle.md) | Standard directory layout (`src/main/java`), Maven phases, Gradle DAG tasks, build wrappers (`mvnw`, `gradlew`), and Fat JAR packaging. | 557 |

---

## 3. Toolchain Verification Checklist

Before proceeding to [01-fundamentals](../01-fundamentals/README.md), verify each assertion in your local terminal:

- [x] **Compiler & Runtime Match:** `java --version` and `javac --version` both report **Java 25.0.1 LTS**.
- [x] **JAVA_HOME Definition:** `$env:JAVA_HOME` (Windows) or `echo $JAVA_HOME` (POSIX) points to the JDK 25 root without a trailing slash.
- [x] **REPL Readiness:** Launching `jshell` displays `Welcome to JShell -- Version 25`, and evaluating `1 + 1` yields `$1 ==> 2`.
- [x] **IDE SDK Alignment:** IntelliJ IDEA Project Structure (`Ctrl+Alt+Shift+S`) has Project SDK set to JDK 25 and Language Level set to `25 - Scoped values, Flexible constructor bodies, Module imports`.
- [x] **Build Wrapper Operational:** Running `./mvnw clean test` or `./gradlew test` executes successfully.

---

## 4. Troubleshooting Common Environment Defects

| Symptom | Underlying Root Cause | Verification & Fix |
| :--- | :--- | :--- |
| `java: command not found` | The JDK `bin/` directory is not included in the operating system `PATH`. | Prepend `%JAVA_HOME%\bin;` (Windows) or `export PATH="$JAVA_HOME/bin:$PATH"` (POSIX). |
| `UnsupportedClassVersionError: class file version 69.0` | Application was compiled with JDK 25 (version 69), but executed on an older runtime (e.g., Java 17 or 21). | Ensure `java` launcher matches compiler: run `where java` or `which java` to eliminate older paths. |
| `javac: command not found` but `java` works | A legacy standalone JRE was installed rather than a full JDK. | Install a full OpenJDK 25 distribution (Eclipse Temurin or Oracle JDK). |
| IntelliJ reports false red syntax errors on valid Java 25 code | Outdated IDE indexes or mismatched Language Level. | Run **File $\to$ Invalidate Caches / Restart...** and verify Project Language Level is set to 25. |

---

| [← Return to Track Root](../README.md) | [Next Module: 01-fundamentals →](../01-fundamentals/README.md) |
