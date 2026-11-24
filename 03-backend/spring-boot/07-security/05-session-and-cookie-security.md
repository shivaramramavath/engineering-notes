# Session & Cookie Security: Session Fixation, Concurrency & Spring Session Redis

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate / Advanced |
| **Prerequisites** | `01-spring-security-architecture.md`, HTTP Cookie standards (RFC 6265bis), Redis fundamentals |
| **Estimated Reading Time** | 30 minutes |
| **Status** | Active |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework** | 6.2.x / 7.0.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven / Gradle |
| **Tested With** | Spring Session Redis 3.4.x, Lettuce 6.4.x, Redis 7.4 / Valkey |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
   - [5.1 Hardened Session Management in SecurityFilterChain](#51-hardened-session-management-in-securityfilterchain)
   - [5.2 Concurrent Session Control with Maximum Sessions and Eviction](#52-concurrent-session-control-with-maximum-sessions-and-eviction)
   - [5.3 Production Cookie Hardening (HttpOnly, Secure, SameSite, Partitioned)](#53-production-cookie-hardening-httponly-secure-samesite-partitioned)
   - [5.4 Anti-Pattern: Session Fixation Vulnerability via sessionFixation().none()](#54-anti-pattern-session-fixation-vulnerability-via-sessionfixationnone)
   - [5.5 Distributed Clustering with Spring Session Redis](#55-distributed-clustering-with-spring-session-redis)
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
In stateful web applications, user authentication state is tracked via a session identifier stored in an HTTP cookie (`JSESSIONID`). If session identifiers and cookies are not managed with strict cryptographic controls, applications become vulnerable to devastating session hijacking attacks:
1. **Session Fixation**: An attacker forces a known session ID onto a victim's browser, waits for the victim to log in, and then hijacks the authenticated session.
2. **Cross-Site Scripting (XSS) Cookie Theft**: Malicious JavaScript reads the session cookie from `document.cookie` if `HttpOnly` is missing.
3. **Session Replay & Cross-Site Leaks**: Cookies sent over unencrypted HTTP or without `SameSite` restrictions leak sessions across origins.
4. **Uncontrolled Concurrent Logins**: Credential sharing or account compromises remain undetected when multiple users log into the same account simultaneously from different geographic locations.

### Historical Evolution
Early servlet containers provided basic in-memory session tracking bound to a single JVM. Spring Security introduced comprehensive session lifecycle management: automated session fixation protection (`migrateSession`), concurrent session limits (`maximumSessions`), and session expiration handling. For modern cloud-native architectures that deploy across multiple autoscaling nodes, **Spring Session Redis** transparently replaces in-memory servlet sessions with a distributed Redis key-value store, enabling seamless failover and cluster-wide session invalidation.

### Real-World Relevance
Regulatory compliance frameworks (PCI-DSS, HIPAA) mandate concurrent login restrictions, secure cookie flags, and session invalidation upon privilege escalation. Hardening session configurations prevents account takeovers in customer portals, banking platforms, and enterprise dashboards.

---

## 2. Core Concepts

### Session Creation Policies (`SessionCreationPolicy`)
- `ALWAYS`: An `HttpSession` will always be created if one does not exist.
- `NEVER`: Spring Security will never create an `HttpSession`, but will use one if it already exists.
- `IF_REQUIRED` (Default for Web): Creates an `HttpSession` only when required (e.g., storing authentication state upon successful login).
- `STATELESS`: Spring Security will never create or use an `HttpSession` (standard for REST APIs using Bearer tokens).

### Session Fixation Protection
When an anonymous user logs in, Spring Security alters the session identifier to neutralize fixation attacks:
- `changeSessionId()` (Default in Servlet 3.1+): Utilizes servlet container API to update the ID while preserving attributes.
- `migrateSession()`: Creates a completely new session, copies all existing session attributes over, and invalidates the old session.
- `newSession()`: Creates a new clean session without copying existing attributes.
- `none()`: **Prohibited in production**. Disables session fixation protection.

### Concurrent Session Control
Limits the number of simultaneous active sessions a single user can maintain:
- `maximumSessions(1)`: Restricts users to a single active session.
- `maxSessionsPreventsLogin(true)`: Rejects new login attempts if the user already has an active session.
- `maxSessionsPreventsLogin(false)` (Default): Allows the new login and **expires the previous session**, forcing the earlier device to log in again.

### Modern Cookie Security Attributes
- `HttpOnly`: Prevents client-side scripts (`document.cookie`) from accessing the cookie, mitigating XSS theft.
- `Secure`: Instructs browsers to transmit the cookie strictly over encrypted HTTPS connections.
- `SameSite`: Controls cross-site cookie transmission (`Strict`, `Lax`, or `None`) to mitigate CSRF.
- `Partitioned` (CHIPS): Isolates cookies per top-level site in cross-site iframe scenarios.

---

## 3. How It Works Internally

```
+------------------------------------------------------------------------------------+
|                         Session Fixation Defense Mechanics                         |
+------------------------------------------------------------------------------------+
|                                                                                    |
|  1. Anonymous User arrives with Session ID: 'JSESSIONID=1111'                      |
|     (Potentially injected by an attacker via phishing link!)                       |
|         |                                                                          |
|         v                                                                          |
|  2. User Submits Valid Login Credentials                                           |
|         |                                                                          |
|         v                                                                          |
|  SessionManagementFilter / ChangeSessionIdAuthenticationStrategy                   |
|     - Detects transition from anonymous to authenticated                           |
|     - Invokes HttpServletRequest.changeSessionId()                                 |
|     - Generates NEW cryptographically random Session ID: 'JSESSIONID=8888'         |
|     - Re-maps session attributes in container registry                             |
|     - Overwrites client cookie in Set-Cookie header: 'JSESSIONID=8888; HttpOnly'  |
|         |                                                                          |
|         v                                                                          |
|  3. Attacker checking 'JSESSIONID=1111' remains unauthenticated! (Attack Thwarted!)|
|                                                                                    |
+------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### Maven Dependency for Distributed Sessions
```xml
<dependency>
    <groupId>org.springframework.session</groupId>
    <artifactId>spring-session-data-redis</artifactId>
</dependency>
```

### Basic SecurityFilterChain Session Configuration
```java
@Bean
public SecurityFilterChain filterChain(HttpSecurity http) throws Exception {
    return http
        .sessionManagement(session -> session
            .sessionCreationPolicy(SessionCreationPolicy.IF_REQUIRED)
            .sessionFixation(fixation -> fixation.changeSessionId())
            .maximumSessions(1)
            .maxSessionsPreventsLogin(false)
        )
        .build();
}
```

---

## 5. Practical Examples

### 5.1 Hardened Session Management in SecurityFilterChain
```java
package com.example.security.config;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.security.config.annotation.web.builders.HttpSecurity;
import org.springframework.security.config.annotation.web.configuration.EnableWebSecurity;
import org.springframework.security.config.http.SessionCreationPolicy;
import org.springframework.security.core.session.SessionRegistry;
import org.springframework.security.core.session.SessionRegistryImpl;
import org.springframework.security.web.SecurityFilterChain;
import org.springframework.security.web.authentication.session.CompositeSessionAuthenticationStrategy;
import org.springframework.security.web.session.HttpSessionEventPublisher;

@Configuration
@EnableWebSecurity
public class StatefulSessionSecurityConfig {

    @Bean
    public SecurityFilterChain sessionSecurityFilterChain(HttpSecurity http) throws Exception {
        return http
            .sessionManagement(session -> session
                .sessionCreationPolicy(SessionCreationPolicy.IF_REQUIRED)
                // 1. Session Fixation Defense: Always rotate session ID on login
                .sessionFixation(fixation -> fixation.changeSessionId())
                // 2. Concurrent Session Control: Max 1 concurrent session per user
                .maximumSessions(1)
                // 3. Kick out earlier session when new login occurs
                .maxSessionsPreventsLogin(false)
                .expiredUrl("/login?expired=true")
                .sessionRegistry(sessionRegistry())
            )
            .authorizeHttpRequests(auth -> auth
                .requestMatchers("/login", "/public/**").permitAll()
                .anyRequest().authenticated()
            )
            .formLogin(form -> form.loginPage("/login").permitAll())
            .build();
    }

    @Bean
    public SessionRegistry sessionRegistry() {
        return new SessionRegistryImpl();
    }

    /**
     * Publishes session lifecycle events to Spring Security to track active sessions.
     */
    @Bean
    public HttpSessionEventPublisher httpSessionEventPublisher() {
        return new HttpSessionEventPublisher();
    }
}
```

### 5.2 Concurrent Session Control with Maximum Sessions and Eviction
```java
package com.example.security.service;

import org.springframework.security.core.session.SessionInformation;
import org.springframework.security.core.session.SessionRegistry;
import org.springframework.stereotype.Service;

import java.util.List;

@Service
public class ActiveSessionManagementService {

    private final SessionRegistry sessionRegistry;

    public ActiveSessionManagementService(SessionRegistry sessionRegistry) {
        this.sessionRegistry = sessionRegistry;
    }

    /**
     * Lists all active concurrent sessions across the entire system.
     */
    public List<Object> getAllActivePrincipals() {
        return sessionRegistry.getAllPrincipals();
    }

    /**
     * Forcefully evicts/invalidates all active sessions for a compromised account.
     */
    public void revokeAllUserSessions(Object principal) {
        List<SessionInformation> sessions = sessionRegistry.getAllSessions(principal, false);
        for (SessionInformation sessionInfo : sessions) {
            // Marks session expired; on next request, user is redirected to expiredUrl
            sessionInfo.expireNow();
        }
    }
}
```

### 5.3 Production Cookie Hardening (HttpOnly, Secure, SameSite, Partitioned)
```yaml
# application-prod.yaml - Production HTTP Cookie Security
server:
  servlet:
    session:
      timeout: 30m
      cookie:
        name: SECURE_SESSION_ID
        http-only: true   # Blocks JavaScript access (XSS prevention)
        secure: true      # Strictly transmitted over HTTPS
        same-site: STRICT # Prevents CSRF by withholding cookie on cross-site requests
        max-age: 1800     # 30 minutes in seconds
```

```java
package com.example.security.config;

import org.springframework.boot.web.server.Cookie;
import org.springframework.boot.web.servlet.ServletContextInitializer;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;

import jakarta.servlet.SessionCookieConfig;

@Configuration
public class CookieHardeningConfig {

    @Bean
    public ServletContextInitializer servletContextInitializer() {
        return servletContext -> {
            SessionCookieConfig cookieConfig = servletContext.getSessionCookieConfig();
            cookieConfig.setHttpOnly(true);
            cookieConfig.setSecure(true);
            cookieConfig.setName("__Host-SESSIONID"); // __Host- prefix guarantees Secure and root path!
        };
    }
}
```

### 5.4 Anti-Pattern: Session Fixation Vulnerability via sessionFixation().none()
```java
// =========================================================================
// ANTI-PATTERN: Disabling Session Fixation Protection
// =========================================================================
@Bean
public SecurityFilterChain badSecurityFilterChain(HttpSecurity http) throws Exception {
    return http
        .sessionManagement(session -> session
            // CATASTROPHIC SECURITY FLAW:
            // An attacker injects a known JSESSIONID into a victim via an XSS link or unencrypted HTTP.
            // When the victim logs in, Spring retains the SAME session ID!
            // The attacker uses the known ID to access the victim's authenticated account!
            .sessionFixation(fixation -> fixation.none()) 
        )
        .build();
}

// =========================================================================
// PRODUCTION FIX: Always enforce changeSessionId() or migrateSession()
// =========================================================================
// Default behavior: session.sessionFixation(fixation -> fixation.changeSessionId())
```

### 5.5 Distributed Clustering with Spring Session Redis
```yaml
# application.yaml - Spring Session Redis Configuration
spring:
  session:
    store-type: redis
    redis:
      namespace: spring:session:myapp
      flush-mode: on_save
      save-mode: on_set_attribute
  data:
    redis:
      host: localhost
      port: 6379
```

```java
package com.example.security.config;

import org.springframework.context.annotation.Configuration;
import org.springframework.session.data.redis.config.annotation.web.http.EnableRedisHttpSession;

/**
 * Replaces Tomcat's in-memory session manager with a distributed Redis-backed store.
 * User sessions survive application restarts and scale across 100+ server nodes!
 */
@Configuration
@EnableRedisHttpSession(maxInactiveIntervalInSeconds = 1800) // 30 minutes
public class DistributedSessionConfig {
}
```

---

## 6. Best Practices

1. **Always Use `changeSessionId()` or `migrateSession()`**: Ensure session fixation protection is active on every stateful application.
2. **Always Enable `HttpOnly` and `Secure` on Cookies**: Protect session cookies from client-side script theft and unencrypted transmission.
3. **Use `SameSite=Lax` or `SameSite=Strict`**: Prevent cross-site request forgery by configuring browser cookie dispatch policies.
4. **Register `HttpSessionEventPublisher` When Using `SessionRegistry`**: If tracking concurrent sessions or active users, the `HttpSessionEventPublisher` bean is mandatory; without it, session destruction events are never captured.
5. **Enforce Reasonable Session Timeouts**: Configure idle timeouts between 15 and 30 minutes (`server.servlet.session.timeout=30m`) to prevent abandoned session hijacking.
6. **Use `__Host-` Prefix for Sensitive Cookies**: Naming your session cookie `__Host-SESSIONID` signals modern browsers to reject the cookie if it is not sent over HTTPS or attempts to set subdomains.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Omitting `HttpSessionEventPublisher`** | `SessionRegistry` fails to detect session expirations, preventing users from logging in after session timeouts. | Register `@Bean public HttpSessionEventPublisher httpSessionEventPublisher()`. |
| **Setting `Secure = true` in Local HTTP Development** | Browsers refuse to accept or store the cookie on non-HTTPS `http://localhost`, breaking login. | Use profiles: enable `secure = true` strictly in staging and production. |
| **Using `SessionCreationPolicy.ALWAYS` on REST APIs** | Server creates an unnecessary `JSESSIONID` cookie and allocates memory for every incoming API call. | Set `SessionCreationPolicy.STATELESS`. |
| **Missing Redis Serializer Configuration in Spring Session** | Serializing non-serializable objects into the distributed session causes runtime `NotSerializableException`. | Ensure all session attributes implement `java.io.Serializable`. |
| **Disabling Session Fixation Defense (`none()`)** | Allows attackers to hijack accounts by pre-setting session IDs. | Always use `changeSessionId()`. |
| **Failing to Invalidate Session on Logout** | User logs out, but the session ID remains active on the server. | Ensure `logout.invalidateHttpSession(true)` is configured. |

---

## 8. Performance & Security Notes

### Performance
- **Distributed Session Serialization Overhead**: In Spring Session Redis, modifying session attributes triggers serialization. Keep session payloads minimal (store only user IDs, not full entity graphs).
- **Redis `flush-mode: on_save`**: Defaulting to `on_save` batches Redis updates to the end of the HTTP request, minimizing network latency compared to immediate writes.

### Security
- **Session Hijacking via Subdomains**: Without `__Host-` prefixes, compromised subdomains (e.g. `vulnerable.example.com`) can overwrite cookies for the parent domain (`example.com`).
- **Concurrent Session Limits as a Fraud Control**: Limiting concurrent sessions to 1 prevents multiple users from sharing corporate subscription credentials or detecting account theft in real-time.

---

## 9. Comparison / When to Use What

| Session Management Strategy | Storage Location | Cluster Scalable? | Memory Overhead | Best Use Case |
| :--- | :--- | :--- | :--- | :--- |
| **Servlet In-Memory (Standard)** | Single JVM Heap | No (Sticky sessions) | Low | Monolithic single-instance web apps |
| **Spring Session Redis** | Distributed Redis Cluster | **Yes (Fully scalable)** | Moderate (Network I/O) | High-availability clustered web apps |
| **Stateless JWT Tokens** | Client Cookie / Header | **Yes (No storage)** | None on server | Mobile backends & REST microservices |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is a Session Fixation attack and how does Spring Security prevent it?</strong></summary>

A Session Fixation attack occurs when an attacker obtains a valid anonymous session identifier, forces it onto a victim's browser (via a malicious link or header injection), and waits for the victim to authenticate. If the application retains the same session ID post-authentication, the attacker uses the known ID to access the victim's account. Spring Security prevents this by automatically calling `HttpServletRequest.changeSessionId()` upon successful login, generating a fresh, random session ID and transferring existing attributes.
</details>

<details>
<summary><strong>2. Why is the <code>HttpSessionEventPublisher</code> bean mandatory when configuring concurrent session control?</strong></summary>

Spring Security's `SessionRegistry` tracks active sessions by maintaining an in-memory or distributed map of users to session IDs. When a session expires or is destroyed, the servlet container fires an `HttpSessionDestroyedEvent`. Without `HttpSessionEventPublisher` registered as a Spring bean, these servlet container events are never propagated to the Spring application context, causing the `SessionRegistry` to permanently retain stale session records and block future logins.
</details>

<details>
<summary><strong>3. What is the difference between <code>SameSite=Strict</code> and <code>SameSite=Lax</code>?</strong></summary>

- `SameSite=Strict`: The browser never sends the cookie in cross-site requests, even when following standard top-level links (e.g. clicking a link from an external email will require the user to log in again).
- `SameSite=Lax`: The browser withholds the cookie on cross-site sub-resource requests (like images or cross-site AJAX `POST` requests), but *permits* sending the cookie on safe, top-level navigation links (like clicking a regular `<a>` link).
</details>

<details>
<summary><strong>4. What does the <code>__Host-</code> cookie prefix enforce?</strong></summary>

The `__Host-` prefix is a browser security standard. When a cookie name starts with `__Host-`:
1. It must be set with the `Secure` attribute over HTTPS.
2. It must originate from the secure host (not an insecure HTTP fallback).
3. It must not include a `Domain` attribute (restricting it strictly to the exact host and preventing subdomains from reading or overwriting it).
4. Its `Path` attribute must be set to `/`.
</details>

<details>
<summary><strong>5. How does Spring Session Redis handle session expiration?</strong></summary>

Spring Session Redis creates two keys in Redis for each session:
1. `spring:session:sessions:<sessionId>`: A hash containing session attributes with an expiration slightly longer than the session timeout (e.g., 5 minutes extra).
2. `spring:session:expirations:<time>`: A set containing sessions scheduled to expire at that minute.
A background task or Redis keyspace notification fires when the expiration key expires, allowing Spring Session to publish an `AbstractSessionEvent` and trigger clean teardown.
</details>

<details>
<summary><strong>6. What is the difference between <code>maxSessionsPreventsLogin(true)</code> and <code>maxSessionsPreventsLogin(false)</code>?</strong></summary>

- `maxSessionsPreventsLogin(true)`: Rejects the new login attempt with an error if the user has reached their maximum session limit, keeping the existing session active.
- `maxSessionsPreventsLogin(false)`: Allows the new login to succeed, but immediately invalidates/expires the oldest active session, causing the earlier device to receive an expired session notification on its next request.
</details>

<details>
<summary><strong>7. Why should you avoid storing large JPA entities in an <code>HttpSession</code>?</strong></summary>

JPA entities contain active database proxies and persistence context references. Storing them in an `HttpSession` leads to memory bloat, causes `LazyInitializationException` when accessed on subsequent requests, and triggers serialization failures when migrating to distributed session stores like Redis. Only lightweight DTOs or user IDs should be stored in sessions.
</details>

<details>
<summary><strong>8. What happens if a user accesses an expired session?</strong></summary>

Spring Security's `ConcurrentSessionFilter` intercepts the request, detects that `SessionInformation.isExpired() == true`, invalidates the remaining session state, clears `SecurityContextHolder`, and redirects the user to the configured `expiredUrl` (e.g., `/login?expired=true`).
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Basic Cookie Attribute Configuration in YAML</strong></summary>

**Task**: Write the `application.yaml` snippet to configure a session timeout of 15 minutes with `HttpOnly`, `Secure`, and `SameSite=Lax`.

```yaml
server:
  servlet:
    session:
      timeout: 15m
      cookie:
        http-only: true
        secure: true
        same-site: LAX
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Registering HttpSessionEventPublisher</strong></summary>

**Task**: Create a configuration class that registers the `HttpSessionEventPublisher` bean required for concurrent session monitoring.

```java
@Configuration
public class SessionPublisherConfig {
    @Bean
    public HttpSessionEventPublisher httpSessionEventPublisher() {
        return new HttpSessionEventPublisher();
    }
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Limiting Concurrent Sessions to 1</strong></summary>

**Task**: Configure `SecurityFilterChain` to enforce a maximum of 1 active session, expiring earlier sessions when a new login occurs.

```java
@Bean
public SecurityFilterChain filterChain(HttpSecurity http, SessionRegistry registry) throws Exception {
    return http
        .sessionManagement(session -> session
            .maximumSessions(1)
            .maxSessionsPreventsLogin(false)
            .sessionRegistry(registry)
        )
        .build();
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Programmatic Session Invalidation</strong></summary>

**Task**: Implement a service method that takes a user's principal and terminates all their active sessions immediately.

```java
public void terminateSessionsForUser(SessionRegistry registry, Object principal) {
    for (SessionInformation info : registry.getAllSessions(principal, false)) {
        info.expireNow();
    }
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Distributed Redis Session Test with MockMvc</strong></summary>

**Task**: Write an integration test using Testcontainers Redis asserting that a session created on one request retains attributes across subsequent calls.

```java
@Test
void testRedisSessionRetention() throws Exception {
    MvcResult result = mockMvc.perform(post("/login").param("username", "user").param("password", "pass"))
        .andExpect(status().isOk())
        .andReturn();

    HttpSession session = result.getRequest().getSession();
    assertThat(session).isNotNull();

    // Perform follow-up request with same session
    mockMvc.perform(get("/api/profile").session((MockHttpSession) session))
        .andExpect(status().isOk());
}
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Custom Cookie Partitioning Servlet Handler</strong></summary>

**Task**: Create a custom `HeaderWriter` that appends the `Partitioned;` attribute to the `Set-Cookie` header to support Google Chrome CHIPS standards for iframes.

```java
@Bean
public HeaderWriter partitionedCookieHeaderWriter() {
    return (req, res) -> {
        Collection<String> headers = res.getHeaders("Set-Cookie");
        for (String header : headers) {
            if (header.contains("JSESSIONID") && !header.contains("Partitioned")) {
                res.setHeader("Set-Cookie", header + "; Partitioned;");
            }
        }
    };
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------------+
| Fixation:     ALWAYS use sessionFixation(f -> f.changeSessionId())                     |
| Concurrency:  maximumSessions(1) + maxSessionsPreventsLogin(false)                    |
| Event Bridge: Register HttpSessionEventPublisher bean for SessionRegistry to work!    |
| Cookie Flags: HttpOnly=true (No JS access), Secure=true (HTTPS only), SameSite=Lax/Strict |
| Distributed:  Use @EnableRedisHttpSession for multi-node cloud deployments            |
| Golden Rule:  NEVER use sessionFixation().none() in production!                       |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Spring Security Session Management Documentation](https://docs.spring.io/spring-security/reference/servlet/authentication/session-management.html)
- [Spring Session Redis Official Guide](https://docs.spring.io/spring-session/reference/guides/boot-redis.html)
- [OWASP Session Management Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Session_Management_Cheat_Sheet.html)
- [MDN Web Docs: Using HTTP Cookies Securely](https://developer.mozilla.org/en-US/docs/Web/HTTP/Cookies)
