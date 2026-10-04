# 05 - Packages and Modules

How Java code is **organized**, **named**, **found** and **packaged**. Small programs live in one file; real projects have hundreds of classes, many libraries, and a build that produces a runnable artifact. This folder explains the units involved, from the smallest to the largest.

```
class ─► package ─► JAR ─► module
 type     namespace   file     strongly encapsulated unit
```

## Four words that are often confused

| Term | What it is | Defined by |
|------|-----------|------------|
| **Package** | A namespace that groups related classes (`com.example.orders`) | `package` statement + folder layout |
| **JAR** | A ZIP file containing compiled classes and resources | `jar` tool, build tools |
| **Module** (Java, JPMS) | A named group of packages with explicit `requires`/`exports` | `module-info.java` |
| **Maven/Gradle module** | A sub-project in a multi-project build | `pom.xml` / `settings.gradle` |
| **Classpath** | The list of folders and JARs the JVM searches for classes | `-cp` flag or build tool |
| **Library / dependency** | Someone else's JAR(s) your code uses | Declared in the build file ([18-build-and-dependencies](../18-build-and-dependencies/README.md)) |

"Module" means two different things: a **Java module** (JPMS, this folder) and a **Maven/Gradle module** (a build-level sub-project). Most Java development uses packages, JARs and the classpath; JPMS is optional.

## Prerequisites

[04-oop](../04-oop/README.md), mainly [access modifiers](../04-oop/04_encapsulation-and-access-modifiers.md), and [how Java works](../00-setup/02_how-java-works.md) (compile, run, classpath basics).

## Reading order

| # | File | You will learn |
|---|------|----------------|
| 0 | [00_packages-and-imports.md](./00_packages-and-imports.md) | Package naming and layout, `import`, static imports, name conflicts, package design |
| 1 | [01_classpath-and-jars.md](./01_classpath-and-jars.md) | How the JVM finds classes, building and running JARs, fat JARs, classic classpath errors |
| 2 | [02_java-modules.md](./02_java-modules.md) | JPMS: `module-info.java`, `requires`/`exports`/`opens`, `jlink`, when to use modules |

## Practice

| After file | Try |
|------------|-----|
| 00 | Put three classes into two packages by hand, compile with `javac -d out`, run with `java -cp out`; resolve a `Date` name conflict |
| 01 | Package that program into an executable JAR; add a library JAR and run it with `-cp`; break it on purpose (remove the JAR) and read the error |
| 02 | Split the sample into two modules (`api` and `app`), export only the API package, and prove the internal package is inaccessible |

## You are done when you can

- [ ] Explain how a package name maps to a folder and why the default package is avoided
- [ ] Say what `import` does and does not do (no copying, no loading)
- [ ] Compile and run a multi-package program from the command line
- [ ] Explain `ClassNotFoundException` vs `NoClassDefFoundError`
- [ ] Build an executable JAR and explain why `-cp` is ignored with `-jar`
- [ ] Describe what `exports`, `requires` and `opens` do, and when plain classpath projects are enough

## Key takeaways

- Packages are namespaces, matching folder structure, named with a reversed domain
- `import` only shortens names; the JVM finds classes on the **classpath**
- A JAR is a ZIP of classes; build tools assemble the classpath for you
- JPMS adds strong encapsulation and `jlink` runtimes, at a cost; most application code still runs fine on the classpath

**Next:** [06-exceptions-and-debugging](../06-exceptions-and-debugging/README.md)
