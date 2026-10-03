# 00 - Setup

Install Java, run your first program, and set up the tools used in the rest of the repo. Setup content goes stale fast, so these files stay short and link to official docs instead of copying them.

```
install JDK ─► manage versions ─► understand compile/run ─► experiment (JShell) ─► IDE ─► build tool
```

## Prerequisites

None. You need a computer and a terminal.

## Reading order

| # | File | You will learn |
|---|------|----------------|
| 1 | [00_jdk-installation.md](./00_jdk-installation.md) | Install a JDK, set `JAVA_HOME` and `PATH` |
| 2 | [01_java-version-management.md](./01_java-version-management.md) | LTS vs non-LTS, switch versions per project |
| 3 | [02_how-java-works.md](./02_how-java-works.md) | Source → bytecode → JVM; JDK vs JRE vs JVM |
| 4 | [03_jshell.md](./03_jshell.md) | Run Java snippets with no files |
| 5 | [04_intellij-idea.md](./04_intellij-idea.md) | Set up the IDE, run and debug |
| 6 | [05_first-project-with-maven-and-gradle.md](./05_first-project-with-maven-and-gradle.md) | Standard project layout and build commands |

## Verify your setup

You are done when every item passes.

- [ ] `java -version` and `javac -version` print the **same major version**
- [ ] `echo $JAVA_HOME` (`echo %JAVA_HOME%` on Windows cmd) points to that JDK
- [ ] `jshell` starts and `1 + 1` prints `$1 ==> 2`
- [ ] A Hello World compiles with `javac` and runs with `java`
- [ ] IntelliJ runs and debugs the same program
- [ ] `mvn -v` or `./gradlew -v` works

## Recommended version

Use the **latest LTS** release. As of this writing that is **Java 25** (September 2025); the next LTS is Java 29 (September 2027). Older LTS releases such as 21 and 17 are still widely used in industry. Check [adoptium.net](https://adoptium.net) for the current list.

The examples in this repo target Java 21 or later unless a file says otherwise.

## Common problems

| Symptom | Likely cause | Fix |
|---------|--------------|-----|
| `java: command not found` | JDK not on `PATH` | See [00_jdk-installation.md](./00_jdk-installation.md) |
| `javac: command not found` but `java` works | Only a JRE is installed | Install a full JDK |
| `java -version` and `javac -version` differ | Two JDKs, wrong `PATH` order | See [01_java-version-management.md](./01_java-version-management.md) |
| IDE works, terminal fails (or reverse) | IDE uses its own JDK setting | Set both to the same JDK |

## Key takeaways

- Install a **JDK** (not just a JRE), preferably the latest LTS
- Keep `java`, `javac`, `JAVA_HOME`, the IDE and the build tool on the same version
- Verify with the checklist before moving on

**Next:** [01-fundamentals](../01-fundamentals/README.md)
