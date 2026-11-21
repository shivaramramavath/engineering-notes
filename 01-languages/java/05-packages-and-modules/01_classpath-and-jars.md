# Classpath, JAR Files, and Class Loading Mechanics in Java

> A comprehensive deep-dive into the Java Classpath, JAR archive structure, ClassLoader delegation hierarchies, Fat JARs, and Multi-Release JARs.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [00_packages-and-imports.md](./00_packages-and-imports.md) |
| **Time to Read** | 25 minutes |
| **Status** | Active |
| **Java Version** | Java 8+ (Multi-Release JARs in Java 9+) |

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

Before Java can execute bytecode, the JVM must locate and load `.class` files into memory. The **Classpath** is the search path supplied to the JVM specifying directories and archive files where compiled class files and resources reside.

A **JAR (Java ARchive)** is a standardized compressed archive format based on the ZIP file format. It bundles compiled `.class` files, metadata, images, and configuration files into a single deployable artifact.

Historically, managing the Classpath was one of Java's most notorious operational challenges, colloquially known as **"Classpath Hell"**:
- Duplicate or conflicting JAR versions on the classpath.
- Missing classes triggering runtime `NoClassDefFoundError` or `ClassNotFoundException`.
- Shadowing of classes based on arbitrary classpath ordering.

Understanding how classloaders traverse the classpath and unpack JARs is fundamental to packaging and debugging enterprise applications.

---

## 2. Core Concepts

### 2.1 The Classpath Argument (`-cp` or `-classpath`)
The classpath is passed to the `java` and `javac` commands as a list of paths separated by the platform-specific path separator (`:` on Linux/macOS, `;` on Windows):
```bash
# Windows
java -cp "target/classes;lib/guava.jar;lib/jackson.jar" com.app.Main

# Linux/macOS
java -cp "target/classes:lib/guava.jar:lib/jackson.jar" com.app.Main
```

### 2.2 Anatomical Structure of a JAR File
```
myapp.jar
├── META-INF/
│   ├── MANIFEST.MF       (Archive metadata, Main-Class, Class-Path)
│   └── maven/            (Build tool metadata)
├── com/
│   └── app/
│       ├── Main.class
│       └── service/
│           └── OrderService.class
└── application.properties (Resource bundle)
```

---

## 3. How It Works Internally

### 3.1 The ClassLoader Delegation Hierarchy
In modern Java (Java 9 through Java 25), the JVM implements a hierarchical delegation model for class loading:

```
┌────────────────────────────────────────────────────────┐
│              Bootstrap ClassLoader                     │  Native C++ (Loads java.base,
│                                                        │  core JVM runtime classes)
└──────────────────────────┬─────────────────────────────┘
                           │ parent
┌──────────────────────────▼─────────────────────────────┐
│              Platform ClassLoader                      │  (Formerly Extension ClassLoader;
│                                                        │  loads standard extensions/SQL/XML)
└──────────────────────────┬─────────────────────────────┘
                           │ parent
┌──────────────────────────▼─────────────────────────────┐
│             Application ClassLoader                    │  (System ClassLoader; loads classes
│                                                        │  from -cp / -classpath)
└────────────────────────────────────────────────────────┘
```

#### The Delegation Protocol:
When `loadClass("com.app.Service")` is called:
1. The `Application ClassLoader` checks its cache to see if the class is already loaded.
2. If not, it **delegates upwards** to its parent (`Platform ClassLoader`).
3. The `Platform ClassLoader` delegates to the `Bootstrap ClassLoader`.
4. If the parent cannot find the class, the child searches its own classpath entries.
5. If no classloader finds the class, `ClassNotFoundException` is thrown.

### 3.2 Multi-Release JAR Files (MRJARs - JEP 238)
Introduced in Java 9, Multi-Release JARs allow a single JAR file to provide different versions of a class optimized for specific Java runtime releases:
```
my-library.jar
├── com/example/Crypto.class            (Java 8 baseline version)
└── META-INF/
    └── versions/
        ├── 17/
        │   └── com/example/Crypto.class (Optimized for Java 17)
        └── 25/
            └── com/example/Crypto.class (Optimized using Java 25 FFM API!)
```
When running on Java 25, the JVM automatically loads the class from `META-INF/versions/25/`; on Java 17, it loads from `versions/17/`; on Java 8, it loads the baseline root class!

---

## 4. Syntax & Usage

### 4.1 Creating and Inspecting JARs with the `jar` CLI
```bash
# 1. Create a JAR file with a Main-Class manifest entry
jar --create --file myapp.jar --main-class com.app.Main -C target/classes .

# 2. Execute an executable JAR directly
java -jar myapp.jar

# 3. Inspect table of contents in a JAR
jar --list --file myapp.jar

# 4. Extract specific contents from a JAR
jar --extract --file myapp.jar META-INF/MANIFEST.MF
```

### 4.2 The `MANIFEST.MF` Structure
```manifest
Manifest-Version: 1.0
Created-By: 25.0.1 (Oracle Corporation)
Main-Class: com.app.Main
Class-Path: lib/dependency1.jar lib/dependency2.jar
```

---

## 5. Practical Examples

### Example 1: Loading Resources from the Classpath
```java
import java.io.BufferedReader;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;

public class ResourceLoaderDemo {
    public static void main(String[] args) throws Exception {
        // Classpath resource lookup using ClassLoader
        try (InputStream in = ResourceLoaderDemo.class.getResourceAsStream("/banner.txt")) {
            if (in == null) {
                System.out.println("Resource not found on classpath!");
                return;
            }
            try (BufferedReader reader = new BufferedReader(new InputStreamReader(in, StandardCharsets.UTF_8))) {
                reader.lines().forEach(System.out::println);
            }
        }
    }
}
```

### Example 2: Inspecting ClassLoader Hierarchy Programmatically
```java
public class ClassLoaderHierarchyDemo {
    public static void main(String[] args) {
        ClassLoader appLoader = ClassLoaderHierarchyDemo.class.getClassLoader();
        System.out.println("1. App ClassLoader:      " + appLoader);

        ClassLoader platformLoader = appLoader.getParent();
        System.out.println("2. Platform ClassLoader: " + platformLoader);

        ClassLoader bootstrapLoader = platformLoader.getParent();
        // Bootstrap loader is written in native C++; represented as null in Java!
        System.out.println("3. Bootstrap ClassLoader: " + bootstrapLoader);

        // Core JDK classes (String) are loaded by Bootstrap ClassLoader
        System.out.println("String ClassLoader:      " + String.class.getClassLoader());
    }
}
```
**Output:**
```text
1. App ClassLoader:      jdk.internal.loader.ClassLoaders$AppClassLoader@...
2. Platform ClassLoader: jdk.internal.loader.ClassLoaders$PlatformClassLoader@...
3. Bootstrap ClassLoader: null
String ClassLoader:      null
```

### Example 3: Edge Case – Wildcard Classpath Ordering Trap
```bash
# Using wildcard in classpath:
java -cp "lib/*;classes" com.app.Main
```
In Java, `lib/*` expands to all `.jar` files in `lib/`. However, the order in which files are expanded depends on the underlying filesystem! If two JARs contain the same class name (`com.service.Helper`), whichever JAR happens to be enumerated first by the OS will shadow the other, leading to intermittent, environment-specific bugs!

### Example 4: Failure Case – `ClassNotFoundException` vs `NoClassDefFoundError`
```java
public class ClassNotFoundVsNoClassDef {
    public static void main(String[] args) {
        // 1. ClassNotFoundException: Checked exception when reflective lookup fails
        try {
            Class.forName("com.missing.NonExistentClass");
        } catch (ClassNotFoundException e) {
            System.out.println("Caught ClassNotFoundException: Class not found on classpath.");
        }

        // 2. NoClassDefFoundError: Fatal JVM Error when a class was available at COMPILE time,
        // but missing at RUNTIME (or failed static initialization <clinit>).
        System.out.println("NoClassDefFoundError occurs when runtime classpath is incomplete!");
    }
}
```
**Output:**
```text
Caught ClassNotFoundException: Class not found on classpath.
NoClassDefFoundError occurs when runtime classpath is incomplete!
```

### Example 5: Optimized / Idiomatic – Modern Multi-Release JAR Verification
```bash
# Verify whether a JAR is a multi-release JAR
jar --describe-module --file modern-lib.jar
```
If compiled as a multi-release JAR, the manifest contains `Multi-Release: true`, and the JVM dynamically chooses bytecode version paths matching the active runtime.

---

## 6. Best Practices

1. **Use Build Tools (Maven/Gradle) for Dependency Management:** Never manually manage loose JAR files in a `lib/` directory. Build tools enforce dependency resolution trees and eliminate version collisions.
2. **Favor Fat JARs (Shadow JARs) for Standalone CLI Services:** Tools like `maven-shade-plugin` or `gradle shadowJar` merge dependencies into a single deployable artifact with relocated conflicting packages.
3. **Always Load Resources via `getResourceAsStream()`:** Never hardcode absolute filesystem paths (`C:\app\config.properties`). Classpath resource lookups work uniformly across local development, Docker containers, and JAR files.
4. **Beware Classpath Shadowing:** If multiple JARs define the same package and class, the first occurrence in the classpath wins and permanently masks all subsequent versions.
5. **Migrate to Java Modules (JPMS) for Strong Encapsulation:** Unlike the classpath where all public classes are globally accessible, modular JARs enforce strict export boundaries.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Is Wrong | Correct Idiom |
| :--- | :--- | :--- |
| Hardcoding path separator | Using `:` on Windows or `;` on Linux breaks cross-platform scripts. | Use `File.pathSeparator`. |
| Putting `/` at start of ClassLoader resource | `ClassLoader.getResourceAsStream("/file")` returns `null` (classloader lookups must be relative). | Omit leading `/` when using ClassLoader. |
| Multiple versions of same JAR | Leads to Classpath Hell; JVM picks arbitrary version based on filesystem enumeration. | Exclude duplicate transitive dependencies in Maven/Gradle. |
| Forgetting `Main-Class` in manifest | Running `java -jar app.jar` outputs: `no main manifest attribute`. | Configure `Main-Class` in build tool or `jar` command. |
| Confusing `-cp` with `-jar` | When `java -jar` is used, the `-cp` flag is **completely ignored** by the JVM! | Specify classpath inside `MANIFEST.MF` Class-Path. |

---

## 8. Performance & Security Notes

### 8.1 Classpath Verification Overhead
During JVM startup, every JAR on the classpath must be opened and its central directory read to verify signatures and locate classes. In legacy applications with 300+ JARs on the classpath, this causes substantial startup latency (seconds). Modern containerized apps reduce this using `jlink` runtime images or Class Data Sharing (CDS).

---

## 9. Comparison: Classpath vs Modulepath

| Dimension | Classpath (`-cp`) | Modulepath (`-p` / `--module-path`) |
| :--- | :--- | :--- |
| **Introduced** | Java 1.0 | Java 9 (JPMS) |
| **Encapsulation** | ❌ None (All public classes visible)| ✅ Strong (`exports` in `module-info.java`)|
| **Validation** | Lazy (Fails at runtime when class loaded)| Eager (Fails at startup if dependency missing)|
| **Conflict Handling** | Silent shadowing | Fatal startup error on duplicate modules |
| **Reflection** | Unrestricted (`setAccessible(true)` works)| Restricted (Requires `opens` directive) |

---

## 10. Interview Questions

<details>
<summary><strong>1. Explain the difference between ClassNotFoundException and NoClassDefFoundError.</strong></summary>

- `ClassNotFoundException`: A **checked exception** thrown when an application tries to load a class by name at runtime (e.g. `Class.forName()`, `ClassLoader.loadClass()`) and no definition of the class can be found on the classpath.
- `NoClassDefFoundError`: An **unchecked JVM Error** that occurs when the class was present at compile time (so code compiled cleanly), but at runtime the class definition is missing from the classpath, or the class failed during static initialization (`<clinit>`).
</details>

<details>
<summary><strong>2. How does the Parent-Delegation Model work in Java class loading?</strong></summary>

When a classloader is requested to load a class, it does not search for the class immediately. Instead, it delegates the request to its parent classloader. This delegation continues up to the root Bootstrap ClassLoader. Only if the parent classloader cannot find the class does the child classloader attempt to search its own classpath. This prevents untrusted classes from overriding core platform classes (like `java.lang.String`).
</details>

<details>
<summary><strong>3. What is a "Fat JAR" (Uber-JAR)?</strong></summary>

A Fat JAR is a single deployable JAR archive that packages an application's compiled classes together with all of its third-party dependency JARs extracted and bundled into a single flat archive. It allows running complex applications with a simple `java -jar app.jar` without managing external classpath directories.
</details>

<details>
<summary><strong>4. What is a Multi-Release JAR (MRJAR)?</strong></summary>

Introduced in Java 9 (JEP 238), an MRJAR is a JAR archive that contains different versions of a class file compiled for different Java releases under the `META-INF/versions/` directory. When running on a newer JVM, the JVM automatically selects the optimized bytecode matching its version, while older JVMs load the baseline version.
</details>

<details>
<summary><strong>5. Why is the -cp flag ignored when running java -jar myapp.jar?</strong></summary>

The Java specification states that when executing with `-jar`, the specified JAR file is treated as the sole deployment unit. The runtime classpath is taken exclusively from the `Class-Path` attribute in the JAR's `META-INF/MANIFEST.MF` file. Any `-cp` or `-classpath` arguments provided on the command line are intentionally ignored.
</details>

<details>
<summary><strong>6. Why does String.class.getClassLoader() return null?</strong></summary>

Core JDK classes in the `java.base` module are loaded by the **Bootstrap ClassLoader**. In HotSpot, the Bootstrap ClassLoader is implemented in native C++ rather than as a Java object. In the Java API, native classloaders are represented as `null`.
</details>

<details>
<summary><strong>7. How can two classes with the exact same fully qualified name exist simultaneously in memory?</strong></summary>

In Java, a class's identity is defined by its **Fully Qualified Class Name + defining ClassLoader**. If two distinct `ClassLoader` instances each load `com.example.User`, the JVM treats them as completely separate, incompatible types on the heap, and casting between them throws `ClassCastException`.
</details>

<details>
<summary><strong>8. What is the difference between Class.getResourceAsStream() and ClassLoader.getResourceAsStream()?</strong></summary>

`Class.getResourceAsStream("path")` resolves relative paths relative to the package of that class (e.g. `com/app/path`), unless prefixed with `/`. `ClassLoader.getResourceAsStream("path")` always treats the path as absolute from the root of the classpath, and does **not** accept a leading `/`.
</details>

---

## 11. Practice Exercises

### Easy
1. **Create Executable JAR:** Compile a Hello World program and package it into an executable JAR using `jar --create --main-class`.
2. **Inspect Manifest:** Extract and read the `META-INF/MANIFEST.MF` of an existing JAR archive.
3. **Print ClassLoader:** Print the ClassLoader name for `ArrayList`, `SQLPermission`, and a custom application class.

### Medium
4. **Classpath Resource Reader:** Write a robust utility loading a JSON configuration file from the classpath using `getResourceAsStream()`.
5. **Custom Network ClassLoader:** Implement a custom `ClassLoader` that downloads and defines `.class` files over HTTP.
6. **Class Shadowing Demonstration:** Create two JARs containing identical class names with different outputs and demonstrate how classpath order dictates execution.

### Hard
7. **Multi-Release JAR Assembly:** Author a Multi-Release JAR that uses `Thread.sleep()` on Java 8 and Virtual Threads (`Thread.startVirtualThread()`) on Java 25.
8. **Class Relocation with Shade:** Configure Maven Shade Plugin to relocate conflicting package `com.google.common` to `com.app.shaded.guava`.
9. **Zero-Classpath Native Runner:** Use Java 25 `jlink` to build a modular runtime image that executes without any classpath specification.

<details>
<summary><strong>Exercise Solutions</strong></summary>

```java
// Solution to Exercise 4: Classpath Resource Reader
public class ConfigReader {
    public static String readResource(String resourcePath) {
        String path = resourcePath.startsWith("/") ? resourcePath.substring(1) : resourcePath;
        try (var in = Thread.currentThread().getContextClassLoader().getResourceAsStream(path)) {
            if (in == null) throw new IllegalArgumentException("Resource not found: " + path);
            return new String(in.readAllBytes(), java.nio.charset.StandardCharsets.UTF_8);
        } catch (java.io.IOException e) {
            throw new RuntimeException("Failed to read resource", e);
        }
    }
}
```
</details>

---

## 12. Summary & Cheat Sheet

```
┌────────────────────────────────────────────────────────────────────────┐
│                      CLASSPATH & JARS CHEAT SHEET                      │
├────────────────────────────────────────────────────────────────────────┤
│ Classpath Flag:     java -cp "dir/*;app.jar" Main                      │
│ Path Separators:    Windows uses ; | Linux/macOS uses :                │
│ JAR Command:        jar --create --file a.jar --main-class C -C dir .  │
│ Manifest Location:  META-INF/MANIFEST.MF                               │
│ -jar Caveat:        java -jar IGNORES the -cp command line flag!       │
│ Delegation:         Bootstrap (C++) -> Platform -> Application         │
│ Resource Lookup:    ClassLoader.getResourceAsStream("path/file.txt")   │
│ MRJARs:             META-INF/versions/N/ contains version-specific code│
│ Missing Class:      ClassNotFoundException (Dynamic) vs NoClassDefFound│
└────────────────────────────────────────────────────────────────────────┘
```

---

## 13. Further Reading
- Java Language Specification (JLS) Section 12.2: Loading of Classes and Interfaces
- [JEP 238: Multi-Release JAR Files (Java 9)](https://openjdk.org/jeps/238)
- Oracle Official Documentation: JAR File Specification
