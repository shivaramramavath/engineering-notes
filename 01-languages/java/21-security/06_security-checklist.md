# Production Security Checklist for Java Applications

> The authoritative operational and architectural security checklist for enterprise Java: JVM flags, TLS/mTLS configuration, cryptography, web hardening, data protection, and compliance readiness.

| Metadata | Details |
| :--- | :--- |
| **Level** | Comprehensive / Enterprise Production |
| **Prerequisites** | All Module 21 notes ([00](00_input-validation-and-injection.md) - [05](05_dependency-security.md)) |
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

### What is the Production Security Checklist?
This checklist is an authoritative, audit-ready operational framework that systematically verifies every layer of an enterprise Java workload before deploying to production:
- **JVM & Runtime Security**: JVM command-line flags, entropy configuration, serialization filters.
- **Transport Layer Security**: Modern TLS 1.3 enforcement, cipher suite selection, certificate validation.
- **Authentication & Authorization**: Token verification, password hashing parameters, session management.
- **Data Protection & Cryptography**: AES-256-GCM, key rotation, memory zeroing, PII sanitization.
- **Application & API Hardening**: HTTP security headers, CORS, CSRF, input validation, rate limiting.
- **Observability & Audit Trails**: Tamper-evident logging, secret masking, SIEM integration.

### Why Production Checklists are Indispensable
Vulnerabilities rarely result from a failure of cryptographic theory; they result from operational misconfigurations:
- A debug port (`-agentlib:jdwp`) left open to the public internet.
- A weak iteration count in password hashing.
- Unmasked authorization headers written to application logs.
- Running Java processes as the `root` user inside container environments.
Using an exhaustive checklist guarantees that systems are hardened uniformly across teams and environments.

---

## 2. Core Concepts

### Defense-in-Depth Model
Security cannot rely on a single defensive layer. The enterprise Java defense-in-depth model comprises:
1. **Infrastructure & Container**: Non-root UID, read-only root filesystems, minimal container base images (Distroless / Chainguard).
2. **JVM Runtime**: Restricted permissions, JEP 290 serialization filters, disabled remote debugging.
3. **Application Framework**: Strict request validation, role-based access control (RBAC), constant-time comparisons.
4. **Data Layer**: Authenticated encryption at rest (AEAD), parameterized SQL queries, masked logs.

```
+--------------------------------------------------------------------------+
|                     ENTERPRISE JAVA DEFENSE-IN-DEPTH                     |
|                                                                          |
| [ Layer 1: Network & Edge ] -> TLS 1.3, WAF, Rate Limiting, CORS         |
| [ Layer 2: Container/OS   ] -> Non-root user, Distroless, Read-only FS   |
| [ Layer 3: JVM Runtime    ] -> JEP 290 Filters, Heap Dump Controls       |
| [ Layer 4: Application    ] -> PreparedStatements, Bean Validation       |
| [ Layer 5: Data Storage   ] -> AES-256-GCM, Argon2id, Salted Hashes      |
+--------------------------------------------------------------------------+
```

---

## 3. How It Works Internally

### Hardening the JVM Execution Environment
When launching a production Java service in a container:

```bash
exec java   -XX:+UseContainerSupport   -XX:MaxRAMPercentage=75.0   -Djava.security.egd=file:/dev/urandom   -Djdk.serialFilter="com.company.dto.*;java.lang.*;java.util.*;!*"   -XX:-HeapDumpOnOutOfMemoryError   -Dfile.encoding=UTF-8   -jar application.jar
```

1. **`-XX:+UseContainerSupport`**: Automatically detects cgroup CPU and memory limits inside Docker/Kubernetes.
2. **`-Djava.security.egd=file:/dev/urandom`**: Prevents JVM thread blocking on entropy depletion in Linux environments.
3. **`-Djdk.serialFilter="...!*"`**: Closes native serialization gadget chains.
4. **`-XX:-HeapDumpOnOutOfMemoryError`**: Prevents unencrypted heap memory dumps containing plaintext secrets from landing on container disk storage.

---

## 4. Syntax & Usage

### 1. Enforcing Strict Security Headers in Java HTTP Services
```java
import java.util.Map;

public class SecurityHeadersEnforcer {
    public static Map<String, String> getMandatoryHeaders() {
        return Map.of(
            "Content-Security-Policy", "default-src 'self'; frame-ancestors 'none';",
            "X-Content-Type-Options", "nosniff",
            "X-Frame-Options", "DENY",
            "Strict-Transport-Security", "max-age=31536000; includeSubDomains; preload",
            "Referrer-Policy", "strict-origin-when-cross-origin",
            "Permissions-Policy", "geolocation=(), microphone=(), camera=()"
        );
    }
}
```

---

## 5. Practical Examples

### Example 1: The Master Production Hardening Audit Table
Use this matrix during architecture and release reviews:

| Domain | Control Item | Requirement / Standard | Audit Verification |
| :--- | :--- | :--- | :--- |
| **JVM** | Execution User | Run as unprivileged UID (e.g. `UID 10001`), NEVER `root` | `docker inspect --format '{{.Config.User}}' <image>` |
| **JVM** | Debug Ports | JDWP debug agent (`-agentlib:jdwp`) must be removed | Verify JVM launch script has no `-agentlib` |
| **JVM** | Serialization | Enforce JEP 290 allow-list filter | `-Djdk.serialFilter="...!*"` present |
| **Network** | TLS Protocol | Enforce TLSv1.3 and TLSv1.2; disable SSLv3, TLS 1.0, 1.1 | `nmap --script ssl-enum-ciphers -p 443 <host>` |
| **Storage** | Database SQL | 100% parameterized queries via `PreparedStatement` | Static analysis scan: zero string concatenation in SQL |
| **Storage** | Passwords | Argon2id ($m=64\text{MB}, t=3, p=4$) or PBKDF2 ($600\text{k}$) | Verify authentication service hasher config |
| **Storage** | Secrets | No hardcoded credentials; inject via Vault or K8s secret | `gitleaks detect` passes clean on git history |
| **HTTP** | HSTS Header | Enforce `Strict-Transport-Security` ($> 1\text{ year}$) | `curl -I https://api.service.com` checks HSTS |
| **Logging** | PII & Secret Masking| Mask card numbers, passwords, Bearer tokens in logs | Audit log files for absence of sensitive patterns |
| **Supply Chain**| Vulnerability Gate| Zero CVEs with CVSS $\ge 7.0$ in dependency tree | `mvn verify` passes OWASP Dependency-Check |

### Example 2: Container Dockerfile Hardening
**Goal:** Production-grade multi-stage Dockerfile running Java 21 on a non-root distroless container.

```dockerfile
# Stage 1: Build & Package
FROM maven:3.9.6-eclipse-temurin-21 AS builder
WORKDIR /app
COPY pom.xml .
RUN mvn dependency:go-offline
COPY src ./src
RUN mvn clean package -DskipTests

# Stage 2: Hardened Minimal Production Runtime
FROM gcr.io/distroless/java21-debian12:nonroot
WORKDIR /app
COPY --from=builder /app/target/production-service.jar /app/service.jar

# Run explicitly as non-root user (distroless default UID is 65532)
USER nonroot:nonroot

EXPOSE 8080
ENTRYPOINT ["java",             "-XX:+UseContainerSupport",             "-XX:MaxRAMPercentage=75.0",             "-Djdk.serialFilter=!*",             "-jar", "/app/service.jar"]
```

### Example 3: Fail-Fast Security Health Indicator
**Goal:** A startup validator that checks whether the running JVM environment satisfies production security prerequisites.

```java
import java.security.NoSuchAlgorithmException;
import java.security.SecureRandom;
import javax.crypto.Cipher;

public class SecuritySanityCheck {
    public static void verifyProductionSecurity() {
        // 1. Verify unlimited strength cryptography
        try {
            int maxKeyLen = Cipher.getMaxAllowedKeyLength("AES");
            if (maxKeyLen < 256) {
                throw new SecurityException("JCE Unlimited Strength Jurisdiction Policy missing!");
            }
        } catch (NoSuchAlgorithmException e) {
            throw new SecurityException("AES algorithm missing", e);
        }

        // 2. Verify SecureRandom CSPRNG health
        byte[] testBytes = new byte[32];
        new SecureRandom().nextBytes(testBytes);

        // 3. Verify user is not root in Linux
        String user = System.getProperty("user.name");
        if ("root".equalsIgnoreCase(user)) {
            throw new SecurityException("SECURITY VIOLATION: JVM running as root user!");
        }
    }
}
```

---

### Example 4: Hardening TLS 1.3 in Embedded Web Servers Programmatically
**Goal:** Restrict embedded Tomcat or Jetty to TLS 1.3 exclusively with high-strength forward-secret cipher suites.

```java
import java.security.KeyStore;
import javax.net.ssl.*;

public class SecureSslContextFactory {
    public static SSLContext createStrictTls13Context(
            KeyStore keyStore, char[] keyPass, KeyStore trustStore) throws Exception {
        
        KeyManagerFactory kmf = KeyManagerFactory.getInstance(KeyManagerFactory.getDefaultAlgorithm());
        kmf.init(keyStore, keyPass);

        TrustManagerFactory tmf = TrustManagerFactory.getInstance(TrustManagerFactory.getDefaultAlgorithm());
        tmf.init(trustStore);

        SSLContext sslContext = SSLContext.getInstance("TLSv1.3");
        sslContext.init(kmf.getKeyManagers(), tmf.getTrustManagers(), null);

        // Verify default parameters enforce TLS 1.3
        SSLParameters params = sslContext.getDefaultSSLParameters();
        params.setProtocols(new String[]{"TLSv1.3"});
        params.setCipherSuites(new String[]{
            "TLS_AES_256_GCM_SHA384",
            "TLS_CHACHA20_POLY1305_SHA256",
            "TLS_AES_128_GCM_SHA256"
        });

        return sslContext;
    }
}
```

### Example 5: Operational Bash Command Verification Playbook
**Goal:** Execute rapid, automated verification of remote endpoints during release validation.

```bash
#!/usr/bin/env bash
set -euo pipefail

TARGET_HOST="api.production.internal"
TARGET_PORT="443"

echo "[1/4] Verifying TLS 1.3 support..."
openssl s_client -connect "${TARGET_HOST}:${TARGET_PORT}" -tls1_3 </dev/null 2>/dev/null | grep "Protocol  : TLSv1.3"

echo "[2/4] Verifying obsolete TLS 1.0 is rejected..."
if openssl s_client -connect "${TARGET_HOST}:${TARGET_PORT}" -tls1 </dev/null 2>&1 | grep -q "handshake failure"; then
    echo "PASS: TLS 1.0 rejected."
else
    echo "FAIL: TLS 1.0 accepted!"
    exit 1
fi

echo "[3/4] Verifying HTTP Security Headers..."
HEADERS=$(curl -sI "https://${TARGET_HOST}")
echo "${HEADERS}" | grep -i "Strict-Transport-Security"
echo "${HEADERS}" | grep -i "X-Content-Type-Options: nosniff"
echo "${HEADERS}" | grep -i "X-Frame-Options: DENY"

echo "[4/4] Security Verification PASSED."
```

## 6. Best Practices

1. **Run as Non-Root**: Never run Java applications under the `root` account inside Docker containers or virtual machines.
2. **Disable Remote Debugging in Production**: Never enable JDWP debug agents on production instances.
3. **Enforce HTTPS with HSTS**: Mandate TLS 1.3 with HTTP Strict Transport Security (`Strict-Transport-Security: max-age=31536000`).
4. **Sanitize Error Responses**: Never return stack traces, internal SQL errors, or class names to client HTTP responses.
5. **Mask Application Logs**: Configure log pipelines to redact authorization headers, credit cards, and sensitive customer data.
6. **Implement Continuous Vulnerability Scanning**: Run automated container and dependency scans on every commit.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Secure Fix |
| :--- | :--- | :--- |
| **Running container as `root`** | A container escape gives the attacker full control over host OS. | Use `USER nonroot` or explicit non-root UID in Dockerfile. |
| **Leaking stack traces in HTTP 500 errors** | Reveals internal class names, database vendors, and library versions. | Return generic error payloads (`{"error": "Internal Error", "id": "<uuid>"}`). |
| **Leaving `-agentlib:jdwp` enabled** | JDWP has zero authentication; anyone connecting achieves instant RCE. | Remove debug flags from production configurations. |
| **Allowing wildcard CORS (`*`) with credentials** | Allows malicious websites to make authenticated cross-origin requests. | Specify explicit allowed origins and set `credentials: true`. |
| **Logging `Authorization: Bearer <token>`** | Leaks active session tokens into log aggregators (Elasticsearch, Datadog). | Redact the `Authorization` header in HTTP logging filters. |
| **Trusting default container memory limits** | JVM allocates heap based on host physical RAM instead of container limits. | Always use `-XX:+UseContainerSupport -XX:MaxRAMPercentage=...`. |

---

## 8. Performance & Security Notes

### Modern TLS 1.3 Performance
Enforcing TLS 1.3 provides both superior security and superior performance:
- TLS 1.3 reduces connection handshake round-trips from 2-RTT (TLS 1.2) to **1-RTT**, cutting connection latency by 50%.
- Obsolete, CPU-intensive ciphers (CBC mode, static RSA key exchange) are removed entirely.
- All modern JDKs (11+) support TLS 1.3 natively with hardware acceleration.

---

## 9. Comparison / When to Use What

| Environment | Debug Flags Allowed | Min TLS Version | Serialization Filter | Memory Limit Strategy |
| :--- | :--- | :--- | :--- | :--- |
| **Local Development** | Yes (`jdwp:dt_socket`) | TLS 1.2+ | Permissive | Dynamic `-Xmx` |
| **Staging / QA** | No | TLS 1.2 / 1.3 | Strict Allow-List | CGroup percentage (75%) |
| **Production** | STRICTLY FORBIDDEN | TLS 1.3 Mandated | Strict Allow-List (`!*`) | CGroup percentage (75%) |

---

## 10. Interview Questions & Answers

### Q1: What are the primary JVM security flags recommended for production container deployments?
<details>
<summary>View Answer</summary>
(1) `-XX:+UseContainerSupport`: Enforces respect for cgroup limits. (2) `-XX:MaxRAMPercentage=75.0`: Slices available container memory cleanly, leaving 25% for Metaspace, threads, and off-heap memory. (3) `-Djdk.serialFilter="...!*"`: Applies strict allow-list deserialization filters. (4) `-Djava.security.egd=file:/dev/urandom`: Ensures non-blocking CSPRNG entropy gathering. (5) `-XX:-HeapDumpOnOutOfMemoryError`: Prevents unencrypted heap dumps from exposing credentials on disk.
</details>

### Q2: Why is running a Java application as `root` inside Docker dangerous?
<details>
<summary>View Answer</summary>
By default, Docker containers share the host kernel. If a vulnerability in the application (like command injection or an RCE gadget) or a container runtime vulnerability (such as runc escapes) is exploited, the compromised process already has `UID 0` (root). The attacker can potentially escape the container and achieve root control over the underlying host operating system. Running as a dedicated unprivileged user (UID 10001 or distroless `nonroot`) limits the impact to the unprivileged process boundary.
</details>

### Q3: What is HSTS (HTTP Strict Transport Security) and why is it essential?
<details>
<summary>View Answer</summary>
HSTS is an HTTP response header (`Strict-Transport-Security: max-age=31536000; includeSubDomains; preload`) that informs browsers to communicate with the domain exclusively over HTTPS. It prevents SSL-stripping attacks (such as man-in-the-middle proxies downgrading `https://` requests to plain `http://`) and stops users from clicking through certificate error warnings.
</details>

### Q4: How do you prevent internal stack traces from leaking to API clients?
<details>
<summary>View Answer</summary>
Implement a global exception handler (e.g., in Spring `@ControllerAdvice` or standard servlet error page mappings). Log the full stack trace along with a unique correlation ID (`traceId` or `errorId`) internally, and return a sanitized, generic error response to the client containing only the correlation ID and an error message code.
</details>

### Q5: What is the risk of leaving JDWP enabled on a production JVM?
<details>
<summary>View Answer</summary>
The Java Debug Wire Protocol (JDWP) has no native authentication or encryption. Anyone who connects to an open JDWP port can pause threads, inspect variables, invoke arbitrary methods, and load malicious bytecode directly into the JVM, resulting in instant Remote Code Execution (RCE).
</details>

### Q6: How should CORS be configured safely in an enterprise Java REST API?
<details>
<summary>View Answer</summary>
Never set `Access-Control-Allow-Origin: *` in combination with `Access-Control-Allow-Credentials: true`. Instead, configure an explicit allow-list of approved domain origins. Verify the incoming `Origin` header against the allow-list and reflect only verified origins in the response header.
</details>

### Q7: What is the significance of Content-Security-Policy (CSP)?
<details>
<summary>View Answer</summary>
CSP is an HTTP header that restricts the resources (scripts, images, stylesheets, iframes) that a browser is allowed to load for a given page. It is the primary defense against Cross-Site Scripting (XSS) and clickjacking attacks.
</details>

### Q8: Why must `SecureRandom` be configured with `/dev/urandom` on Linux?
<details>
<summary>View Answer</summary>
In legacy JVM configurations, `SecureRandom` could default to reading from `/dev/random`. On Linux servers without physical hardware input, `/dev/random` blocks threads indefinitely when entropy estimates drop, causing severe application latency spikes. Specifying `-Djava.security.egd=file:/dev/urandom` instructs the JVM to read from non-blocking CSPRNG entropy streams.
</details>

---

## 11. Practice Exercises

### Exercise 1: Build a Non-Root Docker Verification Test
**Task:** Write a simple shell command that inspects a container image and asserts it runs under a non-root UID.
<details>
<summary>View Solution</summary>

```bash
USER_ID=$(docker inspect --format='{{.Config.User}}' my-image:latest)
if [ "$USER_ID" = "0" ] || [ "$USER_ID" = "root" ] || [ -z "$USER_ID" ]; then
    echo "FAILED: Image runs as root!"
    exit 1
else
    echo "PASSED: Image runs as unprivileged user: $USER_ID"
fi
```
</details>

### Exercise 2: Masking Sensitive Headers in a Servlet Filter
**Task:** Write a simple servlet filter that strips or masks `Authorization` and `Cookie` headers before logging incoming HTTP requests.
<details>
<summary>View Solution</summary>

```java
import jakarta.servlet.*;
import jakarta.servlet.http.HttpServletRequest;
import java.io.IOException;

public class LogMaskingFilter implements Filter {
    @Override
    public void doFilter(ServletRequest req, ServletResponse res, FilterChain chain)
            throws IOException, ServletException {
        if (req instanceof HttpServletRequest httpReq) {
            String authHeader = httpReq.getHeader("Authorization");
            String maskedAuth = (authHeader != null) ? "Bearer ********" : "NONE";
            // Log masked header safely
            System.out.printf("Incoming request %s %s [Auth: %s]%n",
                    httpReq.getMethod(), httpReq.getRequestURI(), maskedAuth);
        }
        chain.doFilter(req, res);
    }
}
```
</details>

### Exercise 3: JVM Security Audit Script
**Task:** Write a Java main class that prints whether JVM security settings (unlimited crypto, JEP 290 filter, user identity) meet production standards.
<details>
<summary>View Solution</summary>

```java
public class SecurityAuditReport {
    public static void main(String[] args) {
        System.out.println("=== JVM SECURITY AUDIT ===");
        System.out.println("Running as User: " + System.getProperty("user.name"));
        System.out.println("Serial Filter Config: " + System.getProperty("jdk.serialFilter"));
        System.out.println("Java Vendor: " + System.getProperty("java.vendor"));
        System.out.println("Java Version: " + System.getProperty("java.version"));
        System.out.println("==========================");
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Container Hardening**: Always run with non-root UID; use minimal base images.
- **JVM Flags**: Set `-XX:+UseContainerSupport`, `-XX:MaxRAMPercentage=75.0`, and `-Djdk.serialFilter="!*"`.
- **Debug Security**: Never enable JDWP debug ports in production.
- **Transport**: Mandate TLS 1.3 and HSTS (`max-age=31536000`).
- **Data Hardening**: PreparedStatements for SQL, AES-256-GCM for encryption, Argon2id for passwords.
- **Log Masking**: Never write passwords, tokens, or PII into logs.

---

## 13. Further Reading
- CIS Benchmarks: [CIS Oracle Java Benchmark](https://www.cisecurity.org/benchmark/oracle_java)
- OWASP Top 10: [OWASP Top 10 Web Application Security Risks](https://owasp.org/www-project-top-ten/)
- NIST SP 800-53: [Security and Privacy Controls for Information Systems](https://csrc.nist.gov/publications/detail/sp/800-53/rev-5/final)
- Google Cloud Architecture Center: [Container Hardening Best Practices](https://cloud.google.com/architecture/best-practices-for-building-containers)
