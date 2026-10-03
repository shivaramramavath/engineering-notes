# IntelliJ IDEA

An **IDE** (integrated development environment) combines an editor, compiler, debugger, refactoring tools and build integration. IntelliJ IDEA is the most widely used Java IDE. VS Code and Eclipse also work; the ideas below apply to any IDE.

```
IDE = editor + project model + build tool + run/debug + refactoring + version control
```

## Editions

JetBrains has been changing its packaging, so check [jetbrains.com/idea](https://www.jetbrains.com/idea/) for the current options.

| Edition | Covers |
|---------|--------|
| Free tier / Community | Java SE, Maven, Gradle, debugger, JUnit, Git |
| Ultimate (paid, free trial) | Adds Spring, web, database tools, profilers |

Everything in this repo up to `18-build-and-dependencies` works on the free tier.

## Install

```bash
# macOS
brew install --cask intellij-idea

# Windows
winget install JetBrains.IntelliJIDEA.Community   # or use the installer from jetbrains.com

# Linux: use the JetBrains Toolbox app or snap
```

JetBrains Toolbox is a good choice on any OS: it installs and updates the IDE.

## Create and run your first project

1. **New Project** → Language: Java → Build system: **Maven** or **Gradle** (or IntelliJ for a plain project)
2. **JDK**: pick your installed JDK, or choose *Download JDK*
3. Open `Main.java`, click the green ▶ next to `main`, or press the run shortcut
4. Output appears in the **Run** panel

```java
public class Main {
    public static void main(String[] args) {
        System.out.println("Hello from IntelliJ!");
    }
}
```

## Project structure

```
my-project/
├── pom.xml  or  build.gradle     ◄── build file (source of truth)
├── src/
│   ├── main/java/                ◄── Sources root (blue folder)
│   └── test/java/                ◄── Test sources root (green folder)
└── .idea/                        ◄── IDE settings (do not commit)
```

For Maven or Gradle projects, **the build file controls the project**. Change dependencies and Java version there, then reload.

## Set the JDK correctly

| Setting | Location |
|---------|----------|
| Project SDK | File → Project Structure → Project |
| Language level | File → Project Structure → Project |
| Maven/Gradle JDK | Settings → Build, Execution, Deployment → Build Tools → Maven (or Gradle) |

All three should match the version in your build file (see [version management](./01_java-version-management.md)).

## Essential shortcuts

| Action | Windows / Linux | macOS |
|--------|-----------------|-------|
| Search everywhere | Double `Shift` | Double `Shift` |
| Find action (any command) | `Ctrl+Shift+A` | `Cmd+Shift+A` |
| Run | `Shift+F10` | `Ctrl+R` |
| Debug | `Shift+F9` | `Ctrl+D` |
| Go to declaration | `Ctrl+B` | `Cmd+B` |
| Go to class / file | `Ctrl+N` / `Ctrl+Shift+N` | `Cmd+O` / `Cmd+Shift+O` |
| Find usages | `Alt+F7` | `Option+F7` |
| Generate (constructor, getters, `equals`) | `Alt+Insert` | `Cmd+N` |
| Quick fix / suggestions | `Alt+Enter` | `Option+Enter` |
| Rename (safe, everywhere) | `Shift+F6` | `Shift+F6` |
| Extract variable / method | `Ctrl+Alt+V` / `Ctrl+Alt+M` | `Cmd+Option+V` / `Cmd+Option+M` |
| Reformat code | `Ctrl+Alt+L` | `Cmd+Option+L` |
| Optimize imports | `Ctrl+Alt+O` | `Ctrl+Option+O` |
| Comment line | `Ctrl+/` | `Cmd+/` |

Shortcuts differ by keymap. Use *Find action* if one does not work.

**Live templates** (type, then `Tab`): `psvm` → `main` method, `sout` → `System.out.println`, `fori` → indexed `for` loop.

## Debugging

```java
int total = 0;
for (int i = 1; i <= 5; i++) {
    total += i * i;       // ← click the left gutter to set a breakpoint
}
System.out.println(total);
```

| Step | Key (Windows/Linux) |
|------|---------------------|
| Toggle breakpoint | `Ctrl+F8` |
| Step over (next line) | `F8` |
| Step into (enter method) | `F7` |
| Step out | `Shift+F8` |
| Resume | `F9` |
| Evaluate expression | `Alt+F8` |

Useful features:
- **Variables** and **Watches** panels show values as you step
- **Conditional breakpoint:** right-click the breakpoint, add a condition such as `i == 3`
- **Evaluate expression** lets you call methods on live objects

More on debugging: [06-exceptions-and-debugging](../06-exceptions-and-debugging/README.md).

## Settings worth changing

| Setting | Why |
|---------|-----|
| Auto-import (Editor → General → Auto Import) | Adds imports as you type |
| Format on save (Actions on Save) | Keeps files consistently formatted |
| Reload Maven/Gradle changes automatically | Avoids stale dependencies |
| Show method separators, inlay hints | Readability |

## Useful plugins

| Plugin | Use |
|--------|-----|
| Maven Helper | Dependency tree and conflict analysis |
| SonarLint | Finds bugs and smells as you type |
| Rainbow Brackets | Easier nesting |
| Key Promoter X | Teaches shortcuts for actions you click |

Install only what you need; extra plugins slow the IDE.

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Project SDK not set | `Cannot resolve symbol String`, red everywhere | Project Structure → set SDK |
| Folder not marked as Sources root | Classes not found, package errors | Right-click → Mark Directory as → Sources Root |
| Maven/Gradle changes not reloaded | New dependency not found | Click the reload (🗘) icon |
| IDE uses a different JDK than the terminal | Passes in one, fails in the other | Align all JDK settings |
| Editing code in `target/` or `build/` | Changes disappear on rebuild | Edit `src/` only |
| Committing `.idea/` or `*.iml` | Noisy diffs, machine-specific paths | Add them to `.gitignore` |
| Relying only on the IDE to build | "Works on my machine" | Verify with `mvn` or `./gradlew` too |

## Key takeaways

- An IDE saves time with navigation, refactoring and debugging; learn 10 shortcuts, not 100
- For Maven/Gradle projects, the build file is the source of truth, and the IDE follows it
- Set the JDK in the project, in the build tool settings, and keep it equal to the terminal's
- Learn the debugger early; it beats `println` for understanding programs

**Next:** [First Project with Maven and Gradle](./05_first-project-with-maven-and-gradle.md)
