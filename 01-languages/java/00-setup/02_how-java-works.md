# How Java Works

Java source code is not compiled to machine code for one CPU. It is compiled to **bytecode**, which a **JVM** runs on any platform. This is what "write once, run anywhere" means.

```
Hello.java ──javac──► Hello.class ──java──► JVM ──► Operating system / CPU
 (source)             (bytecode)            (loads, verifies, runs)
```

## JDK vs JRE vs JVM

```
┌─────────────────────────── JDK ───────────────────────────┐
│  javac  jshell  jar  javadoc  jcmd  jlink  javap  ...     │
│  ┌──────────────────────── JRE ────────────────────────┐  │
│  │  Standard library (java.base, java.sql, ...)        │  │
│  │  ┌──────────────────── JVM ─────────────────────┐   │  │
│  │  │ class loader, interpreter, JIT, GC, memory   │   │  │
│  │  └──────────────────────────────────────────────┘   │  │
│  └─────────────────────────────────────────────────────┘  │
└───────────────────────────────────────────────────────────┘
```

| Term | What it is | Who needs it |
|------|-----------|--------------|
| **JVM** | The virtual machine that executes bytecode | Everyone (inside the JRE/JDK) |
| **JRE** | JVM + standard library | Running programs |
| **JDK** | JRE + compiler and developer tools | Developers |

Since Java 11, Oracle and most vendors no longer publish a separate JRE download. You install a JDK, and you can build a small custom runtime for deployment with `jlink`.

## The compile-and-run steps

```java
// Hello.java
public class Hello {
    public static void main(String[] args) {
        System.out.println("Hello, Java!");
    }
}
```

```bash
javac Hello.java      # 1. compile: Hello.java → Hello.class
java Hello            # 2. run:     JVM loads Hello.class, calls main
```

### Single-file launch (Java 11+)

```bash
java Hello.java       # compiles in memory and runs; no .class file written
```

Great for scripts and learning. Java 22+ also runs multi-file programs this way. Java 25 additionally allows **compact source files** and instance `main` methods, so a small program can skip the class wrapper (see [12-modern-java](../12-modern-java/README.md)).

## What happens inside the JVM

```
.class files
     │
     ▼
[1] Class loader     loads classes on first use
     │
     ▼
[2] Verifier         checks bytecode is safe and well-formed
     │
     ▼
[3] Execution        interpreter runs bytecode at start
     │                       │
     │                       └─► JIT compiler turns "hot" methods into native code
     ▼
[4] Memory + GC      heap, stacks, automatic garbage collection
```

| Stage | Purpose |
|-------|---------|
| Class loading | Finds and loads `.class` data when first needed |
| Verification | Rejects invalid or unsafe bytecode |
| Interpretation | Starts running immediately, no waiting |
| **JIT compilation** | Compiles frequently run code to native machine code while the app runs |
| Garbage collection | Frees objects that are no longer reachable |

This is why Java programs start slower and then speed up (**warm-up**). Details: [15-jvm-internals](../15-jvm-internals/README.md).

## Inspecting bytecode

```bash
javap -c Hello.class
```

```
public static void main(java.lang.String[]);
  Code:
     0: getstatic     #7    // Field java/lang/System.out
     3: ldc           #13   // String Hello, Java!
     5: invokevirtual #15   // Method java/io/PrintStream.println
     8: return
```

A `.class` file always starts with the bytes `CA FE BA BE`. It also stores the Java version it was built for (see [version management](./01_java-version-management.md)).

## The classpath

The **classpath** tells the JVM where to look for `.class` files and JARs.

```bash
java -cp . Hello                          # current directory
java -cp out Hello                        # compiled classes in ./out
java -cp "out:lib/*" com.example.App      # Linux/macOS (use ; on Windows)
javac -d out src/com/example/App.java     # -d puts class files in out/
```

Packages map to folders: class `com.example.App` must be at `com/example/App.class` under a classpath root. More in [05-packages-and-modules](../05-packages-and-modules/README.md).

## Platform independence

```
Same Hello.class ──► JVM for Windows ──► runs
                 ├─► JVM for macOS   ──► runs
                 └─► JVM for Linux   ──► runs
```

The **bytecode** is portable; the **JVM** is platform-specific. Anything compiled for a particular Java version needs that version or newer to run.

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| `java Hello.class` | `Could not find or load main class Hello.class` | `java Hello` |
| Running from the wrong directory | `Could not find or load main class` | `cd` to the class root, or set `-cp` |
| Forgetting the package name | `Error: Could not find or load main class App` | Use `java -cp out com.example.App` |
| Confusing `javac` and `java` | `javac Hello` → `file not found` | `javac` takes a **source file** (`Hello.java`), `java` takes a **class name** |
| Compiled with newer Java than runtime | `UnsupportedClassVersionError` | Use the same or newer JVM, or compile with `--release` |
| Expecting Java to be "interpreted only" | Confusion about warm-up | Bytecode is interpreted, then JIT-compiled |

## Key takeaways

- `javac` compiles source to **bytecode**; `java` starts a **JVM** that runs it
- **JDK ⊃ JRE ⊃ JVM**: install a JDK to develop
- The JVM loads, verifies, interprets and JIT-compiles code, and manages memory with a GC
- The classpath tells the JVM where classes live; packages mirror folders
- Use `java File.java` for quick runs and `javap -c` to look at bytecode

**Next:** [JShell](./03_jshell.md)
