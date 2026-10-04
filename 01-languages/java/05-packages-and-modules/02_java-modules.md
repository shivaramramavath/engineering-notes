# Java Modules

The **Java Platform Module System (JPMS)**, introduced in Java 9, groups packages into **named modules** that declare what they **require** and what they **export**. It gives Java **strong encapsulation** (a `public` class can still be hidden from other modules) and **reliable configuration** (missing or conflicting modules are detected at startup).

```
module com.example.app
   requires ──────────►  module com.example.greetings
                              exports com.example.greetings.api     ✅ visible
                              (com.example.greetings.internal)      ❌ hidden, even though classes are public
```

## Why modules exist

| Problem on the classpath | JPMS answer |
|--------------------------|-------------|
| Every `public` class is accessible to everyone, so internals leak and get depended on | Only **exported** packages are accessible |
| Missing JARs are discovered at runtime (`NoClassDefFoundError`) | `requires` are verified at **startup** |
| Duplicate packages across JARs silently clash | **Split packages** are an error |
| The JDK itself was a monolith (`rt.jar`) | The JDK is split into modules (`java.base`, `java.sql`, ...) |
| Apps ship a full JDK | `jlink` builds a **minimal runtime** with only the modules needed |

## A module declaration

A module is described by `module-info.java` at the root of its source tree.

```java
// src/module-info.java
module com.example.app {
    requires java.sql;                              // needs the JDK's java.sql module
    requires com.example.greetings;                 // needs another module
    exports com.example.app.api;                    // makes this package visible to others
}
```

| Directive | Meaning |
|-----------|---------|
| `requires M;` | This module needs module `M` (and can read its exported packages) |
| `requires transitive M;` | Anyone who requires **me** also reads `M` (use when `M`'s types appear in your public API) |
| `requires static M;` | `M` is needed at compile time, optional at runtime |
| `exports p;` | Package `p`'s public types are accessible to all modules |
| `exports p to A, B;` | Qualified export: only to modules `A` and `B` |
| `opens p;` | Allows **deep reflection** (private members) on `p` at runtime |
| `opens p to A;` | Deep reflection only for module `A` |
| `uses S;` | This module consumes service `S` (via `ServiceLoader`) |
| `provides S with Impl;` | This module supplies an implementation of `S` |
| `open module m { }` | Opens **all** packages for reflection |

Every module implicitly `requires java.base` (core types: `String`, `List`, ...).

### Naming

Use a reversed-domain name, usually matching the root package: `com.example.app`. Names should be **stable**: renaming a module is a breaking change.

## Readability and accessibility

For code in module `A` to use a class in module `B`, **three** things must hold:

1. `A` **requires** `B` (readability)
2. `B` **exports** the class's package (accessibility)
3. The class and member are `public` (normal access rules)

```java
// In module com.example.greetings
package com.example.greetings.internal;
public class Secret { }          // public, but package is not exported
// com.example.app cannot use Secret: compile error "package ... is not visible"
```

**`public` no longer means "public to everyone"**: it means public to modules that can access the package.

## Example: two modules

```
src/
├── com.example.greetings/
│   ├── module-info.java
│   └── com/example/greetings/
│       ├── Greeter.java
│       └── internal/Formatter.java
└── com.example.app/
    ├── module-info.java
    └── com/example/app/Main.java
```

```java
// com.example.greetings/module-info.java
module com.example.greetings {
    exports com.example.greetings;               // internal is NOT exported
}
```
```java
// com.example.greetings/com/example/greetings/Greeter.java
package com.example.greetings;
import com.example.greetings.internal.Formatter;
public class Greeter {
    public String greet(String name) { return Formatter.shout("Hello, " + name); }
}
```
```java
// com.example.app/module-info.java
module com.example.app {
    requires com.example.greetings;
}
```
```java
// com.example.app/com/example/app/Main.java
package com.example.app;
import com.example.greetings.Greeter;
public class Main {
    public static void main(String[] args) {
        System.out.println(new Greeter().greet("modules"));
    }
}
```

Compile and run:

```bash
# Compile every module into mods/<module-name>/
javac -d mods --module-source-path src $(find src -name "*.java")

# Run by module and main class
java --module-path mods -m com.example.app/com.example.app.Main
```

| Flag | Meaning |
|------|---------|
| `--module-path` / `-p` | Where to find modules (folders of exploded modules, or modular JARs) |
| `--module-source-path` | Source layout with one folder per module |
| `-m` / `--module` | `module/mainClass` to run |
| `--add-modules` | Add root modules (for example, ones nobody `requires`) |

Packaged as modular JARs: `jar --create --file mods/greetings.jar -C mods/com.example.greetings .`, then `java -p mods -m com.example.app/com.example.app.Main`.

## Types of modules

| Kind | What it is | Notes |
|------|-----------|-------|
| **Named / explicit** | Has `module-info.java` | The goal |
| **Automatic** | A plain JAR placed on the **module path** | Name comes from `Automatic-Module-Name` in the manifest, or the file name; exports **everything**, reads everything |
| **Unnamed** | Everything on the **classpath** | Reads all modules; exports everything; cannot be `required` by named modules |
| **Platform** | Provided by the JDK | `java.base`, `java.sql`, `java.net.http`, `java.logging`, ... |

Library authors should at least add an `Automatic-Module-Name` to the manifest, so users can depend on a stable name even before the library is fully modular.

## Looking at modules

```bash
java --list-modules                          # all observable modules (JDK + module path)
java --describe-module java.sql
java -p mods --describe-module com.example.app
jar --describe-module --file mods/app.jar
jdeps --module-path mods -s app.jar          # analyze dependencies of a JAR or classes
jdeps --print-module-deps app.jar            # which JDK modules does this app need?
```

`jdeps` is very useful when migrating and when deciding what to feed to `jlink`.

## `jlink`: a custom runtime

```bash
jlink --module-path "$JAVA_HOME/jmods:mods" \
      --add-modules com.example.app \
      --output build/runtime \
      --strip-debug --no-header-files --no-man-pages

build/runtime/bin/java -m com.example.app/com.example.app.Main
```

The result contains **only** the required JDK modules plus yours: typically tens of MB instead of a full JDK, with no separate Java installation needed. Common for containers, desktop installers (`jpackage`) and CLI tools. Note that `jlink` requires **all** dependencies to be modules (automatic modules are not allowed).

## Services: loose coupling across modules

`provides ... with` and `uses` combine with `java.util.ServiceLoader` to find implementations without compile-time dependencies:

```java
// api module
module com.example.payments.api { exports com.example.payments.api; }          // interface PaymentProvider

// provider module
module com.example.payments.stripe {
    requires com.example.payments.api;
    provides com.example.payments.api.PaymentProvider with com.example.payments.stripe.StripeProvider;
}

// consumer module
module com.example.shop {
    requires com.example.payments.api;
    uses com.example.payments.api.PaymentProvider;
}
```
```java
ServiceLoader<PaymentProvider> loader = ServiceLoader.load(PaymentProvider.class);
for (PaymentProvider p : loader) { ... }
```

Adding or swapping a provider only changes the module path. (On the classpath, the equivalent is `META-INF/services/` files.)

## Reflection and `opens`

Frameworks (JSON mappers, ORMs, dependency injection) use **deep reflection** on your classes. A package that is merely `exports`ed gives access to **public** members only; reflecting on private fields needs `opens`:

```java
module com.example.app {
    requires com.fasterxml.jackson.databind;
    opens com.example.app.model to com.fasterxml.jackson.databind;     // Jackson may reflect on private members
}
```

Without it: `InaccessibleObjectException: Unable to make field private ... accessible: module ... does not "opens ..."`.

### The JDK's own internals

Since Java 17, `sun.misc`-style internals of the JDK are **strongly encapsulated**, and the old `--illegal-access` option is gone. Old libraries that reflect into JDK internals fail on modern Java unless you pass escape hatches (use them only as a stopgap and prefer upgrading the library):

```bash
java --add-opens java.base/java.lang=ALL-UNNAMED -jar app.jar
java --add-exports java.base/sun.nio.ch=ALL-UNNAMED -jar app.jar
```

See [13-advanced-language-features/01_reflection.md](../13-advanced-language-features/01_reflection.md).

## Rules and restrictions

| Rule | Consequence |
|------|-------------|
| **No cyclic dependencies** between modules | Module A requires B requires A is an error |
| **No split packages**: a package may exist in only one module | Two JARs with the same package cannot both be named modules on the module path |
| One `module-info.java` per module | Compiled to `module-info.class` |
| Module names are global and unique | Choose reversed-domain names |
| Named modules cannot read the classpath | Use automatic modules or migrate dependencies |

## Should you use modules?

| Situation | Recommendation |
|-----------|----------------|
| Typical Spring Boot or enterprise application | **Classpath** is the norm; frameworks and fat JARs work best there |
| Library you publish | Add `Automatic-Module-Name` at minimum; full `module-info.java` if you want strong encapsulation |
| Large codebase that needs enforced boundaries | Modules (or Maven/Gradle modules plus an architecture-test tool such as ArchUnit) |
| Small runtime images, CLI tools, desktop apps | `jlink` / `jpackage` with modules |
| Learning | Understand the concepts; do one small example |

Strong boundaries without JPMS are also available through package-private classes, internal packages and build-tool modules ([00_packages-and-imports.md](./00_packages-and-imports.md)).

### Not the same as Maven/Gradle modules

A Maven multi-module project is just several build sub-projects. Each can become a JPMS module by adding `module-info.java`, but they are independent concepts, and a project can use either, both or neither.

## Migrating to modules

1. Run `jdeps` to learn what your code depends on
2. Make sure dependencies have module names (automatic modules are acceptable temporarily)
3. Migrate **bottom-up**: leaf libraries first, the application last
4. Add `module-info.java`; export only the API packages; add `opens` for reflection users
5. Fix split packages and cycles
6. Test on the module path

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Forgetting `requires` | `package x is not visible (package x is declared in module y, but module z does not read it)` | Add `requires y;` |
| Forgetting `exports` | `package x is declared in module y, which does not export it` | Add `exports x;` |
| Missing `opens` for reflection | `InaccessibleObjectException` at runtime | `opens pkg to framework.module;` |
| Using `requires` where `requires transitive` is needed | Callers get `package ... not visible` for your API's types | Use `requires transitive` for types exposed in your API |
| Split packages | `Package x in both module a and module b` | Rename or merge packages |
| Cyclic `requires` | `cyclic dependency` | Extract shared code into a third module |
| Putting a modular JAR on the classpath | `module-info.class` ignored; everything open | Use `--module-path` |
| Depending on automatic modules in a `jlink` build | `jlink` fails | Convert dependencies or avoid `jlink` for those |
| Relying on JDK internals (`sun.*`) | `IllegalAccessError` / `InaccessibleObjectException` on new Java versions | Upgrade the library, use public APIs, `--add-opens` only as a stopgap |
| Naming modules after changeable details | Breaking renames | Stable, reversed-domain names |
| Treating JPMS as a security boundary | False sense of security | It prevents accidents; use real security controls |
| Adding JPMS to a small app with no benefit | Extra complexity | Stay on the classpath |

## Key takeaways

- A module = a named set of packages with `requires` (dependencies) and `exports` (public surface)
- `public` plus an `exports` is needed for cross-module access; `opens` is for deep reflection
- JPMS gives strong encapsulation, startup-time dependency checks and `jlink` runtimes
- Modules, automatic modules and the classpath coexist; most applications still run on the classpath
- Maven/Gradle "modules" are a different concept
- Use `jdeps` to analyze, and migrate bottom-up

**Next:** [06-exceptions-and-debugging](../06-exceptions-and-debugging/README.md)
