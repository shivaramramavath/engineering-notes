# Java Release Timeline

Java ships a new version **every six months** (March and September). Knowing how that works, which versions are LTS, and when each feature became *final* tells you which syntax you can use, what a framework's "requires Java 17" really means, and which version to put in your build file.

> **Status snapshot (October 2026):** Java 27 reached general availability on 15 September 2026. The latest LTS is **Java 25** (September 2025). The next LTS is **Java 29** (September 2027). Verify against [openjdk.org](https://openjdk.org/projects/jdk/) before relying on this: it will age.

**Prerequisites:** [How Java Works](../00-setup/02_how-java-works.md), [Java Version Management](../00-setup/01_java-version-management.md).

---

## 1. The release model

- **Feature releases every 6 months** since Java 10 (March 2018): 10, 11, 12, … Each is a complete JDK.
- **LTS (Long-Term Support)** releases get years of updates from vendors. Everything else ("non-LTS" or short-term) is supported only until the next release, about six months.
- **LTS cadence:** 8 (2014) → 11 (2018) → 17 (2021) → **21 (2023)** → **25 (2025)** → 29 (2027). Starting with Java 21, an LTS arrives every **two years** (earlier gaps were three).
- Updates and support are provided by several OpenJDK vendors (Oracle, Eclipse Temurin, Amazon Corretto, Azul Zulu, Microsoft, and others). Terms and dates differ, so check your vendor.

```text
 8 ─────── 11 ─────── 17 ─────── 21 ─────── 25 ─────── 29
 LTS       LTS        LTS        LTS        LTS        LTS (Sep 2027)
        (3 years)  (3 years)  (2 years)  (2 years)  (2 years)
           with 12,13,14,15,16, 18,19,20, 22,23,24, 26,27,28 ... in between
```

Most production systems run an LTS and upgrade between LTS versions. Non-LTS releases are useful for trying features early.

---

## 2. Incubator, preview, final

New features don't appear all at once. They go through stages so the community can give feedback:

| Stage | Meaning | Rules |
|---|---|---|
| **Incubator** (APIs) | An API in a separate module for early feedback | Needs `--add-modules`; may change or vanish |
| **Preview** | Complete language/API feature, **not yet permanent** | Needs `--enable-preview`; can change or be removed |
| **Final** | Part of the standard platform | Works normally; compatible going forward |

```bash
javac --release 25 --enable-preview Main.java
java --enable-preview Main
```

Practical rules:

- Code compiled with preview features **only runs on the exact Java version that compiled it**, with `--enable-preview`. Don't ship libraries that depend on preview features.
- Features often go through two to five preview rounds. Some change substantially between rounds, and some are withdrawn (string templates were previewed and then removed).
- "Final" in version N means usable without flags from N on.

---

## 3. Version-by-version highlights

Only the changes that matter to day-to-day development. **LTS versions are in bold.**

| Version | Released | Highlights |
|---|---|---|
| **8** | Mar 2014 | Lambdas, streams, `Optional`, `java.time`, default methods |
| 9 | Sep 2017 | Modules (JPMS), JShell, `List.of`/`Map.of`, `Stream.takeWhile`, private interface methods |
| 10 | Mar 2018 | **`var`** (local variable type inference) |
| **11** | Sep 2018 | Standard `HttpClient`, `String.isBlank/strip/lines/repeat`, `Files.readString`, run a single file with `java Hello.java`, `var` in lambda params |
| 12 | Mar 2019 | `Collectors.teeing`; switch expressions (preview) |
| 13 | Sep 2019 | Text blocks (preview) |
| 14 | Mar 2020 | **Switch expressions** (final), helpful `NullPointerException` messages, records & pattern `instanceof` (preview) |
| 15 | Sep 2020 | **Text blocks** (final), sealed classes (preview) |
| 16 | Mar 2021 | **Records**, **pattern matching for `instanceof`** (final), `Stream.toList()` |
| **17** | Sep 2021 | **Sealed classes** (final), strong encapsulation of JDK internals, Security Manager deprecated for removal |
| 18 | Mar 2022 | **UTF-8 by default**, `jwebserver` |
| 19-20 | 2022-23 | Virtual threads, record patterns, and pattern `switch` in preview |
| **21** | Sep 2023 | **Virtual threads**, **pattern matching for `switch`**, **record patterns**, sequenced collections (all final) |
| 22 | Mar 2024 | **Unnamed variables and patterns (`_`)**, Foreign Function & Memory API final, launch multi-file source programs |
| 23 | Sep 2024 | Markdown doc comments, generational ZGC by default |
| 24 | Mar 2025 | **Stream Gatherers**, Class-File API, Security Manager permanently disabled, AOT class loading/linking |
| **25** | Sep 2025 | Scoped values, module import declarations, compact source files and instance `main`, flexible constructor bodies (all final); compact object headers (opt-in), Generational Shenandoah |
| 26 | Mar 2026 | HTTP/3 in `HttpClient`, AOT cache improvements, warnings when `final` fields are mutated through reflection (JEP 500, "prepare to make final mean final"), applet API removed |
| 27 | Sep 2026 | G1 default GC in all environments, compact object headers on by default, post-quantum hybrid key exchange for TLS 1.3 |

Features still in **preview or incubation as of Java 27**: primitive types in patterns (fifth preview), structured concurrency (seventh preview), lazy constants (third preview), PEM encodings (third preview), and the Vector API (twelfth incubator). Treat them as "try, don't depend".

### When each language feature became final

```text
var .................. 10        switch expressions .... 14      text blocks ........ 15
records .............. 16        instanceof patterns ... 16      sealed classes ..... 17
switch patterns ...... 21        record patterns ....... 21      unnamed vars (_) ... 22
```

These are the notes in this module: [Records](01_records.md), [Sealed Classes](02_sealed-classes.md), [Switch Expressions](03_switch-expressions.md), [Pattern Matching](04_pattern-matching.md), [`var`](05_var-and-type-inference.md). Text blocks are in [Text Blocks](../03-strings-and-text/05_text-blocks.md).

---

## 4. Which version should I target?

| Situation | Guidance |
|---|---|
| New application | The **latest LTS** (25 today), unless a framework or platform dictates otherwise |
| Existing app on 8 or 11 | Plan the move to 17, 21, or 25. Older LTS versions get fewer fixes and the ecosystem is dropping them |
| Library meant for wide use | Choose the lowest Java version your users really have, and compile with `--release N` |
| Experimenting | Latest release (27), with preview flags if you like |

Considerations:

- **Frameworks and libraries set a minimum Java version.** Check your framework's documentation: a dependency that requires 17+ forces your whole build to 17+.
- Use **`--release N`** (Maven: `maven.compiler.release`) rather than `-source/-target`. It also restricts the APIs you can call to those that exist in version N, which prevents "compiled fine on JDK 25, crashes on Java 17 at runtime".
- Language level and runtime are separate. You can compile with `--release 17` using JDK 25, and run on any JDK ≥ 17.

```xml
<!-- Maven -->
<properties>
    <maven.compiler.release>21</maven.compiler.release>
</properties>
```

---

## 5. Upgrading between LTS versions: what usually breaks

| Upgrade | Typical friction |
|---|---|
| 8 → 11 | Modules and removed Java EE/CORBA APIs (JAXB, `javax.annotation` must now be added as dependencies); illegal reflective access warnings; old libraries |
| 11 → 17 | **Strong encapsulation**: reflection into `java.*` internals now fails without `--add-opens`; older bytecode-manipulating libraries (ASM, Mockito, Lombok, etc.) need newer versions |
| 17 → 21 | Mostly smooth; update tooling; consider virtual threads |
| 21 → 25 | Mostly smooth; the Security Manager is gone; 32-bit x86 support was removed; check agents and bytecode libraries |

Upgrade checklist: bump build tool and plugins first, update dependencies, run the test suite on the new JDK, scan logs for warnings (deprecations, illegal access), then switch the runtime. Container base images and CI agents often move first, so test early.

---

## 6. Beyond the language: platform changes worth knowing

- **Garbage collection:** G1 is the default collector (and, as of 27, in every environment, including small containers that used to fall back to Serial). ZGC is generational by default since 23. See [Garbage Collection](../15-jvm-internals/04_garbage-collection.md).
- **Concurrency:** virtual threads (21) change how you write blocking server code ([Virtual Threads](../14-concurrency/13_virtual-threads-and-structured-concurrency.md)).
- **Startup and footprint:** AOT cache work (Project Leyden) and compact object headers reduce start time and heap size without code changes.
- **Defaults get safer over time:** UTF-8 default charset (18), strong encapsulation (17), Security Manager removal (24).

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Using preview features in production or in shared libraries | Wait for the final release |
| `-source`/`-target` instead of `--release` | `--release N` |
| Assuming "latest" means "LTS" | Check: 26 and 27 are not LTS |
| Upgrading the runtime but not the build plugins/agents | Update Maven/Gradle plugins and bytecode tools first |
| Copy-pasting syntax from a blog without checking the version | Check the table: `case Circle(double r)` needs 21+ |
| Believing a feature "exists" because it compiles in an IDE with a newer language level | Match the IDE language level to the build's `release` |

### Debugging

- `Unsupported class file major version N` → a class was compiled for a newer Java than the JVM or tool reading it supports (or an old tool such as ASM is reading a newer class).
- `error: records are not supported in -source 11` (or similar) → your `release`/language level is lower than the feature's version.
- `java.lang.UnsupportedClassVersionError: ... compiled by a more recent version` → run on a newer JVM, or compile with a lower `--release`.
- `InaccessibleObjectException` / "module does not open" → strong encapsulation; upgrade the library or, as a last resort, add `--add-opens`.
- `Preview features are not enabled` → compile **and** run with `--enable-preview` (and the same JDK version).

---

## Quick Summary

- New Java every **6 months**; **LTS every 2 years**: 17, 21, **25**, next is 29 (Sep 2027). 26 and 27 are short-term releases.
- Features go **incubator/preview → final**. Preview code needs `--enable-preview` and ties you to one JDK version.
- Language features in this module became final at: `var` 10, switch expressions 14, records 16, `instanceof` patterns 16, sealed classes 17, switch/record patterns 21, `_` 22.
- Target the latest LTS for new work and compile with `--release N`.
- Most upgrade pain comes from strong encapsulation and old bytecode tooling, not from the language.

**Next:** [Records](01_records.md)
