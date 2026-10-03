# JShell

**JShell** is Java's interactive REPL (read-eval-print loop), included in the JDK since Java 9. Type a Java expression and see the result immediately, with no class, no `main` and no compile step.

```
you type ─► JShell compiles + runs it ─► prints the result ─► waits for more
```

## Start and stop

```bash
jshell              # start
jshell -v           # verbose feedback
jshell --enable-preview --release 25    # try preview features
```

```
|  Welcome to JShell -- Version 25
|  For an introduction type: /help intro

jshell> 1 + 1
$1 ==> 2

jshell> /exit
```

## Expressions, variables and methods

```java
jshell> 2 + 3 * 4
$1 ==> 14

jshell> $1 * 2                     // scratch variables: $1, $2, ...
$2 ==> 28

jshell> String name = "Ada"
name ==> "Ada"

jshell> name.toUpperCase()
$4 ==> "ADA"

jshell> int square(int n) { return n * n; }
|  created method square(int)

jshell> square(7)
$6 ==> 49

jshell> record Point(int x, int y) {}
|  created record Point
```

- Semicolons are **optional** for single statements
- You can redefine a variable, method or class at any time
- Common packages (`java.util.*`, `java.io.*`, `java.util.stream.*`, ...) are imported by default

## Multi-line input

```java
jshell> for (int i = 1; i <= 3; i++) {
   ...>     System.out.println("Row " + i);
   ...> }
Row 1
Row 2
Row 3
```

JShell waits until the braces are balanced.

## Essential commands

Commands start with `/` and are not Java.

| Command | Purpose |
|---------|---------|
| `/help` | List all commands |
| `/list` | Show snippets you entered (with IDs) |
| `/list -all` | Include startup and failed snippets |
| `/vars` | Show variables and values |
| `/methods` | Show defined methods |
| `/types` | Show defined classes, records, interfaces |
| `/imports` | Show active imports |
| `/edit` | Open snippets in an editor |
| `/save file.jsh` | Save your snippets to a file |
| `/open file.jsh` | Load and run a file of snippets |
| `/drop name` | Delete a variable, method or type |
| `/reset` | Clear all state |
| `/exit` | Quit |

## Handy features

| Feature | How |
|---------|-----|
| Tab completion | Press `Tab` after `Math.` to list methods |
| Documentation | `Tab` twice after a method name, or `Shift+Tab` then `d` |
| Create variable from expression | Type `new ArrayList<String>()` then `Shift+Tab` then `v` |
| Add missing import | `Shift+Tab` then `i` after an unresolved type |
| History | Up and down arrows |

## Using libraries

```bash
jshell --class-path lib/gson.jar
```

```java
jshell> /env --class-path lib/gson.jar     // inside a session
jshell> import com.google.gson.*
```

## Run a script

```bash
jshell setup.jsh            # run a file, then stay in the session
jshell -                    # read from standard input
```

## When to use JShell vs a file

| Use JShell for | Use a file or IDE for |
|----------------|-----------------------|
| Trying an API ("what does `substring` return?") | Anything with multiple classes |
| Checking operator precedence or casting results | Programs you will run again |
| Quick math, `BigDecimal`, date-time experiments | Code that needs tests or version control |
| Verifying a snippet before asking a question | Debugging with breakpoints |

## Try it: exercises

```java
jshell> 7 / 2                       // 3 (integer division)
jshell> 7 / 2.0                     // 3.5
jshell> (int) 3.99                  // 3
jshell> 0.1 + 0.2                   // 0.30000000000000004
jshell> "Java".repeat(3)
jshell> List.of(3, 1, 2).stream().sorted().toList()
jshell> Integer.MAX_VALUE + 1       // overflow
jshell> "a" == new String("a")      // false; see 03-strings-and-text
```

## Common mistakes

| Mistake | Why it hurts | Fix |
|---------|--------------|-----|
| Treating JShell state as a program | Order of snippets and redefinitions hide bugs | Move real code to a file |
| Forgetting `/` on commands | `list` is read as Java and fails | Write `/list` |
| Expecting `public static void main` | Not needed or used | Just type statements |
| Assuming a class compiled earlier still matches after redefining it | Dependent snippets get updated or marked invalid | Check `/list` and re-run |
| Losing work on `/exit` | Session state is not saved | `/save session.jsh` first |
| Checked exceptions confusion | Some snippets throw without a `try` | JShell wraps statements; use `try` only in real code |

## Key takeaways

- JShell runs Java snippets instantly; no class or `main` needed
- Commands start with `/`; `/list`, `/vars`, `/save` and `/exit` cover most uses
- Use it to explore APIs and verify language behavior, not to write programs
- Use `Tab` completion and `Shift+Tab` shortcuts to work faster

**Next:** [IntelliJ IDEA](./04_intellij-idea.md)
