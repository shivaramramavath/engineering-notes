# 05: Spring Security 6.x Technical Interview Q&A

| Metadata Field | Value |
|---|---|
| **Topic** | Spring Security 6.x Architecture, Lambda DSL, SecurityContext & JWT |
| **Spring Boot Version** | 3.4.x |
| **Spring Security** | 6.4.x |
| **Java Version** | 21 / 25 LTS |
| **Target Level** | Senior / Lead / Staff Backend Engineer |
| **Key Focus Areas** | `SecurityFilterChain`, Virtual Thread `SecurityContext`, Method Security, CSRF Tokens |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [Internals & Architecture](#3-internals--architecture)
4. [Syntax & Practical Usage](#4-syntax--practical-usage)
5. [5 Practical Examples](#5-5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Pitfalls & Anti-Patterns](#7-common-pitfalls--anti-patterns)
8. [Performance & Security Considerations](#8-performance--security-considerations)
9. [Comparison / Decision Matrix](#9-comparison--decision-matrix)
10. [Real-World Interview Q&A](#10-real-world-interview-qa)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference Cheatsheet](#12-summary--quick-reference-cheatsheet)
13. [Further Reading & Official Resources](#13-further-reading--official-resources)

---

## 1. What & Why

Securing enterprise backend applications requires a deep understanding of defense-in-depth principles. Spring Security 6.x (shipped with Spring Boot 3) introduced substantial architectural modernization: completely removing deprecated configuration adapters (`WebSecurityConfigurerAdapter`), migrating exclusively to component-based **Lambda DSL** configuration, introducing strict defaults for CSRF protection, and supporting **Virtual Threads** context propagation.

In senior interviews, candidates must explain how the `FilterChainProxy` routes requests, how `AuthenticationManager` delegates across multiple `AuthenticationProvider`s, how JWT claims map into `GrantedAuthority` collections, how the `SecurityContext` propagates across asynchronous thread boundaries, and how vulnerabilities like Spring4Shell (CVE-2022-22965) and CSRF attacks are systematically neutralized.

---

## 2. Core Concepts

### 2.1 The Security Filter Chain
All HTTP security in Spring is implemented as a chain of standard Servlet Filters managed by `FilterChainProxy` (bridged via `DelegatingFilterProxy`):
1. `HeaderWriterFilter`: Adds security headers (`X-Content-Type-Options`, `HSTS`, `X-Frame-Options`).
2. `CorsFilter` & `CsrfFilter`: Validates cross-origin requests and CSRF tokens.
3. `BearerTokenAuthenticationFilter`: Extracts and parses JWT Bearer tokens.
4. `UsernamePasswordAuthenticationFilter`: Handles form-based authentication.
5. `AuthorizationFilter`: Replaces legacy `FilterSecurityInterceptor` in Spring Security 6; evaluates endpoint permissions (`requestMatchers()`).

### 2.2 SecurityContext & Virtual Thread Compatibility
The authenticated principal is stored in `SecurityContextHolder`. By default, it uses `MODE_THREADLOCAL`. With Java 21 Virtual Threads, `ThreadLocal` context works seamlessly because virtual threads are lightweight instances of `java.lang.Thread`. However, when tasks fan out using `CompletableFuture` or custom executors, `DelegatingSecurityContextAsyncTaskExecutor` or `SecurityContextHolder.createCopy()` must be used to prevent context loss.

---

## 3. Internals & Architecture

```
        [ Inbound HTTP Request with Bearer JWT ]
                           │
                           ▼
  ┌─────────────────────────────────────────────────────────┐
  │                 DelegatingFilterProxy                   │
  └────────────────────────┬────────────────────────────────┘
                           │
                           ▼
  ┌─────────────────────────────────────────────────────────┐
  │              FilterChainProxy (springSecurityFilterChain)│
  │  Matches request URL to specific SecurityFilterChain    │
  └────────────────────────┬────────────────────────────────┘
                           │
                           ▼
  ┌─────────────────────────────────────────────────────────┐
  │         BearerTokenAuthenticationFilter                 │
  │   1. Extract Token from "Authorization: Bearer <jwt>"   │
  │   2. Delegate to AuthenticationManager / JwtDecoder     │
  │   3. Validate Cryptographic Signature (via JWKS)        │
  │   4. Map Claims to GrantedAuthority ("ROLE_ADMIN")      │
  └────────────────────────┬────────────────────────────────┘
                           │
                           ▼
  ┌─────────────────────────────────────────────────────────┐
  │     SecurityContextHolder.setContext(authentication)    │
  │   Stores authenticated principal in ThreadLocal         │
  └────────────────────────┬────────────────────────────────┘
                           │
                           ▼
  ┌─────────────────────────────────────────────────────────┐
  │         AuthorizationFilter (Spring Security 6)         │
  │   Evaluates: .requestMatchers("/admin/**").hasRole(...) │
  └────────────────────────┬────────────────────────────────┘
                           │
         ┌─────────────────┴─────────────────┐
         ▼ (Authorized)                      ▼ (Access Denied)
  ┌─────────────────────────┐       ┌────────────────────────┐
  │ Pass to DispatcherServlet│       │ AccessDeniedHandler    │
  │ (Controller execution)  │       │ (Return HTTP 403)      │
  └─────────────────────────┘       └────────────────────────┘
```

---

## 4. Syntax & Practical Usage

### 4.1 Modern Spring Security 6 Lambda DSL Syntax
```java
@Bean
public SecurityFilterChain securityFilterChain(HttpSecurity http) throws Exception {
    return http
            .csrf(csrf -> csrf.disable())
            .sessionManagement(sm -> sm.sessionCreationPolicy(SessionCreationPolicy.STATELESS))
            .authorizeHttpRequests(auth -> auth
                    .requestMatchers("/api/public/**").permitAll()
                    .requestMatchers("/api/admin/**").hasRole("ADMIN")
                    .anyRequest().authenticated()
            )
            .oauth2ResourceServer(oauth2 -> oauth2.jwt(Customizer.withDefaults()))
            .build();
}
```

---

## 5. 5 Practical Examples

### Example 1: Production JWT Resource Server Configuration

```java
package com.example.security.config;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.core.convert.converter.Converter;
import org.springframework.security.authentication.AbstractAuthenticationToken;
import org.springframework.security.config.annotation.method.configuration.EnableMethodSecurity;
import org.springframework.security.config.annotation.web.builders.HttpSecurity;
import org.springframework.security.config.http.SessionCreationPolicy;
import org.springframework.security.core.GrantedAuthority;
import org.springframework.security.core.authority.SimpleGrantedAuthority;
import org.springframework.security.oauth2.jwt.Jwt;
import org.springframework.security.oauth2.server.resource.authentication.JwtAuthenticationToken;
import org.springframework.security.web.SecurityFilterChain;

import java.util.Collection;
import java.util.List;
import java.util.stream.Collectors;

@Configuration
@EnableMethodSecurity(prePostEnabled = true)
public class SecurityConfiguration {

    @Bean
    public SecurityFilterChain filterChain(HttpSecurity http) throws Exception {
        return http
                .csrf(csrf -> csrf.disable())
                .cors(cors -> cors.configure(http))
                .sessionManagement(sm -> sm.sessionCreationPolicy(SessionCreationPolicy.STATELESS))
                .authorizeHttpRequests(auth -> auth
                        .requestMatchers("/api/v1/auth/**", "/actuator/health").permitAll()
                        .requestMatchers("/api/v1/admin/**").hasAuthority("ROLE_ADMIN")
                        .anyRequest().authenticated()
                )
                .oauth2ResourceServer(oauth2 -> oauth2
                        .jwt(jwt -> jwt.jwtAuthenticationConverter(customJwtConverter()))
                )
                .build();
    }

    @Bean
    public Converter<Jwt, AbstractAuthenticationToken> customJwtConverter() {
        return jwt -> {
            List<String> roles = jwt.getClaimAsStringList("roles");
            Collection<GrantedAuthority> authorities = roles == null ? List.of() :
                    roles.stream()
                            .map(role -> new SimpleGrantedAuthority("ROLE_" + role.toUpperCase()))
                            .collect(Collectors.toList());

            return new JwtAuthenticationToken(jwt, authorities, jwt.getSubject());
        };
    }
}
```

### Example 2: Declarative Pre/Post Method Security

```java
package com.example.security.service;

import org.springframework.security.access.prepost.PreAuthorize;
import org.springframework.stereotype.Service;

import java.util.UUID;

@Service
public class DocumentManagementService {

    // Evaluates Spring Expression Language (SpEL) before executing method
    @PreAuthorize("hasRole('ADMIN') or #ownerId == authentication.name")
    public void deleteDocument(UUID docId, String ownerId) {
        // Business logic to delete document
    }

    @PreAuthorize("hasAuthority('SCOPE_documents:write')")
    public void createDocument(String content) {
        // Create document
    }
}
```

### Example 3: Context Propagation for Asynchronous Executors

```java
package com.example.security.config;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.core.task.AsyncTaskExecutor;
import org.springframework.core.task.support.TaskExecutorAdapter;
import org.springframework.security.concurrent.DelegatingSecurityContextAsyncTaskExecutor;

import java.util.concurrent.Executors;

@Configuration
public class AsyncSecurityConfig {

    @Bean
    public AsyncTaskExecutor asyncTaskExecutor() {
        // Wraps virtual thread executor so SecurityContext propagates to async background threads
        AsyncTaskExecutor virtualExecutor = new TaskExecutorAdapter(Executors.newVirtualThreadPerTaskExecutor());
        return new DelegatingSecurityContextAsyncTaskExecutor(virtualExecutor);
    }
}
```

### Example 4: Single-Page Application (SPA) CSRF Cookie Handler

```java
package com.example.security.csrf;

import jakarta.servlet.FilterChain;
import jakarta.servlet.ServletException;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.servlet.http.HttpServletResponse;
import org.springframework.security.web.csrf.CsrfToken;
import org.springframework.stereotype.Component;
import org.springframework.web.filter.OncePerRequestFilter;

import java.io.IOException;

@Component
public class SpaCsrfTokenFilter extends OncePerRequestFilter {

    @Override
    protected void doFilterInternal(HttpServletRequest request, HttpServletResponse response, FilterChain filterChain)
            throws ServletException, IOException {

        // Render token into cookie readable by client-side JavaScript (Angular / React)
        CsrfToken csrfToken = (CsrfToken) request.getAttribute(CsrfToken.class.getName());
        if (csrfToken != null) {
            response.setHeader(csrfToken.getHeaderName(), csrfToken.getToken());
        }
        filterChain.doFilter(request, response);
    }
}
```

### Example 5: Custom Authentication Entry Point (RFC 9457 JSON Output)

```java
package com.example.security.handler;

import com.fasterxml.jackson.databind.ObjectMapper;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.servlet.http.HttpServletResponse;
import org.springframework.http.HttpStatus;
import org.springframework.http.MediaType;
import org.springframework.http.ProblemDetail;
import org.springframework.security.core.AuthenticationException;
import org.springframework.security.web.AuthenticationEntryPoint;
import org.springframework.stereotype.Component;

import java.io.IOException;
import java.net.URI;
import java.time.Instant;

@Component
public class CustomAuthenticationEntryPoint implements AuthenticationEntryPoint {

    private final ObjectMapper objectMapper;

    public CustomAuthenticationEntryPoint(ObjectMapper objectMapper) {
        this.objectMapper = objectMapper;
    }

    @Override
    public void commence(HttpServletRequest request, HttpServletResponse response, AuthenticationException ex)
            throws IOException {

        response.setStatus(HttpStatus.UNAUTHORIZED.value());
        response.setContentType(MediaType.APPLICATION_PROBLEM_JSON_VALUE);

        ProblemDetail problem = ProblemDetail.forStatusAndDetail(HttpStatus.UNAUTHORIZED, ex.getMessage());
        problem.setTitle("Authentication Required");
        problem.setType(URI.create("https://api.example.com/errors/unauthorized"));
        problem.setProperty("timestamp", Instant.now());
        problem.setProperty("path", request.getRequestURI());

        objectMapper.writeValue(response.getOutputStream(), problem);
    }
}
```

---

## 6. Best Practices

- **Adopt Lambda DSL Exclusively**: Avoid legacy chained `.and()` configurations; use modern nested lambda expressions (`csrf(csrf -> ...)`).
- **Enforce `@EnableMethodSecurity`**: Prefer modern `@EnableMethodSecurity` over deprecated `@EnableGlobalMethodSecurity`.
- **Stateless Session Creation for REST APIs**: Set `SessionCreationPolicy.STATELESS` when using JWTs to prevent session hijacking and memory consumption on the server.
- **Differentiate Roles vs Authorities**: Roles always carry the `ROLE_` prefix in Spring Security (`hasRole('ADMIN')` looks for `ROLE_ADMIN`; `hasAuthority('ADMIN')` looks for `ADMIN`).
- **Use `DelegatingSecurityContextAsyncTaskExecutor`**: Propagate the security context automatically to async or virtual thread workers.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Why It Fails | Correct Approach |
|---|---|---|
| **Inheriting from `WebSecurityConfigurerAdapter`** | Completely removed in Spring Security 6; causes build compilation errors. | Declare a `@Bean SecurityFilterChain filterChain(HttpSecurity http)` method. |
| **Confusing `hasRole` and `hasAuthority`** | `hasRole("ADMIN")` fails if the authority is mapped as `"ADMIN"` instead of `"ROLE_ADMIN"`. | Use `hasAuthority()` or prepend `ROLE_` during claims mapping. |
| **Disabling CSRF for Browser Cookie Sessions** | Opens authenticated web users to Cross-Site Request Forgery attacks. | Only disable CSRF for stateless REST APIs using `Authorization: Bearer` tokens. |
| **Losing SecurityContext in `@Async` Calls** | `ThreadLocal` context does not cross standard thread pool boundaries, leaving principal `null`. | Configure `DelegatingSecurityContextAsyncTaskExecutor`. |
| **Permitting Insecure Spring Expression Language** | Writing unvetted SpEL expressions in `@PreAuthorize` risks remote code execution. | Restrict SpEL expressions to simple permission checks (`#id == authentication.name`). |
| **Missing AccessDeniedHandler Customization** | When a user lacks permissions, the server returns an empty 403 or HTML error page. | Register a custom `AccessDeniedHandler` returning RFC 9457 `ProblemDetail`. |

---

## 8. Performance & Security Considerations

### 8.1 Virtual Threads Security Context
Java 21 Virtual Threads maintain independent `ThreadLocal` bindings; however, creating tens of thousands of heavy ThreadLocal objects can increase heap overhead. Always clean up context in filters.

### 8.2 Mitigating Spring4Shell (CVE-2022-22965)
Ensure Spring Boot 3+ is deployed on Java 17/21. Spring 6 hardened class binding filters against classloader module manipulation attacks.

---

## 9. Comparison / Decision Matrix

| Mechanism | `@Secured` | `@PreAuthorize` | URL `requestMatchers` |
|---|---|---|---|
| **Specification** | Legacy Spring | Modern SpEL | Web Filter Layer |
| **Method Parameter Access** | No | Yes (`#userId == principal.id`)| No |
| **Evaluation Timing** | Method invocation | Method invocation | Edge HTTP ingress |
| **Best For** | Legacy migration | Granular business method rules | Bulk endpoint routing |

---

## 10. Real-World Interview Q&A

<details>
<summary><strong>1. How did Spring Security 6 replace `WebSecurityConfigurerAdapter`?</strong></summary>

Spring Security 6 completely removed `WebSecurityConfigurerAdapter` in favor of a component-based configuration model. Developers register a `@Bean SecurityFilterChain` returning `HttpSecurity.build()`, configured exclusively via the functional Lambda DSL.
</details>

<details>
<summary><strong>2. What is the difference between `hasRole('ADMIN')` and `hasAuthority('ADMIN')`?</strong></summary>

`hasRole('ADMIN')` automatically prepends the prefix `ROLE_` and checks for the authority `ROLE_ADMIN`. `hasAuthority('ADMIN')` checks for the literal exact string `ADMIN` without prefixing.
</details>

<details>
<summary><strong>3. How does `DelegatingFilterProxy` bridge Servlet Filters with Spring beans?</strong></summary>

The Servlet container does not know about Spring's `ApplicationContext`. `DelegatingFilterProxy` is registered in the Servlet container, intercepts inbound requests, and delegates execution to a Spring bean named `springSecurityFilterChain` (`FilterChainProxy`), which manages all security filters.
</details>

<details>
<summary><strong>4. How do you propagate `SecurityContext` to `@Async` or background threads?</strong></summary>

Because `SecurityContextHolder` uses `ThreadLocal` by default, background threads do not inherit the context. Wrap your `TaskExecutor` in a `DelegatingSecurityContextAsyncTaskExecutor` or explicitly wrap tasks using `SecurityContextRunnable`.
</details>

<details>
<summary><strong>5. When should CSRF protection be disabled?</strong></summary>

CSRF protection should ONLY be disabled for stateless REST APIs where clients authenticate exclusively via HTTP headers (`Authorization: Bearer <jwt>`) without using cookies. If browser cookies are used for authentication or sessions, CSRF MUST be enabled.
</details>

<details>
<summary><strong>6. What is the role of `AuthenticationManager` vs `AuthenticationProvider`?</strong></summary>

`AuthenticationManager` (typically `ProviderManager`) is the primary API entry point for authentication. It iterates through a list of registered `AuthenticationProvider`s (e.g. `DaoAuthenticationProvider`, `JwtAuthenticationProvider`) until one returns an authenticated `Authentication` token.
</details>

<details>
<summary><strong>7. How does Spring Security 6 evaluate authorization at the HTTP layer?</strong></summary>

Spring Security 6 replaced `FilterSecurityInterceptor` with `AuthorizationFilter`. It intercepts requests after authentication and delegates authorization decisions to an `AuthorizationManager<RequestAuthorizationContext>`.
</details>

<details>
<summary><strong>8. What is the purpose of `StrictHttpFirewall`?</strong></summary>

`StrictHttpFirewall` rejects suspicious HTTP requests containing path traversal sequences (`//`, `../`), URL-encoded slashes (`%2F`), semicolons, or invalid characters to protect the application from directory traversal and HTTP request smuggling attacks.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Configure Custom JWT Authority Converter</strong></summary>

**Prompt**: Map a JWT claim `cognito:groups` (containing `["admin", "billing"]`) to Spring authorities `ROLE_ADMIN` and `ROLE_BILLING`.

**Solution**:
Inspect Example 1: extract string list from `jwt.getClaimAsStringList("cognito:groups")` and map to `new SimpleGrantedAuthority("ROLE_" + g.toUpperCase())`.
</details>

<details>
<summary><strong>Exercise 2: Write Method Security with SpEL</strong></summary>

**Prompt**: Protect a method so that only the user whose ID matches `#userId` or an admin can access it.

**Solution**:
Inspect Example 2: annotate method with `@PreAuthorize("hasRole('ADMIN') or #userId == authentication.name")`.
</details>

<details>
<summary><strong>Exercise 3: Test Secured Endpoint with `@WithMockUser`</strong></summary>

**Prompt**: Write a MockMvc test verifying that a user with role `ADMIN` accesses `/api/admin` with HTTP 200, while `USER` receives HTTP 403.

**Solution**:
```java
@Test
@WithMockUser(roles = "ADMIN")
void shouldAllowAdmin() throws Exception {
    mockMvc.perform(get("/api/admin")).andExpect(status().isOk());
}
```
</details>

<details>
<summary><strong>Exercise 4: Implement Custom AuthenticationEntryPoint</strong></summary>

**Prompt**: Configure an entry point returning RFC 9457 `ProblemDetail` JSON on unauthorized requests.

**Solution**:
Inspect Example 5: register `CustomAuthenticationEntryPoint` with `http.exceptionHandling(e -> e.authenticationEntryPoint(...))`.
</details>

<details>
<summary><strong>Exercise 5: Configure Async Security Context Propagation</strong></summary>

**Prompt**: Configure a bean ensuring `@Async` methods can read `SecurityContextHolder.getContext().getAuthentication()`.

**Solution**:
Inspect Example 3: wrap task executor in `DelegatingSecurityContextAsyncTaskExecutor`.
</details>

<details>
<summary><strong>Exercise 6: Test CORS Configuration</strong></summary>

**Prompt**: Write an integration test asserting that `OPTIONS` preflight requests from an allowed origin return CORS headers.

**Solution**:
```java
mockMvc.perform(options("/api/v1/orders").header("Origin", "https://app.example.com").header("Access-Control-Request-Method", "POST"))
        .andExpect(status().isOk())
        .andExpect(header().exists("Access-Control-Allow-Origin"));
```
</details>

---

## 12. Summary & Quick Reference Cheatsheet

```
┌────────────────────────────────────────────────────────────────────────┐
│                   SPRING SECURITY 6.x CHEATSHEET                       │
├────────────────────────────────────────────────────────────────────────┤
│ 1. Configuration: Declare @Bean SecurityFilterChain using Lambda DSL   │
│ 2. Sessions: SessionCreationPolicy.STATELESS for REST APIs             │
│ 3. Method Security: @EnableMethodSecurity with @PreAuthorize SpEL      │
│ 4. Roles vs Authorities: hasRole checks for "ROLE_" prefix             │
│ 5. Async Threads: Wrap executors in DelegatingSecurityContextExecutor  │
│ 6. Errors: Implement AuthenticationEntryPoint returning ProblemDetail  │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 13. Further Reading & Official Resources

- [Spring Security 6.4 Reference Documentation](https://docs.spring.io/spring-security/reference/)
- [Migrating to Spring Security 6.x](https://docs.spring.io/spring-security/reference/6.0/migration/index.html)
- [Spring Security Architecture Guide](https://spring.io/guides/topicals/spring-security-architecture)
- [OWASP Secure Coding Practices Checklist](https://owasp.org/www-project-secure-coding-practices-quick-reference-guide/)
