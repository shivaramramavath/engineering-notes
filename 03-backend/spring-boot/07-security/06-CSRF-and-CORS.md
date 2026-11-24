# 06. CSRF & CORS Security in Spring Boot

| Metadata | Value |
|---|---|
| **Level** | Advanced |
| **Prerequisites** | `01-spring-security-architecture.md`, `05-session-and-cookie-security.md`, HTTP Spec (RFC 6454, RFC 7231) |
| **Estimated Reading Time** | 30 minutes |
| **Status** | Complete |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | OpenJDK 21, Spring Boot 3.4.1, Angular 18 / React 19 Clients |

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
Cross-Origin Resource Sharing (CORS) and Cross-Site Request Forgery (CSRF) are two distinct browser-enforced security mechanisms that developers frequently conflate.
- **CSRF** exploits the ambient authority of browser sessions: when a browser automatically appends session cookies (`JSESSIONID`) or client certificates to cross-site requests, an attacker on `evil.com` can trick an authenticated victim into executing unwanted state-changing actions (e.g., fund transfers, password changes) on `bank.com`.
- **CORS** relaxes the browser's Same-Origin Policy (SOP). By default, SOP blocks JavaScript running on `https://app.client.com` from reading HTTP response payloads originating from `https://api.backend.com`. CORS provides an HTTP header handshake allowing servers to whitelist trusted client origins.

Misconfiguring CORS permits unauthorized domains to read sensitive API data, while disabling CSRF protection on cookie-authenticated APIs leaves state-changing endpoints vulnerable to silent user impersonation.

### Historical Evolution
- **Spring Security 2.x - 3.x**: CSRF protection was absent by default; developers relied on manual nonce checks or custom servlet filters. CORS had no dedicated framework support.
- **Spring Security 4.x - 5.x**: CSRF protection became enabled by default for all mutating requests (`POST`, `PUT`, `DELETE`, `PATCH`). Default tokens were session-backed (`HttpSessionCsrfTokenRepository`).
- **Spring Security 6.x / 7.x**: Deprecated XML and chained builder syntax in favor of the functional Lambda DSL (`http.csrf(...)`, `http.cors(...)`). Introduced `CsrfTokenRequestAttributeHandler` to defer CSRF token resolution until explicitly required, eliminating unnecessary cryptographic token generation on read-heavy workloads. Added first-class SPA (Single Page Application) token resolution with `X-XSRF-TOKEN` cookie/header bridging.

### Real-World Relevance
Modern architectures feature decoupled frontends (React, Angular, Vue, Next.js) interacting with Spring Boot backends across different domains or subdomains. Understanding how to configure CORS preflight handling (`OPTIONS`), secure cookie-based CSRF mitigation via `CookieCsrfTokenRepository`, and distinguish when CSRF is required (cookie-based sessions) versus redundant (stateless Bearer JWTs) is essential for cloud-native API development.

---

## 2. Core Concepts

### Architectural Contracts
- **`org.springframework.security.web.csrf.CsrfFilter`**: The gatekeeper filter in the `SecurityFilterChain`. Intercepts every mutating HTTP request (`POST`, `PUT`, `DELETE`, `PATCH`) and validates the request-supplied CSRF token against the token repository.
- **`org.springframework.security.web.csrf.CsrfTokenRepository`**: Persistence contract for CSRF tokens. Implementations include `HttpSessionCsrfTokenRepository` (stores tokens in server-side HTTP session) and `CookieCsrfTokenRepository` (persists tokens in client-accessible HTTP cookies).
- **`org.springframework.security.web.csrf.CsrfTokenRequestHandler`**: Strategy for resolving and generating CSRF tokens per request. In Spring Security 6+, `SpaCsrfTokenRequestHandler` handles both raw and Breached Token (BREACH attack defense) representations.
- **`org.springframework.web.filter.CorsFilter`**: Intercepts cross-origin requests, validates the `Origin` header against permitted origins, and emits the requisite `Access-Control-Allow-*` response headers. Must execute *before* Spring Security's authorization filters to process `OPTIONS` preflight requests properly.

### The Mental Model
```
[ Browser / Single Page App ]
         │
         ├── 1. Preflight [OPTIONS /api/orders] ──────────────────────────┐
         │      Headers: Origin: https://app.example.com                   │
         │               Access-Control-Request-Method: POST               ▼
         │                                                      [ CorsFilter ]
         │                                                      Validates origin.
         │                                                      Emits Allow-* headers.
         │                                                      Short-circuits (200 OK)
         │                                                                 │
         ├── 2. Mutating Request [POST /api/orders] ───────────────────────┘
         │      Headers: Origin: https://app.example.com
         │               Cookie: JSESSIONID=...; XSRF-TOKEN=secretNonce
         │               X-XSRF-TOKEN: secretNonce
         │                                                                 │
         ▼                                                                 ▼
[ SecurityFilterChain ] ──► [ CorsFilter ] ──► [ CsrfFilter ] ──► [ Authorization ]
                                Passes OK       Validates:          Executes Controller
                                                Cookie vs Header
```

### Invariants
1. CORS is a browser-enforced containment policy. Non-browser clients (cURL, Postman, server-to-server microservices) completely ignore CORS headers and can invoke endpoints regardless of CORS policy.
2. Safe HTTP methods (`GET`, `HEAD`, `TRACE`, `OPTIONS`) must never mutate application state. `CsrfFilter` skips token verification on safe methods by design.
3. If an application uses stateless `Authorization: Bearer <JWT>` tokens without cookies, CSRF protection can be safely disabled because browsers never attach `Authorization` headers cross-origin automatically.

---

## 3. How It Works Internally

### The CSRF Filter Pipeline
1. `CsrfFilter.doFilterInternal` evaluates `requireCsrfProtectionMatcher.matches(request)`.
2. Safe methods (`GET`, `HEAD`, `TRACE`, `OPTIONS`) bypass validation.
3. For mutating requests, `CsrfTokenRepository.loadDeferredCsrfToken` resolves the expected `CsrfToken`.
4. `CsrfTokenRequestHandler.resolveCsrfTokenValue` extracts the actual token from the request header (`X-CSRF-TOKEN` or `X-XSRF-TOKEN`) or form parameter (`_csrf`).
5. `CsrfFilter` compares actual and expected tokens using constant-time string comparison (`MessageDigest.isEqual`) to thwart timing attacks.
6. If tokens match or validation passes, execution continues to `filterChain.doFilter()`. If tokens do not match, `AccessDeniedHandler.handle()` raises `InvalidCsrfTokenException` or `MissingCsrfTokenException` resulting in HTTP 403 Forbidden.

### CORS Preflight Execution Order
Spring Security places `CorsFilter` directly before `CsrfFilter` and `AuthorizationFilter`:
```
ApplicationFilterChain
  │
  ├── 1. CorsFilter (Spring Web / Security)
  │      ├─ Is it preflight (OPTIONS + Origin)?
  │      │    ├─ Yes: Validate Origin, emit Access-Control-Allow-Origin, return 200 OK.
  │      │    └─ No: Attach CORS headers to response, proceed down chain.
  │      │
  ├── 2. CsrfFilter
  │      ├─ Is it GET/HEAD/OPTIONS?
  │      │    ├─ Yes: Proceed.
  │      │    └─ No: Validate token. If mismatch, return 403 Forbidden.
  │      │
  └── 3. AuthorizationFilter
         └─ Validate roles/authorities.
```

---

## 4. Syntax & Usage

### 1. Modern Lambda DSL Configuration
```java
package com.example.security.config;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.security.config.Customizer;
import org.springframework.security.config.annotation.web.builders.HttpSecurity;
import org.springframework.security.config.annotation.web.configuration.EnableWebSecurity;
import org.springframework.security.web.SecurityFilterChain;
import org.springframework.security.web.csrf.CookieCsrfTokenRepository;
import org.springframework.security.web.csrf.CsrfTokenRequestAttributeHandler;
import org.springframework.web.cors.CorsConfiguration;
import org.springframework.web.cors.CorsConfigurationSource;
import org.springframework.web.cors.UrlBasedCorsConfigurationSource;

import java.util.List;

@Configuration
@EnableWebSecurity
public class WebSecurityConfig {

    @Bean
    public SecurityFilterChain securityFilterChain(HttpSecurity http) throws Exception {
        return http
            .cors(cors -> cors.configurationSource(corsConfigurationSource()))
            .csrf(csrf -> csrf
                .csrfTokenRepository(CookieCsrfTokenRepository.withHttpOnlyFalse())
                .csrfTokenRequestHandler(new CsrfTokenRequestAttributeHandler())
            )
            .authorizeHttpRequests(auth -> auth
                .requestMatchers("/api/public/**").permitAll()
                .anyRequest().authenticated()
            )
            .httpBasic(Customizer.withDefaults())
            .build();
    }

    @Bean
    public CorsConfigurationSource corsConfigurationSource() {
        CorsConfiguration config = new CorsConfiguration();
        config.setAllowedOrigins(List.of("https://dashboard.example.com", "https://checkout.example.com"));
        config.setAllowedMethods(List.of("GET", "POST", "PUT", "DELETE", "PATCH", "OPTIONS"));
        config.setAllowedHeaders(List.of("Authorization", "Content-Type", "X-XSRF-TOKEN"));
        config.setExposedHeaders(List.of("X-Total-Count", "Link"));
        config.setAllowCredentials(true);
        config.setMaxAge(3600L); // 1 hour preflight cache

        UrlBasedCorsConfigurationSource source = new UrlBasedCorsConfigurationSource();
        source.registerCorsConfiguration("/api/**", config);
        return source;
    }
}
```

---

## 5. Practical Examples

### Example 1: Stateless REST API (CSRF Disabled + Strict CORS)
When building microservices authenticated solely via stateless Bearer tokens (`Authorization: Bearer <jwt>`) without cookies, disabling CSRF reduces CPU overhead while strict CORS guards browser access.

```java
package com.example.security.stateless;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.security.config.annotation.web.builders.HttpSecurity;
import org.springframework.security.config.annotation.web.configurers.AbstractHttpConfigurer;
import org.springframework.security.config.http.SessionCreationPolicy;
import org.springframework.security.web.SecurityFilterChain;
import org.springframework.web.cors.CorsConfiguration;
import org.springframework.web.cors.CorsConfigurationSource;
import org.springframework.web.cors.UrlBasedCorsConfigurationSource;

import java.util.List;

@Configuration
public class StatelessSecurityConfig {

    @Bean
    public SecurityFilterChain apiSecurityFilterChain(HttpSecurity http) throws Exception {
        return http
            .securityMatcher("/api/**")
            .cors(cors -> cors.configurationSource(apiCorsSource()))
            .csrf(AbstractHttpConfigurer::disable) // Safe: Bearer tokens are immune to ambient authority CSRF
            .sessionManagement(session -> session.sessionCreationPolicy(SessionCreationPolicy.STATELESS))
            .authorizeHttpRequests(auth -> auth.anyRequest().authenticated())
            .oauth2ResourceServer(oauth2 -> oauth2.jwt(org.springframework.security.config.Customizer.withDefaults()))
            .build();
    }

    @Bean
    public CorsConfigurationSource apiCorsSource() {
        CorsConfiguration config = new CorsConfiguration();
        config.setAllowedOriginPatterns(List.of("https://*.corp.internal:[*]"));
        config.setAllowedMethods(List.of("GET", "POST", "PUT", "DELETE"));
        config.setAllowedHeaders(List.of("Authorization", "Content-Type"));
        config.setAllowCredentials(false);

        UrlBasedCorsConfigurationSource source = new UrlBasedCorsConfigurationSource();
        source.registerCorsConfiguration("/api/**", config);
        return source;
    }
}
```

### Example 2: Enterprise Single-Page Application (SPA) CSRF Cookie Filter
Modern SPAs (Angular, React, Vue) read the CSRF cookie (`XSRF-TOKEN`) and mirror it into request headers (`X-XSRF-TOKEN`). In Spring Security 6+, deferred tokens require a dedicated filter to force cookie emission on initial GET requests.

```java
package com.example.security.spa;

import jakarta.servlet.FilterChain;
import jakarta.servlet.ServletException;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.servlet.http.HttpServletResponse;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.security.config.annotation.web.builders.HttpSecurity;
import org.springframework.security.web.SecurityFilterChain;
import org.springframework.security.web.authentication.www.BasicAuthenticationFilter;
import org.springframework.security.web.csrf.*;
import org.springframework.util.StringUtils;
import org.springframework.web.filter.OncePerRequestFilter;

import java.io.IOException;
import java.util.function.Supplier;

@Configuration
public class SpaSecurityConfiguration {

    @Bean
    public SecurityFilterChain spaFilterChain(HttpSecurity http) throws Exception {
        return http
            .csrf(csrf -> csrf
                .csrfTokenRepository(CookieCsrfTokenRepository.withHttpOnlyFalse())
                .csrfTokenRequestHandler(new SpaCsrfTokenRequestHandler())
            )
            .addFilterAfter(new CsrfCookieFilter(), BasicAuthenticationFilter.class)
            .authorizeHttpRequests(auth -> auth
                .requestMatchers("/index.html", "/", "/assets/**", "/api/auth/login").permitAll()
                .anyRequest().authenticated()
            )
            .build();
    }

    /**
     * Custom Request Handler bridging plain tokens and BREACH-protected tokens.
     */
    static final class SpaCsrfTokenRequestHandler extends CsrfTokenRequestAttributeHandler {
        private final CsrfTokenRequestHandler delegate = new XorCsrfTokenRequestAttributeHandler();

        @Override
        public void handle(HttpServletRequest request, HttpServletResponse response, Supplier<CsrfToken> csrfToken) {
            this.delegate.handle(request, response, csrfToken);
        }

        @Override
        public String resolveCsrfTokenValue(HttpServletRequest request, CsrfToken csrfToken) {
            String headerValue = request.getHeader(csrfToken.getHeaderName());
            return (StringUtils.hasText(headerValue)) ? headerValue : super.resolveCsrfTokenValue(request, csrfToken);
        }
    }

    /**
     * OncePerRequestFilter that forces the deferred CSRF token to be rendered into the cookie.
     */
    static final class CsrfCookieFilter extends OncePerRequestFilter {
        @Override
        protected void doFilterInternal(HttpServletRequest request, HttpServletResponse response, FilterChain filterChain)
                throws ServletException, IOException {
            CsrfToken csrfToken = (CsrfToken) request.getAttribute(CsrfToken.class.getName());
            if (csrfToken != null) {
                // Invoking getToken() forces evaluation of the deferred token supplier, writing the cookie
                csrfToken.getToken();
            }
            filterChain.doFilter(request, response);
        }
    }
}
```

### Example 3: Edge Case – Dynamic Origin Validation with Regex & Database Lookups
Multi-tenant platforms frequently require validating origins dynamically based on registered tenant subdomains rather than static configuration lists.

```java
package com.example.security.cors;

import jakarta.servlet.http.HttpServletRequest;
import org.springframework.stereotype.Component;
import org.springframework.web.cors.CorsConfiguration;
import org.springframework.web.cors.CorsConfigurationSource;

import java.util.List;
import java.util.regex.Pattern;

@Component
public class TenantDynamicCorsConfigurationSource implements CorsConfigurationSource {

    private static final Pattern TENANT_ORIGIN_PATTERN = 
        Pattern.compile("^https://([a-zA-Z0-9-]+)\\.platform\\.cloud$");

    @Override
    public CorsConfiguration getCorsConfiguration(HttpServletRequest request) {
        String origin = request.getHeader("Origin");
        if (origin == null) {
            return null; // Not a CORS request
        }

        if (TENANT_ORIGIN_PATTERN.matcher(origin).matches()) {
            CorsConfiguration config = new CorsConfiguration();
            config.setAllowedOrigins(List.of(origin)); // Reflect validated origin
            config.setAllowedMethods(List.of("GET", "POST", "PUT", "DELETE", "PATCH"));
            config.setAllowedHeaders(List.of("Authorization", "Content-Type", "X-Tenant-ID"));
            config.setAllowCredentials(true);
            config.setMaxAge(1800L);
            return config;
        }

        return null; // Reject unregistered origins
    }
}
```

### Example 4: Failure Anti-Pattern & Production Fix
**Anti-Pattern**: Using `config.addAllowedOrigin("*")` alongside `config.setAllowCredentials(true)`.

```java
// BUGGY: Throws runtime IllegalArgumentException or fails browser preflight checks
@Bean
public CorsConfigurationSource brokenCors() {
    CorsConfiguration config = new CorsConfiguration();
    config.addAllowedOrigin("*"); // Wildcard
    config.setAllowCredentials(true); // CONFLICT: Spec forbids wildcard with credentials!
    UrlBasedCorsConfigurationSource source = new UrlBasedCorsConfigurationSource();
    source.registerCorsConfiguration("/**", config);
    return source;
}
```

**Production Fix**:
Use `setAllowedOriginPatterns` with explicit patterns or specify concrete domain names.

```java
// FIXED: RFC-compliant origin pattern matching with credentials enabled
@Bean
public CorsConfigurationSource compliantCors() {
    CorsConfiguration config = new CorsConfiguration();
    config.setAllowedOriginPatterns(List.of("https://*.mycompany.com"));
    config.setAllowedMethods(List.of("GET", "POST", "PUT", "DELETE", "OPTIONS"));
    config.setAllowedHeaders(List.of("*"));
    config.setAllowCredentials(true);
    config.setMaxAge(3600L);

    UrlBasedCorsConfigurationSource source = new UrlBasedCorsConfigurationSource();
    source.registerCorsConfiguration("/**", config);
    return source;
}
```

### Example 5: Modern High-Performance Non-Blocking WebFlux CSRF & CORS
In reactive architectures with Spring Boot WebFlux, security operations leverage non-blocking `WebFilter` chains and reactive `ServerCsrfTokenRepository`.

```java
package com.example.security.reactive;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.security.config.annotation.web.reactive.EnableWebFluxSecurity;
import org.springframework.security.config.web.server.ServerHttpSecurity;
import org.springframework.security.web.server.SecurityWebFilterChain;
import org.springframework.security.web.server.csrf.CookieServerCsrfTokenRepository;
import org.springframework.web.cors.CorsConfiguration;
import org.springframework.web.cors.reactive.CorsConfigurationSource;
import org.springframework.web.cors.reactive.UrlBasedCorsConfigurationSource;

import java.util.List;

@Configuration
@EnableWebFluxSecurity
public class ReactiveSecurityConfig {

    @Bean
    public SecurityWebFilterChain reactiveSecurityFilterChain(ServerHttpSecurity http) {
        return http
            .cors(cors -> cors.configurationSource(reactiveCorsConfigurationSource()))
            .csrf(csrf -> csrf
                .csrfTokenRepository(CookieServerCsrfTokenRepository.withHttpOnlyFalse())
            )
            .authorizeExchange(exchanges -> exchanges
                .pathMatchers("/public/**").permitAll()
                .anyExchange().authenticated()
            )
            .build();
    }

    @Bean
    public CorsConfigurationSource reactiveCorsConfigurationSource() {
        CorsConfiguration config = new CorsConfiguration();
        config.setAllowedOrigins(List.of("https://app.reactive-system.io"));
        config.setAllowedMethods(List.of("GET", "POST", "PUT", "DELETE"));
        config.setAllowedHeaders(List.of("Authorization", "Content-Type", "X-XSRF-TOKEN"));
        config.setAllowCredentials(true);

        UrlBasedCorsConfigurationSource source = new UrlBasedCorsConfigurationSource();
        source.registerCorsConfiguration("/**", config);
        return source;
    }
}
```

---

## 6. Best Practices

1. **Keep CSRF Enabled for Session/Cookie Backends**: Never call `csrf.disable()` on architectures relying on `JSESSIONID` or session cookies.
2. **Use `setAllowedOriginPatterns` Instead of Wildcards**: Always restrict CORS origins to trusted hostnames. If wildcards are required, use regex patterns with protocol enforcement (`https://`).
3. **Register `CorsFilter` at the Highest Precedence**: Ensure the Spring Security CORS configuration source is used rather than standalone servlet filters, guaranteeing preflights are authorized before hitting Spring MVC dispatchers.
4. **Leverage `CookieCsrfTokenRepository.withHttpOnlyFalse()` for SPAs**: Frontends like Angular and React need to read the token value to inject it into HTTP request headers (`X-XSRF-TOKEN`).
5. **Mitigate BREACH Attacks via `XorCsrfTokenRequestAttributeHandler`**: Mask dynamic CSRF tokens with unique XOR per-request random nonces to prevent side-channel compression attacks against encrypted responses.
6. **Set Explicit `maxAge` on CORS Configurations**: Avoid round-trip preflight penalties for every API call by setting preflight cache durations between 1800 and 3600 seconds.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Disabling CSRF on cookie-authenticated APIs | Attackers can forge cross-site state-changing requests using ambient browser credentials. | Retain `CookieCsrfTokenRepository` or `HttpSessionCsrfTokenRepository`. |
| Allowing `*` origin with `allowCredentials=true` | Browsers reject the response with a JavaScript console security error per W3C CORS spec. | Use `setAllowedOriginPatterns` with specific domains or reflect valid incoming origins. |
| Missing `@CrossOrigin` or `CorsFilter` order | Spring Security `AuthorizationFilter` intercepts `OPTIONS` requests before MVC controller `@CrossOrigin` executes, returning 401/403. | Configure CORS globally in `SecurityFilterChain` via `http.cors(...)`. |
| Expecting CSRF protection on `GET` requests | Attackers execute state changes via query parameters on `GET` endpoints because `CsrfFilter` ignores safe methods. | Enforce strict HTTP method semantics; mutating operations must use `POST`, `PUT`, `DELETE`. |
| Deferred CSRF token not sent on initial SPA load | In Spring Security 6+, tokens are deferred until accessed. SPAs cannot read the cookie unless forced. | Implement `CsrfCookieFilter` to evaluate `csrfToken.getToken()` on every request. |
| Omitting custom headers from `exposedHeaders` | Frontend JavaScript cannot inspect response headers like `X-Total-Count` or `Authorization` due to browser isolation. | Explicitly register headers in `corsConfiguration.setExposedHeaders(...)`. |

---

## 8. Performance & Security Notes

- **Memory & Session Overhead**: `HttpSessionCsrfTokenRepository` allocates token objects inside the `HttpSession`. In massive multi-user deployments, this inflates server memory footprints. Prefer `CookieCsrfTokenRepository`, which offloads token storage entirely to the client.
- **BREACH Attack Protection**: HTTP compression (gzip/brotli) combined with static reflection of secrets in responses enables token recovery via differential compression sizes. Spring Security 6 defaults to XOR masking (`XorCsrfTokenRequestAttributeHandler`), guaranteeing that even identical tokens have distinct ciphertexts on every request.
- **Preflight Cache Saturation**: Mobile and desktop browsers cap `Access-Control-Max-Age` (e.g., Chromium caps at 7200 seconds, Firefox at 86400 seconds). Setting 3600 seconds (1 hour) balances security revocation speed with preflight network reduction.

---

## 9. Comparison / When to Use What

| Architecture | CSRF Recommended? | CSRF Strategy | CORS Strategy |
|---|---|---|---|
| **Traditional Server-Side MVC (Thymeleaf/JSP)** | **Yes (Mandatory)** | `HttpSessionCsrfTokenRepository` with `_csrf` hidden form fields | Disabled / Not Applicable (Same Origin) |
| **Decoupled SPA + Cookie Session (`JSESSIONID`)** | **Yes (Mandatory)** | `CookieCsrfTokenRepository.withHttpOnlyFalse()` + `CsrfCookieFilter` | `CorsConfigurationSource` with strict origins + `allowCredentials(true)` |
| **Stateless Mobile / SPA + Bearer JWT** | **No (Safe to Disable)** | `AbstractHttpConfigurer::disable` | `CorsConfigurationSource` with strict origins + `allowCredentials(false)` |
| **B2B Machine-to-Machine API (API Keys / mTLS)** | **No (Safe to Disable)** | `AbstractHttpConfigurer::disable` | Disabled / Not Applicable (Non-browser clients) |

---

## 10. Interview Questions

<details>
<summary>1. What is the fundamental difference between CSRF and CORS?</summary>

**Answer:**
CSRF (Cross-Site Request Forgery) is an attack where a malicious site tricks a victim's browser into executing unwanted actions on a trusted site where the victim is authenticated, exploiting ambient credentials (cookies).
CORS (Cross-Origin Resource Sharing) is a browser defense mechanism that relaxes the Same-Origin Policy, allowing servers to declare which external origins are permitted to read their HTTP responses via JavaScript. CORS does not prevent requests from reaching the server; it prevents the browser from sharing the response with malicious scripts.
</details>

<details>
<summary>2. Why does Spring Security 6 defer CSRF token generation by default?</summary>

**Answer:**
In previous versions, `CsrfFilter` eagerly generated and saved a token for every request, creating unnecessary CPU work (random byte generation) and triggering session creation even for public read-only (`GET`) requests. Spring Security 6 introduced deferred tokens (`DeferredCsrfToken`), computing the token only when application code, templates, or request handlers explicitly invoke `csrfToken.getToken()`.
</details>

<details>
<summary>3. Can an API authenticated via `Authorization: Bearer <token>` be attacked via CSRF?</summary>

**Answer:**
No. Browsers only transmit credentials automatically under ambient authority (cookies, HTTP Basic auth headers stored in browser cache, TLS client certs). Browsers never automatically attach custom `Authorization: Bearer` headers across origins. Therefore, APIs strictly relying on Bearer headers without fallback to cookies are immune to CSRF, making CSRF protection redundant.
</details>

<details>
<summary>4. Why does an `OPTIONS` preflight request fail with HTTP 401/403 when configured only via `@CrossOrigin`?</summary>

**Answer:**
Spring Security's filter chain executes before Spring MVC dispatching. If the `SecurityFilterChain` does not explicitly enable CORS (`http.cors(...)`), the incoming `OPTIONS` preflight request hits `AuthorizationFilter` before reaching the MVC controller that houses the `@CrossOrigin` annotation. Since preflight requests do not carry credentials, authorization fails immediately.
</details>

<details>
<summary>5. What is the purpose of XOR masking in Spring Security 6's `XorCsrfTokenRequestAttributeHandler`?</summary>

**Answer:**
XOR masking mitigates the BREACH (Browser Reconnaissance and Exfiltration via Adaptive Compression of Hypertext) attack. Attackers observing response byte sizes in compressed HTTP streams can deduce static secrets reflected in responses. By XOR-ing the underlying CSRF token with a fresh random key on every HTTP request, the token representation changes continuously, defeating compression-based side-channel analysis.
</details>

<details>
<summary>6. Why must `Access-Control-Allow-Origin: *` never be paired with `Access-Control-Allow-Credentials: true`?</summary>

**Answer:**
The W3C CORS specification explicitly prohibits this combination to prevent catastrophic data leaks. If allowed, any malicious website on the internet could perform credentialed requests against the backend and read the authenticated user's private data. Browsers enforce this rule and fail the request immediately if both headers are present.
</details>

<details>
<summary>7. How does `CookieCsrfTokenRepository.withHttpOnlyFalse()` work with client frameworks?</summary>

**Answer:**
Setting `HttpOnly=false` permits client-side JavaScript (such as Angular's `$http` or Axios interceptors) to read the cookie value (typically named `XSRF-TOKEN`). The client script copies this value into a custom request header (such as `X-XSRF-TOKEN`) on subsequent mutating HTTP requests (`POST`, `PUT`, `DELETE`). The server verifies that the header token matches the cookie token (Double Submit Cookie pattern).
</details>

<details>
<summary>8. How do you permit CORS on error endpoints (e.g., `/error`)?</summary>

**Answer:**
When an exception occurs during request processing, the servlet container dispatches an internal forward to `/error`. If `/error` is not matched in the `CorsConfigurationSource` or is guarded by authorization checks, the browser drops the error response due to missing CORS headers, masking the real error with a generic CORS failure. You must ensure `/error` matches your CORS configuration and is permitted in `authorizeHttpRequests`.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Basic SPA CSRF Configuration (Easy)</summary>

**Task**: Configure a `SecurityFilterChain` that protects all `/api/**` endpoints using `CookieCsrfTokenRepository.withHttpOnlyFalse()`, permits anonymous `GET` requests on `/api/public/**`, and authenticates all other requests.

**Solution**:
```java
@Bean
public SecurityFilterChain basicSpaCsrfChain(HttpSecurity http) throws Exception {
    return http
        .csrf(csrf -> csrf
            .csrfTokenRepository(CookieCsrfTokenRepository.withHttpOnlyFalse())
        )
        .authorizeHttpRequests(auth -> auth
            .requestMatchers(HttpMethod.GET, "/api/public/**").permitAll()
            .anyRequest().authenticated()
        )
        .httpBasic(Customizer.withDefaults())
        .build();
}
```
</details>

<details>
<summary>Exercise 2: Global CORS Whitelist with Preflight Caching (Easy)</summary>

**Task**: Define a `CorsConfigurationSource` bean that allows `https://admin.example.com` to perform `GET`, `POST`, and `DELETE` requests on all routes, caches preflight checks for 30 minutes, and exposes the `X-Response-Time` header.

**Solution**:
```java
@Bean
public CorsConfigurationSource customCorsConfig() {
    CorsConfiguration config = new CorsConfiguration();
    config.setAllowedOrigins(List.of("https://admin.example.com"));
    config.setAllowedMethods(List.of("GET", "POST", "DELETE"));
    config.setExposedHeaders(List.of("X-Response-Time"));
    config.setMaxAge(1800L);

    UrlBasedCorsConfigurationSource source = new UrlBasedCorsConfigurationSource();
    source.registerCorsConfiguration("/**", config);
    return source;
}
```
</details>

<details>
<summary>Exercise 3: Angular-Compatible CSRF Cookie Handshake (Medium)</summary>

**Task**: Implement a custom `OncePerRequestFilter` that intercepts all incoming requests and ensures the deferred CSRF token is published into the `XSRF-TOKEN` cookie, avoiding the Spring Security 6 deferred token starvation bug in single-page apps.

**Solution**:
```java
public class CsrfCookiePublishingFilter extends OncePerRequestFilter {
    @Override
    protected void doFilterInternal(HttpServletRequest request, HttpServletResponse response, FilterChain filterChain)
            throws ServletException, IOException {
        CsrfToken token = (CsrfToken) request.getAttribute(CsrfToken.class.getName());
        if (token != null) {
            token.getToken(); // Forces resolution and cookie attachment
        }
        filterChain.doFilter(request, response);
    }
}
```
</details>

<details>
<summary>Exercise 4: Subdomain Wildcard CORS with Credentials (Medium)</summary>

**Task**: Configure CORS in Spring Boot such that any HTTPS subdomain of `company.com` (e.g., `https://app.company.com`, `https://billing.company.com`) is allowed with credentials enabled and headers `Authorization` and `Content-Type`.

**Solution**:
```java
@Bean
public CorsConfigurationSource subdomainCorsSource() {
    CorsConfiguration config = new CorsConfiguration();
    config.setAllowedOriginPatterns(List.of("https://*.company.com"));
    config.setAllowedMethods(List.of("GET", "POST", "PUT", "DELETE", "OPTIONS"));
    config.setAllowedHeaders(List.of("Authorization", "Content-Type", "X-Requested-With"));
    config.setAllowCredentials(true);

    UrlBasedCorsConfigurationSource source = new UrlBasedCorsConfigurationSource();
    source.registerCorsConfiguration("/**", config);
    return source;
}
```
</details>

<details>
<summary>Exercise 5: Dynamic Origin Whitelist from Distributed Cache (Hard)</summary>

**Task**: Implement a dynamic `CorsConfigurationSource` that validates the `Origin` header against an in-memory or Redis-backed tenant registry. If valid, dynamically construct and return a custom `CorsConfiguration`. If invalid, return `null`.

**Solution**:
```java
@Component
public class DistributedTenantCorsSource implements CorsConfigurationSource {

    private final Set<String> registeredTenants = ConcurrentHashMap.newKeySet();

    public DistributedTenantCorsSource() {
        registeredTenants.addAll(List.of("https://tenant-a.com", "https://tenant-b.com"));
    }

    @Override
    public CorsConfiguration getCorsConfiguration(HttpServletRequest request) {
        String origin = request.getHeader("Origin");
        if (origin == null || !registeredTenants.contains(origin)) {
            return null;
        }

        CorsConfiguration config = new CorsConfiguration();
        config.setAllowedOrigins(List.of(origin));
        config.setAllowedMethods(List.of("GET", "POST", "PUT", "PATCH", "DELETE"));
        config.setAllowedHeaders(List.of("Authorization", "Content-Type", "X-Client-Version"));
        config.setAllowCredentials(true);
        config.setMaxAge(3600L);
        return config;
    }
}
```
</details>

<details>
<summary>Exercise 6: MockMvc Test Suite for CSRF and CORS (Hard)</summary>

**Task**: Write a complete JUnit 5 / Spring Boot test demonstrating that a `POST` request to `/api/orders` fails with 403 Forbidden without a CSRF token, passes with `csrf()`, and verifies `Access-Control-Allow-Origin` headers on preflight `OPTIONS`.

**Solution**:
```java
@WebMvcTest(controllers = OrderController.class)
@Import(WebSecurityConfig.class)
class SecurityCsrfCorsTests {

    @Autowired
    private MockMvc mockMvc;

    @Test
    void postWithoutCsrfShouldFailWith403() throws Exception {
        mockMvc.perform(post("/api/orders")
                .contentType(MediaType.APPLICATION_JSON)
                .content("{\"item\":\"Book\"}"))
            .andExpect(status().isForbidden());
    }

    @Test
    void postWithCsrfShouldSucceed() throws Exception {
        mockMvc.perform(post("/api/orders")
                .with(csrf())
                .contentType(MediaType.APPLICATION_JSON)
                .content("{\"item\":\"Book\"}"))
            .andExpect(status().isOk());
    }

    @Test
    void optionsPreflightShouldReturnCorsHeaders() throws Exception {
        mockMvc.perform(options("/api/orders")
                .header("Origin", "https://dashboard.example.com")
                .header("Access-Control-Request-Method", "POST"))
            .andExpect(status().isOk())
            .andExpect(header().string("Access-Control-Allow-Origin", "https://dashboard.example.com"))
            .andExpect(header().string("Access-Control-Allow-Credentials", "true"));
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **CSRF is for Ambient Authority**: Required when using session cookies or HTTP basic auth stored in browser state. Unnecessary for purely stateless `Authorization: Bearer` token architectures.
- **CORS Relaxes SOP**: CORS never blocks a request from reaching the server; it prevents browser scripts from reading responses across origins.
- **Preflights Require `OPTIONS`**: Always configure CORS via `http.cors(cors -> cors.configurationSource(...))` in `SecurityFilterChain` so preflights bypass authentication filters smoothly.
- **Double Submit Cookie for SPAs**: Use `CookieCsrfTokenRepository.withHttpOnlyFalse()` so frontend frameworks can read the `XSRF-TOKEN` cookie and set the `X-XSRF-TOKEN` header.
- **Never Wildcard with Credentials**: `*` origin + `allowCredentials(true)` is strictly prohibited by browser specifications.

---

## 13. Further Reading
- [Spring Security Reference: CSRF Protection](https://docs.spring.io/spring-security/reference/servlet/exploits/csrf.html)
- [Spring Security Reference: CORS Configuration](https://docs.spring.io/spring-security/reference/servlet/integrations/cors.html)
- [W3C Cross-Origin Resource Sharing Specification](https://www.w3.org/TR/cors/)
- [OWASP Cross-Site Request Forgery Prevention Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Cross-Site_Request_Forgery_Prevention_Cheat_Sheet.html)
- [RFC 6454: The Web Origin Concept](https://datatracker.ietf.org/doc/html/rfc6454)
