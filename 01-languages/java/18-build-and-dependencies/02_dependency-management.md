# Dependency Management

A typical Java service declares 20 libraries and ends up with 200 on its classpath. Everything beyond the 20 you chose arrived **transitively**, brought in by something else, at versions you didn't pick. Dependency management is the discipline of knowing what's on your classpath, keeping versions consistent, updating deliberately, and not being the victim of a vulnerable or malicious library you never knowingly installed.

It's one of the highest-leverage skills in day-to-day Java: it explains `NoSuchMethodError` at 3 a.m., security audit findings, and "it worked until we bumped one library".

**Prerequisites:** [Maven](00_maven.md), [Gradle](01_gradle.md), [Class Loading](../15-jvm-internals/01_class-loading.md).

---

## 1. Transitive dependencies and the graph

```text
 your-app
  ├─ spring-web 6.x
  │    └─ jackson-databind 2.15   (transitive)
  │         └─ jackson-core 2.15
  └─ some-sdk 3.2
       └─ jackson-databind 2.12   (transitive: a DIFFERENT version)
```

Both Maven and Gradle compute a **resolved graph**: one version per library ends up on the classpath, and the others are dropped. Always look at it:

```bash
# Maven
mvn dependency:tree -Dverbose               # shows omitted/conflicting versions
mvn dependency:analyze                      # unused declared / used undeclared

# Gradle
./gradlew dependencies --configuration runtimeClasspath
./gradlew dependencyInsight --dependency jackson-databind
```

**Direct** dependencies are what you declare. **Transitive** ones come with them. You're responsible for the whole graph, because everything on the classpath runs in your process with your permissions.

Practical rule: **declare what you use directly** (not rely on it arriving transitively). If your code imports `com.fasterxml.jackson...`, depend on Jackson explicitly. Otherwise a change in some other library's dependencies can silently remove it ([Maven `dependency:analyze`](00_maven.md#3-dependencies) flags this).

---

## 2. Conflicts: how they are resolved

When two paths request different versions of the same library, the tool must pick one. The default rules differ:

| Tool | Default rule |
|---|---|
| **Maven** | **Nearest definition wins** (fewest hops from your project). On a tie, the **first declared** wins. It does *not* pick the highest version |
| **Gradle** | **Highest version wins** among all requested versions, unless a constraint, `strictly`, or a BOM says otherwise |

Consequences:

- With Maven, **declaration order and tree shape matter**, and the "winning" version can be *older* than one that another library needs. That library may then break at runtime.
- With Gradle, a library compiled against 2.12 may silently get 2.17. Usually compatible, but not always.

### The symptom: "JAR hell"

```text
java.lang.NoSuchMethodError: 'com.fasterxml.jackson.databind.JsonNode ...'
java.lang.NoClassDefFoundError / ClassNotFoundException / AbstractMethodError / IncompatibleClassChangeError
```

A library was compiled against version A of a dependency, but version B (older or newer) won at runtime, and a method/class it needs isn't there. The code compiled fine. It fails only when that code path runs ([Class Loading](../15-jvm-internals/01_class-loading.md#5-the-errors-decoded)).

### Fixing conflicts

1. **Find it:** `dependency:tree -Dverbose` / `dependencyInsight`, and note which versions are requested by whom.
2. **Align the versions** with a **BOM** for that family ([section 3](#3-boms-and-version-policy)). This fixes the large majority of cases (Jackson, Spring, Netty, gRPC, cloud SDKs).
3. Otherwise **force a version**:

```xml
<!-- Maven: pin it in dependencyManagement (applies to transitive uses too) -->
<dependencyManagement><dependencies>
  <dependency>
    <groupId>com.fasterxml.jackson.core</groupId><artifactId>jackson-databind</artifactId>
    <version>2.x.y</version>
  </dependency>
</dependencies></dependencyManagement>
```

```kotlin
// Gradle: a constraint or strict version
dependencies {
    constraints { implementation("com.fasterxml.jackson.core:jackson-databind:2.x.y") }
}
```

4. **Exclude** a transitive dependency you don't want (or an unwanted second implementation), and add the right one yourself.
5. **Enforce it** so it can't regress: Maven's `maven-enforcer-plugin` with `dependencyConvergence`/`requireUpperBoundDeps`, and Gradle's dependency locking and `failOnVersionConflict()`.

### Duplicate implementations of the same thing

The other classic mess: two logging backends, two JSON libraries, or two versions of a library under different coordinates (`javax.servlet` vs `jakarta.servlet`). Symptoms include `SLF4J: Class path contains multiple SLF4J providers` and behavior that depends on classpath order. Choose one implementation per concern. Use facades (SLF4J) in libraries and bind exactly one backend in the application.

---

## 3. BOMs and version policy

### BOMs (Bill of Materials)

A **BOM** is a POM that lists tested-together versions for a family of artifacts. Import it once, and declare dependencies without versions:

```xml
<!-- Maven -->
<dependencyManagement><dependencies>
  <dependency>
    <groupId>org.springframework.boot</groupId><artifactId>spring-boot-dependencies</artifactId>
    <version>x.y.z</version><type>pom</type><scope>import</scope>
  </dependency>
</dependencies></dependencyManagement>
```

```kotlin
// Gradle
dependencies {
    implementation(platform("org.springframework.boot:spring-boot-dependencies:x.y.z"))
    implementation("org.springframework:spring-web")             // version from the platform
}
```

Common BOMs: Spring Boot, Jackson, JUnit, Netty, gRPC, Micrometer, AWS/Google Cloud SDKs, Testcontainers. A BOM turns "pick 12 compatible versions" into "pick one".

### Version rules of thumb

- **Use exact release versions.** Avoid ranges (`[1.0,2.0)`, `1.+`), `LATEST`, `RELEASE`, and `latest.release`. They make builds non-reproducible: the same commit builds differently next week.
- **No `-SNAPSHOT` dependencies in releases.**
- **Keep versions in one place** (BOM, `dependencyManagement`, version catalog, or properties).
- **Understand SemVer, and don't trust it blindly:** `MAJOR.MINOR.PATCH` means breaking changes bump MAJOR, additions bump MINOR, fixes bump PATCH. Many libraries follow it, some don't, and even minor/patch updates can change behavior. Read changelogs for anything that matters, and rely on tests.
- **Lock when you need exact reproducibility.** Gradle supports **dependency locking** (a lockfile recording the resolved versions). Maven achieves the same by pinning everything via management sections plus enforcement.

---

## 4. Keeping the graph healthy

- **Fewer dependencies is better.** Each one adds attack surface, upgrade work, and conflict risk. Prefer the JDK, or a few small well-maintained libraries, over big frameworks you use 2% of. Be wary of tiny "left-pad" libraries.
- **Narrow scopes/configurations** (`test`, `runtimeOnly`, `compileOnly`) keep the production classpath small.
- **Remove unused dependencies** (`dependency:analyze`, Gradle's dependency-analysis plugin).
- **Prefer libraries with healthy maintenance:** recent releases, responsive issue tracker, multiple maintainers, good documentation, security policy.
- **Avoid shading/relocating** (copying a library's classes into yours) unless you're building a library that must isolate its dependencies. It hides what's inside the artifact from scanners.
- Mind **`javax.*` → `jakarta.*`**: libraries on opposite sides of this namespace change can't be mixed with the same container.
- **Libraries (published artifacts)** should depend on as little as possible, use `implementation`/`provided`/`optional` correctly, and never force logging backends on consumers.

---

## 5. Updating dependencies

Letting versions rot is how you end up with a "we can't upgrade, the jump is huge" crisis and unpatched vulnerabilities. Make updating a **routine**, not an event.

- **Automate:** **Dependabot** and **Renovate** open pull requests when new versions appear (grouped by family, scheduled, with changelog links). CI runs your tests against each. For ad-hoc checks: `mvn versions:display-dependency-updates`, `mvn versions:display-plugin-updates`, or a Gradle dependency-updates plugin.
- **Small, frequent updates** are far easier to debug than annual big-bang ones. If a patch bump breaks the build, you know exactly what did it.
- **Updating a BOM** (Spring Boot, Jackson) moves many libraries consistently. Prefer that to bumping individual artifacts.
- **Let new releases settle** (a week or two) before adopting them on critical systems, except for security fixes. A brand-new release is when compromised or broken versions most often appear (see the next section). Renovate and other tools have "minimum release age" settings for this.
- **Tests are your safety net.** Contract and integration tests catch behavioral drift that compilation can't ([JSON Patterns](../17-json-and-data-formats/03_json-patterns-and-pitfalls.md#9-testing), [Integration Testing](../19-testing/03_integration-testing.md)).
- **Plan for major upgrades** (Java version, Spring Boot major, Jackson 2 → 3, `javax` → `jakarta`): read the migration guide, upgrade in a branch, and fix deprecations ahead of time ([Release Timeline](../12-modern-java/00_java-release-timeline.md#5-upgrading-between-lts-versions-what-usually-breaks)).

---

## 6. Repositories and supply-chain security

Every dependency is code you run. The threats are real:

| Threat | What happens |
|---|---|
| **Known vulnerabilities** (CVEs) | A library you use (often transitively) has a published flaw, like Log4Shell. Attackers scan for it |
| **Dependency confusion** | You use an internal artifact `com.acme:billing`. An attacker publishes a public artifact with the same coordinates and a higher version, and your build fetches it |
| **Typosquatting** | A malicious package with a name similar to a popular one |
| **Compromised maintainer/package** | A legitimate library publishes a malicious release |
| **Build/CI tampering** | Someone alters artifacts between build and deploy |

Defenses:

### Repository hygiene

- Use a **repository manager** (Nexus, Artifactory) as the single gateway: it proxies Central, caches artifacts, hosts private ones, and can block unknown or malicious packages.
- **Route internal groupIds only to your internal repository** (exclusive repository content/routing rules), so a public lookalike can't win.
- **Don't add arbitrary extra repositories** (especially http or personal ones). Each is a trust decision.
- **Use HTTPS**, and keep repository credentials in CI secrets and `settings.xml`/Gradle credentials, never in the repo.

### Verify what you download

- Maven checks **checksums** (configurable policy). Central artifacts are published with PGP signatures.
- Gradle offers **dependency verification** (`gradle/verification-metadata.xml`) to pin checksums (and signatures) of every dependency, failing the build if anything changes unexpectedly.
- Pin versions (section 3) so a hijacked "latest" can't slip in.

### Scan continuously

- Run a **software composition analysis (SCA)** scan in CI and on a schedule: OWASP Dependency-Check, OSV-Scanner, Snyk, GitHub's Dependabot alerts, or your repository manager's scanner. New CVEs appear for code you shipped months ago, so scanning is a continuous process, not a one-time gate.
- **Triage** findings: is the vulnerable code reachable in your usage? Is a fixed version available? Upgrade, or mitigate and document an exception. Don't let the alert list grow stale.
- Remember **transitive** vulnerabilities. The fix is often to bump the BOM or add a constraint that forces the patched version.

### Know what you ship: SBOM

Generate a **Software Bill of Materials** (CycloneDX or SPDX format) at build time, using the CycloneDX Maven/Gradle plugins, and store it with each release artifact. When the next Log4Shell lands, you can answer "which of our services contain it?" in minutes. See [Dependency Security](../21-security/05_dependency-security.md) and [CI/CD](03_ci-cd-pipelines.md#7-secrets-and-supply-chain-in-the-pipeline).

---

## 7. Licenses

Every dependency has a license, and some impose obligations (attribution, copyleft, source disclosure). Common permissive licenses (Apache-2.0, MIT, BSD) are generally easy. **GPL/AGPL** and similar copyleft licenses can create obligations on your own code depending on how you use them. Use license-checking plugins/scanners in CI, keep an allow-list policy agreed with your organization's legal guidance, and include required attribution/NOTICE files. (This is general information, not legal advice.)

---

## 8. If you publish a library

- Version with **SemVer**, and keep **API compatibility** within a major version.
- Expose only what consumers need (`api` vs `implementation` in Gradle; `<optional>`/`provided` in Maven). A transitive dependency you leak becomes part of your contract.
- Publish a **BOM** if you ship several artifacts.
- Provide **sources and Javadoc JARs**, a clean POM (name, description, license, SCM, developers), and **signed** artifacts for Maven Central. Publishing goes through the Sonatype **Central Portal** (the older OSSRH service was retired in 2025). Follow the current publishing guide.
- Document the **minimum Java version** and compile with `--release` to match it.
- Declare module names (`Automatic-Module-Name` or a `module-info.java`) so JPMS users aren't surprised ([Java Modules](../05-packages-and-modules/02_java-modules.md)).

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Not looking at the dependency tree until something breaks | Review `dependency:tree`/`dependencies` regularly and in code review of dependency changes |
| Relying on transitive dependencies for classes you import | Declare direct dependencies explicitly |
| Mixed versions within a library family | Import the family's BOM |
| Version ranges / `latest` / unpinned plugins | Exact versions |
| Multiple logging backends or JSON libraries | One implementation per concern |
| Ignoring `NoSuchMethodError` as a "random" runtime bug | It's a version conflict: find the winner and the loser |
| Never updating, then facing a huge upgrade | Automated small updates (Dependabot/Renovate) |
| One-time vulnerability scan before release | Continuous scanning plus SBOM |
| Adding random third-party repositories to "make the build work" | Use your repository manager, and review each repo |
| Internal artifact names that could clash with public ones | Exclusive routing for internal groupIds, namespaced coordinates |
| Treating every CVE alert as an emergency (or ignoring them all) | Triage by reachability and severity |
| Copy-pasting dependency snippets from the internet without checking the version or source | Verify coordinates, publisher, and age |

### Debugging flow: "it broke after a dependency change"

```text
NoSuchMethodError / NoClassDefFoundError / strange behavior after adding or bumping a library
  1. Print the tree (-Dverbose / dependencyInsight) and find the library's resolved version
  2. Which dependency requested which version?   → find the loser (the library compiled against another version)
  3. Where does the class really load from?      → -Xlog:class+load, or Foo.class.getProtectionDomain().getCodeSource()
  4. Align versions: BOM → constraint/forced version → exclusion
  5. Lock it in: enforcer rules / dependency locking, and add a test that exercises the path
```

Also check for **shaded copies** of the library inside another JAR (`jar tf some.jar | grep Foo`), which can shadow your version.

---

## Quick Summary

- You're responsible for the **whole resolved graph**, not just the dependencies you declared. Inspect it (`dependency:tree`, `dependencyInsight`).
- **Conflict rules differ:** Maven = nearest wins; Gradle = highest wins. Version mismatches surface as `NoSuchMethodError`/`ClassNotFoundException` at runtime.
- Fix with **BOMs** first, then forced versions or exclusions, and **enforce** with the enforcer plugin or dependency locking.
- Use **exact versions**, narrow scopes, few dependencies, and **declare direct dependencies explicitly**.
- **Update routinely** with Dependabot/Renovate, small steps, and tests as a safety net. Let fresh releases settle, apart from security fixes.
- **Supply-chain defense:** a repository manager, exclusive routing for internal groups (against dependency confusion), checksum/signature verification, continuous SCA scanning, and an **SBOM** for every release.
- Mind **licenses**, and publish libraries with SemVer, a minimal API surface, signed artifacts, and a documented minimum Java version.

**Next:** [CI/CD Pipelines](03_ci-cd-pipelines.md)