# JDK Installation

The **JDK (Java Development Kit)** contains everything needed to write, compile and run Java: the compiler (`javac`), the runtime (`java`), and tools such as `jshell`, `jar`, `javadoc` and `jcmd`.

```
JDK  =  JVM  +  standard library  +  developer tools (javac, jshell, jar, jcmd ...)
```

## Which version?

| Choice | Recommendation |
|--------|----------------|
| Learning | Latest LTS (Java 25) |
| Following a job or project | The version it uses (often 17 or 21) |
| Non-LTS (22, 23, 24, 26 ...) | Only to try new features; support ends after about 6 months |

See [01_java-version-management.md](./01_java-version-management.md) for how to keep several versions.

## Which distribution?

All OpenJDK distributions run the same Java. They differ in vendor support, update policy and licence.

| Distribution | Notes |
|--------------|-------|
| **Eclipse Temurin** | Vendor-neutral, free, a safe default |
| Amazon Corretto | Free, long-term support by AWS |
| Azul Zulu | Free builds, paid support available |
| Oracle JDK | Free under Oracle's current terms, check licence for production use |

**Default pick: Temurin.** Download from [adoptium.net](https://adoptium.net).

## Install by operating system

### Windows

```powershell
winget search Temurin          # find the exact package id
winget install EclipseAdoptium.Temurin.25.JDK
```

Or use the `.msi` installer from adoptium.net and tick **"Set JAVA_HOME"** and **"Add to PATH"** in the installer.

### macOS

```bash
brew install --cask temurin          # latest
brew install --cask temurin@21       # a specific LTS
/usr/libexec/java_home -V            # list installed JDKs
```

### Linux

```bash
# Debian / Ubuntu (package names vary by release: check with apt search)
sudo apt update
apt search openjdk | grep jdk
sudo apt install openjdk-21-jdk

# Fedora / RHEL
sudo dnf install java-21-openjdk-devel
```

On any OS, [SDKMAN](./01_java-version-management.md) is often the easiest option.

## Set `JAVA_HOME` and `PATH`

`JAVA_HOME` points to the JDK folder. Maven, Gradle and many tools read it. `PATH` must contain `$JAVA_HOME/bin` so `java` and `javac` are found.

```bash
# macOS / Linux: add to ~/.zshrc or ~/.bashrc
export JAVA_HOME="$(/usr/libexec/java_home -v 25)"     # macOS
# export JAVA_HOME=/usr/lib/jvm/java-25-openjdk-amd64  # Linux example path
export PATH="$JAVA_HOME/bin:$PATH"
```

```powershell
# Windows (PowerShell, current user)
[Environment]::SetEnvironmentVariable("JAVA_HOME", "C:\Program Files\Eclipse Adoptium\jdk-25", "User")
# Then add %JAVA_HOME%\bin to Path via System Properties > Environment Variables
```

Open a **new terminal** afterwards; existing terminals keep the old values.

## Verify

```bash
java -version
javac -version
echo $JAVA_HOME          # Windows cmd: echo %JAVA_HOME%
which java               # Windows: where java
```

Both commands must print the same major version, and `which java` must point into your JDK.

## Hello World

```java
// Hello.java
public class Hello {
    public static void main(String[] args) {
        System.out.println("Hello, Java!");
    }
}
```

```bash
javac Hello.java     # produces Hello.class
java Hello           # runs it
java Hello.java      # or compile and run in one step (Java 11+)
```

The file name must match the `public` class name, including case.

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Installed a JRE only | `javac: command not found` | Install a JDK |
| Old Java earlier on `PATH` | `java -version` shows the old one | Put `$JAVA_HOME/bin` first, or remove the old install |
| Terminal not restarted | Changes seem ignored | Open a new terminal |
| `JAVA_HOME` points to `bin` | Tools fail to start | It must be the JDK root, not `.../bin` |
| Spaces or trailing slash in the path | Odd tool errors | Quote the path, remove the trailing slash |
| `java Hello.class` | `Could not find or load main class` | Use `java Hello` (no extension) |
| File name differs from the public class | `class Hello is public, should be declared in a file named Hello.java` | Rename the file or class |

## Key takeaways

- Install a **JDK**, not only a JRE; Temurin is a good default
- `JAVA_HOME` = JDK root; `PATH` includes its `bin`
- Check `java -version` **and** `javac -version`; they must match
- Open a new terminal after changing environment variables

**Next:** [Java Version Management](./01_java-version-management.md)
