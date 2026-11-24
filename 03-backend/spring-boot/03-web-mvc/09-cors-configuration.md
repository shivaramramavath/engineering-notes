# CORS Configuration in Spring Boot

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate to Advanced |
| **Prerequisites** | HTTP Specifications (RFC 6454), Spring Security Filter Chain, Spring MVC |
| **Estimated Reading Time** | 22 minutes |
| **Status** | Production-Ready |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Baseline** | OpenJDK 21 LTS / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | OpenJDK 21.0.2, Spring Security 6.4 |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Configuration](#4-syntax--configuration)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance & Security Considerations](#8-performance--security-considerations)
9. [Comparison: Global vs Controller vs Security CORS](#9-comparison-global-vs-controller-vs-security-cors)
10. [Interview Questions & Deep Dives](#10-interview-questions--deep-dives)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference](#12-summary--quick-reference)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

In modern decoupled architectures, single-page web applications (React, Angular, Vue) are hosted on distinct domains or subdomains from backend REST API services (e.g., `https://app.example.com` calling `https://api.example.com`).

### The Problem: Same-Origin Policy (SOP)
Web browsers enforce the Same-Origin Policy (SOP) for security. A browser prevents client scripts running on origin A from accessing response data from origin B unless origin B explicitly gives permission. Without properly configured Cross-Origin Resource Sharing (CORS):
1. Browsers reject API responses with JavaScript console errors (`CORS Missing Allow Origin`).
2. Preflight `OPTIONS` requests fail with HTTP 401 or 403 when Spring Security is active.
3. Developers resort to insecure wildcards (`allowedOrigins = "*"`) combined with credentials, creating severe cross-site data theft vulnerabilities.

### The Solution: Spring Boot CORS Architecture
Spring Boot provides native, multi-tiered CORS configuration mechanisms: fine-grained controller annotations (`@CrossOrigin`), global MVC mapping rules (`WebMvcConfigurer.addCorsMappings`), and dedicated Spring Security integration via `CorsConfigurationSource`.

---

## 2. Core Concepts

### Mental Model: The CORS Preflight Handshake
```
   Browser (https://app.example.com)                   Spring Boot API (https://api.example.com)
            │                                                      │
            │ 1. Preflight Request (HTTP OPTIONS)                  │
            │    Origin: https://app.example.com                   │
            │    Access-Control-Request-Method: POST               │
            │    Access-Control-Request-Headers: Authorization     │
            │─────────────────────────────────────────────────────►│
            │                                                      │ Evaluates CorsConfiguration:
            │                                                      │ • Is Origin allowed? YES
            │                                                      │ • Is Method allowed? YES
            │                                                      │ • Are Headers allowed? YES
            │ 2. Preflight Response (HTTP 204 / 200)               │
            │    Access-Control-Allow-Origin: https://app.example.com
            │    Access-Control-Allow-Methods: GET, POST, OPTIONS  │
            │    Access-Control-Allow-Headers: Authorization       │
            │    Access-Control-Max-Age: 3600                      │
            │◄─────────────────────────────────────────────────────│
            │                                                      │
            │ 3. Actual Request (HTTP POST /orders)                │
            │    Origin: https://app.example.com                   │
            │    Authorization: Bearer <token>                     │
            │─────────────────────────────────────────────────────►│
            │                                                      │ Executes Controller Method
            │ 4. Actual Response (HTTP 201 Created)                │
            │    Access-Control-Allow-Origin: https://app.example.com
            │    Access-Control-Allow-Credentials: true            │
            │◄─────────────────────────────────────────────────────│
```

### Invariants
1. **Security Filter Order**: When Spring Security is present, CORS preflight requests **must be processed before** authentication filters. If an unauthenticated `OPTIONS` request hits a security filter first, it will be rejected with HTTP 401/403 before reaching MVC CORS logic.
2. **Wildcard & Credentials Mutually Exclusive**: Under standard browser CORS specifications, `Access-Control-Allow-Origin: *` cannot be combined with `Access-Control-Allow-Credentials: true`. Browsers will strictly block the response.
3. **CORS is a Browser-Only Constraint**: Server-to-server HTTP calls (e.g. via cURL, Postman, or RestClient) do not enforce CORS; CORS is purely a browser-enforced security sandbox.

---

## 3. How It Works Internally

### The Preflight Resolution Pipeline
1. When an HTTP `OPTIONS` request arrives with `Origin` and `Access-Control-Request-Method` headers, Spring identifies it as a preflight request.
2. In Spring Security, `CorsFilter` intercepts the request at the very beginning of the `SecurityFilterChain`.
3. It delegates lookup to a `CorsConfigurationSource` (e.g. `UrlBasedCorsConfigurationSource`).
4. `CorsProcessor` (default `DefaultCorsProcessor`) inspects the incoming request headers against the configured `CorsConfiguration`:
   - Checks allowed origins / origin patterns.
   - Checks allowed methods and headers.
   - Appends matching `Access-Control-Allow-*` headers to the response.
5. If valid, the preflight request returns HTTP 200 or 204 immediately, short-circuiting downstream filters and controllers.

---

## 4. Syntax & Configuration

### Standard Spring Security CORS Configuration
```java
package com.example.web.config;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.security.config.Customizer;
import org.springframework.security.config.annotation.web.builders.HttpSecurity;
import org.springframework.security.web.SecurityFilterChain;
import org.springframework.web.cors.CorsConfiguration;
import org.springframework.web.cors.CorsConfigurationSource;
import org.springframework.web.cors.UrlBasedCorsConfigurationSource;

import java.util.List;

@Configuration(proxyBeanMethods = false)
public class SecurityCorsConfig {

    @Bean
    public SecurityFilterChain filterChain(HttpSecurity http) throws Exception {
        http
            .cors(Customizer.withDefaults()) // Looks up CorsConfigurationSource bean
            .authorizeHttpRequests(auth -> auth
                .requestMatchers("/api/public/**").permitAll()
                .anyRequest().authenticated()
            );
        return http.build();
    }

    @Bean
    public CorsConfigurationSource corsConfigurationSource() {
        CorsConfiguration config = new CorsConfiguration();
        
        // Use allowedOriginPatterns for pattern matching (subdomains, localhost ports)
        config.setAllowedOriginPatterns(List.of(
            "https://*.example.com",
            "https://example.com",
            "http://localhost:[*]"
        ));
        
        config.setAllowedMethods(List.of("GET", "POST", "PUT", "DELETE", "PATCH", "OPTIONS"));
        config.setAllowedHeaders(List.of("Authorization", "Content-Type", "X-Requested-With", "X-Correlation-ID"));
        config.setExposedHeaders(List.of("X-Total-Count", "Content-Disposition"));
        config.setAllowCredentials(true);
        config.setMaxAge(3600L); // Cache preflight response for 1 hour

        UrlBasedCorsConfigurationSource source = new UrlBasedCorsConfigurationSource();
        source.registerCorsConfiguration("/**", config);
        return source;
    }
}
```

---

## 5. Practical Examples

### Example 1: Controller-Level `@CrossOrigin` Annotation

```java
package com.example.web.controller;

import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.util.Map;

@RestController
@RequestMapping("/api/v1/public-stats")
@CrossOrigin(
    origins = "https://partner-portal.com",
    methods = {RequestMethod.GET, RequestMethod.OPTIONS},
    maxAge = 1800
)
public class PublicStatsController {

    @GetMapping
    public ResponseEntity<Map<String, Integer>> getPlatformStats() {
        return ResponseEntity.ok(Map.of("activeUsers", 14200, "totalOrders", 884000));
    }
}
```

### Example 2: Global MVC CORS Configuration (Without Spring Security)

```java
package com.example.web.config;

import org.springframework.context.annotation.Configuration;
import org.springframework.web.servlet.config.annotation.CorsRegistry;
import org.springframework.web.servlet.config.annotation.WebMvcConfigurer;

@Configuration(proxyBeanMethods = false)
public class WebMvcCorsConfig implements WebMvcConfigurer {

    @Override
    public void addCorsMappings(CorsRegistry registry) {
        registry.addMapping("/api/**")
                .allowedOriginPatterns("https://*.mycompany.internal")
                .allowedMethods("GET", "POST", "PUT", "DELETE")
                .allowedHeaders("*")
                .allowCredentials(true)
                .maxAge(3600);
    }
}
```

### Example 3: Edge Case — Dynamic Tenant-Based CORS Origin Resolution

In multi-tenant SaaS systems, allowed origins reside in a database and vary per tenant.

```java
package com.example.web.cors;

import jakarta.servlet.http.HttpServletRequest;
import org.springframework.stereotype.Component;
import org.springframework.web.cors.CorsConfiguration;
import org.springframework.web.cors.CorsConfigurationSource;

import java.util.List;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;

@Component
public class DynamicTenantCorsConfigurationSource implements CorsConfigurationSource {

    // Cache of active tenant domains loaded from database
    private final Set<String> registeredTenantOrigins = ConcurrentHashMap.newKeySet();

    public DynamicTenantCorsConfigurationSource() {
        registeredTenantOrigins.add("https://tenant-alpha.client-app.com");
        registeredTenantOrigins.add("https://tenant-beta.client-app.com");
    }

    @Override
    public CorsConfiguration getCorsConfiguration(HttpServletRequest request) {
        String origin = request.getHeader("Origin");
        if (origin == null || !registeredTenantOrigins.contains(origin)) {
            return null; // Null indicates CORS is not allowed for this origin
        }

        CorsConfiguration config = new CorsConfiguration();
        config.setAllowedOrigins(List.of(origin)); // Exact matching origin returned
        config.setAllowedMethods(List.of("GET", "POST", "PUT", "DELETE", "OPTIONS"));
        config.setAllowedHeaders(List.of("Authorization", "Content-Type"));
        config.setAllowCredentials(true);
        config.setMaxAge(1800L);
        return config;
    }
}
```

### Example 4: Anti-Pattern vs Production Fix

#### Anti-Pattern: Using Insecure Wildcard with Credentials Enabled
```java
// ANTI-PATTERN: Insecure wildcard origin combined with credentials
CorsConfiguration config = new CorsConfiguration();
config.addAllowedOrigin("*"); // Browsers reject this when allowCredentials is true!
config.setAllowCredentials(true);
```

#### Production Fix: Use `allowedOriginPatterns` with Specific Subdomain Whitelists
```java
// PRODUCTION FIX: Use pattern matching to validate origin dynamically
CorsConfiguration config = new CorsConfiguration();
config.setAllowedOriginPatterns(List.of("https://*.trusted-partner.com", "https://localhost:[*]"));
config.setAllowCredentials(true); // Works cleanly; Spring reflects matching origin in header
```

### Example 5: High-Performance Custom Preflight Cache Header Tuning

```java
package com.example.web.config;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.web.cors.CorsConfiguration;
import org.springframework.web.cors.UrlBasedCorsConfigurationSource;
import org.springframework.web.filter.CorsFilter;

import java.util.List;

@Configuration(proxyBeanMethods = false)
public class HighPerformanceCorsFilterConfig {

    /**
     * Instantiating CorsFilter as a standalone Servlet Filter bean guarantees
     * execution at the outermost boundary of the container.
     */
    @Bean
    public CorsFilter corsFilter() {
        CorsConfiguration config = new CorsConfiguration();
        config.setAllowedOriginPatterns(List.of("https://app.example.com"));
        config.setAllowedMethods(List.of("GET", "POST", "PUT", "DELETE", "OPTIONS"));
        config.setAllowedHeaders(List.of("*"));
        config.setAllowCredentials(true);
        // Chromium caps max-age at 7200s (2 hours); Firefox caps at 86400s (24 hours)
        config.setMaxAge(7200L);

        UrlBasedCorsConfigurationSource source = new UrlBasedCorsConfigurationSource();
        source.registerCorsConfiguration("/api/**", config);
        return new CorsFilter(source);
    }
}
```

---

## 6. Best Practices

1. **Use `CorsConfigurationSource` with Spring Security**: If Spring Security is present on the classpath, configure CORS through `http.cors()` and define a `CorsConfigurationSource` bean.
2. **Never Use `*` for Origins in Authenticated APIs**: Always whitelist explicit domains or domain patterns using `allowedOriginPatterns`.
3. **Configure `maxAge` to Minimize Preflight Traffic**: Set `maxAge` to 3600 or 7200 seconds to prevent browsers from issuing redundant `OPTIONS` requests before every API call.
4. **Expose Custom Headers**: If the client needs access to custom headers (such as `X-Total-Count` or `Content-Disposition`), explicitly list them in `exposedHeaders`.
5. **Handle Subdomains via `allowedOriginPatterns`**: Use `https://*.example.com` to safely support dynamic staging or PR preview environments.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Using `WebMvcConfigurer` while Spring Security is Enabled** | Spring Security intercepts and rejects `OPTIONS` preflight requests with HTTP 401/403 before Spring MVC ever runs. | Configure CORS via Spring Security's `http.cors()` and `CorsConfigurationSource`. |
| **Setting `allowedOrigins("*")` with `allowCredentials(true)`** | Modern browsers reject this configuration due to CORS security specifications. | Use `allowedOriginPatterns` with specific domains instead of raw `*`. |
| **Omitting Preflight `OPTIONS` in Security Matchers** | If custom security rules block `OPTIONS`, preflight fails. | Call `http.cors()` which automatically permits preflight `OPTIONS` requests. |
| **Forgetting to Expose Headers in `exposedHeaders`** | Client JavaScript cannot read response headers like `X-Total-Count` or `Location` even though they exist. | Add headers to `config.setExposedHeaders(List.of(...))`. |
| **Expecting CORS to Block Server-Side Attacks** | CORS is strictly browser-enforced. Attackers can call APIs freely using cURL or Python. | Implement strong token-based authentication (OAuth2/JWT); never rely on CORS for authorization. |
| **Protocol Mismatch in Origin** | `http://localhost:3000` is NOT the same origin as `https://localhost:3000`. | Ensure scheme, host, and port match exactly. |

---

## 8. Performance & Security Considerations

### Preflight Latency Overhead
Every cross-origin non-simple request (e.g. requests with `Content-Type: application/json` or `Authorization: Bearer ...`) requires an initial round-trip `OPTIONS` preflight request. In high-latency mobile networks, an uncached preflight doubles API response time. Set `config.setMaxAge(3600L)` to allow browsers to cache the preflight handshake.

### CSRF vs CORS Distinction
- **CORS** relaxes the Same-Origin Policy to allow reading data across origins.
- **CSRF** protects against malicious sites issuing state-changing requests using ambient credentials (cookies).
- Permissive CORS rules (`Access-Control-Allow-Origin: *`) do not replace CSRF defenses.

---

## 9. Comparison: Global vs Controller vs Security CORS

| Feature | `@CrossOrigin` Annotation | `WebMvcConfigurer.addCorsMappings` | Spring Security `CorsConfigurationSource` |
| :--- | :--- | :--- | :--- |
| **Scope** | Controller or method only | All MVC routes | Entire application filter chain |
| **Preflight Timing** | Evaluated in `DispatcherServlet` | Evaluated in `DispatcherServlet` | Evaluated at container entry in `CorsFilter` |
| **Spring Security Compatible** | No (preflights rejected if secured) | No (preflights rejected if secured) | Yes (seamless preflight bypass) |
| **Best Used For** | Quick prototyping / isolated endpoints | MVC apps without Spring Security | Production enterprise Spring Boot services |

---

## 10. Interview Questions & Deep Dives

<details>
<summary><strong>Q1: Why does a CORS preflight request fail with HTTP 401 or 403 when Spring Security is added to a project?</strong></summary>

**Answer:**
Browsers do not attach credentials (such as `Authorization: Bearer` or session cookies) to HTTP `OPTIONS` preflight requests. If Spring Security is present without explicit CORS configuration (`http.cors()`), the security filter chain treats the incoming `OPTIONS` call as an unauthenticated request and immediately returns HTTP 401 or 403 before the request ever reaches the Spring MVC CORS mapping layer. Calling `http.cors()` inserts `CorsFilter` at the beginning of the chain to handle preflights before authentication checks.
</details>

<details>
<summary><strong>Q2: What is the difference between a "Simple Request" and a "Preflighted Request" in CORS?</strong></summary>

**Answer:**
- **Simple Request**: Uses HTTP `GET`, `HEAD`, or `POST`, with safe headers only (`Accept`, `Accept-Language`, `Content-Language`), and `Content-Type` restricted to `application/x-www-form-urlencoded`, `multipart/form-data`, or `text/plain`. It executes immediately without an `OPTIONS` handshake.
- **Preflighted Request**: Any request using verbs like `PUT`, `DELETE`, `PATCH`, or custom headers (`Authorization`, `X-Correlation-ID`), or `Content-Type: application/json`. The browser must send an `OPTIONS` preflight request first to obtain explicit server permission.
</details>

<details>
<summary><strong>Q3: Why is `allowedOriginPatterns` preferred over `allowedOrigins` in Spring Boot?</strong></summary>

**Answer:**
`allowedOrigins` only accepts literal strings. When using `allowCredentials(true)`, wildcards like `*` are forbidden by browsers. `allowedOriginPatterns` supports wildcards (e.g. `https://*.example.com` or `http://localhost:[*]`). When a matching origin arrives, Spring dynamically echoes that exact origin in the `Access-Control-Allow-Origin` header, satisfying browser security rules while supporting flexible domain patterns.
</details>

<details>
<summary><strong>Q4: What is the purpose of the `Access-Control-Expose-Headers` header?</strong></summary>

**Answer:**
By default, browsers only expose "simple" response headers (`Cache-Control`, `Content-Language`, `Content-Type`, `Expires`, `Last-Modified`, `Pragma`) to client JavaScript code. If the server emits custom headers like `X-Total-Count`, `X-RateLimit-Remaining`, or `Location`, they must be explicitly listed in `Access-Control-Expose-Headers` (via `setExposedHeaders`) or the browser's `fetch()` API will hide them from the client.
</details>

<details>
<summary><strong>Q5: Can CORS protect an API against denial-of-service or malicious scrapers?</strong></summary>

**Answer:**
No. CORS is enforced exclusively by standard web browsers. Non-browser clients (such as cURL, Postman, Python scripts, and automated bots) do not implement the Same-Origin Policy and ignore CORS headers completely. API security must always be enforced via authentication, authorization tokens, rate limiting, and WAF rules.
</details>

<details>
<summary><strong>Q6: What is the `Access-Control-Max-Age` header and why is it important?</strong></summary>

**Answer:**
`Access-Control-Max-Age` indicates how long (in seconds) the browser can cache the preflight `OPTIONS` response. Caching the preflight avoids issuing redundant `OPTIONS` requests on every subsequent API call, reducing latency and backend load.
</details>

<details>
<summary><strong>Q7: How does Spring Boot handle `Origin` header checking when requests originate from the same host?</strong></summary>

**Answer:**
If the incoming `Origin` header matches the scheme, host, and port of the receiving server, Spring treats it as a same-origin request and skips CORS processing.
</details>

<details>
<summary><strong>Q8: What happens if an API returns multiple `Access-Control-Allow-Origin` headers?</strong></summary>

**Answer:**
The CORS specification mandates that `Access-Control-Allow-Origin` contain either a single origin or `*`. If misconfigured reverse proxies or filters append duplicate headers, browsers treat the response as invalid and block it.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Add Controller-Level CORS</strong></summary>

**Task**: Allow cross-origin GET requests from `http://localhost:3000` on a `@RestController` endpoint `/api/greeting`.

```java
// Solution:
@RestController
@CrossOrigin(origins = "http://localhost:3000", methods = RequestMethod.GET)
public class GreetingController {
    @GetMapping("/api/greeting")
    public String getGreeting() { return "Hello Cross-Origin World!"; }
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Expose Custom Header in MVC Config</strong></summary>

**Task**: Configure `WebMvcConfigurer` to expose the `X-Server-Time` header to cross-origin clients calling `/api/**`.

```java
// Solution:
@Configuration
public class CorsConfig implements WebMvcConfigurer {
    @Override
    public void addCorsMappings(CorsRegistry registry) {
        registry.addMapping("/api/**")
                .allowedOrigins("https://app.example.com")
                .exposedHeaders("X-Server-Time");
    }
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Spring Security CorsConfigurationSource Bean</strong></summary>

**Task**: Define a `CorsConfigurationSource` bean permitting `https://*.company.com` subdomains with credentials and a 1-hour preflight cache.

```java
// Solution:
@Bean
public CorsConfigurationSource corsConfigurationSource() {
    CorsConfiguration config = new CorsConfiguration();
    config.setAllowedOriginPatterns(List.of("https://*.company.com"));
    config.setAllowedMethods(List.of("GET", "POST", "PUT", "DELETE", "OPTIONS"));
    config.setAllowedHeaders(List.of("*"));
    config.setAllowCredentials(true);
    config.setMaxAge(3600L);

    UrlBasedCorsConfigurationSource source = new UrlBasedCorsConfigurationSource();
    source.registerCorsConfiguration("/**", config);
    return source;
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Integration Testing Preflight with MockMvc</strong></summary>

**Task**: Write a `@WebMvcTest` asserting that an HTTP `OPTIONS` request with `Origin: https://app.example.com` returns HTTP 200 and expected `Access-Control-Allow-Origin` header.

```java
// Solution:
@Test
void testCorsPreflight() throws Exception {
    mockMvc.perform(options("/api/orders")
            .header("Origin", "https://app.example.com")
            .header("Access-Control-Request-Method", "POST"))
            .andExpect(status().isOk())
            .andExpect(header().string("Access-Control-Allow-Origin", "https://app.example.com"))
            .andExpect(header().exists("Access-Control-Allow-Methods"));
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Dynamic Regex Origin Validator</strong></summary>

**Task**: Create a custom `CorsConfigurationSource` that validates incoming `Origin` headers against a regular expression pattern loaded from an external Vault secret configuration.
</details>

<details>
<summary><strong>Exercise 6 (Hard): Multi-Tenant Domain Whitelist Filter</strong></summary>

**Task**: Build a CORS filter that checks incoming origins against tenant-specific database records, returning custom error responses if an untrusted tenant origin attempts access.
</details>

---

## 12. Summary & Quick Reference

- **Spring Security Priority**: Always configure CORS via `http.cors()` + `CorsConfigurationSource`.
- **Wildcard + Credentials**: Never mix `allowedOrigins("*")` with `allowCredentials(true)`. Use `allowedOriginPatterns(...)`.
- **Preflight Cache**: Set `maxAge(3600L)` to optimize browser network performance.
- **Header Visibility**: Expose custom response headers via `exposedHeaders(...)`.

---

## 13. Further Reading

- [W3C Cross-Origin Resource Sharing Specification](https://www.w3.org/TR/cors/)
- [MDN Web Docs: Cross-Origin Resource Sharing (CORS)](https://developer.mozilla.org/en-US/docs/Web/HTTP/CORS)
- [Spring Security Reference: CORS Integration](https://docs.spring.io/spring-security/reference/servlet/integrations/cors.html)
- [OWASP Cross-Origin Resource Sharing Security Best Practices](https://cheatsheetseries.owasp.org/cheatsheets/Cross-Origin_Resource_Sharing_Cheat_Sheet.html)
