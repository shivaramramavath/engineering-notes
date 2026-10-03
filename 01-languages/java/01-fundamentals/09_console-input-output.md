# Console Input and Output

Programs talk to the terminal through three standard streams. This file covers console I/O only; reading and writing **files** is in [11-io-and-networking](../11-io-and-networking/README.md).

```
keyboard ──► System.in  ──► your program ──► System.out ──► terminal
                                         └─► System.err ──► terminal (separate stream)
```

| Stream | Type | Purpose |
|--------|------|---------|
| `System.in` | `InputStream` | Standard input (keyboard or a redirected file) |
| `System.out` | `PrintStream` | Normal output |
| `System.err` | `PrintStream` | Errors and diagnostics |

## Output

```java
System.out.println("Hello");          // prints, then a line break
System.out.print("No line break");    // prints only
System.out.println();                 // blank line
System.out.println(42);               // any type: int, double, boolean, char, Object
System.out.println("Total: " + total);

System.err.println("Something went wrong");   // stderr, can be redirected separately
```

`println(Object)` calls `toString()`. A `char[]` prints its characters, but other arrays print something like `[I@1b6d3586`; use `Arrays.toString` ([arrays](./07_arrays.md)).

## Formatted output: `printf`

```java
System.out.printf("Name: %s, Age: %d%n", name, age);
System.out.printf("Price: %.2f%n", 19.987);          // Price: 19.99
String s = String.format("%05d", 42);                // "00042" (no printing)
```

Use `%n` (platform line separator) instead of `\n` in format strings.

| Specifier | Meaning | Example | Output |
|-----------|---------|---------|--------|
| `%d` | Integer | `%d`, 42 | `42` |
| `%5d` | Width 5, right-aligned | `%5d`, 42 | `   42` |
| `%-5d` | Width 5, left-aligned | `%-5d\|`, 42 | `42   \|` |
| `%05d` | Zero-padded | `%05d`, 42 | `00042` |
| `%,d` | Thousands separator | `%,d`, 1234567 | `1,234,567` |
| `%f` | Floating-point (6 decimals) | `%f`, 3.14159 | `3.141590` |
| `%.2f` | 2 decimals | `%.2f`, 3.14159 | `3.14` |
| `%8.2f` | Width 8, 2 decimals | `%8.2f`, 3.14159 | `    3.14` |
| `%e` | Scientific | `%e`, 12345.678 | `1.234568e+04` |
| `%s` | String (calls `toString`) | `%s`, "hi" | `hi` |
| `%10s` / `%-10s` | Width 10, right / left | | |
| `%c` | Character | `%c`, 'A' | `A` |
| `%b` | Boolean | `%b`, true | `true` |
| `%x` / `%X` | Hexadecimal | `%x`, 255 | `ff` |
| `%%` | A literal percent | | `%` |

A table example:

```java
System.out.printf("%-10s %5s %8s%n", "Item", "Qty", "Price");
System.out.printf("%-10s %5d %8.2f%n", "Pen", 10, 1.5);
System.out.printf("%-10s %5d %8.2f%n", "Notebook", 2, 12.25);
```

```
Item         Qty    Price
Pen           10     1.50
Notebook       2    12.25
```

**Locale:** `%f` uses the default locale, so some systems print `3,14`. For fixed output:

```java
System.out.printf(Locale.US, "%.2f%n", 3.14159);
```

More on formatting: [03-strings-and-text/04_string-formatting.md](../03-strings-and-text/04_string-formatting.md).

## Input with `Scanner`

```java
import java.util.Scanner;

public class Greeter {
    public static void main(String[] args) {
        Scanner in = new Scanner(System.in);

        System.out.print("Name: ");
        String name = in.nextLine();          // a whole line

        System.out.print("Age: ");
        int age = in.nextInt();               // one int

        System.out.printf("Hello %s, you are %d%n", name, age);
        in.close();
    }
}
```

| Method | Reads |
|--------|-------|
| `nextLine()` | The rest of the current line (without the newline) |
| `next()` | The next whitespace-delimited word |
| `nextInt()`, `nextLong()`, `nextDouble()`, `nextBoolean()` | The next token as that type |
| `hasNext()`, `hasNextInt()`, `hasNextLine()` | Whether more input is available |

### The `nextInt()` then `nextLine()` bug

`nextInt()` reads the number but **leaves the newline** in the buffer, so the next `nextLine()` returns an empty string.

```java
int age = in.nextInt();
String name = in.nextLine();       // "" (consumes the leftover newline)

int age = in.nextInt();
in.nextLine();                     // discard the leftover newline
String name = in.nextLine();       // now it works
```

A common safer style is to always read with `nextLine()` and parse yourself:

```java
int age = Integer.parseInt(in.nextLine().trim());
```

### Validating input

```java
int n;
while (true) {
    System.out.print("Enter a positive number: ");
    String line = in.nextLine().trim();
    try {
        n = Integer.parseInt(line);
        if (n > 0) break;
        System.out.println("Must be positive.");
    } catch (NumberFormatException e) {
        System.out.println("Not a number.");
    }
}
```

Alternatively `hasNextInt()` avoids exceptions; `nextInt()` on bad input throws `InputMismatchException` and does **not** consume the bad token.

### Reading until end of input

```java
while (in.hasNextLine()) {
    String line = in.nextLine();
    System.out.println(line.toUpperCase());
}
```

Press `Ctrl+D` (macOS/Linux) or `Ctrl+Z` then Enter (Windows) to signal end of input in a terminal.

### Closing

Closing a `Scanner(System.in)` also closes `System.in`, so you cannot read from it again. Create **one** `Scanner` for the whole program and close it at the end, or use try-with-resources in `main`:

```java
try (Scanner in = new Scanner(System.in)) {
    // all reading here
}
```

Scanner is also slow for large input. Competitive-style programs use `BufferedReader`:

## Faster input: `BufferedReader`

```java
import java.io.*;

try (BufferedReader br = new BufferedReader(new InputStreamReader(System.in))) {
    String line = br.readLine();               // null at end of input
    int n = Integer.parseInt(line.trim());
} catch (IOException e) {
    e.printStackTrace();
}
```

Both `readLine()` and construction can throw `IOException`, a checked exception ([06-exceptions-and-debugging](../06-exceptions-and-debugging/README.md)).

## Command-line arguments

```java
public class Add {
    public static void main(String[] args) {
        if (args.length != 2) {
            System.err.println("Usage: java Add <a> <b>");
            System.exit(1);
        }
        int a = Integer.parseInt(args[0]);
        int b = Integer.parseInt(args[1]);
        System.out.println(a + b);
    }
}
```

```bash
java Add.java 3 4        # prints 7
```

`args` is empty (not `null`) when no arguments are given. In IntelliJ set them under *Run → Edit Configurations → Program arguments*.

## Exit codes

```java
System.exit(0);     // success
System.exit(1);     // failure (non-zero by convention)
```

`System.exit` stops the JVM immediately, so avoid it in library code. Scripts and CI read the exit code.

## Redirection

```bash
java App < input.txt            # read stdin from a file
java App > output.txt           # write stdout to a file
java App 2> errors.txt          # write stderr to a file
java App < in.txt > out.txt 2>&1
```

Separate `System.out` and `System.err` make this possible, so send **errors to `System.err`**.

## Other options

| Tool | Notes |
|------|-------|
| `System.console()` | `java.io.Console`; supports password input without echo, but returns `null` in IDEs and when redirected |
| `IO.println`, `IO.readln` | Beginner-friendly helpers added in Java 25 for compact source files; `System.out` works everywhere |
| Logging frameworks | For real applications use logging instead of `println` ([22-production-engineering/00_logging.md](../22-production-engineering/00_logging.md)) |

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| `nextInt()` followed by `nextLine()` | Empty string | Consume the newline or use `nextLine()` + `parseInt` |
| `Scanner` on bad input | `InputMismatchException` | Validate with `hasNextInt()` or parse with `try/catch` |
| Creating several `Scanner(System.in)` objects | Lost or missing input | Use one |
| Closing the `Scanner` too early | `NoSuchElementException` on the next read | Close once at the end |
| `nextLine()` at end of input | `NoSuchElementException` | Check `hasNextLine()` |
| `\n` in `printf` | Wrong line endings on Windows | `%n` |
| `%d` with a `double` (or `%f` with an `int`) | `IllegalFormatConversionException` | Match specifier to type |
| Printing an array directly | `[I@...` | `Arrays.toString` |
| Errors printed on `System.out` | Cannot separate output from errors | `System.err` |
| `println` for debugging in production | Noise, no levels or timestamps | A logging framework |
| Assuming `System.console()` exists | `NullPointerException` in the IDE | Fall back to `Scanner` |

## Key takeaways

- `System.out` for output, `System.err` for errors, `System.in` for input
- `printf` and `%n` give controlled formatting; pass a `Locale` when output must be fixed
- `Scanner` is easy but has the `nextInt`/`nextLine` newline trap; `BufferedReader` is faster
- Validate input; user data is untrusted
- `args` carries command-line arguments; exit codes tell scripts whether you succeeded

**Next:** [02-methods](../02-methods/README.md)
