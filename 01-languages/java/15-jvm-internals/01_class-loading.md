# Class Loading

The JVM doesn't load your whole program up front. It loads each class **on demand**, the first time it's needed. **Class loading** is the process that finds a `.class` file, turns it into a `Class` object in memory, checks it, and runs its static initialization.

It explains an entire family of confusing errors (`ClassNotFoundException`, `NoClassDefFoundError`, `NoSuchMethodError`, `ClassCastException` between "the same" class), how plugins and application servers isolate code, and part of why startup takes time.

**Prerequisites:** [JVM Architecture](00_jvm-architecture.md), [Classpath and JARs](../05-packages-and-modules/01_classpath-and-jars.md), [Initialization Order](../04-oop/05_initialization-order.md).

---

## 1. The lifecycle of a class

```text
 Loading ──► Linking ──────────────────────────────► Initialization ──► Use ──► Unloading
              ├─ Verification   (is the bytecode valid and safe?)
              ├─ Preparation    (allocate static fields, set DEFAULT values: 0/null/false)
              └─ Resolution     (replace symbolic references with direct ones; often lazy)
```

| Phase | What happens |
|---|---|
| **Loading** | A class loader finds the binary form (`.class` bytes) and creates the `java.lang.Class` object and its metadata (in metaspace) |
| **Verification** | The JVM checks the bytecode is structurally valid and type-safe, so corrupt or malicious classes can't break memory safety |
| **Preparation** | Static fields get memory and **default values** (not your initializers yet) |
| **Resolution** | Symbolic names in the constant pool (class, method, field references) become direct references. Usually done lazily on first use |
| **Initialization** | **Static initializers and static field initializers run**, in textual order, once |
| **Unloading** | When a class's loader becomes unreachable, the class can be garbage collected |

---

## 2. Class loaders and delegation

A **class loader** is an object that knows how to find classes. The built-in loaders form a hierarchy:

```text
 Bootstrap loader        core JDK classes (java.base: java.lang.*, java.util.*)   [native; appears as null]
      ▲
 Platform loader         other JDK modules (java.sql, java.xml, ...)
      ▲
 Application loader      your classpath / module path   (a.k.a. system class loader)
      ▲
 custom loaders          plugins, app servers, frameworks (optional)
```

```java
String.class.getClassLoader();                       // null → loaded by the bootstrap loader
java.sql.Driver.class.getClassLoader();              // platform class loader
Main.class.getClassLoader();                         // app class loader
ClassLoader.getSystemClassLoader();                  // the application loader
```

### Parent-first delegation

When asked to load a class, a loader **first asks its parent**, and only if the parent can't find it does it look itself:

```text
 loadClass("com.acme.Foo")
   app loader → asks platform loader → asks bootstrap loader → not found
              ← platform: not found
   app loader: searches the classpath → found → defines the class
```

Why? **Security and consistency:** nobody can replace `java.lang.String` by putting their own copy on the classpath, because the bootstrap loader always answers first.

### Class identity = name + loader

Two classes with the same name loaded by **different loaders are different types**:

```text
ClassCastException: class com.acme.Plugin cannot be cast to class com.acme.Plugin
   (com.acme.Plugin is in unnamed module of loader 'app'; ... of loader com.acme.PluginLoader @1a2b3c)
```

This looks absurd ("it's the same class!") until you see that two loaders each loaded their own copy. It's the classic symptom of a class present in two places (a library duplicated in an app server and in the application).

---

## 3. Initialization: when static code runs

A class is **initialized lazily**, on the first **active use**:

- `new Foo()`
- calling a static method of `Foo`, or reading/writing a non-constant static field
- `Class.forName("Foo")` (with initialization)
- initializing a subclass (the superclass is initialized first)
- it's the main class that the JVM launches

**Not** triggered by: referencing `Foo.class`, declaring a variable of type `Foo`, creating an *array* of `Foo`, or reading a **compile-time constant** (`static final int MAX = 10`: it's inlined at compile time).

```java
class Config {
    static final int VERSION = 3;                 // compile-time constant: inlined, doesn't trigger init
    static final String NAME = compute();         // NOT a constant: needs initialization
    static { System.out.println("Config init"); }
    static String compute() { return "x"; }
}

int v = Config.VERSION;     // prints nothing: constant inlined
String n = Config.NAME;     // prints "Config init" first
```

Rules:

- Initialization happens **once per class**, and the JVM makes it **thread-safe** by holding an initialization lock. That's why the *initialization-on-demand holder* idiom is a safe lazy singleton ([Thread Safety](../14-concurrency/02_thread-safety.md#3-safe-publication)).
- The superclass is initialized before the subclass. Static fields and blocks run **in textual order**.
- If a static initializer throws, you get **`ExceptionInInitializerError`**, and the class is unusable: every later use throws **`NoClassDefFoundError: Could not initialize class X`**. Look in the logs for the *first* error, which carries the real cause.
- Two classes whose static initializers depend on each other, initialized from two threads, can **deadlock** ([Deadlock](../14-concurrency/14_deadlock-livelock-starvation.md)).

`Class.forName(name)` loads **and initializes**. `loader.loadClass(name)` loads **without** initializing.

---

## 4. Custom class loaders and the context class loader

You write a custom loader by extending `ClassLoader` and overriding `findClass`:

```java
class PluginLoader extends ClassLoader {
    private final Path dir;
    PluginLoader(Path dir, ClassLoader parent) { super(parent); this.dir = dir; }

    @Override
    protected Class<?> findClass(String name) throws ClassNotFoundException {
        try {
            byte[] bytes = Files.readAllBytes(dir.resolve(name.replace('.', '/') + ".class"));
            return defineClass(name, bytes, 0, bytes.length);
        } catch (IOException e) {
            throw new ClassNotFoundException(name, e);
        }
    }
}
```

(`URLClassLoader` already handles the common "load from these JARs" case.)

Uses: **plugin systems**, **hot reload**, **isolating applications** from each other (application servers give each web app its own loader, so each can use its own library versions), and frameworks that generate classes at runtime.

Some designs use **child-first** delegation (try the loader's own classes before the parent's) so a plugin can ship its own version of a library. It's powerful and a common source of the two-copies `ClassCastException` above.

### The context class loader

Library code that needs to find *your* classes (JNDI, `ServiceLoader`, JDBC drivers, frameworks) can't see them from its own loader if it lives higher in the hierarchy. A thread carries a **context class loader** that frameworks set so such code knows where to look:

```java
Thread.currentThread().getContextClassLoader();
```

Wrong or missing context loaders cause "class not found" errors in app servers and when moving work to new threads.

`ServiceLoader.load(Service.class)` finds implementations declared in `META-INF/services` or `module-info.java` (`provides ... with ...`).

### Unloading

A class can be unloaded only when **its loader** is unreachable, so classes loaded by the bootstrap/platform/application loaders effectively live for the whole run. Redeploying a web app creates a new loader. If anything still references the old one (a static field, a `ThreadLocal`, a thread, a driver registration), the old loader and **all its classes stay in metaspace**: a *class loader leak* that ends in `OutOfMemoryError: Metaspace` ([Memory Areas](02_jvm-memory-areas.md)).

---

## 5. The errors, decoded

| Error | Meaning | Typical cause |
|---|---|---|
| `ClassNotFoundException` (checked) | You asked for a class **by name at runtime** (`Class.forName`, `loadClass`, reflection, config) and no loader found it | Missing JAR/dependency, wrong name, wrong class loader |
| `NoClassDefFoundError` (an `Error`) | The class was present at **compile time** but is missing, or failed to initialize, at **runtime** | Dependency not packaged/`provided` scope, `ExceptionInInitializerError` earlier, version trimmed from the classpath |
| `NoSuchMethodError` / `NoSuchFieldError` / `AbstractMethodError` / `IncompatibleClassChangeError` | The class is found, but its shape differs from what the calling code was compiled against | **Version conflict**: two versions of a library on the classpath ("JAR hell"), and the wrong one won |
| `ClassCastException: X cannot be cast to X` | Same name, different loaders | Duplicate classes across loaders |
| `UnsupportedClassVersionError` | Class compiled for a newer Java than the JVM running it | Build with a higher `--release` than the runtime ([Release Timeline](../12-modern-java/00_java-release-timeline.md)) |
| `VerifyError` | Bytecode fails verification | Corrupt or bad bytecode-generation/instrumentation |
| `LinkageError` (parent of several above) | Problem linking classes | See the message |

### Debugging class loading

```bash
java -Xlog:class+load=info -cp ... Main          # log every class loaded and where it came from (Java 9+)
java -verbose:class -cp ... Main                 # older equivalent
java -Xlog:class+load=info:file=classes.log ...
```

The log lines include the **source** (`jrt:/java.base`, `file:/.../lib.jar`, `shared objects file`), so you can see *which JAR* supplied a class, which is the fastest way to find a duplicate.

```java
System.out.println(Foo.class.getProtectionDomain().getCodeSource().getLocation());   // which JAR/dir?
System.out.println(Foo.class.getClassLoader());
```

Dependency analysis helps too: `mvn dependency:tree` / `gradle dependencies` show conflicting versions ([Dependency Management](../18-build-and-dependencies/02_dependency-management.md)). `jcmd <pid> VM.classloaders` lists loaders in a running JVM.

---

## 6. Startup cost and Class Data Sharing

Loading, parsing, and verifying thousands of classes takes real time, which is a big part of JVM startup. **Class Data Sharing (CDS)** stores pre-parsed class metadata in a memory-mappable archive:

- The JDK ships a default archive for core classes (enabled by default since Java 12).
- **AppCDS** extends this to your application's classes.
- Newer JDKs go further with ahead-of-time caches that include loaded and linked classes (see [JIT Compiler](06_jit-compiler.md)).

Other startup levers: fewer classes (trim dependencies), avoid heavy static initializers, avoid classpath scanning at startup, and defer work until first use.

Bytecode verification is part of linking and can't be turned off in production; the old `-Xverify:none` is deprecated.

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Treating `NoClassDefFoundError` like `ClassNotFoundException` | Check packaging/scope, and search the logs for an earlier `ExceptionInInitializerError` |
| Two versions of the same library on the classpath | Resolve in the build (`dependency:tree`), exclude duplicates |
| Putting a library both in the app server and in the WAR | Mark it `provided`, or exclude one copy |
| Heavy or fragile static initializers | Keep them trivial. Failures poison the class for the rest of the run |
| Class loader leaks on redeploy (ThreadLocals, static registries, drivers) | Clean up in a shutdown hook/context-destroyed callback; `remove()` thread locals |
| Using `Class.forName` with user-supplied names | Allow-list the class names |
| Assuming `static final` fields always trigger initialization | Compile-time constants don't |
| Writing a custom loader when `ServiceLoader`/`URLClassLoader` would do | Use the existing mechanisms |

### Debugging flow

```text
NoClassDefFoundError / ClassNotFoundException
   ├─ Search the log for an earlier ExceptionInInitializerError             → static init failed
   ├─ Is the JAR actually in the runtime classpath/image? (-Xlog:class+load) → packaging/scope
   ├─ Same error only inside a server/plugin container?                     → class loader / context loader
   └─ NoSuchMethodError instead?                                            → version conflict: dependency tree
```

---

## Quick Summary

- Classes are loaded **lazily**: load → verify → prepare → resolve → **initialize** (static code runs once, thread-safely) → use → unload.
- Loaders form a hierarchy (**bootstrap → platform → application → custom**) with **parent-first delegation**. Class identity is **name + loader**.
- Static initializers run on first active use. Compile-time constants are inlined and don't trigger them. A failure poisons the class (`ExceptionInInitializerError`, then `NoClassDefFoundError`).
- `ClassNotFoundException` = not found by name; `NoClassDefFoundError` = was there at compile time, not usable now; `NoSuchMethodError` = version mismatch.
- Debug with `-Xlog:class+load`, `getProtectionDomain().getCodeSource()`, and the dependency tree.
- Leaked class loaders end in `OutOfMemoryError: Metaspace`. CDS and AOT caches speed up startup.

**Next:** [JVM Memory Areas](02_jvm-memory-areas.md)