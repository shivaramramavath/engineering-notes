# 18 · Build and Dependencies

Real Java projects are never one file compiled by hand. They have libraries (which have their own libraries), tests, multiple modules, packaging steps, and a path to production. A **build tool** automates all of it: it resolves dependencies, compiles, runs tests, packages artifacts, and does the same thing on your laptop and on the CI server.

Most "mysterious" Java problems at this level (`NoSuchMethodError`, "works on my machine", a vulnerable library nobody knew was there, a 20-minute build) are build and dependency problems. This module covers the two dominant tools, how to manage dependencies without getting hurt, and how builds become pipelines.

## Contents

| # | Note | Focus |
|---|------|-------|
| 00 | [Maven](00_maven.md) | POM, coordinates, lifecycle, dependencies and scopes, plugins, multi-module builds, `settings.xml`, wrapper |
| 01 | [Gradle](01_gradle.md) | Tasks and the build graph, Kotlin DSL, configurations, version catalogs, toolchains, performance features |
| 02 | [Dependency Management](02_dependency-management.md) | Transitive dependencies, conflicts, BOMs, versions, updates, vulnerabilities, supply-chain security |
| 03 | [CI/CD Pipelines](03_ci-cd-pipelines.md) | Pipeline stages, GitHub Actions, caching, packaging and Docker, releases, secrets, deployment strategies |

## Maven or Gradle?

| | Maven | Gradle |
|---|---|---|
| Model | Declarative XML (`pom.xml`), fixed lifecycle | Programmable build scripts (Kotlin or Groovy DSL), a graph of tasks |
| Strength | **Convention and predictability**, enormous ecosystem, easy to read any project | **Flexibility and speed** (incremental builds, caching, daemon), strong for multi-project and non-Java builds |
| Learning curve | Gentle, but XML is verbose | Steeper (more power, more ways to do it) |
| Typical home | Enterprise Java, Spring projects, libraries | Android (the standard), large multi-module builds, polyglot repos |

Both are first-class: the choice is usually dictated by the existing codebase or team. **Learn Maven's concepts first** (they transfer), then Gradle's.

## Version snapshot (October 2026)

- **Maven 3.9.x** is the stable line. **Maven 4.0.0** has been in release-candidate stages for a long time (the latest candidates describe themselves as the last before GA), so treat it as pre-release until it ships.
- **Gradle 9.x** is current: it requires **Java 17+ to run** (toolchains can still build for older Java versions), bundles Kotlin 2.x and Groovy 4, makes the configuration cache the *preferred* (not yet default) mode, and uses Kotlin DSL by default for new builds.
- Both tools track new JDKs with a lag: check each release's compatibility notes when adopting a new Java version.

Verify against the current documentation, as these details move.

## Two rules that prevent most pain

1. **Use the wrapper** (`mvnw` / `gradlew`) and commit it, so everyone and every CI job uses the same build-tool version.
2. **Let the build declare the Java version** (`maven.compiler.release` / Gradle toolchains) instead of "whatever JDK happens to be installed" ([Release Timeline](../12-modern-java/00_java-release-timeline.md#4-which-version-should-i-target)).

## Prerequisites

[Classpath and JARs](../05-packages-and-modules/01_classpath-and-jars.md), [Packages and Imports](../05-packages-and-modules/00_packages-and-imports.md), and [How Java Works](../00-setup/02_how-java-works.md). The earlier setup note [First Project with Maven and Gradle](../00-setup/05_first-project-with-maven-and-gradle.md) is the gentle introduction; this module goes deeper.

## Related

[Class Loading](../15-jvm-internals/01_class-loading.md) (why dependency conflicts break at runtime) · [Dependency Security](../21-security/05_dependency-security.md) · [Packaging and Deployment](../22-production-engineering/02_packaging-and-deployment.md) · [Maven and Gradle command cheatsheet](../29-cheatsheets/08_maven-and-gradle-commands.md)

**Next module:** [Testing](../19-testing/README.md)