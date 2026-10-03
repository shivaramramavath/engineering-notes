# First Project with Maven and Gradle

Real Java projects are built with a **build tool** that compiles code, downloads libraries, runs tests and packages the result. **Maven** and **Gradle** are the two standard choices. This file is a quick start only; the full treatment is in [18-build-and-dependencies](../18-build-and-dependencies/README.md).

```
source + build file ──► build tool ──► download dependencies ─► compile ─► test ─► package (JAR)
```

## Which one?

| | Maven | Gradle |
|---|-------|--------|
| Build file | `pom.xml` (XML) | `build.gradle` (Groovy) or `build.gradle.kts` (Kotlin) |
| Style | Convention-driven, fixed lifecycle | Flexible, task-based |
| Learning curve | Gentle, very common in enterprise | Steeper, faster builds on large projects |
| Use it if | Your team or job uses it (most common default) | Android, large multi-module, or your team uses it |

Learn **one first** (Maven is a safe choice), then skim the other. Never mix both in one project.

## Standard project layout

Both tools use the same layout, so IDEs and teammates know where everything is.

```
my-app/
├── pom.xml  (or build.gradle)
└── src/
    ├── main/
    │   ├── java/com/example/App.java        ◄── production code
    │   └── resources/                       ◄── config files, templates
    └── test/
        ├── java/com/example/AppTest.java    ◄── test code
        └── resources/
```

Build output goes to `target/` (Maven) or `build/` (Gradle). Never edit it and never commit it.

## The code (same for both)

```java
// src/main/java/com/example/App.java
package com.example;

public class App {
    public static String greet(String name) {
        return "Hello, " + name + "!";
    }

    public static void main(String[] args) {
        System.out.println(greet("Java"));
    }
}
```

```java
// src/test/java/com/example/AppTest.java
package com.example;

import static org.junit.jupiter.api.Assertions.assertEquals;
import org.junit.jupiter.api.Test;

class AppTest {
    @Test
    void greetsByName() {
        assertEquals("Hello, Ada!", App.greet("Ada"));
    }
}
```

## Maven

### Create the project

```bash
mvn -v                                     # check installation
mvn archetype:generate -DgroupId=com.example -DartifactId=my-app \
    -DarchetypeArtifactId=maven-archetype-quickstart -DinteractiveMode=false
```

The generated sample targets an old Java version. Replace the `pom.xml` with the minimal one below.

### Minimal `pom.xml`

```xml
<project xmlns="http://maven.apache.org/POM/4.0.0"
         xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
         xsi:schemaLocation="http://maven.apache.org/POM/4.0.0 http://maven.apache.org/xsd/maven-4.0.0.xsd">
  <modelVersion>4.0.0</modelVersion>

  <groupId>com.example</groupId>        <!-- your organization / package prefix -->
  <artifactId>my-app</artifactId>       <!-- project name -->
  <version>1.0-SNAPSHOT</version>       <!-- SNAPSHOT = in development -->

  <properties>
    <maven.compiler.release>21</maven.compiler.release>
    <project.build.sourceEncoding>UTF-8</project.build.sourceEncoding>
  </properties>

  <dependencies>
    <dependency>
      <groupId>org.junit.jupiter</groupId>
      <artifactId>junit-jupiter</artifactId>
      <version>5.11.4</version>          <!-- check for the latest 5.x or 6.x -->
      <scope>test</scope>
    </dependency>
  </dependencies>
</project>
```

### Commands

| Command | Does |
|---------|------|
| `mvn compile` | Compile `src/main` |
| `mvn test` | Compile and run tests |
| `mvn package` | Test, then build `target/my-app-1.0-SNAPSHOT.jar` |
| `mvn clean` | Delete `target/` |
| `mvn clean verify` | Full clean build with tests (use this in CI) |
| `mvn dependency:tree` | Show dependencies, including transitive ones |

```bash
mvn clean package
java -cp target/classes com.example.App          # Hello, Java!
```

Running the jar directly with `java -jar` needs a `Main-Class` manifest entry (see [18-build-and-dependencies](../18-build-and-dependencies/README.md)).

### Maven Wrapper

Pins the Maven version so no one has to install it.

```bash
mvn wrapper:wrapper
./mvnw clean package        # Windows: mvnw.cmd
```

## Gradle

### Create the project

```bash
gradle -v
gradle init          # answer the prompts: application, Java, Groovy/Kotlin DSL, JUnit Jupiter
```

### Minimal `build.gradle`

```groovy
plugins {
    id 'application'
}

repositories {
    mavenCentral()
}

java {
    toolchain {
        languageVersion = JavaLanguageVersion.of(21)
    }
}

dependencies {
    testImplementation 'org.junit.jupiter:junit-jupiter:5.11.4'     // check for the latest
    testRuntimeOnly 'org.junit.platform:junit-platform-launcher'
}

application {
    mainClass = 'com.example.App'
}

tasks.named('test') {
    useJUnitPlatform()
}
```

### Commands

| Command | Does |
|---------|------|
| `./gradlew build` | Compile, test, package (`build/libs/`) |
| `./gradlew test` | Run tests |
| `./gradlew run` | Run the `application` main class |
| `./gradlew clean` | Delete `build/` |
| `./gradlew dependencies` | Show the dependency tree |
| `./gradlew tasks` | List available tasks |

### Gradle Wrapper

```bash
gradle wrapper                 # creates gradlew, gradlew.bat, gradle/wrapper/
./gradlew run                  # always use the wrapper afterwards
```

## Maven vs Gradle: same task, side by side

| Task | Maven | Gradle |
|------|-------|--------|
| Compile | `mvn compile` | `./gradlew compileJava` |
| Test | `mvn test` | `./gradlew test` |
| Package | `mvn package` | `./gradlew build` |
| Clean | `mvn clean` | `./gradlew clean` |
| Add a library | `<dependency>` in `pom.xml` | `implementation '...'` in `build.gradle` |
| Test-only library | `<scope>test</scope>` | `testImplementation` |

## Open it in the IDE

In IntelliJ: **File → Open** and select the folder (or the `pom.xml` / `build.gradle`). The IDE imports the project from the build file. After editing the build file, click the reload icon. See [intellij-idea.md](./04_intellij-idea.md).

## `.gitignore` essentials

```gitignore
target/
build/
.gradle/
.idea/
*.iml
*.class
```

Commit the build file and the wrapper files (`mvnw`, `gradlew`, `.mvn/`, `gradle/wrapper/`).

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Wrong layout (`src/Main.java`) | "No sources to compile" | Use `src/main/java/...` |
| Package does not match folders | Class not found at runtime | `package com.example;` must live in `com/example/` |
| Editing generated files in `target/` or `build/` | Changes vanish | Edit `src/` only |
| Mixing Maven and Gradle files in one project | Two sources of truth, IDE confusion | Keep one |
| Test class not found by the build | Tests silently skipped | Name `*Test`, put under `src/test/java`, and check the JUnit dependency and `useJUnitPlatform()` |
| JDK differs from the build's Java version | `invalid target release` | Align with [version management](./01_java-version-management.md) |
| Using `mvn`/`gradle` instead of the wrapper | "Works on my machine" | Commit and use `mvnw` / `gradlew` |
| Forgetting to reload after editing the build file | New dependency unresolved | Reload in the IDE |

## Key takeaways

- Build tools compile, manage dependencies, test and package, driven by a build file
- Both use `src/main/java` and `src/test/java`; output goes to `target/` or `build/`
- Start with **one** tool and commit its wrapper
- Set the Java version in the build file (`release` or toolchain), not just on your machine
- This is a quick start; depth lives in [18-build-and-dependencies](../18-build-and-dependencies/README.md)

**Next:** [01-fundamentals](../01-fundamentals/README.md)
