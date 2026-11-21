# Console Input, Output, and Formatted Text in Java

> Java communicates with standard terminal input and output streams through `System.in`, `System.out`, and `System.err`, supporting formatted stream interpolation via `printf`, token scanning via `Scanner`, high-throughput buffered reading, and secure masked password entry via `Console`.

| Metadata | Value |
| :--- | :--- |
| **Level** | Beginner to Intermediate |
| **Prerequisites** | [01_variables-and-data-types.md](file:///E:/Learning/engineering-notes/01-languages/java/01-fundamentals/01_variables-and-data-types.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Java LTS Baseline** | Java 25 LTS (with `java.lang.IO` and UTF-8 default charset) |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance & Security Notes](#8-performance--security-notes)
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### Problem Statement
Command-line applications, microservices, and debugging tools must communicate with the operating system environment. They must emit standard log messages, report error diagnostics, read user keystrokes, and pipe binary data between processes. Without standardized abstractions for standard input (stdin), standard output (stdout), and standard error (stderr), writing portable CLI tools would require OS-specific system calls.

### Historical Context & Design Principles
Java integrated the standard POSIX I/O model directly into `java.lang.System`:
- `System.out` (`PrintStream`): For normal program output and user interaction.
- `System.err` (`PrintStream`): For diagnostics, warnings, and error traces (unbuffered by default in many environments).
- `System.in` (`InputStream`): For reading incoming byte sequences.
- Over time, Java added rich abstractions: `java.util.Scanner` (Java 5) for regex-based token parsing, `java.io.Console` (Java 6) for password masking, UTF-8 as the universal default charset (Java 18 JEP 400), and `java.lang.IO` (Java 25 JEP 512) for zero-ceremony interactive I/O.

### Real-World Relevance
Understanding console streams and formatting is essential for writing production CLI tools, managing container stdout/stderr logging in Docker and Kubernetes, and avoiding common performance bottlenecks when writing high-volume diagnostic logs.

---

## 2. Core Concepts

### The Three Standard Streams

```
                       ┌────────────────────────────────────────────────────────┐
                       │                   Operating System                     │
                       └───────────────────────────┬────────────────────────────┘
                                                   │
                ┌──────────────────────────────────┼──────────────────────────────────┐
                ▼                                  ▼                                  ▼
      [ Standard Input ]                 [ Standard Output ]                [ Standard Error ]
        File Descriptor 0                  File Descriptor 1                  File Descriptor 2
          (System.in)                        (System.out)                       (System.err)
               │                                  ▲                                  ▲
               ▼                                  │                                  │
    [ InputStream (bytes) ]             [ PrintStream (text) ]             [ PrintStream (text) ]
               │                                  │                                  │
               ▼                                  │                                  │
      Scanner / BufferedReader                    └──────────────────────────────────┘
```

### Formatted Printing (`printf` and `String.format`)
Java provides `printf` (derived from C's `printf`), controlled by format specifiers:
`%[flags][width][.precision]conversion-character`

| Specifier | Data Type | Example Input | Formatted Output |
| :--- | :--- | :--- | :--- |
| `%s` | String / Object | `"Java"` | `Java` |
| `%d` | Decimal Integer | `42` | `42` |
| `%f` | Floating-Point | `3.14159` (`%.2f`) | `3.14` |
| `%b` | Boolean | `true` | `true` |
| `%x` / `%X` | Hexadecimal | `255` | `ff` / `FF` |
| `%n` | Platform Newline | N/A | `\r\n` (Win) or `\n` (POSIX) |

### Reading User Input: The 4 Primary Mechanisms
1. **`java.util.Scanner`:** Parses primitive types and strings using regular expressions. Simple for beginners, but slower for massive I/O.
2. **`java.io.BufferedReader`:** Wraps `new InputStreamReader(System.in)` to buffer incoming bytes into characters. High-throughput line-by-line reading.
3. **`java.io.Console`:** Direct terminal interface. Provides `readPassword()` which disables character echo to protect sensitive credentials.
4. **`java.lang.IO` (Java 25 JEP 512):** Directly exposes `IO.println()` and `IO.readln()` in compact source files.

---

## 3. How It Works Internally

### Standard Stream Initialization & Thread Synchronization
During JVM bootstrap (`System.initPhase1()`), the runtime creates the standard streams:
1. Native file descriptors 0, 1, and 2 are bound to `FileInputStream` and `FileOutputStream` instances.
2. `System.out` and `System.err` are wrapped in `PrintStream` instances.
3. **Thread Safety & Contention:** `PrintStream` methods (`println`, `print`, `write`) are internally synchronized on the `PrintStream` instance monitor lock. High-frequency logging from multiple virtual threads directly to `System.out` can cause lock contention and thread carrier pinning!

### Character Decoding (UTF-8 Standard Baseline)
Since Java 18 (JEP 400), the default charset of the Java runtime is **UTF-8**. When reading from `System.in` or writing to `System.out`, bytes are translated to and from UTF-16 in-memory `char` sequences using the UTF-8 charset unless overridden by `-Dfile.encoding`.

---

## 4. Syntax & Usage

### Basic Printing Commands
```java
System.out.print("No line break. ");
System.out.println("Appends line break.");
System.out.printf("Formatted integer: %05d%n", 42); // 00042
```

### Formatting Flags and Width Controls
```java
// Right-aligned in 10-character field
System.out.printf("|%10s|%n", "test");   // |      test|

// Left-aligned in 10-character field (flag '-')
System.out.printf("|%-10s|%n", "test");  // |test      |

// Thousands separator comma (flag ',')
System.out.printf("Count: %,d%n", 1_000_000); // Count: 1,000,000

// Zero-padded numbers (flag '0')
System.out.printf("ID: %06d%n", 123);   // ID: 000123
```

---

## 5. Practical Examples

### Example 1: Formatted Table Output Generation
- **Goal:** Render a clean tabular dashboard to the console with aligned columns, currency formatting, and numeric padding.
```java
public class FormattedTableDemo {
    public static void main(String[] args) {
        String format = "| %-12s | %-8s | %8s | %10s |%n";
        String line = "+--------------+----------+----------+------------+";

        System.out.println(line);
        System.out.printf(format, "Server Name", "Status", "Threads", "Memory (MB)");
        System.out.println(line);
        System.out.printf(format, "auth-node-1", "ACTIVE", 48, String.format("%,.1f", 1245.5));
        System.out.printf(format, "gateway-lb", "STANDBY", 16, String.format("%,.1f", 512.0));
        System.out.printf(format, "db-worker-3", "ACTIVE", 128, String.format("%,.1f", 4096.2));
        System.out.println(line);
    }
}
```
- **Console Output:**
```text
+--------------+----------+----------+------------+
| Server Name  | Status   |  Threads | Memory (MB) |
+--------------+----------+----------+------------+
| auth-node-1  | ACTIVE   |       48 |    1,245.5 |
| gateway-lb   | STANDBY  |       16 |      512.0 |
| db-worker-3  | ACTIVE   |      128 |    4,096.2 |
+--------------+----------+----------+------------+
```
- **Explanation:** Using width specifiers (`%-12s`, `%8s`) ensures perfect horizontal alignment regardless of text length.

---

### Example 2: The `Scanner.nextInt()` Followed by `nextLine()` Trap
- **Goal:** Demonstrate the classic bug where `nextLine()` skips input after `nextInt()`, and provide the clean architectural remedy.
```java
import java.util.Scanner;

public class ScannerTrapDemo {
    public static void main(String[] args) {
        String inputData = "42\nJohn Doe\n";
        Scanner scanner = new Scanner(inputData);

        // Read integer age
        int age = scanner.nextInt();

        // THE TRAP: nextInt() consumes '42' but leaves the trailing newline '\n' in buffer!
        // Calling scanner.nextLine() immediately consumes that empty newline!
        scanner.nextLine(); // CONSUME RESIDUAL NEWLINE TO AVOID BUG!

        String name = scanner.nextLine();
        System.out.printf("Parsed: age=%d, name='%s'%n", age, name);
    }
}
```
- **Console Output:**
```text
Parsed: age=42, name='John Doe'
```
- **Explanation:** `nextInt()`, `nextDouble()`, and `next()` parse tokens and stop before whitespace. The user's trailing Enter key (`\n`) remains in the buffer. An explicit `scanner.nextLine()` is required to clear the newline before reading subsequent strings.

---

### Example 3: Edge Case — Stream Interleaving (`out` vs. `err`)
- **Goal:** Demonstrate why mixing `System.out` and `System.err` prints in jumbled order in console logs.
```java
public class StreamInterleavingDemo {
    public static void main(String[] args) {
        for (int i = 0; i < 3; i++) {
            System.out.println("NORMAL OUT " + i);
            System.err.println("ERROR ERR  " + i);
        }
    }
}
```
- **Console Output (Sample non-deterministic order):**
```text
NORMAL OUT 0
ERROR ERR  0
ERROR ERR  1
NORMAL OUT 1
NORMAL OUT 2
ERROR ERR  2
```
- **Explanation:** `System.out` and `System.err` are two distinct native OS streams with independent buffers. The operating system kernel schedules writes asynchronously. To avoid jumbled logs, production applications use logging frameworks (SLF4J, Logback) that route all events through a single unified handler.

---

### Example 4: Failure Case — `System.console()` Null Pointer Crash
- **Goal:** Demonstrate why `System.console()` returns `null` in IDEs or when piped in bash, and show safe fallback handling.
```java
import java.io.Console;
import java.io.BufferedReader;
import java.io.InputStreamReader;

public class ConsoleNullCheckDemo {
    public static void main(String[] args) throws Exception {
        Console console = System.console();

        if (console != null) {
            // Safe to interact with terminal
            String user = console.readLine("Username: ");
            char[] password = console.readPassword("Password: ");
            System.out.println("Authenticated: " + user);
            java.util.Arrays.fill(password, ' '); // Wipe sensitive password from RAM!
        } else {
            // Fallback when executed inside IDE terminal or piped: `cat input | java App`
            System.out.println("Notice: No direct interactive TTY attached. Falling back to stdin reader.");
            BufferedReader reader = new BufferedReader(new InputStreamReader(System.in));
            System.out.print("Username: ");
            String user = reader.readLine();
            System.out.println("Received: " + user);
        }
    }
}
```
- **Console Output (When run without attached TTY):**
```text
Notice: No direct interactive TTY attached. Falling back to stdin reader.
Username: admin
Received: admin
```
- **Explanation:** If a program's standard input or output is redirected (e.g., in a CI/CD pipeline or Docker container), no interactive TTY exists and `System.console()` returns `null`. Calling methods on it without a null check causes `NullPointerException`.

---

### Example 5: High-Performance / Modern Idiom — High-Throughput Buffered Token Reading
- **Goal:** Read 1,000,000 numeric tokens from an input stream up to 5x faster than `java.util.Scanner`.
```java
import java.io.BufferedReader;
import java.io.StringReader;
import java.util.StringTokenizer;

public class FastReaderDemo {
    static class FastScanner {
        private final BufferedReader reader;
        private StringTokenizer tokenizer;

        public FastScanner(Readable readable) {
            this.reader = new BufferedReader((java.io.Reader) readable);
        }

        public String next() throws Exception {
            while (tokenizer == null || !tokenizer.hasMoreTokens()) {
                String line = reader.readLine();
                if (line == null) return null;
                tokenizer = new StringTokenizer(line);
            }
            return tokenizer.nextToken();
        }

        public int nextInt() throws Exception {
            return Integer.parseInt(next());
        }
    }

    public static void main(String[] args) throws Exception {
        String testData = "10 20 30 40 50\n60 70 80 90 100\n";
        FastScanner scanner = new FastScanner(new StringReader(testData));

        int sum = 0;
        String token;
        while ((token = scanner.next()) != null) {
            sum += Integer.parseInt(token);
        }
        System.out.println("Fast Tokenizer Sum: " + sum);
    }
}
```
- **Console Output:**
```text
Fast Tokenizer Sum: 550
```
- **Explanation:** `java.util.Scanner` compiles regex patterns on every token inspection, creating significant CPU overhead. Wrapping `BufferedReader` with `StringTokenizer` avoids regex and yields maximum parsing throughput for competitive programming and big data ingests.

---

## 6. Best Practices

1. **Always Use `%n` in `printf` and `String.format`:** Never hardcode `\n` in format strings. `%n` outputs the platform-specific line separator (`\r\n` on Windows, `\n` on Linux/macOS).
2. **Never Leave Passwords in `String` Objects:** When prompting for passwords, use `System.console().readPassword()` which returns a `char[]`. Strings are immutable and remain in the heap memory pool until garbage collected; `char[]` arrays can be overwritten with zeros (`Arrays.fill(pwd, ' ')`) immediately after authentication.
3. **Always Check `System.console() != null`:** Provide a fallback using `BufferedReader` for environments where no physical TTY is attached.
4. **Clear Residual Newlines After `nextInt()` / `nextDouble()`:** Call `scanner.nextLine()` immediately after reading primitives before attempting to read string lines.
5. **Use Logging Frameworks Over `System.out.println`:** For enterprise applications, use SLF4J with Logback/Log4j2 to enable asynchronous writing, log level filtering (DEBUG, INFO, ERROR), and structured JSON logging.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Skipped Input After `nextInt()`** | `nextInt()` leaves `\n` in the buffer; subsequent `nextLine()` returns empty string. | Call `scanner.nextLine()` to consume the trailing newline. |
| **`NullPointerException` on `System.console()`** | `System.console()` returns `null` in IDE runners, Gradle, and background daemons. | Always check `if (console != null)` before invocation. |
| **Hardcoding `\n` in Format Strings** | Produces POSIX linebreaks on Windows, causing file corruption in Windows Notepad. | Use `%n` in all format strings. |
| **Calling `System.in.close()`** | Closes file descriptor 0 globally for the entire JVM process; cannot be reopened! | Never close `System.in` or standard streams. |
| **Unsynchronized `System.out` Bottlenecks** | `PrintStream` synchronizes on its monitor, causing thread contention under heavy load. | Use asynchronous loggers in multi-threaded systems. |
| **Mismatched Specifiers in `printf`** | Passing a `String` to `%d` throws runtime `IllegalFormatConversionException`. | Ensure argument types match format specifiers. |

---

## 8. Performance & Security Notes

### Virtual Thread Pinning Risk
In Java 21+ / 25, invoking `System.out.println()` inside a high-frequency virtual thread synchronizes on the internal `PrintStream` monitor. In certain legacy JVM scenarios, synchronizing on a monitor lock while executing blocking I/O can pin the virtual thread to its underlying OS carrier thread, reducing scheduler elasticity. Always delegate high-volume logging to asynchronous, non-blocking logging frameworks.

### Zero-Allocation String Formatting
`String.format()` compiles format strings dynamically on every invocation. For high-frequency performance-critical code paths, avoid repeated `String.format()` calls inside tight loops. Use `StringBuilder` concatenation, which compiles into optimized `invokedynamic` bytecodes via `StringConcatFactory`.

---

## 9. Comparison / When to Use What

| Tool / Class | Input / Output | Parsing Method | Speed / Performance | Ideal Scenario |
| :--- | :--- | :--- | :--- | :--- |
| **`java.util.Scanner`** | Input | Regex-based tokenization | Slow (Regex overhead) | Small interactive console utilities |
| **`BufferedReader`** | Input | Line-based buffer | **Fast** (Minimal allocation) | High-volume file/console data ingestion |
| **`System.console()`** | I/O | Direct TTY character stream | Fast | Interactive terminal passwords (`readPassword`) |
| **`System.out.printf`** | Output | Formatted template | Moderate | CLI dashboards, formatted table outputs |
| **`java.lang.IO` (Java 25)** | I/O | Direct methods (`readln`, `println`) | Fast | Single-file source scripts, education |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why is `System.out.println()` thread-safe, and what is its performance implication?</strong></summary>

**Answer:**  
`System.out` is an instance of `PrintStream`. Internally, all write operations synchronize on the `PrintStream` object instance or its underlying character lock:
```java
public void println(String x) {
    synchronized (this) {
        print(x);
        newLine();
    }
}
```
This ensures that multiple concurrent threads printing messages do not interleave individual characters. However, under high concurrency, threads queue up on this monitor lock, converting multi-threaded execution into a serialized bottleneck.
</details>

<details>
<summary><strong>2. Why should passwords be stored in `char[]` rather than `String`?</strong></summary>

**Answer:**  
`String` objects are immutable and placed in the JVM heap (and potentially interned in the String Pool). Once created, a `String` cannot be modified or cleared; its bytes remain in heap memory until the garbage collector reclaims the memory, leaving sensitive credentials vulnerable to heap dump inspections and memory scanners. A `char[]` array is mutable: as soon as authentication completes, the developer can immediately overwrite the array with zeros (`Arrays.fill(pwd, '0')`), minimizing the window of vulnerability.
</details>

<details>
<summary><strong>3. What happens if you invoke `scanner.close()` on a Scanner wrapping `System.in`?</strong></summary>

**Answer:**  
Calling `scanner.close()` closes the underlying stream passed to its constructor. If the scanner wraps `System.in`, closing it permanently closes native File Descriptor 0 for the entire JVM process. Any subsequent attempt to read from `System.in` anywhere in the application will throw an `IOException: Stream closed`. Standard system streams should never be closed.
</details>

<details>
<summary><strong>4. Why does `System.console()` return `null` when running inside an IDE like IntelliJ or Eclipse?</strong></summary>

**Answer:**  
`System.console()` only returns a valid `Console` object if the operating system process has a physical, interactive TTY/terminal attached to standard input and output. IDE run windows launch Java applications as child processes with redirected pipes (stdin/stdout redirected to IDE GUI text panes), so no true interactive TTY exists. The JVM launcher detects this redirection and sets `System.console()` to `null`.
</details>

<details>
<summary><strong>5. What is the difference between `%d` and `%i` in Java's `printf`?</strong></summary>

**Answer:**  
In C, `%i` is supported as an alternative specifier for integers. In Java’s `java.util.Formatter` specification, `%i` is **not** supported and will throw `java.util.UnknownFormatConversionException: Conversion = 'i'`. Java strictly requires `%d` for decimal integer formatting.
</details>

<details>
<summary><strong>6. Why is `%n` preferred over `\n` in format strings?</strong></summary>

**Answer:**  
`\n` explicitly emits an ASCII linefeed (0x0A), which is the newline standard for POSIX operating systems (Linux, macOS). On Windows, the standard newline is a carriage return followed by a linefeed (`\r\n`, 0x0D 0x0A). `%n` is platform-independent: it queries the system property `line.separator` and emits the exact OS-specific newline sequence.
</details>

<details>
<summary><strong>7. How did Java 18 (JEP 400) impact console I/O across operating systems?</strong></summary>

**Answer:**  
Prior to Java 18, the default character encoding depended on the host operating system and regional locale (e.g., Windows typically defaulted to Windows-1252 in North America or Shift-JIS in Japan). This caused character encoding corruption (mojibake) when reading UTF-8 text on Windows. JEP 400 standardized the default charset across all platforms to **UTF-8**, ensuring predictable Unicode I/O regardless of host OS configuration.
</details>

<details>
<summary><strong>8. What is the difference between `System.out` and `System.err` at the operating system level?</strong></summary>

**Answer:**  
At the operating system level, `System.out` writes to file descriptor 1 (stdout), while `System.err` writes to file descriptor 2 (stderr). In shell environments, they can be redirected independently (e.g., `java App > output.log 2> error.log`). Furthermore, standard error streams are typically unbuffered at the OS level to ensure fatal panic messages and crash traces are flushed to the terminal immediately.
</details>

---

## 11. Practice Exercises

### Exercise 1: Formatted Currency Column (Easy)
- **Task:** Write a format string that prints a floating-point price right-aligned in a 10-character field, padded with spaces, with thousands separators and exactly two decimal places, followed by a platform-specific newline.
<details>
<summary>Solution</summary>

```java
public class FormatExercise {
    public static void main(String[] args) {
        double price = 12345.6;
        // %10.2f with comma flag: %,10.2f%n
        System.out.printf("Price: %,10.2f%n", price); // Price:  12,345.60
    }
}
```
</details>

### Exercise 2: Interactive Menu Loop with Scanner (Medium)
- **Task:** Implement an interactive terminal menu using `Scanner` that presents options (1: View Balance, 2: Deposit, 3: Exit), handles invalid non-integer inputs gracefully without crashing, and loops until the user selects Exit.
<details>
<summary>Solution</summary>

```java
import java.util.Scanner;

public class InteractiveMenu {
    public static void main(String[] args) {
        Scanner scanner = new Scanner(System.in);
        boolean running = true;

        while (running) {
            System.out.println("\n--- BANKING MENU ---");
            System.out.println("1. View Balance");
            System.out.println("2. Deposit");
            System.out.println("3. Exit");
            System.out.print("Select an option (1-3): ");

            if (!scanner.hasNextInt()) {
                System.out.println("ERROR: Please enter a valid integer.");
                scanner.nextLine(); // Clear invalid token
                continue;
            }

            int choice = scanner.nextInt();
            scanner.nextLine(); // Consume trailing newline

            switch (choice) {
                case 1 -> System.out.println("Current Balance: $1,000.00");
                case 2 -> System.out.println("Deposit processing unavailable in demo.");
                case 3 -> {
                    System.out.println("Exiting. Goodbye!");
                    running = false;
                }
                default -> System.out.println("Invalid selection. Choose 1, 2, or 3.");
            }
        }
    }
}
```
</details>

### Exercise 3: Secure Password Validator with Zeroing (Hard)
- **Task:** Write a console utility that attempts to read a password via `System.console().readPassword()`. Validate that the password meets complexity requirements (length $\ge 8$, contains uppercase, lowercase, digit), prints validation results, and immediately overwrites the `char[]` buffer with null bytes (`\0`). Provide a fallback if `System.console()` is null.
<details>
<summary>Solution</summary>

```java
import java.io.Console;
import java.util.Arrays;

public class SecurePasswordPrompt {
    public static boolean validateComplexity(char[] pwd) {
        if (pwd == null || pwd.length < 8) return false;
        boolean hasUpper = false, hasLower = false, hasDigit = false;
        for (char c : pwd) {
            if (Character.isUpperCase(c)) hasUpper = true;
            if (Character.isLowerCase(c)) hasLower = true;
            if (Character.isDigit(c)) hasDigit = true;
        }
        return hasUpper && hasLower && hasDigit;
    }

    public static void main(String[] args) {
        Console console = System.console();
        char[] password;

        if (console != null) {
            password = console.readPassword("Enter new password: ");
        } else {
            System.out.println("WARNING: No TTY; fallback to non-masked input.");
            java.util.Scanner s = new java.util.Scanner(System.in);
            password = s.nextLine().toCharArray();
        }

        try {
            boolean valid = validateComplexity(password);
            System.out.println("Password complexity valid: " + valid);
        } finally {
            // CRITICAL DEFENSE: Overwrite sensitive password memory immediately
            if (password != null) {
                Arrays.fill(password, '\0');
                System.out.println("Security: Password buffer zeroed out from memory.");
            }
        }
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Standard Streams:**
  - `System.in`: `InputStream` (raw bytes from stdin).
  - `System.out`: `PrintStream` (stdout; normal logs).
  - `System.err`: `PrintStream` (stderr; unbuffered error traces).
- **Format Specifiers:** `%s` = string, `%d` = integer, `%f` = float, `%,d` = comma thousands, `%05d` = zero-padded, `%-10s` = left-aligned, `%n` = OS newline.
- **Scanner Trap:** `nextInt()` leaves `\n` in buffer; call `scanner.nextLine()` to clear.
- **Console:** Use `System.console().readPassword()` for secure masked input; always check for `null`.
- **Performance:** Use `BufferedReader` with `StringTokenizer` for high-throughput stream ingestion.

---

## 13. Further Reading

- [Official Java SE 25 Formatter Syntax Guide](https://docs.oracle.com/en/java/javase/25/docs/api/java.base/java/util/Formatter.html)
- [JEP 400: UTF-8 by Default](https://openjdk.org/jeps/400)
- [JEP 512: Compact Source Files and Instance Main Methods (IO.println)](https://openjdk.org/jeps/512)

---

| [← Previous: 08_numeric-precision-and-math.md](file:///E:/Learning/engineering-notes/01-languages/java/01-fundamentals/08_numeric-precision-and-math.md) | [Section Index](file:///E:/Learning/engineering-notes/01-languages/java/01-fundamentals/README.md) | [Next Module: 02-methods →](file:///E:/Learning/engineering-notes/01-languages/java/02-methods/README.md) |
