# Maven

**Apache Maven** builds Java projects from a declarative description: a `pom.xml` (Project Object Model) states *what* the project is and *which* libraries it needs, and Maven's fixed **lifecycle** and **plugins** know *how* to build it. Its core idea is **convention over configuration**. If you follow the standard directory layout, you need almost no build logic.

Maven is the most widely used Java build tool, and its concepts (coordinates, repositories, scopes, BOMs) show up everywhere else, Gradle included.

**Prerequisites:** [Classpath and JARs](../05-packages-and-modules/01_classpath-and-jars.md), [First Project with Maven and Gradle](../00-setup/05_first-project-with-maven-and-gradle.md).

---

## 1. The project layout and the POM

```text
 my-app/
 ├── pom.xml
 ├── mvnw, mvnw.cmd, .mvn/wrapper/        ← Maven Wrapper (section 8)
 ├── src/
 │   ├── main/java/          production code
 │   ├── main/resources/     config files, templates (copied to the classpath)
 │   ├── test/java/          tests
 │   └── test/resources/
 └── target/                 build output (never commit)
```

A minimal `pom.xml`:

```xml
<project xmlns="http://maven.apache.org/POM/4.0.0"
         xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
         xsi:schemaLocation="http://maven.apache.org/POM/4.0.0 https://maven.apache.org/xsd/maven-4.0.0.xsd">
  <modelVersion>4.0.0</modelVersion>

  <groupId>com.acme</groupId>
  <artifactId>orders</artifactId>
  <version>1.0.0-SNAPSHOT</version>
  <packaging>jar</packaging>

  <properties>
    <maven.compiler.release>21</maven.compiler.release>
    <project.build.sourceEncoding>UTF-8</project.build.sourceEncoding>
  </properties>

  <dependencies>
    <dependency>
      <groupId>org.junit.jupiter</groupId>
      <artifactId>junit-jupiter</artifactId>
      <version>5.x.y</version>          <!-- use the current JUnit release -->
      <scope>test</scope>
    </dependency>
  </dependencies>
</project>
```

### Coordinates

Every artifact is identified by **GAV** coordinates:

| Part | Meaning | Example |
|---|---|---|
| `groupId` | Organization/namespace (reverse domain) | `com.fasterxml.jackson.core` |
| `artifactId` | The library/project name | `jackson-databind` |
| `version` | Release version | `2.18.0` |
| `packaging` | `jar` (default), `war`, `pom`, ... | `jar` |
| (classifier) | A variant (`sources`, `javadoc`, `linux-x86_64`) | |

`1.0.0-SNAPSHOT` means "work in progress": Maven may re-download newer snapshot builds. A version without `-SNAPSHOT` is a **release**, and must be immutable (once published, never changed).

Set the Java version with **`maven.compiler.release`** (not `source`/`target`): it also restricts the APIs you may call to those of that Java release ([Release Timeline](../12-modern-java/00_java-release-timeline.md#4-which-version-should-i-target)).

---

## 2. The lifecycle: phases and goals

Maven runs **phases** in a fixed order. Each phase executes the **plugin goals** bound to it. Asking for a phase runs every earlier phase too.

```text
 validate → compile → test → package → verify → install → deploy
    │          │        │        │         │         │         └ upload to a remote repository
    │          │        │        │         │         └ copy the artifact into your local repo (~/.m2)
    │          │        │        │         └ integration tests, quality checks
    │          │        │        └ build the JAR/WAR
    │          │        └ run unit tests (surefire)
    │          └ compile (compiler plugin)
    └ check the project is correct
```

(Plus `generate-sources`, `process-resources`, `test-compile`, `pre-integration-test`, `integration-test` and others in between.) Two other lifecycles: **`clean`** (delete `target/`) and **`site`** (documentation).

Commands you'll use daily:

```bash
mvn clean verify            # the standard "build everything and run all tests"
mvn test                    # compile + unit tests only
mvn package -DskipTests     # build the artifact, skip tests (use sparingly)
mvn install                 # also install into ~/.m2 so other local projects can use it
mvn -B -ntp verify          # CI-friendly: batch mode, no transfer progress noise
mvn -Dtest=OrderServiceTest test         # one test class
mvn -Dtest=OrderServiceTest#cancels test # one method
mvn -pl order-service -am verify         # one module (+ the modules it depends on)
mvn -T 1C verify            # parallel build: 1 thread per CPU core
mvn -o verify               # offline: use only the local repository
mvn -U verify               # force-update snapshots and failed lookups
mvn -P integration verify   # activate a profile
mvn -X verify               # debug output (when something is wrong)
```

You can also invoke a plugin goal directly: `mvn dependency:tree`, `mvn versions:display-dependency-updates`.

---

## 3. Dependencies

```xml
<dependency>
  <groupId>com.fasterxml.jackson.core</groupId>
  <artifactId>jackson-databind</artifactId>
  <version>2.x.y</version>
</dependency>
```

Maven downloads the JAR (and **its** dependencies, transitively) from a repository into your local cache `~/.m2/repository`, and puts them on the classpath.

### Scopes

| Scope | Compile classpath | Test classpath | Runtime classpath | Typical use |
|---|---|---|---|---|
| `compile` (default) | ✔ | ✔ | ✔ | Normal libraries |
| `provided` | ✔ | ✔ | ✘ | APIs supplied by the runtime (servlet API, Lombok) |
| `runtime` | ✘ | ✔ | ✔ | JDBC drivers, logging backends: needed to run, not to compile |
| `test` | ✘ | ✔ | ✘ | JUnit, Mockito, Testcontainers |
| `import` | | | | Only in `dependencyManagement` for BOMs (section 4) |

Pick the narrowest scope that works. It keeps test libraries out of production artifacts and makes accidental coupling to implementation libraries a compile error.

### Transitive dependencies and exclusions

Dependencies bring their own dependencies. See what you actually get:

```bash
mvn dependency:tree                       # the resolved tree
mvn dependency:tree -Dverbose             # include omitted duplicates and conflicts
mvn dependency:analyze                    # declared-but-unused / used-but-undeclared
```

Remove an unwanted transitive dependency with an exclusion (or declare it `<optional>` in your own library to avoid forcing it on consumers):

```xml
<dependency>
  <groupId>org.example</groupId>
  <artifactId>legacy-client</artifactId>
  <version>1.2.0</version>
  <exclusions>
    <exclusion>
      <groupId>commons-logging</groupId>
      <artifactId>commons-logging</artifactId>
    </exclusion>
  </exclusions>
</dependency>
```

How Maven resolves two different versions of the same library in the tree is covered in [Dependency Management](02_dependency-management.md#2-conflicts-how-they-are-resolved).

---

## 4. Managing versions: `dependencyManagement` and BOMs

Declaring versions in many places invites drift. `dependencyManagement` centralizes them, and child modules then declare dependencies **without** a version:

```xml
<dependencyManagement>
  <dependencies>
    <!-- import a BOM: a POM that lists compatible versions of a whole library family -->
    <dependency>
      <groupId>com.fasterxml.jackson</groupId>
      <artifactId>jackson-bom</artifactId>
      <version>2.x.y</version>
      <type>pom</type>
      <scope>import</scope>
    </dependency>
  </dependencies>
</dependencyManagement>

<dependencies>
  <dependency>
    <groupId>com.fasterxml.jackson.core</groupId>
    <artifactId>jackson-databind</artifactId>   <!-- version comes from the BOM -->
  </dependency>
</dependencies>
```

A **BOM** (Bill of Materials) guarantees that all artifacts in a family (Jackson, Spring, JUnit, cloud SDKs) use versions tested together. Importing the BOM is the standard way to avoid mixed-version breakage ([Jackson](../17-json-and-data-formats/01_jackson.md#1-setup)).

`dependencyManagement` only **declares** versions. It doesn't add dependencies to the project.

---

## 5. Plugins

Maven itself does almost nothing. **Plugins** do the work, and many are bound to the lifecycle by default. The ones you'll meet:

| Plugin | Purpose |
|---|---|
| `maven-compiler-plugin` | Compiles Java (`release` setting) |
| `maven-surefire-plugin` | Runs **unit tests** (in `test`). Default patterns: `*Test`, `Test*`, `*Tests`, `*TestCase` |
| `maven-failsafe-plugin` | Runs **integration tests** (in `integration-test`/`verify`): default pattern `*IT`, `IT*`, `*ITCase` ([CI/CD](03_ci-cd-pipelines.md#4-test-stages-in-the-pipeline)) |
| `maven-jar-plugin` | Builds the JAR, sets the manifest (`Main-Class`) |
| `maven-shade-plugin` / `maven-assembly-plugin` | "Fat" / "uber" JARs containing dependencies |
| `spring-boot-maven-plugin` | Executable Spring Boot JARs and image building |
| `maven-enforcer-plugin` | Enforce rules: Java/Maven version, dependency convergence, banned dependencies |
| `jacoco-maven-plugin` | Code coverage |
| `versions-maven-plugin` | Report and update dependency/plugin versions |
| `maven-dependency-plugin` | Tree, analysis, copying dependencies |

Configure a plugin in `<build><plugins>`, and pin versions in `<pluginManagement>` so builds are reproducible:

```xml
<build>
  <pluginManagement>
    <plugins>
      <plugin>
        <artifactId>maven-surefire-plugin</artifactId>
        <version>3.x.y</version>
      </plugin>
    </plugins>
  </pluginManagement>
  <plugins>
    <plugin>
      <artifactId>maven-enforcer-plugin</artifactId>
      <version>3.x.y</version>
      <executions>
        <execution>
          <goals><goal>enforce</goal></goals>
          <configuration>
            <rules>
              <requireJavaVersion><version>[21,)</version></requireJavaVersion>
              <dependencyConvergence/>
            </rules>
          </configuration>
        </execution>
      </executions>
    </plugin>
  </plugins>
</build>
```

An unpinned plugin version means "whatever Maven's defaults or latest rules choose today", which is a classic cause of builds that break after an upgrade. Run `mvn help:effective-pom` to see the fully resolved configuration.

---

## 6. Multi-module projects

Large systems split into modules built together by one **reactor**:

```text
 shop/
 ├── pom.xml                  ← parent + aggregator (packaging: pom)
 ├── shop-domain/pom.xml
 ├── shop-persistence/pom.xml   (depends on shop-domain)
 └── shop-web/pom.xml           (depends on both)
```

```xml
<!-- parent pom.xml -->
<packaging>pom</packaging>
<modules>
  <module>shop-domain</module>
  <module>shop-persistence</module>
  <module>shop-web</module>
</modules>

<!-- child pom.xml -->
<parent>
  <groupId>com.acme</groupId>
  <artifactId>shop</artifactId>
  <version>1.0.0-SNAPSHOT</version>
</parent>
<artifactId>shop-persistence</artifactId>
<dependencies>
  <dependency>
    <groupId>com.acme</groupId>
    <artifactId>shop-domain</artifactId>
    <version>${project.version}</version>
  </dependency>
</dependencies>
```

- **Parent POM** = shared configuration inherited by children (dependencyManagement, plugins, properties). **Aggregator** = lists `<modules>` to build together. They're often the same file but are distinct ideas.
- Maven orders module builds by their **dependencies**, not by the order listed.
- Build a slice: `mvn -pl shop-web -am verify` (`-am` also builds what it needs).
- Keep module dependencies **acyclic and layered**. Cyclic dependencies are rejected.
- Maven 4 (pre-release) renames the child list to `<subprojects>` and introduces a new model version with improved inheritance and version inference. The classic 3.x model above remains the supported baseline.

### Profiles

```xml
<profiles>
  <profile>
    <id>integration</id>
    <build> ... run failsafe, start containers ... </build>
  </profile>
</profiles>
```

Activate with `-P integration`, by JDK, OS, property, or file. Use profiles sparingly: builds that behave differently depending on hidden activation are hard to reproduce.

---

## 7. Repositories and `settings.xml`

```text
 your build ──► local repository (~/.m2/repository)   cache of everything downloaded or installed
            └─► remote repositories: Maven Central (default), your company's Nexus/Artifactory, mirrors
```

- **Maven Central** is the default remote. Companies usually run a **repository manager** (Nexus, Artifactory) that proxies Central, hosts private artifacts, and acts as a controlled gateway ([Dependency Management](02_dependency-management.md#6-repositories-and-supply-chain-security)).
- **`~/.m2/settings.xml`** holds *user-specific* configuration, not project config: mirrors (route all downloads through your manager), server credentials (referenced by repository `<id>`), proxies, and profiles. **Never put credentials in the project's `pom.xml`** or commit `settings.xml` secrets. CI injects them through environment variables or secret stores ([Secrets Management](../21-security/03_secrets-management.md)).
- Publishing: `mvn deploy` uploads to a repository defined in `<distributionManagement>`. Publishing to Maven Central requires signed artifacts (GPG) with sources and Javadoc JARs, and is done through the Sonatype **Central Portal** (the older OSSRH service was retired in 2025). Check the current publishing guide, as this process has changed.

---

## 8. The Maven Wrapper

Different machines having different Maven versions causes "works for me" failures. The **wrapper** pins the Maven version in the repo:

```bash
mvn wrapper:wrapper                  # generates mvnw, mvnw.cmd, .mvn/wrapper/maven-wrapper.properties
./mvnw -B verify                      # downloads the pinned Maven on first use, then runs it
```

Commit `mvnw`, `mvnw.cmd`, and `.mvn/wrapper/`. Use `./mvnw` in documentation and CI. The `.mvn/` directory can also hold `maven.config` (default CLI options) and `jvm.config` (JVM options for Maven itself).

---

## 9. Reproducible and clean builds

- Pin **plugin versions** and use `dependencyManagement`/BOMs for dependency versions. Avoid version ranges (`[1.0,2.0)`) and `LATEST`/`RELEASE`.
- Set `project.build.outputTimestamp` so identical sources produce **byte-identical** JARs (useful for supply-chain verification and caching).
- Treat `-SNAPSHOT` dependencies as temporary: a release build shouldn't depend on snapshots.
- `mvn clean` removes stale outputs when a build behaves strangely, but if you *need* `clean` to get a correct result regularly, something is wrong with the build.

---

## 10. Maven 4 status

Maven 4.0.0 has been in release-candidate stages for a long time. It brings a new POM model version, a split between the *build* POM and the *consumer* POM published to repositories, `<subprojects>`, better multi-module and version-inference features, parallel-build improvements, and an `mvnup` tool to help upgrade. Until a GA release, treat it as pre-release: **Maven 3.9.x is the stable choice**, and follow the 4.x release notes when it ships. Most of what's in this note is unchanged between them.

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Different Maven versions per developer/CI | Commit the wrapper, always use `./mvnw` |
| `maven.compiler.source/target` only | `maven.compiler.release` |
| Unpinned plugin versions | `<pluginManagement>` with explicit versions |
| Versions scattered across modules | Parent `dependencyManagement` / BOM imports |
| Everything in `compile` scope | `provided`/`runtime`/`test` where appropriate |
| Version ranges, `LATEST`, snapshots in releases | Fixed release versions |
| Credentials in `pom.xml` or committed `settings.xml` | CI secrets, user-level `settings.xml` |
| Integration tests in surefire (slowing every `mvn test`) | Name them `*IT` and run them with failsafe in `verify` |
| `-DskipTests` as a habit | Fix or isolate slow/flaky tests |
| Ignoring `dependency:tree` conflicts | Review them and align versions via a BOM |
| Editing `target/` or committing it | `.gitignore` it |
| Profiles that silently change what's built | Keep them few, explicit, and documented |

### Debugging

- **`Could not resolve dependencies` / `Could not transfer artifact`** → check network/proxy and mirror settings in `settings.xml`, repository credentials, the exact coordinates, and whether the artifact exists in the repositories you use. Retry with `-U` to bypass cached failures (`.lastUpdated` marker files in `~/.m2`).
- **`Unsupported class file major version N`** or plugin crashes on a new JDK → an old plugin (compiler, surefire, a bytecode tool) can't read new class files. Upgrade the plugin, or build with a supported JDK ([Class Loading](../15-jvm-internals/01_class-loading.md#5-the-errors-decoded)).
- **Tests don't run** → check class names against the surefire patterns, the JUnit engine dependency, and `-Dtest` filters. Check `surefire-reports/`.
- **Works in the IDE, fails in Maven (or vice versa)** → IDE classpath/scopes or JDK differ. Reimport the project, and compare `mvn dependency:tree` to the IDE's module settings.
- **Which POM settings are actually in effect?** `mvn help:effective-pom`, `mvn help:effective-settings`.
- **Which JDK is Maven using?** `./mvnw -v`.
- **Slow builds** → `-T 1C`, skip unneeded modules (`-pl ... -am`), enable a build cache extension if your team uses one, profile with `-Dmaven.test.skip` experiments to find the slow phase, or check for network round-trips to slow mirrors.

---

## Quick Summary

- Maven = **declarative POM + fixed lifecycle + plugins**. Follow the standard layout (`src/main/java`, `src/test/java`).
- Artifacts are identified by **GAV**. Releases are immutable. `-SNAPSHOT` is work in progress.
- Lifecycle: `validate → compile → test → package → verify → install → deploy`. `mvn clean verify` is the daily command.
- Use **narrow scopes**, inspect the tree with `dependency:tree`, and manage versions with **`dependencyManagement` and BOMs**.
- **Pin plugin versions**; surefire = unit tests (`*Test`), failsafe = integration tests (`*IT`).
- Multi-module: parent/aggregator POM, reactor ordering, `-pl ... -am`.
- Credentials belong in `settings.xml`/CI secrets. **Commit the wrapper** (`mvnw`). Maven 3.9.x is stable, and Maven 4 is still pre-release.

**Next:** [Gradle](01_gradle.md)