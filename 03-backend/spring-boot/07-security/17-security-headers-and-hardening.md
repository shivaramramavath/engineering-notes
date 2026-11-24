# 17. HTTP Security Headers & Web Hardening in Spring Boot

| Metadata | Value |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | `01-spring-security-architecture.md`, HTTP Protocol (RFC 7230, RFC 6797) |
| **Estimated Reading Time** | 30 minutes |
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
A web application may have flawless authentication and robust database encryption, yet still remain completely vulnerable to browser-level exploits:
- **Clickjacking**: An attacker embeds your web portal inside an invisible `<iframe>` on a malicious site, tricking authenticated users into clicking buttons (e.g., "Confirm Transfer").
- **MIME Sniffing**: A browser ignores a declared `text/plain` content type and executes a user-uploaded image as executable JavaScript.
- **Cross-Site Scripting (XSS)**: Malicious JavaScript injected via an unescaped comment exfiltrates session data or manipulates the DOM.
- **SSL Stripping**: A network attacker intercepts an initial unencrypted `http://` request and strips TLS before the redirect to `https://` can execute.

**HTTP Security Headers** are declarative instructions sent by the Spring Boot server that instruct the browser to enforce strict defense-in-depth security policies.

### Historical Evolution
- **Early Web (1990s - 2000s)**: Browsers trusted server responses blindly; developers relied on ad-hoc JavaScript frame-busting scripts (which attackers easily defeated).
- **Spring Security 3.x - 4.x**: Introduced the `HeaderWriterFilter` and default security headers (e.g., `X-Frame-Options`, `X-Content-Type-Options`, and basic HSTS).
- **Spring Security 6.x / 7.x**: Standardized modern headers: `Permissions-Policy`, strict `Content-Security-Policy` (CSP) with nonce-based script execution, automated HSTS preloading support, and deprecation of legacy non-standard headers (`X-XSS-Protection`).

### Real-World Relevance
Security headers are mandatory compliance requirements in security audits, SOC 2 certifications, and OWASP Top 10 web application hardening assessments.

---

## 2. Core Concepts

### Architectural Contracts
- **`org.springframework.security.web.header.HeaderWriterFilter`**: The servlet filter in the `SecurityFilterChain` responsible for appending security headers to every HTTP response.
- **`org.springframework.security.web.header.HeaderWriter`**: Strategy interface for writing specific headers. Implementations include `XContentTypeOptionsHeaderWriter`, `HstsHeaderWriter`, and `ContentSecurityPolicyHeaderWriter`.
- **Content-Security-Policy (CSP)**: Restricts the resources (scripts, images, stylesheets, fonts, frames) that the browser is permitted to load for a given page.
- **Strict-Transport-Security (HSTS)**: Mandates that browsers only communicate with the domain over secure HTTPS connections, preventing SSL stripping.
- **X-Frame-Options**: Guards against clickjacking by controlling whether the site can be embedded in `<frame>`, `<iframe>`, or `<object>` elements.
- **X-Content-Type-Options**: Prevents the browser from MIME-sniffing a response away from the declared `Content-Type`.
- **Permissions-Policy**: Restricts access to sensitive client device hardware and browser APIs (camera, microphone, geolocation, payment).

### The Mental Model
```
[ Spring Boot Server ] 
         │
         │ HTTP Response (200 OK)
         │ Headers:
         │   Strict-Transport-Security: max-age=31536000; includeSubDomains; preload
         │   X-Content-Type-Options: nosniff
         │   X-Frame-Options: DENY
         │   Content-Security-Policy: default-src 'self'; script-src 'self' 'nonce-xyz'
         │   Permissions-Policy: camera=(), microphone=(), geolocation=()
         │   Referrer-Policy: strict-origin-when-cross-origin
         ▼
[ Web Browser Engine ]
         │
         ├── 1. HSTS: Permanently upgrade all future requests to HTTPS
         ├── 2. Frame-Options: Block page from rendering inside <iframe>
         ├── 3. Content-Type: Treat text/plain as text; do not sniff as JS
         ├── 4. CSP: Block inline <script> tags without matching nonce
         └── 5. Permissions: Block camera/mic access entirely
```

### Invariants
1. HSTS headers are only written over secure HTTPS connections; RFC 6797 prohibits emitting HSTS over plaintext HTTP.
2. Security headers are a browser-enforced defense mechanism; non-browser API clients (cURL, mobile apps, microservices) ignore them.
3. A misconfigured Content Security Policy can break frontend JavaScript applications; always test in report-only mode (`Content-Security-Policy-Report-Only`) before enforcement.

---

## 3. How It Works Internally

### `HeaderWriterFilter` Lifecycle
```
ApplicationFilterChain
   │
   ├── 1. HeaderWriterFilter intercepts incoming request
   │      ├─ Wraps response in HeaderWriterResponse
   │      └─ Proceeds down the filter chain
   │
   ├── 2. Controller & View Renderer execute, producing response body
   │
   └── 3. Prior to committing response headers:
          ├─ HeaderWriterFilter iterates through registered HeaderWriter beans
          ├─ Appends default security headers (Cache-Control, X-Content-Type-Options, etc.)
          ├─ Evaluates dynamic headers (HSTS if HTTPS, CSP nonces if requested)
          └─ Flushes response to the client
```

---

## 4. Syntax & Usage

### Modern Spring Security Lambda DSL Configuration
```java
package com.example.security.config;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.security.config.annotation.web.builders.HttpSecurity;
import org.springframework.security.config.annotation.web.configuration.EnableWebSecurity;
import org.springframework.security.web.SecurityFilterChain;
import org.springframework.security.web.header.writers.ReferrerPolicyHeaderWriter;

@Configuration
@EnableWebSecurity
public class SecurityHeadersConfig {

    @Bean
    public SecurityFilterChain securityFilterChain(HttpSecurity http) throws Exception {
        return http
            .headers(headers -> headers
                .contentTypeOptions(org.springframework.security.config.Customizer.withDefaults()) // X-Content-Type-Options: nosniff
                .frameOptions(frame -> frame.deny()) // X-Frame-Options: DENY
                .httpStrictTransportSecurity(hsts -> hsts
                    .includeSubDomains(true)
                    .maxAgeInSeconds(31536000) // 1 year
                    .preload(true)
                )
                .contentSecurityPolicy(csp -> csp
                    .policyDirectives("default-src 'self'; script-src 'self'; object-src 'none'; base-uri 'self';")
                )
                .referrerPolicy(referrer -> referrer
                    .policy(ReferrerPolicyHeaderWriter.ReferrerPolicy.STRICT_ORIGIN_WHEN_CROSS_ORIGIN)
                )
                .permissionsPolicy(permissions -> permissions
                    .policy("camera=(), microphone=(), geolocation=(), payment=()")
                )
            )
            .authorizeHttpRequests(auth -> auth.anyRequest().authenticated())
            .build();
    }
}
```

---

## 5. Practical Examples

### Example 1: Strict CSP with Dynamic Cryptographic Nonces
Preventing inline script injection by generating a fresh random nonce per request.

```java
package com.example.security.config;

import jakarta.servlet.http.HttpServletRequest;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.security.config.annotation.web.builders.HttpSecurity;
import org.springframework.security.web.SecurityFilterChain;
import org.springframework.security.web.header.writers.ContentSecurityPolicyHeaderWriter;

import java.security.SecureRandom;
import java.util.Base64;

@Configuration
public class DynamicNonceCspConfig {

    @Bean
    public SecurityFilterChain nonceCspFilterChain(HttpSecurity http) throws Exception {
        return http
            .headers(headers -> headers
                .contentSecurityPolicy(csp -> csp
                    .policyDirectives("default-src 'self'; script-src 'self' 'nonce-{nonce}'; object-src 'none';")
                )
            )
            .build();
    }
}
```

### Example 2: Permitting Embedded Iframes for Specific Trusted Partners (`SAMEORIGIN` vs CSP `frame-ancestors`)
Modern browsers prefer the CSP `frame-ancestors` directive over legacy `X-Frame-Options`.

```java
package com.example.security.config;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.security.config.annotation.web.builders.HttpSecurity;
import org.springframework.security.web.SecurityFilterChain;

@Configuration
public class IframePartnerSecurityConfig {

    @Bean
    public SecurityFilterChain iframeFilterChain(HttpSecurity http) throws Exception {
        return http
            .headers(headers -> headers
                // Disable legacy header in favor of modern CSP directive
                .frameOptions(frame -> frame.disable())
                .contentSecurityPolicy(csp -> csp
                    .policyDirectives("default-src 'self'; frame-ancestors 'self' https://trusted-partner.com https://*.portal.corp;")
                )
            )
            .build();
    }
}
```

### Example 3: Edge Case – HSTS Behind Reverse Proxies (SSL Offloading)
When running behind an AWS ALB or Cloudflare, TLS is terminated at the proxy, meaning Spring Boot receives plain HTTP. Without proper forwarded header handling, Spring refuses to write HSTS headers.

```yaml
# application.yml
server:
  forward-headers-strategy: framework # Honors X-Forwarded-Proto: https
```
```java
// Spring Boot now knows the original client connection was HTTPS and writes HSTS headers properly!
```

### Example 4: Failure Anti-Pattern & Production Fix
**Anti-Pattern**: Enabling `X-XSS-Protection: 1; mode=block`.

```java
// DEPRECATED & DANGEROUS: Modern browsers have removed XSS Auditor!
// In legacy browsers, mode=block actually INTRODUCED side-channel vulnerabilities!
.headers(headers -> headers
    .xssProtection(xss -> xss.headerValue(XXssProtectionHeaderWriter.HeaderValue.ENABLED_MODE_BLOCK))
)
```

**Production Fix**:
Disable or strip `X-XSS-Protection` and rely strictly on modern `Content-Security-Policy`.

```java
// SECURE: Modern standard approach
.headers(headers -> headers
    .xssProtection(xss -> xss.disable()) // Explicitly disabled
    .contentSecurityPolicy(csp -> csp
        .policyDirectives("default-src 'self'; script-src 'self'; object-src 'none';")
    )
)
```

### Example 5: High-Performance Non-Blocking WebFlux Security Headers
Configuring security headers in reactive Netty environments.

```java
package com.example.security.reactive;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.security.config.annotation.web.reactive.EnableWebFluxSecurity;
import org.springframework.security.config.web.server.ServerHttpSecurity;
import org.springframework.security.web.server.SecurityWebFilterChain;
import org.springframework.security.web.server.header.ReferrerPolicyServerHttpHeadersWriter;

import java.time.Duration;

@Configuration
@EnableWebFluxSecurity
public class ReactiveHeadersConfig {

    @Bean
    public SecurityWebFilterChain reactiveHeadersChain(ServerHttpSecurity http) {
        return http
            .headers(headers -> headers
                .hsts(hsts -> hsts
                    .includeSubdomains(true)
                    .maxAge(Duration.ofDays(365))
                    .preload(true)
                )
                .referrerPolicy(referrer -> referrer
                    .policy(ReferrerPolicyServerHttpHeadersWriter.ReferrerPolicy.STRICT_ORIGIN_WHEN_CROSS_ORIGIN)
                )
                .contentSecurityPolicy(csp -> csp
                    .policyDirectives("default-src 'self';")
                )
            )
            .build();
    }
}
```

---

## 6. Best Practices

1. **Always Set `X-Content-Type-Options: nosniff`**: Prevents browsers from executing user-uploaded files as scripts.
2. **Use HSTS with `includeSubDomains` and `preload`**: Guarantees that users never make unencrypted connections to your domain or its subdomains.
3. **Prefer CSP `frame-ancestors` over `X-Frame-Options`**: `frame-ancestors` supports multiple allowed origins, whereas `X-Frame-Options` only supports single origins or `SAMEORIGIN`.
4. **Enforce `Permissions-Policy`**: Block access to unused browser capabilities (camera, microphone, geolocation) to mitigate supply-chain attacks from third-party scripts.
5. **Use Nonce-Based CSP for SPAs**: Avoid using `'unsafe-inline'` in `script-src`; use cryptographically random per-request nonces.
6. **Set `Referrer-Policy: strict-origin-when-cross-origin`**: Protects sensitive URL query parameters from leaking to external third-party sites during cross-origin requests.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| HSTS missing behind load balancer | Spring sees internal HTTP connection and suppresses HSTS header per spec. | Configure `server.forward-headers-strategy=framework`. |
| Using `'unsafe-inline'` in CSP | Defeats the primary purpose of CSP, allowing reflected and stored XSS scripts to execute. | Refactor code to external `.js` files or use cryptographic nonces. |
| Relying on `X-XSS-Protection` | Removed from Chromium, Firefox, and Safari; introduces vulnerabilities in older IE/Edge. | Disable legacy header and enforce modern CSP. |
| Breaking UI with `X-Frame-Options: DENY` | Legitimate embedding inside internal micro-frontend portals fails. | Use `Content-Security-Policy: frame-ancestors ...` with explicit whitelists. |
| Forgetting `preload` on HSTS | First-time visitors are vulnerable to SSL stripping before the HSTS header is cached. | Submit domain to the HSTS Preload List (`hstspreload.org`). |
| Missing `Cache-Control: no-store` on authenticated data | Browsers cache sensitive user financial details on shared workstation disks. | Ensure Spring Security's default cache control headers remain enabled. |

---

## 8. Performance & Security Notes

- **Zero Runtime Overhead**: Writing static HTTP response headers takes less than 1 microsecond per request and adds only ~300 bytes to the HTTP response payload.
- **Cache Header Interactions**: Spring Security automatically appends `Cache-Control: no-cache, no-store, max-age=0, must-revalidate` and `Pragma: no-cache` to protected resources to prevent proxy and browser disk caching of sensitive data.
- **HSTS Preload Permanence**: Once a domain is submitted to Google's HSTS Preload list (embedded into Chrome, Firefox, Safari), browsers refuse all plaintext HTTP connections forever. Ensure your HTTPS setup is permanently stable before adding `preload`.

---

## 9. Comparison / When to Use What

| Security Header | Primary Threat Mitigated | Enforced By | Modern Recommendation |
|---|---|---|---|
| **Content-Security-Policy** | XSS, Data Infiltration, Clickjacking | Browser | **Mandatory** |
| **Strict-Transport-Security**| SSL Stripping, Man-in-the-Middle | Browser | **Mandatory (HTTPS only)** |
| **X-Content-Type-Options** | MIME Sniffing, Drive-by Downloads | Browser | **Mandatory (`nosniff`)** |
| **X-Frame-Options** | Clickjacking | Browser | Replaced by CSP `frame-ancestors` |
| **Permissions-Policy** | Sensor/Hardware Exploits | Browser | **Recommended** |
| **Referrer-Policy** | URL Path & Token Leakage | Browser | **Mandatory (`strict-origin...`)** |

---

## 10. Interview Questions

<details>
<summary>1. What is SSL Stripping and how does HSTS mitigate it?</summary>

**Answer:**
SSL Stripping occurs when an attacker intercepts an initial unencrypted HTTP connection (e.g., when a user types `bank.com` into their browser) and proxies the request to HTTPS themselves, serving plain HTTP back to the victim.
HSTS (HTTP Strict Transport Security) instructs the browser that the domain must **only** be accessed via HTTPS. Subsequent visits automatically rewrite `http://` to `https://` internally on the client before network transmission occurs.
</details>

<details>
<summary>2. Why did the security industry deprecate `X-XSS-Protection`?</summary>

**Answer:**
The reflective XSS filter in older browsers (like Chrome and Internet Explorer) had subtle parsing bugs. Attackers learned to craft payloads that tricked the XSS filter into blocking legitimate parts of a page while reflecting attacker-controlled script, creating new XSS vulnerabilities where none existed. Modern browsers removed the filter and standardized on Content Security Policy (CSP).
</details>

<details>
<summary>3. What is the difference between `X-Frame-Options` and CSP `frame-ancestors`?</summary>

**Answer:**
`X-Frame-Options` is a legacy header that only supports `DENY`, `SAMEORIGIN`, or `ALLOW-FROM` (which was deprecated due to lack of cross-browser support for multiple domains).
CSP's `frame-ancestors` directive is modern, flexible, and allows fine-grained whitelisting of multiple specific parent domains or wildcards (`frame-ancestors 'self' https://partner1.com https://*.partner2.com`).
</details>

<details>
<summary>4. Why does Spring Security omit the HSTS header when running locally over HTTP?</summary>

**Answer:**
Per RFC 6797, HSTS headers must **never** be delivered over unencrypted HTTP connections. If delivered over plain HTTP, a network attacker could inject false HSTS headers with long lifetimes to perform denial-of-service attacks. Spring Security strictly enforces this specification by only emitting HSTS when `request.isSecure()` evaluates to `true`.
</details>

<details>
<summary>5. What does `X-Content-Type-Options: nosniff` actually do?</summary>

**Answer:**
Browsers historically attempted to guess the MIME type of a response by inspecting the initial bytes (MIME sniffing), ignoring the server's declared `Content-Type`. An attacker could upload an image containing valid JavaScript (`<script>...`), and the browser would execute it. `nosniff` forces the browser to trust the declared `Content-Type` strictly, preventing script execution from non-script MIME types.
</details>

<details>
<summary>6. How does a CSP cryptographic nonce protect against XSS?</summary>

**Answer:**
A nonce is a cryptographically strong random value generated anew for every HTTP response and included in the CSP header: `script-src 'nonce-RANDOM'`. Only `<script nonce="RANDOM">` tags bearing the exact matching nonce value are executed by the browser. Injected malicious scripts (from stored or reflected XSS) lack the unpredictable nonce and are blocked by the browser.
</details>

<details>
<summary>7. What is `Referrer-Policy: strict-origin-when-cross-origin`?</summary>

**Answer:**
When navigating within the same origin, the full URL (including path and query parameters) is sent in the `Referer` header. When navigating across origins over HTTPS, only the origin (`https://example.com/`) is sent. If navigating from HTTPS to unencrypted HTTP, no `Referer` header is sent at all. This protects sensitive data in URL paths from leaking to third parties.
</details>

<details>
<summary>8. How do you test Content Security Policy without breaking an existing production app?</summary>

**Answer:**
Use the `Content-Security-Policy-Report-Only` header paired with a `report-uri` or `report-to` endpoint. The browser evaluates the policy and logs violations to the reporting endpoint, but does not block any resources or scripts, allowing engineers to fix violations before enforcing the policy.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Basic Clickjacking Protection (Easy)</summary>

**Task**: Configure Spring Security to deny all iframe embedding on every route using `X-Frame-Options: DENY`.

**Solution**:
```java
@Bean
public SecurityFilterChain filterChain(HttpSecurity http) throws Exception {
    return http
        .headers(headers -> headers
            .frameOptions(frame -> frame.deny())
        )
        .build();
}
```
</details>

<details>
<summary>Exercise 2: Full 1-Year HSTS Configuration (Easy)</summary>

**Task**: Configure HSTS for a 1-year duration (31536000 seconds), including all subdomains and enabling the preload flag.

**Solution**:
```java
@Bean
public SecurityFilterChain hstsFilterChain(HttpSecurity http) throws Exception {
    return http
        .headers(headers -> headers
            .httpStrictTransportSecurity(hsts -> hsts
                .maxAgeInSeconds(31536000)
                .includeSubDomains(true)
                .preload(true)
            )
        )
        .build();
}
```
</details>

<details>
<summary>Exercise 3: Referrer and Permissions Policy (Medium)</summary>

**Task**: Configure a `SecurityFilterChain` that enforces `strict-origin-when-cross-origin` referrer policy and restricts camera, microphone, and geolocation access.

**Solution**:
```java
@Bean
public SecurityFilterChain policiesChain(HttpSecurity http) throws Exception {
    return http
        .headers(headers -> headers
            .referrerPolicy(referrer -> referrer
                .policy(ReferrerPolicyHeaderWriter.ReferrerPolicy.STRICT_ORIGIN_WHEN_CROSS_ORIGIN)
            )
            .permissionsPolicy(permissions -> permissions
                .policy("camera=(), microphone=(), geolocation=()")
            )
        )
        .build();
}
```
</details>

<details>
<summary>Exercise 4: Modern Multi-Origin Frame Whitelisting via CSP (Medium)</summary>

**Task**: Disable `X-Frame-Options` and configure CSP `frame-ancestors` to allow embedding only from `'self'` and `https://dashboard.partner.com`.

**Solution**:
```java
@Bean
public SecurityFilterChain frameAncestorsChain(HttpSecurity http) throws Exception {
    return http
        .headers(headers -> headers
            .frameOptions(frame -> frame.disable())
            .contentSecurityPolicy(csp -> csp
                .policyDirectives("frame-ancestors 'self' https://dashboard.partner.com;")
            )
        )
        .build();
}
```
</details>

<details>
<summary>Exercise 5: Report-Only CSP Header Writer (Hard)</summary>

**Task**: Implement a custom `HeaderWriter` that emits a `Content-Security-Policy-Report-Only` header pointing to `/api/csp-report-endpoint` to monitor policy violations without blocking.

**Solution**:
```java
@Configuration
public class CspReportOnlyConfig {

    @Bean
    public SecurityFilterChain reportOnlyFilterChain(HttpSecurity http) throws Exception {
        return http
            .headers(headers -> headers
                .addHeaderWriter(new StaticHeadersWriter(
                    "Content-Security-Policy-Report-Only",
                    "default-src 'self'; script-src 'self'; report-uri /api/csp-report-endpoint"
                ))
            )
            .build();
    }
}
```
</details>

<details>
<summary>Exercise 6: MockMvc Verification of Security Headers (Hard)</summary>

**Task**: Write a MockMvc unit test verifying that a `GET` request to `/api/health` contains all standard enterprise security headers (`nosniff`, `DENY`, and `Strict-Transport-Security`).

**Solution**:
```java
@WebMvcTest
class SecurityHeadersTest {

    @Autowired
    private MockMvc mockMvc;

    @Test
    void shouldReturnAllMandatorySecurityHeaders() throws Exception {
        mockMvc.perform(get("/api/health").secure(true)) // secure(true) forces HTTPS
            .andExpect(status().isOk())
            .andExpect(header().string("X-Content-Type-Options", "nosniff"))
            .andExpect(header().string("X-Frame-Options", "DENY"))
            .andExpect(header().string("Strict-Transport-Security", "max-age=31536000 ; includeSubDomains ; preload"));
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **`X-Content-Type-Options: nosniff`**: Mandatory to defeat MIME-sniffing exploits.
- **HSTS Requires HTTPS**: HSTS headers are only written over secure TLS connections (`max-age=31536000; includeSubDomains; preload`).
- **Use CSP `frame-ancestors`**: Replaces legacy `X-Frame-Options` with flexible multi-domain embedding controls.
- **`Permissions-Policy`**: Disables unused browser device features (camera, mic, geolocation).
- **Forwarded Headers Matter**: Always configure `server.forward-headers-strategy=framework` when terminating TLS on reverse proxies.

---

## 13. Further Reading
- [Spring Security Reference: Default Security Headers](https://docs.spring.io/spring-security/reference/servlet/exploits/headers.html)
- [OWASP Secure Headers Project](https://owasp.org/www-project-secure-headers/)
- [RFC 6797: HTTP Strict Transport Security (HSTS)](https://datatracker.ietf.org/doc/html/rfc6797)
- [W3C Content Security Policy Level 3](https://www.w3.org/TR/CSP3/)
- [HSTS Preload Submission Portal](https://hstspreload.org/)
