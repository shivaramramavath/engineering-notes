# Classpath and JARs

The **classpath** tells the JVM (and the compiler) where to look for compiled classes and resources. A **JAR** (Java ARchive) is how classes are packaged and distributed. Most "class not found" and "wrong version loaded" problems come from a misunderstanding of these two.

```
java -cp "out:lib/gson.jar:lib/slf4j.jar" com.example.app.Main
          └─ classpath: folders and JARs searched, left to right ─┘
```

## The classpath

A list of **entries**; each entry is a directory of `.class` files (the root of a package tree), a JAR file, or a wildcard of JARs.

```
Looking for com.example.util.Strings
        │
        ▼ for each classpath entry, in order:
   out/com/example/util/Strings.class?      → found? use it and STOP
   lib/gson.jar → com/example/util/Strings.class inside?
   lib/slf4j.jar → ...
        │
        ▼ not found anywhere
   ClassNotFoundException / NoClassDefFoundError
```

### Specifying it

| Way | Example |
|-----|---------|
| `-cp` / `-classpath` / `--class-path` (preferred) | `java -cp out:lib/gson.jar com.example.Main` |
| `CLASSPATH` environment variable | Avoid: global, invisible, causes surprises |
| Manifest `Class-Path` | Inside an executable JAR (see below) |
| Build tool | Maven and Gradle compute it from your declared dependencies |
| Default (nothing given) | The **current directory** (`.`) |

| OS | Separator | Example |
|----|-----------|---------|
| Linux / macOS | `:` | `out:lib/a.jar` |
| Windows | `;` | `out;lib\a.jar` |

```bash
java -cp "lib/*:out" com.example.Main      # lib/* = every .jar in lib (not subfolders, not .class files)
```

Quote wildcards so the shell does not expand them. Order of entries matters.

### The classpath root

An entry must be the folder **containing the top-level package directory**, not the package folder itself:

```
out/com/example/Main.class      → -cp out        ✅
                                → -cp out/com    ❌ (JVM would look for out/com/com/example/Main.class)
```

### Same classes twice: first one wins

```bash
java -cp "lib/lib-1.0.jar:lib/lib-2.0.jar" Main      # 1.0 wins for every class present in both
```

Two versions of one library on the classpath produce unpredictable mixtures: the **"JAR hell"** problem. Build tools resolve versions for you ([18-build-and-dependencies/02_dependency-management.md](../18-build-and-dependencies/02_dependency-management.md)).

## Compiling against the classpath

```bash
javac -cp "lib/*" -d out src/com/example/App.java
java  -cp "out:lib/*" com.example.App
```

The compiler needs the classes your source references; the JVM needs them again at runtime. Both use `-cp`.

## JAR files

A JAR is a **ZIP** with a conventional layout:

```
app.jar
├── META-INF/
│   └── MANIFEST.MF              ◄── metadata (entry point, classpath, ...)
├── com/example/app/Main.class
├── com/example/util/Strings.class
└── config/default.properties    ◄── resources
```

### Creating and inspecting

```bash
# Create a JAR from compiled classes in out/, with an entry point
jar --create --file app.jar --main-class com.example.app.Main -C out .
# (short form: jar cfe app.jar com.example.app.Main -C out .)

jar tf app.jar                      # list contents (t = table, f = file)
unzip -l app.jar                    # same, with sizes
jar xf app.jar                      # extract
```

`-C out .` means "change to `out`, then add everything". Without it, paths inside the JAR would contain `out/`, and the classes would not be found.

### Running

```bash
java -jar app.jar                   # uses Main-Class from the manifest
java -cp app.jar com.example.app.Main   # no manifest needed
```

**With `-jar`, the `-cp` option and the `CLASSPATH` variable are ignored.** The classpath comes only from the manifest:

```
Manifest-Version: 1.0
Main-Class: com.example.app.Main
Class-Path: lib/gson.jar lib/slf4j-api.jar        ◄── space-separated, relative to the JAR's location
```

Alternative: `java -cp "app.jar:lib/*" com.example.app.Main`.

### The manifest

| Attribute | Purpose |
|-----------|---------|
| `Main-Class` | Entry point for `java -jar` |
| `Class-Path` | Extra JARs next to this one |
| `Automatic-Module-Name` | Stable module name for library JARs ([02](./02_java-modules.md)) |
| `Multi-Release: true` | Version-specific classes under `META-INF/versions/N/` |
| `Implementation-Version` | Readable with `Package.getImplementationVersion()` |

Build tools generate the manifest: Maven's `maven-jar-plugin`, Gradle's `jar { manifest { ... } }`.

## Library JARs vs "fat" JARs

| | Thin JAR | Fat / uber JAR |
|---|----------|----------------|
| Contains | Only your classes | Your classes **plus** all dependencies |
| Run with | Classpath listing other JARs | `java -jar app.jar` |
| Built by | `jar`, Maven, Gradle | Maven Shade or Assembly plugin, Gradle Shadow, Spring Boot plugin |
| Pros | Small; dependencies updated independently | One file to deploy |
| Cons | Needs a correct classpath at runtime | Large; class/resource conflicts when merging; harder to patch one dependency |

Spring Boot's fat JAR uses a nested-JAR layout and its own launcher, not a flattened merge. Containers often prefer **layered** images so that dependencies rarely change ([22-production-engineering/02_packaging-and-deployment.md](../22-production-engineering/02_packaging-and-deployment.md)).

## Resources on the classpath

Files such as `config.properties`, templates, SQL scripts and images placed in `src/main/resources` are copied to the classpath root and read through the class loader, **not** through file paths:

```java
try (InputStream in = Main.class.getResourceAsStream("/config/default.properties")) {
    if (in == null) throw new IllegalStateException("resource missing");
    Properties props = new Properties();
    props.load(in);
}

URL url = Thread.currentThread().getContextClassLoader().getResource("config/default.properties");   // no leading slash with ClassLoader
```

| Call | Path rule |
|------|-----------|
| `Class.getResourceAsStream("/a/b.txt")` | Leading `/` = from the classpath root; without it, relative to the class's package |
| `ClassLoader.getResourceAsStream("a/b.txt")` | Always from the root, **no** leading `/` |

A resource inside a JAR is not a file: `new File(url.getPath())` fails. Use streams ([11-io-and-networking](../11-io-and-networking/README.md)).

## Class loaders (short version)

The JVM loads classes **lazily** through a hierarchy of class loaders (bootstrap → platform → application). The application class loader searches the classpath. Details: [15-jvm-internals/01_class-loading.md](../15-jvm-internals/01_class-loading.md).

## Typical classpath errors

| Error | Meaning | Usual cause |
|-------|---------|-------------|
| `Error: Could not find or load main class X` | The JVM cannot find the **starting class** | Wrong classpath root, missing package prefix, typo, class not compiled |
| `ClassNotFoundException` (checked exception) | A class requested **by name at runtime** (`Class.forName`, reflection, JDBC drivers, config-driven loading) is missing | Library JAR not on the classpath |
| `NoClassDefFoundError` (an `Error`) | A class was present at compile time but is missing (or failed to initialize) at runtime | Dependency not packaged; static initializer threw earlier ([05-initialization-order](../04-oop/05_initialization-order.md)) |
| `NoSuchMethodError` / `NoSuchFieldError` / `AbstractMethodError` | Class found, but a **different version** than you compiled against | Version conflict, two JARs of one library |
| `IncompatibleClassChangeError` | Class layout changed between compile and run | Same as above |
| `UnsupportedClassVersionError` | Class built for a newer Java than the running JVM | Run on a newer JDK ([01_java-version-management](../00-setup/01_java-version-management.md)) |
| `package x does not exist` / `cannot find symbol` (compile time) | Compiler cannot see the library | Add `-cp`, or the build dependency |
| `LinkageError` | Same class loaded twice by different loaders | Duplicate libraries in app servers or plugins |

### Diagnosing

```bash
java -verbose:class -cp out com.example.Main            # prints each class and where it was loaded from
java -XshowSettings:properties -version | grep class.path
jar tf lib/some.jar | grep MyClass                      # is the class in this JAR?
mvn dependency:tree                                     # which versions did Maven choose, and why
mvn dependency:build-classpath                          # the exact classpath Maven uses
```

In code: `MyClass.class.getProtectionDomain().getCodeSource().getLocation()` shows which JAR or folder a class came from.

## Classpath vs module path

| | Classpath | Module path ([02_java-modules.md](./02_java-modules.md)) |
|---|-----------|----------------|
| Visibility | Everything `public` is visible to everyone | Only **exported** packages |
| Dependencies | Implicit, order-based | Explicit `requires` checked at startup |
| Duplicates | Silently first-wins | Detected as errors (split packages) |
| Typical use | Almost all current applications and builds | Libraries and apps that opt into JPMS |

Libraries on the classpath live in the **unnamed module** and can read everything.

## With Maven and Gradle

You rarely set the classpath yourself:

```xml
<dependency>
  <groupId>com.google.code.gson</groupId>
  <artifactId>gson</artifactId>
  <version>2.11.0</version>
</dependency>
```

The build tool downloads the JAR (and its transitive dependencies) from a repository, adds them to the compile and test classpaths, and, for packaging, copies them or builds a fat JAR ([18-build-and-dependencies](../18-build-and-dependencies/README.md)). Understanding the classpath is what lets you debug the build tool when it goes wrong.

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| `-cp` points at the package folder instead of its root | `Could not find or load main class` | Use the folder containing the first package directory |
| Using `;` on Linux or `:` on Windows | Entries ignored | Use the correct separator |
| Unquoted `lib/*` | The shell expands it | Quote it: `"lib/*"` |
| Using `-cp` together with `-jar` | Classpath ignored | Put libraries in the manifest `Class-Path`, or use `-cp` with the main class |
| Reading resources with `new File("src/main/resources/x")` | Works in the IDE, fails in the JAR | `getResourceAsStream` |
| Leading `/` mistakes in resource paths | `null` stream | Class: `/path`, ClassLoader: `path` |
| Two versions of one library | `NoSuchMethodError`, odd behavior | Resolve with `mvn dependency:tree` and exclusions/BOMs |
| Setting the global `CLASSPATH` variable | Mysterious differences between machines | Unset it; use `-cp` |
| Fat JAR merging services files badly (`META-INF/services`) | Providers not found | Use the shade plugin's service transformers |
| Missing `-C out .` when building a JAR | Classes nested under `out/` in the JAR | Use `-C` correctly |
| Forgetting `Main-Class` | `no main manifest attribute, in app.jar` | Add `--main-class` / manifest entry |
| Catching `NoClassDefFoundError` to "handle" it | Hides a broken deployment | Fix the packaging |

## Key takeaways

- The classpath is an ordered list of folders and JARs; the first match wins, so duplicates are dangerous
- Entries must point to the root of the package tree; use `:` on Unix and `;` on Windows
- A JAR is a ZIP with a manifest; `java -jar` ignores `-cp` and uses the manifest's `Main-Class` and `Class-Path`
- Load resources with the class loader, not file paths
- Know the error vocabulary: `ClassNotFoundException` (by name), `NoClassDefFoundError` (missing at runtime), `NoSuchMethodError` (version mismatch)
- Build tools build the classpath for you; learn to inspect it

**Next:** [Java Modules](./02_java-modules.md)
