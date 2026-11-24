# 18. OWASP Top 10 Mapping & Common Vulnerabilities in Spring Boot

| Metadata | Value |
|---|---|
| **Level** | Advanced |
| **Prerequisites** | `01-spring-security-architecture.md`, `06-CSRF-and-CORS.md`, OWASP Top 10 (2021/Latest) |
| **Estimated Reading Time** | 45 minutes |
| **Status** | Complete |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | OpenJDK 21, Spring Boot 3.4.1 |

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
Building enterprise software in Java without a rigorous mapping to the **OWASP Top 10** web application security risks leaves applications vulnerable to devastating exploits: remote code execution (RCE), massive data breaches, authentication bypasses, and regulatory fines (GDPR, HIPAA, PCI-DSS).

Common historical vulnerabilities in the Spring ecosystem—such as **Spring4Shell (CVE-2022-22965)**, **SpEL Expression Injection**, and unauthenticated **Actuator heap dumps**—demonstrate that framework power and flexibility can introduce severe security risks if misconfigured.

### Historical Evolution
- **The Spring4Shell Crisis (2022)**: CVE-2022-22965 exploited data binding in Spring MVC on JDK 9+ when deployed as a WAR on Tomcat, allowing attackers to manipulate Tomcat AccessLogValve properties via `class.module.classLoader` to achieve remote code execution.
- **OWASP Top 10 (2021 Update)**: Elevated **A01: Broken Access Control** to the #1 most catastrophic risk, followed by **A02: Cryptographic Failures** and **A03: Injection**.
- **Modern Spring Boot 3.x/4.x Hardening**: Dropped legacy Java 8/WAR deployment assumptions, restricted reflection over module class loaders, disabled sensitive Actuator endpoints by default, and integrated compile-time validation of SpEL expressions.

### Real-World Relevance
Every enterprise development team must demonstrate proactive mitigation of each OWASP category to pass security audits and protect customer data in production environments.

---

## 2. Core Concepts: The OWASP Top 10 Mapping Matrix

| OWASP Category | Spring Boot Threat Vector | Spring Boot Defense Mechanism |
|---|---|---|
| **A01: Broken Access Control** | IDOR (Insecure Direct Object References), missing method security, path traversal | Method Security (`@PreAuthorize`), `PermissionEvaluator`, strict `authorizeHttpRequests` |
| **A02: Cryptographic Failures** | Hardcoded secrets, weak password hashing (MD5/SHA1), unencrypted traffic | `BCryptPasswordEncoder`, Argon2, Spring Cloud Vault, HSTS enforcement |
| **A03: Injection** | SQL Injection, SpEL Injection, Log Forging (Log4j / Logback) | Prepared statements (`JdbcClient`, JPA), sanitized SpEL evaluation, parameterized logging |
| **A04: Insecure Design** | Missing rate limiting, unbounded resource allocation, password resets without locks | Bucket4j token bucket rate limiting, circuit breakers, threat modeling |
| **A05: Security Misconfiguration**| Exposed Actuator endpoints (`/actuator/env`, `/heapdump`), debug mode in prod | `management.endpoints.web.exposure.include=health,info`, disabling stack traces |
| **A06: Vulnerable Components** | Outdated third-party dependencies with known CVEs | Software Bill of Materials (CycloneDX SBOM), OWASP Dependency-Check, Dependabot |
| **A07: Identification Failures** | Credential stuffing, weak session IDs, lack of brute-force lockouts | Account lockout services, WebAuthn passkeys, refresh token rotation |
| **A08: Software & Data Integrity**| Untrusted Java object deserialization, tampered CI/CD pipelines | Stateless JSON payloads, Jackson safe type validation, signed build artifacts |
| **A09: Logging & Monitoring** | Failing to log security events, logging plaintext passwords or credit cards | PII masking logback converters, Spring Boot AuditEventRepository, SIEM export |
| **A10: SSRF (Server-Side Request Forgery)**| Microservices fetching untrusted external URLs provided by users | Whitelisting IP addresses in `RestClient`/`WebClient`, blocking metadata IP `169.254.169.254` |

---

## 3. How It Works Internally: Spring4Shell (CVE-2022-22965) Analysis

### The Vulnerability Anatomy
When a Spring MVC controller accepts an unannotated POJO (form binding), Spring's `DataBinder` uses Java reflection to bind request parameters to object properties.
On Java 9+, the module system introduced `java.lang.Class.getModule()`.
Attackers exploited this path to navigate object graphs:
```
request parameter:
class.module.classLoader.resources.context.parent.pipeline.first.pattern=%{prefix}i...
class.module.classLoader.resources.context.parent.pipeline.first.suffix=.jsp
class.module.classLoader.resources.context.parent.pipeline.first.directory=webapps/ROOT
```
Tomcat's `AccessLogValve` properties were overwritten, causing Tomcat to write arbitrary web access logs containing executable JSP web shells directly into the public web root.

### Spring's Internal Fix
Spring Framework updated `PropertyPath` evaluation to disallow any property paths that match or traverse `ClassLoader` or `ProtectionDomain`:
```java
// Spring Framework DataBinder internal check
if (Class.class.equals(beanClass) && ("classLoader".equals(prop) || "protectionDomain".equals(prop))) {
    return; // Disallow access to dangerous classloader fields!
}
```

---

## 4. Syntax & Usage

### 1. Hardening Actuator Endpoints (`application.yml`)
```yaml
management:
  endpoints:
    web:
      exposure:
        include: "health,info,metrics" # Strictly expose ONLY non-sensitive endpoints
      base-path: "/internal/management" # Move off default /actuator
  endpoint:
    health:
      show-details: when-authorized
      roles: "SYSTEM_ADMIN"
    env:
      show-values: NEVER # Prevent leaking API keys and database passwords
```

### 2. Spring Security Defense Matrix Configuration
```java
package com.example.security.config;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.security.config.annotation.method.configuration.EnableMethodSecurity;
import org.springframework.security.config.annotation.web.builders.HttpSecurity;
import org.springframework.security.crypto.argon2.Argon2PasswordEncoder;
import org.springframework.security.crypto.password.PasswordEncoder;
import org.springframework.security.web.SecurityFilterChain;

@Configuration
@EnableMethodSecurity(prePostEnabled = true)
public class HardenedSecurityConfig {

    @Bean
    public SecurityFilterChain securityFilterChain(HttpSecurity http) throws Exception {
        return http
            .authorizeHttpRequests(auth -> auth
                .requestMatchers("/api/public/**").permitAll()
                .requestMatchers("/internal/management/**").hasRole("SYSTEM_ADMIN")
                .anyRequest().authenticated()
            )
            .build();
    }

    @Bean
    public PasswordEncoder passwordEncoder() {
        // A02 Mitigation: Use Argon2id (resistant to GPU/ASIC cracking)
        return Argon2PasswordEncoder.defaultsForSpringSecurity_v5_8();
    }
}
```

---

## 5. Practical Examples

### Example 1: Mitigating A01 (Broken Access Control) via Insecure Direct Object Reference (IDOR) Defense
Preventing users from accessing other users' resources simply by altering a URL identifier (`/api/documents/101`).

```java
package com.example.service;

import com.example.domain.Document;
import com.example.repository.DocumentRepository;
import org.springframework.security.access.prepost.PreAuthorize;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

@Service
public class DocumentService {

    private final DocumentRepository repository;

    public DocumentService(DocumentRepository repository) {
        this.repository = repository;
    }

    // A01 Defense: Enforces that the document owner matches the authenticated user
    @PreAuthorize("hasRole('ADMIN') or @documentSecurity.isOwner(#id, authentication.name)")
    @Transactional(readOnly = true)
    public Document getDocument(Long id) {
        return repository.findById(id).orElseThrow();
    }
}
```

### Example 2: Mitigating A03 (SpEL Injection)
**Vulnerability**: Evaluating untrusted user input directly inside `SpelExpressionParser` allows arbitrary Java method invocation and Remote Code Execution.

```java
// DANGEROUS VULNERABILITY: User-controlled SpEL evaluation!
// Attacker input: "T(java.lang.Runtime).getRuntime().exec('calc.exe')"
Expression exp = parser.parseExpression(userInput);
Object result = exp.getValue(); // Executes attacker code!
```

**Production Fix**:
Use `SimpleEvaluationContext` instead of `StandardEvaluationContext`. `SimpleEvaluationContext` restricts SpEL to data binding and property access, explicitly blocking arbitrary Java class reflection.

```java
package com.example.security.spel;

import org.springframework.expression.Expression;
import org.springframework.expression.ExpressionParser;
import org.springframework.expression.spel.standard.SpelExpressionParser;
import org.springframework.expression.spel.support.SimpleEvaluationContext;
import org.springframework.stereotype.Service;

@Service
public class SafeSpelEvaluator {

    private final ExpressionParser parser = new SpelExpressionParser();

    public Object evaluateSafeProperty(String userPropertyExpression, Object targetObject) {
        // SECURE: SimpleEvaluationContext disallows Java type references (T(...)), method invocations, and constructors
        SimpleEvaluationContext context = SimpleEvaluationContext.forReadOnlyDataBinding().build();
        Expression expression = parser.parseExpression(userPropertyExpression);
        return expression.getValue(context, targetObject);
    }
}
```

### Example 3: Mitigating A10 (Server-Side Request Forgery - SSRF)
Protecting internal microservices from SSRF by validating that user-supplied webhook URLs resolve to public IPs and blocking loopbacks, private subnets (RFC 1918), and cloud metadata IP (`169.254.169.254`).

```java
package com.example.security.ssrf;

import org.springframework.stereotype.Service;

import java.net.InetAddress;
import java.net.URI;
import java.net.UnknownHostException;

@Service
public class SsrfValidationService {

    private static final String CLOUD_METADATA_IP = "169.254.169.254";

    public boolean isSafeUrl(String urlString) {
        try {
            URI uri = URI.create(urlString);
            if (!"https".equalsIgnoreCase(uri.getScheme())) {
                return false; // Require HTTPS
            }

            InetAddress address = InetAddress.getByName(uri.getHost());

            // Block Loopback (127.0.0.1)
            if (address.isLoopbackAddress()) return false;

            // Block Private Subnets (10.x, 172.16.x, 192.168.x)
            if (address.isSiteLocalAddress()) return false;

            // Block Link-Local & Cloud Metadata IP
            if (address.isLinkLocalAddress() || CLOUD_METADATA_IP.equals(address.getHostAddress())) {
                return false;
            }

            return true;
        } catch (UnknownHostException | IllegalArgumentException e) {
            return false;
        }
    }
}
```

### Example 4: Failure Anti-Pattern & Production Fix
**Anti-Pattern**: Exposing the `/actuator/heapdump` endpoint publicly or storing unmasked JWT secret keys inside `application.properties`.

```yaml
# CRITICAL VULNERABILITY: Allows anonymous attackers to download full JVM heap memory!
# Anyone can extract plaintext passwords, JWT private keys, and cached credentials!
management:
  endpoints:
    web:
      exposure:
        include: "*"
```

**Production Fix**:
Expose only `health` and `info`, secure actuator endpoints on a private management port, and protect with role-based security.

```yaml
# SECURE CONFIGURATION
management:
  server:
    port: 9090 # Bind management endpoints to private administrative network interface
  endpoints:
    web:
      exposure:
        include: "health,info"
  endpoint:
    health:
      show-details: never
```

### Example 5: Mitigating A09 (Log Injection & PII Masking)
Preventing log forging (injecting CRLF characters to fake audit records) and masking sensitive Personally Identifiable Information (PII) before writing to logs.

```java
package com.example.security.logging;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Service;

@Service
public class SecureAuditLogger {

    private static final Logger log = LoggerFactory.getLogger(SecureAuditLogger.class);

    public void logUserAction(String username, String action) {
        // Sanitize CRLF (\r\n) characters to defeat log forging/injection attacks
        String cleanUsername = username.replaceAll("[\r\n]", "_");
        String cleanAction = action.replaceAll("[\r\n]", "_");

        log.info("AUDIT: User [{}] performed action [{}]", cleanUsername, cleanAction);
    }

    public static String maskCreditCard(String ccNumber) {
        if (ccNumber == null || ccNumber.length() < 4) return "****";
        return "****-****-****-" + ccNumber.substring(ccNumber.length() - 4);
    }
}
```

---

## 6. Best Practices

1. **Adopt Deny-by-Default Access Control**: Configure `.anyRequest().denyAll()` or `.anyRequest().authenticated()` as the baseline in your `SecurityFilterChain`.
2. **Never Use `StandardEvaluationContext` with User Input**: Strictly use `SimpleEvaluationContext` to evaluate dynamic SpEL expressions safely.
3. **Isolate Spring Boot Actuator**: Bind Actuator to a private port (`management.server.port=9090`) accessible only by internal monitoring clusters.
4. **Automate Dependency Scanning**: Incorporate OWASP Dependency-Check or Snyk in CI/CD pipelines to block builds containing critical CVEs.
5. **Enforce Strong Password Hashing**: Use `Argon2PasswordEncoder` or `BCryptPasswordEncoder` with a minimum cost factor of 12.
6. **Sanitize Log Inputs**: Strip carriage return (`\r`) and newline (`\n`) characters before logging user-controlled input to prevent log forging.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Exposing Actuator `/env` or `/heapdump` | Attackers download database passwords and memory state. | Restrict `management.endpoints.web.exposure.include` to `health,info`. |
| Unsafe SpEL parsing | `StandardEvaluationContext` permits attackers to execute arbitrary Java system calls via `T(Runtime)`. | Use `SimpleEvaluationContext.forReadOnlyDataBinding()`. |
| Direct Object Reference (IDOR) | Endpoints accept IDs without verifying that the caller owns the resource. | Enforce `@PreAuthorize("@ownershipService.isOwner(#id, authentication)")`. |
| Ignoring SSRF on webhooks | Attackers supply `http://169.254.169.254` to steal AWS IAM instance credentials. | Validate and resolve DNS before initiating outbound requests; block local/cloud metadata ranges. |
| Storing raw secrets in git repositories | Credentials leak during supply-chain attacks or repository compromises. | Use HashiCorp Vault, AWS Secrets Manager, or Kubernetes Secrets. |
| Inadequate password cost factor | Fast hashes allow attackers to brute-force leaked database dumps in minutes. | Configure BCrypt with cost 12 or use Argon2id. |

---

## 8. Performance & Security Notes

- **Password Hashing Latency**: Argon2 and BCrypt are intentionally CPU- and memory-intensive to defeat GPU cracking rigs. A BCrypt cost of 12 takes ~200ms per verification. Offload authentication from high-throughput API endpoints by using stateless JWTs for subsequent requests.
- **Dependency Vulnerability Scanning**: Running software composition analysis (SCA) during local compilation slows developer workflows. Run deep vulnerability scans asynchronously in CI/CD pipeline stages.
- **Log Forging Defense Overhead**: Regex sanitization of log strings (`[\r\n]`) adds negligible sub-microsecond overhead while completely mitigating log injection attacks.

---

## 9. Comparison / When to Use What

| Security Measure | OWASP Category | Target Layer | Primary Defense |
|---|---|---|---|
| **`@PreAuthorize` / ACLs** | A01: Broken Access Control | Service Layer | Prevents unauthorized data access & IDOR |
| **Argon2 / BCrypt** | A02: Cryptographic Failures | Data Layer | Protects credentials against offline cracking |
| **SimpleEvaluationContext** | A03: Injection (SpEL) | Framework Engine | Disallows reflection and class loading |
| **Actuator Isolation** | A05: Security Misconfiguration| DevOps / Infrastructure | Prevents internal secret and heap exposure |
| **SSRF IP Whitelisting** | A10: Server-Side Request Forgery| Network Client Layer | Blocks pivot attacks into internal cloud VPCs |

---

## 10. Interview Questions

<details>
<summary>1. What was the root cause of the Spring4Shell vulnerability (CVE-2022-22965)?</summary>

**Answer:**
Spring4Shell occurred because Spring MVC's parameter data binding allowed property graph traversal using reflection. On JDK 9+, Java introduced modularity, exposing `Class.getModule()`. Attackers traversed `class.module.classLoader` to reach Tomcat's `AccessLogValve` properties, overriding log file directories and suffixes to write an executable JSP web shell into the document root.
</details>

<details>
<summary>2. Why is SpEL injection dangerous, and how do you prevent it?</summary>

**Answer:**
Spring Expression Language (SpEL) evaluated with `StandardEvaluationContext` has full access to the Java runtime environment, allowing constructors, method calls, and class loading via `T(...)`. If user input is passed into `parseExpression()`, attackers can execute system commands (`T(java.lang.Runtime).getRuntime().exec(...)`). To prevent this, use `SimpleEvaluationContext`, which restricts evaluation strictly to safe data binding and property access.
</details>

<details>
<summary>3. What is an Insecure Direct Object Reference (IDOR) and how do you mitigate it in Spring Boot?</summary>

**Answer:**
An IDOR occurs when an application exposes a direct database key (e.g., `/api/orders/500`) and fails to verify that the authenticated caller owns or has rights to that specific entity. Mitigation involves:
1. Enforcing method security: `@PreAuthorize("@orderSecurity.isOwner(#orderId, authentication.name)")`.
2. Scoping SQL queries: `WHERE id = :orderId AND user_id = :currentUserId`.
</details>

<details>
<summary>4. Why should the cloud metadata IP `169.254.169.254` be blocked in webhook services?</summary>

**Answer:**
In AWS, GCP, and Azure, `169.254.169.254` is the link-local metadata address that EC2 instances and containers query to retrieve temporary IAM role credentials and environment tokens. If an attacker can trigger an SSRF request to this IP, they can steal the server's cloud credentials and compromise the entire AWS cloud account.
</details>

<details>
<summary>5. How can you protect Actuator endpoints in Spring Boot?</summary>

**Answer:**
1. Expose only safe endpoints: `management.endpoints.web.exposure.include=health,info`.
2. Run Actuator on an isolated internal port: `management.server.port=9090`.
3. Require administrative roles: `requestMatchers("/actuator/**").hasRole("ADMIN")`.
4. Hide environment values: `management.endpoint.env.show-values=NEVER`.
</details>

<details>
<summary>6. What is Log Forging (Log Injection) and how is it mitigated?</summary>

**Answer:**
Log forging occurs when an attacker injects carriage returns and line feeds (`\r\n`) into user-controlled input that is written to server logs. This allows the attacker to fabricate fake log entries (e.g., "User admin logged in successfully") or corrupt log aggregators (Elasticsearch/Splunk). Mitigation requires stripping or replacing `\r` and `\n` characters before logging.
</details>

<details>
<summary>7. Why is Argon2 considered superior to BCrypt for password hashing?</summary>

**Answer:**
Argon2 (specifically Argon2id) was the winner of the Password Hashing Competition. Unlike BCrypt, which is memory-hard only to a limited extent, Argon2 is designed to consume configurable amounts of RAM as well as CPU time. This makes Argon2 dramatically more resistant to hardware-accelerated attacks using GPUs, FPGAs, and ASICs.
</details>

<details>
<summary>8. What is a Software Bill of Materials (SBOM) in modern Spring Boot?</summary>

**Answer:**
An SBOM is a structured machine-readable inventory (e.g., CycloneDX or SPDX) of all direct and transitive third-party dependencies, libraries, and licenses bundled into an application jar or container. Spring Boot can generate SBOMs during the build (via the CycloneDX plugin) to allow security tools to continuously cross-reference components against vulnerability databases.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Actuator Exposure Lockdown (Easy)</summary>

**Task**: Write an `application.yml` snippet that exposes only the `health` endpoint and configures `health` to never show details to anonymous users.

**Solution**:
```yaml
management:
  endpoints:
    web:
      exposure:
        include: "health"
  endpoint:
    health:
      show-details: never
```
</details>

<details>
<summary>Exercise 2: Log Injection Sanitizer (Easy)</summary>

**Task**: Implement a Java utility method that accepts a user string and replaces any carriage return or newline characters with an underscore.

**Solution**:
```java
public String sanitizeLog(String input) {
    if (input == null) return "";
    return input.replaceAll("[\r\n]", "_");
}
```
</details>

<details>
<summary>Exercise 3: Safe SpEL Property Access (Medium)</summary>

**Task**: Write a method that evaluates a property expression on an `Employee` object using `SimpleEvaluationContext` and returns the resolved value safely.

**Solution**:
```java
public Object getSafeEmployeeProperty(Employee emp, String propertyName) {
    ExpressionParser parser = new SpelExpressionParser();
    SimpleEvaluationContext context = SimpleEvaluationContext.forReadOnlyDataBinding().build();
    return parser.parseExpression(propertyName).getValue(context, emp);
}
```
</details>

<details>
<summary>Exercise 4: SSRF IP Range Validator (Medium)</summary>

**Task**: Implement a method that validates whether a destination host IP address is a private local network address (`isSiteLocalAddress()` or loopback).

**Solution**:
```java
public boolean isPrivateAddress(String host) {
    try {
        InetAddress address = InetAddress.getByName(host);
        return address.isLoopbackAddress() || address.isSiteLocalAddress();
    } catch (UnknownHostException e) {
        return true; // Fail closed if unresolvable
    }
}
```
</details>

<details>
<summary>Exercise 5: IDOR Defense with Spring Data JPA (Hard)</summary>

**Task**: Implement a repository query and service method that retrieves an `Invoice` by ID strictly scoped to the authenticated user's `tenant_id` to prevent cross-tenant IDOR access.

**Solution**:
```java
public interface InvoiceRepository extends JpaRepository<Invoice, Long> {
    Optional<Invoice> findByIdAndTenantId(Long id, String tenantId);
}

@Service
public class InvoiceService {
    private final InvoiceRepository repository;

    public InvoiceService(InvoiceRepository repository) {
        this.repository = repository;
    }

    public Invoice getInvoice(Long id, String currentTenantId) {
        return repository.findByIdAndTenantId(id, currentTenantId)
            .orElseThrow(() -> new AccessDeniedException("Invoice not accessible"));
    }
}
```
</details>

<details>
<summary>Exercise 6: MockMvc Security Testing for Actuator Protection (Hard)</summary>

**Task**: Write a MockMvc unit test verifying that unauthenticated access to `/internal/management/env` returns HTTP 401/403, and that access with `ROLE_SYSTEM_ADMIN` succeeds.

**Solution**:
```java
@WebMvcTest
class ActuatorSecurityTest {

    @Autowired
    private MockMvc mockMvc;

    @Test
    void unauthenticatedUserCannotAccessEnv() throws Exception {
        mockMvc.perform(get("/internal/management/env"))
            .andExpect(status().isUnauthorized());
    }

    @Test
    @WithMockUser(roles = "SYSTEM_ADMIN")
    void adminCanAccessEnv() throws Exception {
        mockMvc.perform(get("/internal/management/env"))
            .andExpect(status().isOk());
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **A01 Broken Access Control**: Use method security (`@PreAuthorize`) and scope database queries by current user/tenant.
- **A02 Cryptography**: Use `Argon2PasswordEncoder` or BCrypt (cost $\ge 12$).
- **A03 Injection**: Never evaluate SpEL with `StandardEvaluationContext`; use `SimpleEvaluationContext`.
- **A05 Misconfiguration**: Never expose dangerous Actuator endpoints (`/env`, `/heapdump`) to public networks.
- **A10 SSRF**: Block requests to loopback, private IP ranges (RFC 1918), and cloud metadata IP `169.254.169.254`.

---

## 13. Further Reading
- [OWASP Top 10 Web Application Security Risks](https://owasp.org/www-project-top-ten/)
- [Spring Framework RCE Vulnerability (Spring4Shell - CVE-2022-22965)](https://spring.io/blog/2022/03/31/spring-framework-rce-early-announcement)
- [Spring Boot Actuator Security Guidelines](https://docs.spring.io/spring-boot/docs/current/reference/html/actuator.html#actuator.endpoints.security)
- [OWASP Server-Side Request Forgery Prevention Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Server_Side_Request_Forgery_Prevention_Cheat_Sheet.html)
- [NIST SP 800-63B: Digital Identity Guidelines](https://pages.nist.gov/800-63-3/sp800-63b.html)
