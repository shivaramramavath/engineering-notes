# Packages and Imports

A **package** is a namespace that groups related classes and prevents name clashes. An **import** lets you refer to a class by its short name. Together they decide how code is organized on disk and how it refers to other code.

```java
package com.example.orders;              // 1. which package this file belongs to

import java.time.LocalDate;              // 2. imports
import java.util.List;

public class Order {                     // 3. the class
    private final List<String> items;
    private final LocalDate created = LocalDate.now();
    ...
}
```

The **fully qualified name** is `com.example.orders.Order`.

## Why packages?

| Purpose | Explanation |
|---------|-------------|
| **Avoid name clashes** | Two libraries can both have a class called `Date` or `Logger` |
| **Organize** | Related classes live together (`com.example.billing`, `com.example.users`) |
| **Access control** | Package-private members are visible only inside the package ([04-oop/04](../04-oop/04_encapsulation-and-access-modifiers.md)) |
| **Modularity** | Packages are the unit that modules export ([02_java-modules.md](./02_java-modules.md)) |

## Declaring a package

```java
package com.example.orders;      // must be the first statement (comments aside), once per file
```

### Packages map to folders

```
src/
└── com/
    └── example/
        └── orders/
            └── Order.java         ◄── package com.example.orders;
```

| Rule | Detail |
|------|--------|
| Folder path = package name | `com.example.orders` → `com/example/orders/` |
| Compiled output mirrors it | `out/com/example/orders/Order.class` |
| Maven / Gradle | Sources under `src/main/java/com/example/orders/` ([first project](../00-setup/05_first-project-with-maven-and-gradle.md)) |
| Packages are **flat names** | `com.example` and `com.example.orders` have no special relationship; there is no "inheritance" between them |

### Naming conventions

| Rule | Example |
|------|---------|
| All lowercase, no underscores | `com.example.orderservice` |
| **Reversed domain name** as the prefix | `example.com` → `com.example` |
| Then project / feature | `com.example.shop.payments` |
| No domain? Use something unique and stable | `io.github.yourname.project` |
| Do not use reserved prefixes | `java.*` (JDK only; the JVM refuses to load your classes there), `javax.*`, `jdk.*`; `jakarta.*` belongs to Jakarta EE |
| Avoid Java keywords in names | `com.example.new` is invalid |

### The default (unnamed) package

A file with **no** `package` statement is in the default package.

```java
// Hello.java, no package statement
public class Hello { }
```

Fine for tiny experiments. **Do not use it in real projects**: classes in named packages cannot import classes from the default package, tools and frameworks assume packages, and it does not scale.

## Imports

Without an import you must write the full name:

```java
java.util.List<java.time.LocalDate> dates = new java.util.ArrayList<>();
```

With imports:

```java
import java.time.LocalDate;
import java.util.ArrayList;
import java.util.List;

List<LocalDate> dates = new ArrayList<>();
```

### What an import does (and does not do)

| | |
|---|---|
| ✅ | Tells the **compiler** how to resolve a short name into a fully qualified one |
| ❌ | Does **not** copy code, load classes, or add a dependency; the class must still be available on the classpath/module path |
| ❌ | Has **no runtime cost** (imports disappear after compilation) |

### Kinds of imports

```java
import java.util.List;                 // single-type import (preferred)
import java.util.*;                    // on-demand: all TYPES in java.util
import static java.lang.Math.max;      // static import: a static member
import static java.lang.Math.*;        // all static members
import module java.base;               // module import (Java 25+): see below
```

#### On-demand (`*`) imports

```java
import java.util.*;          // List, Map, Set, ... but NOT java.util.concurrent.* or java.util.function.*
```

- Imports every type in **that package only**, not sub-packages
- Style guides prefer **single-type** imports: you can see where each name comes from, and a new class added to a package cannot silently change what a name means
- IDEs manage imports for you ([IntelliJ](../00-setup/04_intellij-idea.md): *Optimize imports*)

#### `java.lang` is imported automatically

`String`, `Object`, `Math`, `System`, `Integer`, `Thread`, `Record` and the other `java.lang` types need no import.

#### Static imports

```java
import static java.lang.Math.PI;
import static java.util.stream.Collectors.toList;

double area = PI * r * r;
```

Use sparingly: constants, test assertions (`assertEquals`), and very common helpers. Overuse makes it hard to see where a method comes from ([02_static.md](../04-oop/02_static.md)).

#### Module imports (Java 25+)

Java 25 added `import module ...;`, which imports all packages exported by a module, mostly useful for small programs and scripts:

```java
import module java.base;      // List, Map, Stream, Path, LocalDate, ...
```

Specific imports remain the norm for production code.

## Name conflicts

```java
import java.util.Date;
import java.sql.Date;         // ERROR: a type named Date is already imported

import java.util.Date;                       // import one...
java.sql.Date sqlDate = new java.sql.Date(0);   // ...use the other fully qualified
```

Resolution order for a simple name:
1. A type declared **in the same file**, or a single-type import (they cannot conflict)
2. A type in the **same package**
3. Types from **on-demand** imports (`*`) and `java.lang`; if two match: `reference to X is ambiguous`

```java
import java.util.*;
import java.awt.*;
List l;     // ERROR: ambiguous (java.util.List vs java.awt.List)
```

Fix with a single-type import of the one you want, or use a fully qualified name. Also avoid naming your own classes like common JDK types (`String`, `List`, `Date`).

## Package-private access

A class or member with no modifier is visible only to classes in the **same package**:

```java
package com.example.orders;
class OrderValidator { }                   // package-private class: hidden outside the package
public class OrderService {
    OrderValidator validator = new OrderValidator();   // OK
}
```

Packages do **not** nest for access purposes: `com.example.orders.internal` cannot see `com.example.orders`'s package-private members.

## Compiling and running with packages

```
project/
├── src/com/example/app/Main.java            package com.example.app;
└── src/com/example/util/Strings.java        package com.example.util;
```

```bash
javac -d out src/com/example/app/Main.java src/com/example/util/Strings.java
#     └ -d out: write class files under out/, mirroring the packages

java -cp out com.example.app.Main            # run by FULLY QUALIFIED class name
```

| Command | Result |
|---------|--------|
| `java -cp out Main` | `Could not find or load main class Main`: the class name is `com.example.app.Main` |
| `cd out/com/example/app && java Main` | Fails for the same reason: the classpath root must be `out` |
| `java src/com/example/app/Main.java` | Single-file source launch; other source files are **not** found automatically (Java 22+ supports multi-file source launch from the same directory tree) |

More on `-cp`: [01_classpath-and-jars.md](./01_classpath-and-jars.md).

## `package-info.java`

An optional file for package-level documentation and annotations:

```java
/** Order processing: orders, lines and pricing rules. */
package com.example.orders;
```

## Designing package structure

### By layer vs by feature

```
By layer                          By feature (usually better)
com.example.controllers           com.example.orders     (Order, OrderService, OrderRepository)
com.example.services              com.example.users      (User, UserService, UserRepository)
com.example.repositories          com.example.billing
```

| | By layer | By feature |
|---|----------|------------|
| Finding all code for one use case | Scattered across packages | One place |
| Hiding internals | Everything must be `public` to cross layers | Package-private helpers stay hidden |
| Change impact | Touches many packages | Local |
| Fits | Very small apps, strict layered architectures | Most applications and libraries |

See [24-design-patterns/04-architecture/01_layered-architecture.md](../24-design-patterns/04-architecture/01_layered-architecture.md).

### Guidelines

| Guideline | Why |
|-----------|-----|
| Keep packages **cohesive**: classes that change together live together | Easier maintenance |
| Avoid **cyclic dependencies** between packages (`a` ↔ `b`) | Cycles block modularization and testing |
| Put the **public API** in one package and implementation in `internal`/`impl` | Clear boundary (enforced for real by modules) |
| Make classes package-private unless other packages need them | Smaller API |
| Do not create one-class packages "for tidiness" | Noise |
| Test classes use the **same package** as the code under test, in `src/test/java` | Access to package-private members |
| Keep depth reasonable (3-5 levels) | Navigability |

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Package name does not match the folder | `class X is in package a.b but file is in c/` or class not found at runtime | Match them exactly |
| Running `java Main` for a class in a package | `Could not find or load main class Main` | `java -cp out com.example.Main` |
| Wrong classpath root | `ClassNotFoundException` / `NoClassDefFoundError` | Point `-cp` at the folder **above** the top package |
| Using the default package in a real project | Cannot import it from named packages | Use a named package |
| Expecting `import a.*;` to include `a.b` | `cannot find symbol` | Import sub-packages separately |
| Importing a class and thinking it adds the library | `package x does not exist` at compile time | Add the dependency to the build |
| `Date` ambiguity (`java.util` vs `java.sql`) | `reference to Date is ambiguous` | Single-type import, or qualify |
| Overusing `import static` | Unclear where methods come from | Qualify the calls |
| Unused imports left behind | Noise, misleading dependencies | Optimize imports |
| Cyclic package dependencies | Rigid architecture | Move shared types into a common package |
| Declaring a package starting with `java.` | `SecurityException: Prohibited package name` | Use your own domain |

## Key takeaways

- A package is a namespace mirrored by folders; name it with a reversed domain, all lowercase
- `import` only shortens names at compile time; it does not load classes or declare dependencies
- Prefer single-type imports; `java.lang` is implicit; `*` does not include sub-packages
- Run packaged classes by fully qualified name with the classpath pointing at the root folder
- Organize by feature, keep APIs small, and avoid cycles

**Next:** [Classpath and JARs](./01_classpath-and-jars.md)
