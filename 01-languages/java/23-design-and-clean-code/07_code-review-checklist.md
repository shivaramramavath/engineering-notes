# Production Code Review Checklist for Java

> The authoritative engineering checklist for reviewing enterprise Java pull requests: correctness, thread safety, security, resource management, performance, and API design.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core Engineering / Technical Governance |
| **Prerequisites** | All Module 23 notes ([00](00_solid-principles.md) - [06](06_common-java-mistakes.md)) |
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

### What is an Engineering Code Review?
A code review is a peer evaluation process designed to verify that proposed code changes are correct, secure, maintainable, performant, and aligned with team architectural standards before being merged into production branches.

Code review is **not** about checking formatting or whitespace (which should be 100% automated via Spotless or Checkstyle); it is about:
- Verifying logical correctness and handling of edge cases.
- Ensuring thread safety, data consistency, and absence of race conditions.
- Catching security vulnerabilities (injection, insecure deserialization, credential leakage).
- Preventing memory leaks and resource exhaustion.

### The Value of a Standardized Review Checklist
Human memory is unreliable during high-pressure sprints. An authoritative, structured checklist ensures reviewers systematically examine all critical software dimensions—preventing high-severity production outages.

---

## 2. Core Concepts

### The Seven Pillars of Java Code Review
1. **Correctness & Contract**: Does the code fulfill the functional specification? Are edge cases (null, empty, negative, max values) handled?
2. **Security & Injection Defense**: Are all database queries parameterized? Are inputs allow-listed? Are secrets masked?
3. **Concurrency & Thread Safety**: Is mutable shared state protected? Are locks acquired in consistent order? Are virtual threads pinned?
4. **Resource Management**: Are sockets, streams, and database connections wrapped in `try-with-resources`?
5. **Memory & Performance**: Are collections sized appropriately? Is autoboxing avoided in hot paths? Are caches bounded?
6. **API Design & Cleanliness**: Are methods small and focused? Are parameters grouped into records? Are collections immutable?
7. **Test Quality**: Do tests test behavior or implementation details? Are assertions meaningful?

---

## 3. How It Works Internally

### The Peer Review Gate in CI/CD
```
[ Developer Pull Request ]
            |
            v
[ Automated Static Analysis & CI Gates ]
  - Spotless / Google Java Format (Formatting)
  - ErrorProne / SpotBugs (Bug Patterns)
  - OWASP Dependency-Check (CVEs)
  - JUnit 5 / JaCoCo (80%+ Code Coverage)
            |
            | (All Gates PASS)
            v
[ Peer Human Code Review (Checklist Applied) ]
  - Architecture, SOLID, Design Principles
  - Concurrency invariants & Deadlock review
  - Business edge cases & failure recovery
            |
            v
[ Merge to Main / CD Pipeline ]
```

---

## 4. Syntax & Usage

### Pull Request Review Matrix
Use this audit matrix when reviewing pull requests:

| Category | Verification Item | Pass Criteria |
| :--- | :--- | :--- |
| **Architecture** | Single Responsibility | Each class/service has one clear reason to change. |
| **Architecture** | Dependency Injection | Dependencies injected via constructors with `final` fields; no `@Autowired` field injection. |
| **Data Types** | Immutability | DTOs, events, and values modeled as Java records with `List.copyOf()`. |
| **Security** | SQL Queries | Zero string concatenation in SQL; 100% `PreparedStatement` bind parameters. |
| **Security** | Secret Handling | Passwords and keys handled as `char[]`; redacted in `toString()`. |
| **Concurrency** | Shared Mutable State | No unsynchronized mutation of shared collections; use `ConcurrentHashMap` or immutable state. |
| **Resources** | Resource Cleanup | Every `AutoCloseable` resource managed via `try-with-resources`. |
| **Collections** | Null Returns | Zero methods returning `null` for collections or arrays; returns `List.of()`. |
| **Logging** | SLF4J Formatting | Parameterized placeholders (`{}`) used; no string concatenation; MDC cleared. |
| **Testing** | Edge Case Coverage | Tests assert empty lists, null boundaries, boundary values, and exception paths. |

---

## 5. Practical Examples

### Example 1: Annotated Review of a Flawed Pull Request
**Goal:** Review an incoming pull request and identify critical flaws across categories.

**The Submitted Code (Under Review):**
```java
@Service
public class OrderService {
    @Autowired
    private DataSource dataSource; // FLAW 1: Field injection

    public List<Order> getOrders(String user, String status) throws Exception {
        // FLAW 2: String concatenation SQL injection!
        String sql = "SELECT * FROM orders WHERE user = '" + user + "' AND status = '" + status + "'";
        
        Connection conn = dataSource.getConnection(); // FLAW 3: Unclosed Connection leak!
        Statement stmt = conn.createStatement();
        ResultSet rs = stmt.executeQuery(sql);

        List<Order> list = new ArrayList<>();
        while (rs.next()) {
            // FLAW 4: Double constructor precision loss!
            BigDecimal amount = new BigDecimal(rs.getDouble("amount"));
            list.add(new Order(rs.getString("id"), amount));
        }

        if (list.isEmpty()) {
            return null; // FLAW 5: Returning null instead of empty list!
        }
        return list;
    }
}
```

**The Reviewer Feedback & Corrected Code:**
```java
// APPROVED REFACTORED CODE:
@Service
public final class OrderService {
    private final DataSource dataSource;

    // FIX 1: Constructor Injection
    public OrderService(DataSource dataSource) {
        this.dataSource = Objects.requireNonNull(dataSource);
    }

    public List<Order> getOrders(String user, String status) throws SQLException {
        // Validation preconditions
        if (user == null || user.isBlank()) throw new IllegalArgumentException("user required");
        if (status == null || status.isBlank()) throw new IllegalArgumentException("status required");

        // FIX 2: Parameterized query
        String sql = "SELECT id, amount FROM orders WHERE user = ? AND status = ?";

        // FIX 3: Try-with-resources cleans up Connection, PreparedStatement, and ResultSet!
        try (Connection conn = dataSource.getConnection();
             PreparedStatement pstmt = conn.prepareStatement(sql)) {
            
            pstmt.setString(1, user);
            pstmt.setString(2, status);

            try (ResultSet rs = pstmt.executeQuery()) {
                List<Order> list = new ArrayList<>();
                while (rs.next()) {
                    // FIX 4: Exact BigDecimal string representation
                    BigDecimal amount = rs.getBigDecimal("amount");
                    list.add(new Order(rs.getString("id"), amount));
                }
                // FIX 5: Returns unmodifiable list, NEVER null!
                return List.copyOf(list);
            }
        }
    }
}
```

---

### Example 2: Continuous Integration Pull Request Checklist Template (`PULL_REQUEST_TEMPLATE.md`)
**Goal:** Standardize developer self-review before asking for human peer review.

```markdown
## Description
Provide a concise summary of the business requirement and architectural solution.

## Type of Change
- [ ] Bug fix (non-breaking change which fixes an issue)
- [ ] New feature (non-breaking change which adds functionality)
- [ ] Breaking change (fix or feature that causes existing functionality to change)
- [ ] Refactoring (zero behavioral change)

## Author Pre-Review Checklist
- [ ] **Tests**: New unit tests added covering happy path and edge cases (80%+ branch coverage).
- [ ] **Immutability**: DTOs, events, and value objects use Java records with defensive copying.
- [ ] **Security**: All SQL statements use `PreparedStatement`; zero string concatenation.
- [ ] **Resources**: All I/O streams, database connections, and sockets wrapped in `try-with-resources`.
- [ ] **Null Safety**: Methods return empty collections (`List.of()`) or `Optional`, never `null`.
- [ ] **Logging**: Structured SLF4J `{}` placeholders used; no sensitive PII/secrets printed.
- [ ] **Performance**: Checked for N+1 queries, unindexed queries, and collection initial capacities.
- [ ] **Clean Code**: Methods $\le 20$ lines; parameters $\le 3$ (or grouped into records).
```

## 6. Best Practices

1. **Review in Chunks Under 400 Lines**: Reviewing PRs larger than 400 lines causes cognitive fatigue and leads to rubber-stamping.
2. **Automate the Mechanical Stuff**: Use Spotless and linters in CI so reviewers focus on logic, security, and architecture.
3. **Be Constructive and Specific**: Point out *why* an issue is problematic and provide a concrete snippet demonstrating the fix.
4. **Verify the Tests First**: Read the test cases before inspecting implementation code to understand the expected behavior and verify edge cases.
5. **Check for Backward Compatibility**: Verify that database migrations, API changes, and serialized models do not break running instances during rolling deploys.
6. **Look for What Is Missing**: The hardest bugs to catch are missing validations, missing timeouts, missing error handlers, and unclosed resources.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Secure Fix |
| :--- | :--- | :--- |
| **Reviewing formatting manually** | Wastes human reviewer time on trivial whitespace and imports. | Enforce automated formatters (`mvn spotless:check`) in CI. |
| **Rubber-stamping large PRs** | Huge PRs hide security vulnerabilities and architectural regressions. | Require breaking large features into small incremental PRs. |
| **Focusing only on the happy path** | Code breaks in production when networks disconnect or DBs timeout. | Verify timeout configurations, retries, and fallback logic. |
| **Ignoring database query plans** | Unindexed queries or N+1 loops pass functional tests but crash prod. | Verify SQL indexing and query execution plans. |
| **Approving PRs with disabled tests (`@Disabled`)** | Hides broken tests and reduces test coverage. | Require explicit Jira ticket and justification for disabled tests. |
| **Allowing `Thread.sleep()` in tests** | Causes non-deterministic flaky test failures and slow CI builds. | Use Awaitility or explicit synchronization barriers. |

---

## 8. Performance & Security Notes

### The Cost of Reviewing What Is Missing
It is easy to critique code that was written; it is difficult to spot code that was **omitted**:
- Missing socket read timeout (`socket.setSoTimeout(5000)`).
- Missing database connection pool timeout.
- Missing `MDC.clear()` in a `finally` block.
- Missing rate limiter or input length validation.
The checklist serves as an explicit reminder to look for omissions.

---

## 9. Comparison / When to Use What

| Review Phase | Primary Responsibility | Key Artifacts |
| :--- | :--- | :--- |
| **Static Analysis (Automated)** | Formatting, syntax, basic bugs, CVEs | Checkstyle, Spotless, SpotBugs, Trivy |
| **Unit / Integration Tests (Automated)**| Functional contract verification | JUnit 5, Mockito, Testcontainers |
| **Peer Human Review (Checklist)** | Architecture, concurrency, security, edge cases | Pull Request review comments |
| **Performance Evals (Automated/Staging)**| Latency, throughput, GC behavior | JMH benchmarks, load test reports |

---

## 10. Interview Questions & Answers

### Q1: What are the top 3 things you look for when conducting a code review in Java?
<details>
<summary>View Answer</summary>
1. **Correctness and Edge Cases**: Does the code fulfill requirements? Are nulls, empty collections, negative numbers, and boundary values handled gracefully without returning null or throwing uncaught runtime exceptions?
2. **Resource Management and Concurrency**: Are all I/O channels, sockets, and database connections safely closed via `try-with-resources`? Is shared mutable state properly synchronized or eliminated via immutable records?
3. **Security and Resilience**: Are database statements parameterized? Are external network calls bounded by connect and read timeouts? Are credentials and PII masked from logs?
</details>

### Q2: How do you identify potential thread safety bugs during a code review?
<details>
<summary>View Answer</summary>
Inspect class fields in singleton beans (like Spring `@Service` or `@Controller` instances). If a singleton class contains non-final or mutable fields (e.g. `SimpleDateFormat`, `HashMap`, `ArrayList`, or a mutable counter), it is an immediate thread-safety bug because multiple HTTP request threads access that instance concurrently. Check that state is either strictly immutable, stored in `ThreadLocal`/ScopedValues, or managed using thread-safe primitives (`ConcurrentHashMap`, `AtomicLong`).
</details>

### Q3: What is the "N+1 Query Problem" and how do you spot it in code review?
<details>
<summary>View Answer</summary>
The N+1 query problem occurs when an application executes 1 initial query to fetch a list of $N$ parent records, and then executes an additional query inside a loop for each of the $N$ records to fetch related child data. Reviewers spot this by checking for database calls or ORM lazy-loaded collection accesses inside `for` loops. The fix is to execute a single `JOIN FETCH` query or a batch query (`WHERE id IN (...)`).
</details>

### Q4: Why should code reviews reject `@Autowired` field injection?
<details>
<summary>View Answer</summary>
Field injection tightly couples the class to the reflection mechanics of the dependency injection container. The dependencies cannot be declared `final`, meaning the class cannot be immutable. It makes unit testing difficult because dependencies cannot be passed via `new Service(mockDep)`, forcing tests to use reflection or boot a heavy Spring context. Constructor injection should always be mandated.
</details>

### Q5: How do you review test code effectively?
<details>
<summary>View Answer</summary>
Ensure tests follow the Arrange-Act-Assert (AAA) pattern. Verify that tests assert specific business outcomes rather than just verifying that a mock method was called (`verify(dao).save()`). Ensure tests exercise failure paths (bad input, timeouts, database down) rather than just the happy path. Reject tests containing `Thread.sleep()` or non-deterministic logic.
</details>

### Q6: What is the risk of an unclosed `Stream` over I/O resources in Java?
<details>
<summary>View Answer</summary>
Streams that wrap filesystem or I/O resources (such as `Files.lines(path)` or `Files.list(path)`) implement `AutoCloseable` and hold an open OS file descriptor. Unlike standard collections in memory, if an I/O stream is not enclosed in a `try-with-resources` block, the underlying OS file handle remains open until garbage collection sweeps the object. Under high load, this leads to OS file descriptor exhaustion (`Too many open files`).
</details>

### Q7: How do you handle disagreements during code reviews?
<details>
<summary>View Answer</summary>
Anchor discussions on established team standards, authoritative references (Effective Java, Java Language Specification, OWASP), and measurable metrics rather than personal stylistic preferences. If an architectural impasse arises, arrange a quick 10-minute sync to discuss tradeoffs. If consensus cannot be reached, escalate to the technical lead or software architect for a binding decision, and document the resulting consensus in team guidelines.
</details>

### Q8: What is the maximum recommended Pull Request size for an effective review?
<details>
<summary>View Answer</summary>
Industry studies (including Cisco and Google engineering metrics) demonstrate that review effectiveness drops off precipitously beyond 200–400 lines of code. Reviewers reviewing larger PRs suffer from cognitive fatigue and tend to skim, missing critical defects. PRs should ideally be kept under 300 lines of code and represent a single logical, cohesive change.
</details>

---

## 11. Practice Exercises

### Exercise 1: Identify Flaws in a Singleton Service PR
**Task:** Review this singleton service and list all thread-safety and resource management flaws:
```java
@Service
public class ReportService {
    private final SimpleDateFormat dateFormat = new SimpleDateFormat("yyyy-MM-dd");
    private int reportCounter = 0;

    public void generateReport(Path path) throws IOException {
        reportCounter++;
        Stream<String> lines = Files.lines(path);
        // process lines...
    }
}
```
<details>
<summary>View Solution</summary>

```
FLAWS IDENTIFIED:
1. Thread Safety: SimpleDateFormat is NOT thread-safe. Multiple threads calling generateReport concurrently will corrupt internal date formatting state. Fix: Use java.time.format.DateTimeFormatter (immutable and thread-safe).
2. Concurrency: reportCounter++ is not atomic. Fix: Use AtomicInteger.
3. Resource Leak: Files.lines() opens an OS file descriptor and implements AutoCloseable. Failing to wrap it in try-with-resources leaks open file handles. Fix: try (Stream<String> lines = Files.lines(path)) { ... }
```
</details>

### Exercise 2: Code Review Verification Script
**Task:** Write a simple shell script to verify that no files in the PR branch contain `System.out.println` or `@Disabled` tests.
<details>
<summary>View Solution</summary>

```bash
#!/usr/bin/env bash
set -euo pipefail

echo "Checking for System.out.println..."
if git grep -n "System.out.println" -- "src/main/java"; then
    echo "ERROR: Use SLF4J Logger instead of System.out.println!"
    exit 1
fi

echo "Checking for @Disabled tests..."
if git grep -n "@Disabled" -- "src/test/java"; then
    echo "WARNING: Disabled tests detected! Verify justification."
fi

echo "Pre-review scan complete."
```
</details>

### Exercise 3: Refactor Mutable Singleton State
**Task:** Refactor the flawed `ReportService` from Exercise 1 into an approved, thread-safe production class.
<details>
<summary>View Solution</summary>

```java
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.time.LocalDate;
import java.time.format.DateTimeFormatter;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.stream.Stream;

public class ReportService {
    private static final DateTimeFormatter DATE_FORMAT = DateTimeFormatter.ofPattern("yyyy-MM-dd");
    private final AtomicInteger reportCounter = new AtomicInteger(0);

    public void generateReport(Path path) throws IOException {
        reportCounter.incrementAndGet();
        try (Stream<String> lines = Files.lines(path)) {
            lines.forEach(line -> {
                // process lines cleanly
            });
        }
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **PR Sizing**: Keep pull requests $\le 300$ lines for thorough cognitive review.
- **Automate First**: Linters and static analysis must pass before human review.
- **Singletons**: Verify no mutable non-thread-safe fields in Spring `@Service` beans.
- **Resources**: Mandate `try-with-resources` for all streams, sockets, and connections.
- **Security**: Verify parameterized SQL, allow-list validation, and masked credentials.
- **Null Safety**: Reject methods that return `null` for collections or arrays.

---

## 13. Further Reading
- Google Engineering Practices: [How to Do a Code Review](https://google.github.io/eng-practices/review/)
- Effective Java: [Item 51: Design method signatures carefully](https://www.oreilly.com/library/view/effective-java-3rd/9780134686097/)
- Palantir Java Guidelines: [Code Review Guidelines](https://github.com/palantir/gradle-baseline)
- Cisco Systems Code Review Study: [Best Practices for Code Review](https://smartbear.com/learn/code-review/best-practices-for-peer-code-review/)
