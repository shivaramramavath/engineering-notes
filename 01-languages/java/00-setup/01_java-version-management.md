# Java Version Management

Real projects use different Java versions: one needs 17, another 21, your new experiments use 25. You need a safe way to install several JDKs and switch between them without breaking anything.

```
~/.sdkman/candidates/java/
├── 17.0.x-tem      ◄── legacy project
├── 21.0.x-tem      ◄── work project
└── 25.0.x-tem      ◄── current   (one of them is "current" on PATH)
```

## LTS vs non-LTS

Java ships a new version every **6 months**. Every **2 years** one is designated **LTS (Long-Term Support)**.

| Version | Type | Notes |
|---------|------|-------|
| 8 | LTS | Still found in old codebases |
| 11 | LTS | Old but common |
| 17 | LTS | Widely used |
| 21 | LTS | Virtual threads, pattern matching for switch |
| 25 | LTS | Latest LTS at time of writing |
| 29 | LTS (planned) | September 2027 |
| 22-24, 26+ | non-LTS | Updates stop after about 6 months |

**Rule of thumb:** learn on the latest LTS, build production systems on an LTS, use non-LTS only to preview features. Always confirm current dates at [adoptium.net/support](https://adoptium.net/support).

## Option 1: SDKMAN (macOS, Linux, WSL)

The most convenient tool. It installs JDKs under your home directory (no `sudo`) and can switch per shell or per project.

```bash
curl -s "https://get.sdkman.io" | bash
source "$HOME/.sdkman/bin/sdkman-init.sh"

sdk list java                    # available versions and vendors
sdk install java 21.0.5-tem      # use an identifier from the list (-tem = Temurin)
sdk install java                 # latest default
sdk current java                 # what is active now

sdk use java 21.0.5-tem          # this terminal only
sdk default java 25.0.1-tem      # new terminals (identifiers are examples)
```

### Per-project versions with `.sdkmanrc`

```bash
cd my-project
sdk env init                     # creates .sdkmanrc with the current version
cat .sdkmanrc                    # java=21.0.5-tem
sdk env                          # switch to the version in the file
```

Commit `.sdkmanrc` so every teammate uses the same JDK.

## Option 2: Other tools

| Platform | Tool | Switch command |
|----------|------|----------------|
| macOS | `/usr/libexec/java_home` | `export JAVA_HOME=$(/usr/libexec/java_home -v 21)` |
| macOS / Linux | jenv | `jenv local 21` (per directory), `jenv global 25` |
| macOS / Linux | asdf or mise | `.tool-versions` file |
| Debian / Ubuntu | `update-alternatives` | `sudo update-alternatives --config java` |
| Windows | Scoop, winget | `scoop reset temurin21-jdk`, or change `JAVA_HOME` |
| Any | Manual | Change `JAVA_HOME` and `PATH` yourself |

Pick **one** tool. Mixing SDKMAN, jenv and manual exports is the most common source of "wrong Java" bugs.

## Option 3: Project-level control (recommended for teams)

Even with several JDKs installed, the **build tool** can pick the right one so the project does not depend on whatever `JAVA_HOME` happens to be.

**Maven**: set the target release, enforce it with the compiler plugin.

```xml
<properties>
  <maven.compiler.release>21</maven.compiler.release>
</properties>
```

`release` compiles against that version's API and fails if you use newer methods. Prefer it over `source`/`target`.

**Gradle**: toolchains find or download the right JDK automatically.

```groovy
java {
    toolchain {
        languageVersion = JavaLanguageVersion.of(21)
    }
}
```

## The IDE has its own setting

Your terminal, Maven/Gradle, and IntelliJ can each use a **different** JDK.

| Where | How to check |
|-------|--------------|
| Terminal | `java -version`, `echo $JAVA_HOME` |
| Maven | `mvn -v` (prints the Java it uses) |
| Gradle | `./gradlew -v` |
| IntelliJ | File → Project Structure → Project SDK; Settings → Build Tools → Maven/Gradle → JDK |

Make all of them match the project's version.

## Checking what a class file was built for

```bash
javap -v Hello.class | grep "major version"
# major version: 65   (65 = Java 21, 69 = Java 25, 61 = Java 17, 52 = Java 8)
```

Running a class compiled for a newer Java on an older one fails with `UnsupportedClassVersionError`.

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| `JAVA_HOME` hard-coded in `~/.zshrc` | `sdk use` seems to do nothing | Remove the export; let the tool manage it |
| IDE JDK differs from terminal | Builds pass in one, fail in the other | Align both settings |
| `UnsupportedClassVersionError` | App compiled with newer Java than the runtime | Run on the same or newer JDK, or compile with `release` |
| Using `source`/`target` only | Compiles, then fails at runtime on missing newer APIs | Use `maven.compiler.release` |
| Several managers installed | Unpredictable `PATH` order | Keep one manager |
| Using a non-LTS version in production | Security updates end after about 6 months | Use an LTS |

## Key takeaways

- New Java every 6 months; **LTS every 2 years**; use an LTS for real work
- SDKMAN is the easiest manager on macOS/Linux; use only one manager
- Pin the version **in the project** (`.sdkmanrc`, Maven `release`, Gradle toolchain)
- Terminal, build tool and IDE must all use the same JDK

**Next:** [How Java Works](./02_how-java-works.md)
