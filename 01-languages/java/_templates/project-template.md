# Project [XX]: [Project Name] (`[slug]`)

> [One-line summary of project architecture, core capabilities, and real-world system design concepts implemented.]

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-[xx]-[slug]` |
| **Java Baseline** | Java 25 LTS |
| **Complexity** | [Intermediate / Advanced / Production-Grade] |
| **Architectural Style** | [Clean Architecture / Event-Driven / Hexagonal / Reactor Pattern] |
| **Core Concepts** | [Concurrency, NIO, Zero-Copy, Virtual Threads, Records, Custom Parser, etc.] |

---

## 1. Architectural Overview & Component Breakdown

### 1.1 System Architecture
[Detailed architectural rationale and interaction between components...]

```
+-------------------------------------------------------------+
|                      System Architecture                    |
|                                                             |
|  [ Client / Driver ] ---> [ Controller / Handler ]         |
|                                   |                         |
|                                   v                         |
|                           [ Core Service ]                  |
|                            /            \                   |
|                           v              v                  |
|                 [ Domain Models ]   [ Storage / Buffer ]   |
+-------------------------------------------------------------+
```

### 1.2 Component Responsibilities
- **Domain Layer:** Immutable records and value objects defining core business rules.
- **Service / Engine Layer:** Core processing algorithms, state machines, and business workflows.
- **Infrastructure / I/O Layer:** File handling, socket communications, or memory buffers.
- **Harness / CLI Driver:** Standalone entry point demonstrating end-to-end capabilities.

---

## 2. Complete Project Source Code

### Component 1: Domain Models (`src/main/java/.../model/...`)
```java
// Complete domain records, enums, and value types
```

### Component 2: Service & Business Logic (`src/main/java/.../service/...`)
```java
// Complete business logic implementation
```

### Component 3: Storage & Infrastructure (`src/main/java/.../infra/...`)
```java
// Complete infrastructure implementation
```

### Component 4: CLI Application Driver (`src/main/java/.../Main.java`)
```java
// Complete runnable main entry point
```

---

## 3. Build Configuration

### Maven Configuration (`pom.xml`)
```xml
<?xml version="1.0" encoding="UTF-8"?>
<project xmlns="http://maven.apache.org/POM/4.0.0"
         xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
         xsi:schemaLocation="http://maven.apache.org/POM/4.0.0 
         http://maven.apache.org/xsd/maven-4.0.0.xsd">
    <modelVersion>4.0.0</modelVersion>

    <groupId>com.learning.java</groupId>
    <artifactId>[project-artifact-id]</artifactId>
    <version>1.0.0</version>

    <properties>
        <maven.compiler.source>25</maven.compiler.source>
        <maven.compiler.target>25</maven.compiler.target>
        <project.build.sourceEncoding>UTF-8</project.build.sourceEncoding>
        <junit.jupiter.version>5.11.4</junit.jupiter.version>
    </properties>

    <dependencies>
        <dependency>
            <groupId>org.junit.jupiter</groupId>
            <artifactId>junit-jupiter</artifactId>
            <version>${junit.jupiter.version}</version>
            <scope>test</scope>
        </dependency>
    </dependencies>
</project>
```

---

## 4. Comprehensive JUnit 5 Test Suite

```java
// Complete unit and integration test suite
```

---

## 5. Verification & Sample Execution

### Compilation & Test Execution
```bash
$ mvn clean test
```

### Expected Test Output
```text
[INFO] -------------------------------------------------------
[INFO]  T E S T S
[INFO] -------------------------------------------------------
[INFO] Running com.learning.java.ProjectTest
[INFO] Tests run: 5, Failures: 0, Errors: 0, Skipped: 0, Time elapsed: 0.123 s
[INFO] BUILD SUCCESS
```

### Sample Interactive CLI Run
```bash
$ java -cp target/classes com.learning.java.Main
```
```text
[Exact verified output]
```

---

[← Return to Projects Index](../README.md)
