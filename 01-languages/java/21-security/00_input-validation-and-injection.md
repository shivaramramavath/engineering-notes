# Input Validation and Injection Defense in Java

> Comprehensive defense-in-depth strategies against injection attacks (SQL, Command, LDAP, XPath, Log Injection) and malicious user input parsing in modern Java applications.

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced / Enterprise Security |
| **Prerequisites** | [02_statements-and-prepared-statements](../16-jdbc-and-databases/02_statements-and-prepared-statements.md), [03_strings](../03-strings-and-text/00_strings-deep-dive.md), [06_exceptions](../06-exceptions-and-debugging/00_exceptions-overview.md) |
| **Time to Read** | 35 minutes |
| **Status** | Verified Production Guide |
| **Java Baseline** | Java 17 LTS / Java 21 LTS / Java 25 LTS |

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
10. [Interview Questions & Answers](#10-interview-questions--answers)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### What is Input Validation?
Input validation is the programmatic enforcement of data integrity, structure, range, and format constraints on every piece of untrusted data entering a software boundary before that data is processed or forwarded to internal systems.

Untrusted inputs originate from:
- HTTP query parameters, path variables, request bodies (JSON, XML, form data), and HTTP headers (`User-Agent`, `Referer`, `Host`, custom headers).
- Network sockets, gRPC messages, WebSocket frames.
- Message queues (Kafka topics, RabbitMQ queues, AWS SQS) where external actors might produce messages.
- Files uploaded by users (filenames, MIME types, compressed archives, binary headers).
- Database fields populated by third-party systems or external integrations.

### What is Injection?
An injection attack occurs when untrusted user input is concatenated directly into a query, command, interpreter stream, or markup language, causing the execution engine (SQL parser, operating system shell, LDAP directory, XML parser, or logging framework) to interpret user data as executable code or structural directives.

OWASP ranks Injection continuously among the highest-impact security vulnerabilities (A03:2021-Injection). In Java applications, injection risks span:
1. **SQL Injection (SQLi)**: Manipulating relational database queries.
2. **Command Injection (OS Command)**: Injecting shell metacharacters into `ProcessBuilder` or `Runtime.getRuntime().exec()`.
3. **Log Injection (CRLF / Log Forging)**: Injecting carriage return and line feed characters (`\r\n`) into application log files to forge fake log entries or blind SIEM/SOC analysts.
4. **LDAP Injection**: Injecting filter operators (`*`, `&`, `|`) into JNDI or Spring LDAP search queries.
5. **Path Traversal / Directory Climbing**: Injecting `../` sequences into file path resolution logic to read or overwrite arbitrary filesystem files.

---

## 2. Core Concepts

### Allow-list vs. Deny-list Validation
- **Deny-listing (Blocklisting)**: Searching for known malicious characters or words (e.g., `'`, `--`, `UNION`, `<script>`, `; rm -rf`).
  - *Why it fails*: Attackers easily bypass deny-lists using character encoding variations (URL encoding, double encoding, Unicode homoglyphs, hex encoding, alternative SQL syntax).
- **Allow-listing (Safelisting)**: Defining the strict grammatical structure of permissible input (e.g., regex `^[a-zA-Z0-9_-]{3,32}$`, exact enum value matching, integer range checks). Anything that does not explicitly match is rejected immediately. Always prefer allow-listing.

### Syntax vs. Semantic Validation
- **Syntactic Validation**: Ensuring data conforms to the expected format, type, and size:
  - Is it a valid 64-bit integer?
  - Does the string conform to ISO-8601 date format (`yyyy-MM-dd`)?
  - Is the email length under 254 characters and matching the standard pattern?
- **Semantic (Business Logic) Validation**: Verifying that the syntactically valid data makes sense in the current application state:
  - Is `endDate` chronologically after `startDate`?
  - Does the requesting `userId` actually own the target `accountId`?
  - Is the withdrawal amount less than or equal to the account balance?

### Defense-in-Depth for Injection
Validation alone is not sufficient to prevent injection. The primary defense against injection is **parameterization** (separating code from data at the interpreter level). Validation acts as a secondary layer that prevents unexpected data formats from reaching backend subsystems.

---

## 3. How It Works Internally

### How SQL Injection Bypasses Parsers
When an unparameterized SQL statement is built via string concatenation, the database parser receives a single monolithic string:

```
User Input: "105 OR 1=1; --"
Application: "SELECT * FROM orders WHERE user_id = " + input;
Parser View:
  SELECT * FROM orders WHERE user_id = 105 OR 1=1; --
                                       ^^^^^^^^^^
                                  Evaluates to TRUE for all rows!
```

When using a `PreparedStatement`, the JDBC driver sends the query template with parameter placeholders (`?`) to the database engine first. The database compiles the Abstract Syntax Tree (AST) once:

```
Database AST:
       SELECT
      /      \
   Columns    WHERE
     [*]        |
              EQUALS
             /      \
        user_id      ? (Bind Variable 1)
```

The database engine then binds the user value into parameter slot 1 as raw literal data. Even if the user passes `105 OR 1=1; --`, the engine treats that exact string as a literal value for `user_id`. It cannot mutate the compiled syntax tree.

```
+-------------------------------------------------------------------------+
|                  UNPARAMETERIZED STRING CONCATENATION                   |
| User Data --------> String Concat --------> DB Parser --------> AST     |
| (Malicious data changes the AST structure and injects operators)        |
+-------------------------------------------------------------------------+
|                    PREPARED STATEMENT (PARAMETERIZED)                   |
| Query Template ---> DB Parser ----> Compiled AST (Immutable Structure) |
| User Data ------------------------> Value Slot Bindings (Raw Data Only) |
+-------------------------------------------------------------------------+
```

### OS Process Execution Architecture
In Java, executing operating system commands through `Runtime.getRuntime().exec(String command)` passes the entire string to a platform-dependent shell tokenizer. If an application executes:
```java
Runtime.getRuntime().exec("ping -c 3 " + host);
```
And `host` is `127.0.0.1; cat /etc/passwd`, the shell evaluates `;` as a command separator, executing both commands.

By using `ProcessBuilder(List<String> command)`, Java passes the command and its arguments as discrete OS-level argument arrays via the native `CreateProcess` (Windows) or `execve` (Linux/POSIX) system calls:
```
Linux: execve("/bin/ping", ["ping", "-c", "3", "127.0.0.1; cat /etc/passwd"], envp);
```
The OS kernel receives the argument string as a single literal argument passed directly to `ping`. `ping` fails with an invalid host error; the shell is never invoked, and `;` has zero syntactic significance.

---

## 4. Syntax & Usage

### 1. Bean Validation (Jakarta Validation API)
In modern Java, standard validation annotations provide declarative syntactic constraints on data transfer objects (records or classes):

```java
import jakarta.validation.constraints.*;
import java.math.BigDecimal;

public record CreateAccountRequest(
    @NotBlank(message = "Username cannot be blank")
    @Size(min = 3, max = 32, message = "Username must be between 3 and 32 characters")
    @Pattern(regexp = "^[a-zA-Z0-9_-]+$", message = "Username contains illegal characters")
    String username,

    @NotBlank
    @Email(message = "Invalid email format")
    String email,

    @NotNull
    @DecimalMin(value = "0.01", message = "Initial deposit must be positive")
    @Digits(integer = 8, fraction = 2, message = "Deposit precision exceeds 2 decimal places")
    BigDecimal initialDeposit
) {}
```

### 2. Manual Programmatic Validation with Fail-Fast Objects
For high-security domains or libraries without heavy Jakarta validation frameworks, immutable value objects enforce invariants inside constructors:

```java
public record SafeIdentifier(String value) {
    private static final java.util.regex.Pattern SAFE_ID_PATTERN = 
        java.util.regex.Pattern.compile("^[A-Z0-9]{8,16}$");

    public SafeIdentifier {
        java.util.Objects.requireNonNull(value, "Identifier cannot be null");
        if (!SAFE_ID_PATTERN.matcher(value).matches()) {
            throw new IllegalArgumentException("Malformed identifier format: " + value);
        }
    }
}
```

---

## 5. Practical Examples

### Example 1: Basic Input Sanitizer vs. Canonicalizer (Path Traversal Defense)
**Goal:** Prevent directory traversal attacks when reading files requested by a client.

```java
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;

public class SafeFileReader {
    private final Path baseStorageDirectory;

    public SafeFileReader(Path baseStorageDirectory) {
        this.baseStorageDirectory = baseStorageDirectory.toAbsolutePath().normalize();
    }

    public byte[] readFile(String userSuppliedFilename) throws IOException {
        // Step 1: Reject null or empty inputs
        if (userSuppliedFilename == null || userSuppliedFilename.isBlank()) {
            throw new IllegalArgumentException("Filename cannot be empty");
        }

        // Step 2: Prevent null-byte injection attacks (C-string termination bugs)
        if (userSuppliedFilename.contains(" ")) {
            throw new SecurityException("Null byte detected in path parameter");
        }

        // Step 3: Resolve against the base storage root
        Path resolvedPath = baseStorageDirectory.resolve(userSuppliedFilename).normalize();

        // Step 4: Canonical validation check - must stay within base directory
        if (!resolvedPath.startsWith(baseStorageDirectory)) {
            throw new SecurityException("Access denied: Path traversal detected outside root directory: " 
                + userSuppliedFilename);
        }

        // Step 5: Read file safely
        if (!Files.exists(resolvedPath) || !Files.isRegularFile(resolvedPath)) {
            throw new IllegalArgumentException("Requested resource does not exist");
        }

        return Files.readAllBytes(resolvedPath);
    }
}
```

### Example 2: Real-World Safe OS Command Execution
**Goal:** Run a system diagnostic command without shell injection vulnerabilities.

```java
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import java.util.regex.Pattern;

public class SafeDiagnosticRunner {
    private static final Pattern HOSTNAME_REGEX = 
        Pattern.compile("^[a-zA-Z0-9]([a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?(\.[a-zA-Z0-9]([a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?)*$");

    public static List<String> pingHost(String targetHost, int packetCount) throws Exception {
        // Syntactic allow-list validation
        if (targetHost == null || !HOSTNAME_REGEX.matcher(targetHost).matches()) {
            throw new IllegalArgumentException("Invalid host syntax: " + targetHost);
        }
        if (packetCount < 1 || packetCount > 5) {
            throw new IllegalArgumentException("Packet count must be between 1 and 5");
        }

        // Determine OS executable
        String os = System.getProperty("os.name").toLowerCase();
        List<String> command = new ArrayList<>();
        if (os.contains("win")) {
            command.addAll(List.of("ping", "-n", String.valueOf(packetCount), targetHost));
        } else {
            command.addAll(List.of("ping", "-c", String.valueOf(packetCount), targetHost));
        }

        // ProcessBuilder does NOT invoke a shell interpreter (/bin/sh or cmd.exe)
        ProcessBuilder processBuilder = new ProcessBuilder(command);
        processBuilder.redirectErrorStream(true);
        Process process = processBuilder.start();

        List<String> output = new ArrayList<>();
        try (BufferedReader reader = new BufferedReader(
                new InputStreamReader(process.getInputStream(), StandardCharsets.UTF_8))) {
            String line;
            while ((line = reader.readLine()) != null) {
                output.add(line);
            }
        }

        int exitCode = process.waitFor();
        if (exitCode != 0) {
            throw new RuntimeException("Ping command failed with exit code: " + exitCode);
        }
        return output;
    }
}
```

### Example 3: Edge Case — Log Forging & CRLF Neutralization
**Goal:** Prevent attackers from injecting fake logs, breaking CSV logs, or spoofing audit trails.

```java
public class SafeLoggerUtil {
    /**
     * Sanitizes untrusted user strings before printing to loggers.
     * Replaces carriage return () and newline (
) characters to prevent CRLF injection.
     */
    public static String sanitizeForLog(String input) {
        if (input == null) {
            return "<null>";
        }
        // Replace CRLF and tab characters with escaped strings
        return input.replace("", "\r")
                    .replace("
", "\n")
                    .replace("	", "\t");
    }

    public static void logUserAction(org.slf4j.Logger logger, String username, String action) {
        // Even with structured logging, clean raw strings
        String safeUser = sanitizeForLog(username);
        String safeAction = sanitizeForLog(action);
        logger.info("Audit: user='{}' initiated action='{}'", safeUser, safeAction);
    }
}
```

### Example 4: Failure Case — Vulnerable Dynamic SQL vs. Secure Fix
```java
// VULNERABLE CODE - NEVER DO THIS:
public User findVulnerable(Connection conn, String username) throws SQLException {
    String sql = "SELECT * FROM users WHERE username = '" + username + "'";
    try (Statement stmt = conn.createStatement();
         ResultSet rs = stmt.executeQuery(sql)) { // CRITICAL SQL INJECTION VULNERABILITY
        if (rs.next()) return mapUser(rs);
        return null;
    }
}

// SECURE CODE - PARAMETERIZED PREPARED STATEMENT:
public User findSecure(Connection conn, String username) throws SQLException {
    String sql = "SELECT id, username, email FROM users WHERE username = ?";
    try (PreparedStatement pstmt = conn.prepareStatement(sql)) {
        pstmt.setString(1, username); // Database binds parameter cleanly as literal
        try (ResultSet rs = pstmt.executeQuery()) {
            if (rs.next()) return mapUser(rs);
            return null;
        }
    }
}
```

---

## 6. Best Practices

1. **Always Use Allow-Lists**: Define exactly what is permitted (types, lengths, character sets, regex patterns). Discard anything that does not match.
2. **Canonicalize Before Validating**: Decode URLs, resolve file paths, and normalize Unicode (`java.text.Normalizer`) before running validation rules.
3. **Never Concatenate SQL or Shell Commands**: Use `PreparedStatement` with bind variables for databases. Use `ProcessBuilder` with explicit argument lists for OS commands.
4. **Sanitize Log Inputs**: Strip or escape `` and `
` from any untrusted data written to application logs to prevent log injection.
5. **Use Immutable Value Objects**: Enforce domain invariants in record constructors (`java.lang.Record`) so invalid objects cannot exist in memory.
6. **Limit Input Payload Sizes**: Configure web servers and API gateways to reject request bodies exceeding sensible limits (e.g., maximum 2MB for JSON payloads).

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Secure Fix |
| :--- | :--- | :--- |
| **Deny-listing SQL keywords (`SELECT`, `UNION`)** | Attackers bypass with comments (`UN/**/ION`), mixed case (`uNiOn`), or alternative SQL encodings. | Use parameterized queries (`PreparedStatement`) exclusively. |
| **Validating paths before normalization** | Attacker passes `safe_dir/../../etc/passwd`. Validation passes because it starts with `safe_dir`, but filesystem climbs out. | Call `Path.toAbsolutePath().normalize()` first, then check `startsWith(baseDir)`. |
| **Using `Runtime.exec(String)`** | OS shell tokenizes the string, treating `;`, `&&`, `|` as command separators. | Use `new ProcessBuilder(List<String>)` without invoking `/bin/sh` or `cmd.exe`. |
| **Trusting HTTP Headers** | Headers like `X-Forwarded-For`, `User-Agent`, and `Referer` can be spoofed by any HTTP client. | Treat all HTTP headers as completely untrusted user input; validate format. |
| **Ignoring Null Bytes in Paths** | In legacy C-backed file APIs, `file.txt .jpg` would trick Java validation into seeing `.jpg` while C opened `file.txt`. | Reject any path or string containing the null character ` `. |
| **Unescaped LDAP queries** | Characters like `*`, `(`, `)`, `\` alter LDAP search filter logic. | Use RFC 2254 encoding or `LdapEncoder.filterEncode()`. |

---

## 8. Performance & Security Notes

### Performance Overhead of Validation
- **Precompiled RegEx**: Never call `String.matches(regex)` inside high-frequency loops or request paths. `String.matches` compiles the regex `Pattern` on every single invocation. Always compile patterns once as `private static final Pattern`.
- **Validation Timing**: Run syntactic validation at the edge (API gateway or Controller layer) to fail fast before allocating database connections or downstream thread pool capacity.

### ReDoS (Regular Expression Denial of Service)
Poorly constructed regular expressions with nested quantifiers (e.g., `(a+)+$`, `([a-zA-Z0-9]+)*`) exhibit catastrophic backtracking ($O(2^n)$ time complexity) when given pathological non-matching strings.
- Test regex patterns with automated ReDoS checkers.
- Consider configuring Java 21+ regex timeout limits or using non-backtracking engines where available.

---

## 9. Comparison / When to Use What

| Input Category | Recommended Validation Technique | Primary Injection Defense |
| :--- | :--- | :--- |
| **Relational Database Queries** | Length limits, alphanumeric patterns | `PreparedStatement` bind parameters |
| **Operating System Commands** | Strict allow-list regex or fixed enum | `ProcessBuilder(List<String>)` (no shell) |
| **Filesystem Access** | Path normalization check (`startsWith`) | Chrooted directory or object storage (S3) |
| **Logging Outputs** | Replace ``, `
`, `	` with escapes | Structured JSON logging (Logstash/Logback) |
| **HTML / Web Responses** | HTML entity escaping (OWASP Encoder) | Context-aware templating + Content Security Policy (CSP) |
| **JSON / REST Payloads** | Jakarta Bean Validation annotations | Strict parser schema enforcement (Jackson) |

---

## 10. Interview Questions & Answers

### Q1: Why does `PreparedStatement` prevent SQL Injection while `Statement` with string concatenation does not?
<details>
<summary>View Answer</summary>
When a `PreparedStatement` is created, the SQL command template containing parameter placeholders (`?`) is transmitted to the database and compiled into an execution plan (AST) before any user data is bound. When parameters are subsequently supplied via `setString()` or `setInt()`, the database binds them directly into allocated memory slots in the execution plan as pure literal values. The database parser never re-analyzes the query text, meaning input containing SQL operators (`OR 1=1`, `--`, `UNION`) cannot alter the structural syntax tree of the compiled statement.
</details>

### Q2: How can an attacker exploit Log Injection (Log Forging), and how do you prevent it in Java?
<details>
<summary>View Answer</summary>
Log injection occurs when an application logs untrusted user input without sanitization. An attacker submits inputs containing carriage return and newline characters (`
`). When written to a plain text log file, these characters cause line breaks, allowing the attacker to fabricate fake log entries (e.g., forging `INFO: User admin authenticated successfully from IP 10.0.0.1`). This can mislead security analysts or poison log analysis dashboards. Prevention: replace or escape all CR and LF characters prior to logging, or adopt structured logging formats (like JSON) where raw values are safely escaped within JSON string fields.
</details>

### Q3: What is Path Traversal, and why is `path.contains("..")` an inadequate defense?
<details>
<summary>View Answer</summary>
Path traversal allows an attacker to access files outside the intended root directory by supplying relative path indicators (`../`). Checking `contains("..")` is inadequate because attackers can use URL-encoded variants (`%2e%2e%2f`), Unicode homoglyphs, or nested patterns that evade naive string filters. The proper defense is to resolve the target path against the allowed root, normalize it using `Path.normalize()`, and verify that `resolvedPath.toAbsolutePath().startsWith(baseDirectory.toAbsolutePath())`.
</details>

### Q4: Why is `Runtime.getRuntime().exec("sh -c " + command)` vulnerable even if passed as an array?
<details>
<summary>View Answer</summary>
Although the array form of `exec` prevents argument splitting at the OS level, invoking `/bin/sh -c` explicitly spins up an interactive shell interpreter. The shell interpreter evaluates the string argument, interpreting metacharacters like `;`, `&&`, `|`, and backticks. To execute OS commands safely, avoid invoking shell wrappers entirely; execute the binary directly via `new ProcessBuilder("ping", "-c", "3", host)`.
</details>

### Q5: What is ReDoS (Regular Expression Denial of Service), and how do you protect Java services against it?
<details>
<summary>View Answer</summary>
ReDoS occurs when a regular expression with catastrophic backtracking (often caused by nested quantifiers like `(a+)+` or overlapping alternatives) is executed against a carefully crafted malicious input. The regex engine evaluates an exponential number of permutations, spiking CPU usage to 100% and causing a denial of service. Defense: Avoid nested quantifiers, utilize possessive quantifiers (`++`, `*+`) or atomic groups `(?>...)` to prevent backtracking, benchmark regexes with pathological inputs, and enforce maximum input length before running regex checks.
</details>

### Q6: What is the difference between Syntactic and Semantic validation?
<details>
<summary>View Answer</summary>
Syntactic validation verifies that the data matches structural rules (e.g., data type, regex pattern, character length, date formatting). Semantic validation ensures that the syntactically valid data makes logical sense within the application's business rules and current state (e.g., checking that an order quantity does not exceed current warehouse inventory, or that a user has authorization to access the specific account ID provided).
</details>

### Q7: Why is allow-listing always preferred over deny-listing?
<details>
<summary>View Answer</summary>
Deny-listing requires anticipating every possible malicious payload, encoding permutation, and syntactic variation. Attackers constantly discover new bypasses (e.g., using SQL dialect-specific functions or obscure Unicode encodings). In contrast, allow-listing defines an explicit, minimal subset of valid inputs. Anything that deviates from the expected structure is rejected by default, providing robust protection against both known and zero-day injection payloads.
</details>

### Q8: How should large XML or JSON files be validated to prevent denial-of-service vulnerabilities like the Billion Laughs attack?
<details>
<summary>View Answer</summary>
For XML, disable external entity resolution (XXE) and external DTDs by configuring the `DocumentBuilderFactory` or `XMLInputFactory` with `XMLConstants.FEATURE_SECURE_PROCESSING`, `setFeature("http://apache.org/xml/features/disallow-doctype-decl", true)`. For JSON, enforce strict maximum nesting depth limits and maximum payload size limits at the parser configuration level (e.g., in Jackson via `StreamReadConstraints.builder().maxNestingDepth(...)`).
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a Secure Path Validator
**Task:** Implement a utility method `Path validateAndResolve(Path rootDir, String userSubPath)` that safely resolves a user-supplied relative path against `rootDir`. Reject path traversals, null bytes, and symlinks pointing outside the root.
<details>
<summary>View Solution</summary>

```java
import java.io.IOException;
import java.nio.file.*;

public class SecurePathValidator {
    public static Path validateAndResolve(Path rootDir, String userSubPath) throws IOException {
        if (rootDir == null || !Files.isDirectory(rootDir)) {
            throw new IllegalArgumentException("Invalid root directory");
        }
        if (userSubPath == null || userSubPath.isBlank()) {
            throw new IllegalArgumentException("Subpath cannot be empty");
        }
        if (userSubPath.contains(" ")) {
            throw new SecurityException("Null byte detected");
        }

        Path absoluteRoot = rootDir.toAbsolutePath().normalize();
        Path resolved = absoluteRoot.resolve(userSubPath).normalize();

        if (!resolved.startsWith(absoluteRoot)) {
            throw new SecurityException("Traversal attempt detected: " + userSubPath);
        }

        // Verify symlink targets do not escape root
        if (Files.isSymbolicLink(resolved)) {
            Path realTarget = resolved.toRealPath();
            if (!realTarget.startsWith(absoluteRoot)) {
                throw new SecurityException("Symlink points outside root: " + userSubPath);
            }
        }

        return resolved;
    }
}
```
</details>

### Exercise 2: Safe Dynamic SQL Query Builder with Whitelisted Columns
**Task:** Often applications need dynamic `ORDER BY` clauses which cannot use `?` bind variables in standard SQL. Implement a secure method that allows users to sort by permitted columns (`name`, `created_at`, `price`) and direction (`ASC`, `DESC`) without SQL injection.
<details>
<summary>View Solution</summary>

```java
import java.sql.*;
import java.util.Map;
import java.util.Set;

public class DynamicSortService {
    private static final Set<String> ALLOWED_COLUMNS = Set.of("name", "created_at", "price");
    private static final Set<String> ALLOWED_DIRECTIONS = Set.of("ASC", "DESC");

    public static PreparedStatement buildSortedQuery(Connection conn, String column, String direction) 
            throws SQLException {
        String cleanColumn = column.trim().toLowerCase();
        String cleanDirection = direction.trim().toUpperCase();

        if (!ALLOWED_COLUMNS.contains(cleanColumn)) {
            throw new IllegalArgumentException("Disallowed sort column: " + column);
        }
        if (!ALLOWED_DIRECTIONS.contains(cleanDirection)) {
            throw new IllegalArgumentException("Disallowed sort direction: " + direction);
        }

        // Safe to interpolate because cleanColumn and cleanDirection are strictly whitelisted
        String sql = "SELECT id, name, created_at, price FROM products ORDER BY " 
                   + cleanColumn + " " + cleanDirection;
        return conn.prepareStatement(sql);
    }
}
```
</details>

### Exercise 3: CRLF and HTML Sanitizer for Web Responses
**Task:** Write a fast, zero-allocation character-level scanner that validates user-supplied text for header responses (preventing HTTP Response Splitting).
<details>
<summary>View Solution</summary>

```java
public class HeaderSanitizer {
    public static String sanitizeHeaderValue(String value) {
        if (value == null) return "";
        for (int i = 0; i < value.length(); i++) {
            char c = value.charAt(i);
            if (c == '\r' || c == '\n') {
                throw new IllegalArgumentException("CRLF character detected in HTTP header value");
            }
        }
        return value;
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **SQL Injection**: Use `PreparedStatement` with `?` bind parameters for all data values. For dynamic table/column names, use an explicit `Set<String>` allow-list.
- **Command Injection**: Use `new ProcessBuilder(List<String> args)`. Never execute commands through `sh -c` or `cmd.exe /c`.
- **Path Traversal**: Always normalize paths using `Path.normalize()` and verify `path.startsWith(baseDir)`. Check symlinks with `toRealPath()`.
- **Log Injection**: Escape or strip `` and `
` characters from all untrusted strings before sending them to logging frameworks.
- **Bean Validation**: Annotate domain records and DTOs with `@NotNull`, `@Size`, `@Pattern`, `@Email`. Validate early at controller endpoints.
- **ReDoS Defense**: Compile regex once (`static final Pattern`), avoid nested quantifiers, and test against catastrophic backtracking.

---

## 13. Further Reading
- OWASP Top 10: [A03:2021 - Injection](https://owasp.org/Top10/A03_2021-Injection/)
- OWASP Cheat Sheet Series: [Input Validation Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Input_Validation_Cheat_Sheet.html)
- Jakarta Bean Validation Specification: [Jakarta Bean Validation 3.0](https://beanvalidation.org/)
- Java SE Security Guide: [Secure Coding Guidelines for Java SE](https://www.oracle.com/java/technologies/javase/seccodeguide.html)
