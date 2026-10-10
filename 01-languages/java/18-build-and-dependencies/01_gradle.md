# Gradle

**Gradle** is a build tool where the build is a **graph of tasks** described in code (Kotlin or Groovy DSL). Where Maven gives you a fixed lifecycle you configure, Gradle gives you a flexible model you program, with strong support for **incremental builds**, **caching**, and large multi-project setups. It's the standard build tool for Android and common in large Java, Kotlin, and polyglot repositories.

If you know Maven, the concepts map cleanly (coordinates, repositories, scopes → configurations, plugins). What's new is the task graph and the performance features.

**Prerequisites:** [Maven](00_maven.md) (concepts), [First Project with Maven and Gradle](../00-setup/05_first-project-with-maven-and-gradle.md).

---

## 1. Project structure

```text
 my-app/
 ├── settings.gradle.kts        ← project name and which subprojects exist
 ├── build.gradle.kts           ← the build for this project
 ├── gradle.properties          ← build settings (JVM args, caching flags)
 ├── gradle/
 │   ├── libs.versions.toml     ← version catalog (section 5)
 │   └── wrapper/               ← Gradle Wrapper files
 ├── gradlew, gradlew.bat       ← wrapper scripts
 └── src/main/java ...          ← same source layout as Maven
```

A minimal Java application build (Kotlin DSL):

```kotlin
// settings.gradle.kts
rootProject.name = "orders"
```

```kotlin
// build.gradle.kts
plugins {
    java
    application
}

group = "com.acme"
version = "1.0.0"

repositories {
    mavenCentral()
}

java {
    toolchain {
        languageVersion = JavaLanguageVersion.of(21)      // the Java version to compile and test with
    }
}

dependencies {
    implementation("com.fasterxml.jackson.core:jackson-databind:2.x.y")

    testImplementation(platform("org.junit:junit-bom:5.x.y"))
    testImplementation("org.junit.jupiter:junit-jupiter")
    testRuntimeOnly("org.junit.platform:junit-platform-launcher")
}

tasks.test {
    useJUnitPlatform()
}

application {
    mainClass = "com.acme.Main"
}
```

**Kotlin DSL** (`*.gradle.kts`) is the default for new builds, with full IDE completion and type checking. **Groovy DSL** (`*.gradle`) is still fully supported and common in older projects. The concepts are identical.

---

## 2. The model: tasks and the build graph

Everything Gradle does is a **task** (`compileJava`, `test`, `jar`, `build`). Tasks declare **inputs and outputs** and **dependencies** on other tasks, which form a directed acyclic graph:

```text
 build
  ├─ assemble ── jar ── classes ── compileJava ── (processResources)
  └─ check ───── test ── testClasses ── compileTestJava
```

```bash
./gradlew build           # compile, test, package (the daily command; includes `check`)
./gradlew test            # run tests
./gradlew run             # run the application (application plugin)
./gradlew clean           # delete build/
./gradlew tasks           # list available tasks
./gradlew build -x test   # exclude a task (use sparingly)
./gradlew test --tests "com.acme.OrderServiceTest"     # one class (or --tests "*cancels*")
./gradlew --continuous test       # re-run on file changes
./gradlew build --offline         # use only cached dependencies
```

### Three phases

1. **Initialization:** read `settings.gradle.kts`, decide which projects exist.
2. **Configuration:** evaluate every build script and build the task graph. (Code in the *body* of a script runs here, on *every* build, so keep it cheap.)
3. **Execution:** run the selected tasks and their dependencies.

### Why it's fast: up-to-date checks and caching

- **Incremental build / up-to-date:** if a task's inputs and outputs haven't changed since the last run, Gradle **skips it** (`UP-TO-DATE`). That's why it needs well-declared inputs and outputs.
- **Build cache:** task outputs are cached by input hash (locally, and optionally shared by the team/CI). A task whose inputs match a cached result is `FROM-CACHE`.
- **Daemon:** a long-lived JVM keeps the build machinery warm between builds, so you don't pay JVM startup and classloading every time.
- **Configuration cache:** caches the *result of the configuration phase* so repeat builds skip it entirely.

---

## 3. Dependencies and configurations

Gradle's counterpart to Maven's scopes is the **configuration**:

| Configuration | Meaning | Maven analogue |
|---|---|---|
| `implementation` | Needed to compile and run this module. **Not exposed** on consumers' compile classpath | `compile` (but better encapsulated) |
| `api` (needs the `java-library` plugin) | Part of this library's **public API**, exposed to consumers | `compile` |
| `compileOnly` | Compile time only | `provided` |
| `runtimeOnly` | Runtime only (JDBC drivers, logging backends) | `runtime` |
| `testImplementation` / `testRuntimeOnly` | Test classpath | `test` |
| `annotationProcessor` | Annotation processors (Lombok, MapStruct, Dagger), a separate path | (compiler plugin config) |

```kotlin
dependencies {
    api("com.acme:shared-model:1.4.0")                    // leaks into consumers' compile classpath
    implementation("org.apache.commons:commons-lang3:3.x.y")    // internal detail
    runtimeOnly("org.postgresql:postgresql:42.x.y")
    compileOnly("org.projectlombok:lombok:1.18.x")
    annotationProcessor("org.projectlombok:lombok:1.18.x")
}
```

**`implementation` vs `api`** is Gradle's strongest encapsulation feature: with `implementation`, a change to an internal dependency doesn't force consumers to recompile, and consumers can't accidentally depend on it.

Inspect the resolved graph:

```bash
./gradlew dependencies                                  # full tree per configuration
./gradlew dependencies --configuration runtimeClasspath
./gradlew dependencyInsight --dependency jackson-databind    # why is this here, and which version won?
```

---

## 4. Toolchains: choose the Java version in the build

A **toolchain** declares which JDK compiles and tests your code, independent of the JDK running Gradle:

```kotlin
java {
    toolchain {
        languageVersion = JavaLanguageVersion.of(21)
        // vendor = JvmVendorSpec.ADOPTIUM      // optionally restrict the vendor
    }
}
```

Gradle locates a matching installed JDK, and with a *toolchain resolver plugin* (such as the Foojay resolver) can **download** one automatically. Gradle 9 itself requires **Java 17+ to run**, but toolchains let you still build and test for older Java versions (Java 8+). This pins the language level for every developer and CI job: the same goal as `maven.compiler.release` ([Release Timeline](../12-modern-java/00_java-release-timeline.md#4-which-version-should-i-target)).

---

## 5. Version catalogs

Centralize versions in `gradle/libs.versions.toml`:

```toml
[versions]
jackson = "2.x.y"
junit = "5.x.y"

[libraries]
jackson-databind = { module = "com.fasterxml.jackson.core:jackson-databind", version.ref = "jackson" }
junit-bom        = { module = "org.junit:junit-bom", version.ref = "junit" }
junit-jupiter    = { module = "org.junit.jupiter:junit-jupiter" }

[bundles]
testing = ["junit-jupiter"]

[plugins]
spring-boot = { id = "org.springframework.boot", version = "x.y.z" }
```

```kotlin
dependencies {
    implementation(libs.jackson.databind)                 // type-safe accessor with IDE completion
    testImplementation(platform(libs.junit.bom))
    testImplementation(libs.junit.jupiter)
}
```

A catalog gives every module the same versions, one place to bump them, and (via bots such as Renovate and Dependabot) one file for automated updates. For BOM-style alignment, use `platform(...)` (Maven's `import`-scope BOM equivalent).

---

## 6. Multi-project builds

```kotlin
// settings.gradle.kts
rootProject.name = "shop"
include(":domain", ":persistence", ":web")
```

```kotlin
// web/build.gradle.kts
dependencies {
    implementation(project(":domain"))
    implementation(project(":persistence"))
}
```

Run one project: `./gradlew :web:test`. Gradle builds only what the requested task depends on, and parallelizes independent projects.

### Share build logic with convention plugins

Don't copy-paste configuration into every subproject, and avoid the old `allprojects { }` / `subprojects { }` pattern in the root script (it couples projects and hurts caching). Put shared configuration in a **convention plugin**, a small plugin in a `build-logic` included build (or `buildSrc`), then apply it:

```kotlin
// build-logic/src/main/kotlin/java-conventions.gradle.kts
plugins { java }
java { toolchain { languageVersion = JavaLanguageVersion.of(21) } }
tasks.test { useJUnitPlatform() }
```

```kotlin
// each project's build.gradle.kts
plugins { id("java-conventions") }
```

**Composite builds** (`includeBuild("../other-lib")`) let you develop a library and its consumer together without publishing in between.

---

## 7. Performance settings

In `gradle.properties`:

```properties
org.gradle.caching=true                # reuse task outputs (build cache)
org.gradle.parallel=true               # build independent projects in parallel
org.gradle.configuration-cache=true    # cache the configuration phase
org.gradle.jvmargs=-Xmx2g -XX:+UseParallelGC     # memory for the Gradle daemon
```

- The **configuration cache** is Gradle 9's *preferred* execution mode (Gradle prompts you to adopt it) but isn't enabled by default for existing builds yet. Some plugins and custom build logic aren't compatible, and Gradle's report tells you why.
- Keep **configuration-phase work minimal** (no network calls, no heavy file scanning in the script body).
- Declare **task inputs and outputs** properly in custom tasks, or they'll never be up to date (or will be wrongly cached).
- Use `--scan` (Build Scan) or the profiler (`--profile`) to see where time goes.
- A **remote build cache** shared with CI lets one machine's work benefit everyone.

---

## 8. Wrapper, publishing, and version notes

### The Gradle Wrapper

```bash
gradle wrapper --gradle-version <version>       # generates gradlew, gradlew.bat, gradle/wrapper/*
./gradlew build                                  # always use this in docs and CI
```

Commit the wrapper files. The wrapper pins the Gradle version for everyone, exactly like `mvnw` does for Maven.

### Publishing

The `maven-publish` plugin publishes artifacts (and a POM) to Maven repositories, including Central, so Gradle-built libraries are consumable by Maven users.

### Version notes (October 2026)

- **Gradle 9.x** requires Java 17+ to run, uses Kotlin 2.x for Kotlin DSL and Groovy 4, adopts SemVer-style versioning, and made the configuration cache the preferred mode. Java 25 is supported from 9.1 and Java 26 from 9.4.
- Plugin compatibility lags new Gradle majors: when upgrading, update plugins (Spring Boot, Kotlin, Android, Sonar, etc.) and run `./gradlew help --warning-mode all` to see deprecations first.
- The new test framework wiring in Gradle 9 expects the JUnit **launcher** to be declared explicitly (`testRuntimeOnly("org.junit.platform:junit-platform-launcher")`), as shown above.

---

## 9. Maven vs Gradle in practice

| | Maven | Gradle |
|---|---|---|
| Build description | XML, declarative | Kotlin/Groovy code, programmable |
| Extending the build | Write/configure a plugin | Add a task or convention plugin in the same repo |
| Incremental builds and caching | Limited (plugin-level, extensions) | **Built in** (up-to-date checks, build cache, config cache) |
| Dependency model | Scopes | Configurations, `api`/`implementation` separation |
| Large multi-module speed | Good | Usually faster, with remote caching |
| Predictability across projects | **Very high** (everything looks alike) | Varies with how much custom logic exists |
| IDE import/diagnostics | Mature | Mature (Kotlin DSL sync can be slow on huge builds) |
| Android | Not used | The standard |

Neither is "better". Don't migrate for fashion. Migrate when build speed, flexibility, or ecosystem needs justify the cost.

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Using a locally installed `gradle` | `./gradlew` (the wrapper) |
| Everything declared `api` | Default to `implementation`. Use `api` only for true public API |
| Heavy logic in the configuration phase | Move it into task actions. Keep scripts light |
| `allprojects {}`/`subprojects {}` for shared config | Convention plugins |
| Versions scattered across scripts | A version catalog + `platform(...)` BOMs |
| Using `+` or `latest.release` versions | Fixed versions (reproducibility) |
| Custom tasks without declared inputs/outputs | Declare them (`@Input`, `@OutputFile`) so up-to-date and caching work |
| Relying on the JDK that happens to run Gradle | Declare a **toolchain** |
| Ignoring configuration-cache incompatibility reports | Fix the plugin/script usage, or opt out knowingly |
| Committing `build/` or `.gradle/` | `.gitignore` them |
| Disabling the daemon/caches "to fix" a problem | Find the real cause (`--info`, `--scan`) |

### Debugging

- **Why is this dependency/version here?** `./gradlew dependencyInsight --dependency <name> --configuration runtimeClasspath`.
- **Build is slow:** `./gradlew build --profile` or `--scan`, check which tasks run (not `UP-TO-DATE`/`FROM-CACHE`), and whether the configuration phase dominates.
- **A task re-runs when it shouldn't (or doesn't when it should):** inspect with `--info` (it explains why a task wasn't up to date), or `--rerun-tasks` to force. Check declared inputs and outputs.
- **Odd behavior after changing Gradle/Java versions:** `./gradlew --stop` (stop daemons), then rebuild. Clear `.gradle/` and `build/` if needed.
- **`Could not resolve ...`:** repositories declared? Credentials? Offline mode? Check `repositories {}` and any mirrors/proxy settings.
- **Failure details:** `--stacktrace`, `--info`, or `--debug` for increasing verbosity.
- **Unsupported Java for the daemon/Kotlin/plugins:** consult the Gradle compatibility matrix, and upgrade the wrapper and plugins together.

---

## Quick Summary

- Gradle builds a **graph of tasks** from Kotlin (default) or Groovy DSL scripts. Phases: initialization → configuration → execution.
- Speed comes from **up-to-date checks, the build cache, the daemon, and the configuration cache** (preferred in Gradle 9).
- **Configurations** replace scopes: prefer `implementation`, use `api` only for public API, `runtimeOnly`, `compileOnly`, `testImplementation`, `annotationProcessor`.
- **Toolchains** pin the Java version. **Version catalogs** (`libs.versions.toml`) and `platform()` BOMs centralize versions.
- Share logic with **convention plugins**, and avoid `allprojects {}`. Use composite builds for co-developing libraries.
- **Commit the wrapper**. Gradle 9 needs Java 17+ to run.
- Debug with `dependencyInsight`, `--info`, `--scan`, and `--profile`.

**Next:** [Dependency Management](02_dependency-management.md)