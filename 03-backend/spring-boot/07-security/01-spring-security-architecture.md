# Spring Security Architecture: Filter Chains, SecurityContext & Authentication Flow

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | `03-web-mvc/08-interceptors-filters-and-argument-resolvers.md`, Servlet specification, ThreadLocal mechanics |
| **Estimated Reading Time** | 32 minutes |
| **Status** | Active |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework** | 6.2.x / 7.0.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven / Gradle |
| **Tested With** | Spring Security 6.4.x / 7.x, Spring Boot 3.4.x |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
   - [5.1 Modern SecurityFilterChain Bean with Lambda DSL](#51-modern-securityfilterchain-bean-with-lambda-dsl)
   - [5.2 Custom AuthenticationProvider with ProviderManager Flow](#52-custom-authenticationprovider-with-providermanager-flow)
   - [5.3 Inspecting and Populating SecurityContextHolder in Virtual Threads](#53-inspecting-and-populating-securitycontextholder-in-virtual-threads)
   - [5.4 Anti-Pattern: Extending Deprecated WebSecurityConfigurerAdapter](#54-anti-pattern-extending-deprecated-websecurityconfigureradapter)
   - [5.5 Custom Filter Placement in SecurityFilterChain via addFilterBefore](#55-custom-filter-placement-in-securityfilterchain-via-addfilterbefore)
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
In web applications, security encompasses two distinct orthogonal dimensions: **Authentication** (verifying *who* you are) and **Authorization** (verifying *what* you are allowed to do). Enforcing these checks manually within individual controller methods is disastrous: developers forget checks on newly added endpoints, authentication logic leaks into business code, and handling session fixation, cross-site request forgery (CSRF), and cryptographic hashing becomes error-prone and insecure.

### Historical Evolution
Spring Security (originally Acegi Security in 2003) adopted the Servlet `Filter` pipeline to intercept HTTP requests before they reach the `DispatcherServlet`. Early versions required extensive XML configurations. Spring Security 5 introduced configuration adapters (`WebSecurityConfigurerAdapter`), which were completely **removed** in Spring Security 6. Modern Spring Security 6/7 enforces a **component-based Lambda DSL** built around the `SecurityFilterChain` bean, prioritizing immutability, zero inheritance, and native integration with Java 21 virtual threads.

### Real-World Relevance
Spring Security is the enterprise standard for securing Java applications, protecting systems from single-sign-on (SSO) authentication to fine-grained OAuth2/OIDC microservice architectures and OWASP Top 10 vulnerabilities. Understanding filter ordering, the `SecurityContext` propagation contract, and authentication providers is mandatory for securing production services.

---

## 2. Core Concepts

### DelegatingFilterProxy & FilterChainProxy
1. **`DelegatingFilterProxy`**: A standard Servlet Filter registered in the servlet container (Tomcat, Jetty). It does not contain security logic; it simply acts as a bridge, delegating incoming HTTP requests to a Spring-managed bean named `springSecurityFilterChain`.
2. **`FilterChainProxy`**: The Spring bean that contains one or more `SecurityFilterChain` instances, matching incoming request URLs against configured security filter pipelines.

### SecurityFilterChain Order
Within a `SecurityFilterChain`, filters execute in a strict, predefined order:
1. `DisableEncodeUrlFilter`: Prevents URL session encoding.
2. `SecurityContextHolderFilter`: Loads the `SecurityContext` from the `SecurityContextRepository`.
3. `HeaderWriterFilter`: Writes secure HTTP headers (HSTS, CSP, X-Frame-Options).
4. `CorsFilter`: Processes CORS pre-flight `OPTIONS` requests.
5. `CsrfFilter`: Enforces CSRF token validation.
6. `LogoutFilter`: Handles logout actions.
7. `BearerTokenAuthenticationFilter` / `UsernamePasswordAuthenticationFilter`: Extracts credentials and authenticates.
8. `ExceptionTranslationFilter`: Catches `AccessDeniedException` and `AuthenticationException`, converting them to HTTP 401 or 403 responses.
9. `AuthorizationFilter`: Final access decision evaluator matching request matchers against granted authorities.

### SecurityContextHolder & SecurityContext
- `SecurityContext`: Holds the active `Authentication` object.
- `SecurityContextHolder`: Static accessor providing storage strategies:
  - `MODE_THREADLOCAL` (Default): Stores context in a standard `ThreadLocal`.
  - `MODE_INHERITABLETHREADLOCAL`: Inherits context across spawned child threads (not recommended with virtual threads or thread pools).
  - `MODE_GLOBAL`: Static context for standalone desktop applications.

### Authentication & AuthenticationManager
- `Authentication`: Represents the token/identity (e.g., `UsernamePasswordAuthenticationToken`). Contains `principal`, `credentials`, `authorities`, and `isAuthenticated()`.
- `AuthenticationManager`: Central coordinator. Default implementation is `ProviderManager`, which delegates to a list of configured `AuthenticationProvider`s.

---

## 3. How It Works Internally

```
+------------------------------------------------------------------------------------+
|                         Spring Security Architecture Flow                          |
+------------------------------------------------------------------------------------+
|                                                                                    |
|  Incoming HTTP Request                                                             |
|       |                                                                            |
|       v                                                                            |
|  [ Servlet Container (Tomcat) ]                                                    |
|       |                                                                            |
|       v                                                                            |
|  DelegatingFilterProxy ("springSecurityFilterChain")                               |
|       |                                                                            |
|       v                                                                            |
|  FilterChainProxy                                                                  |
|       |                                                                            |
|       +---> Evaluates SecurityFilterChain matching request URL                     |
|             |                                                                      |
|             v                                                                      |
|       SecurityContextHolderFilter (Loads existing SecurityContext from session/token)
|             |                                                                      |
|             v                                                                      |
|       CorsFilter -> CsrfFilter                                                     |
|             |                                                                      |
|             v                                                                      |
|       AuthenticationFilter (e.g. JwtAuthenticationFilter)                          |
|             |                                                                      |
|             +---> Delegates to AuthenticationManager (ProviderManager)             |
|             |     |                                                                |
|             |     +---> Queries AuthenticationProvider (JwtAuthenticationProvider) |
|             |     |     Validates token -> Returns authenticated Authentication    |
|             |     v                                                                |
|             +---> Stores Authentication in SecurityContextHolder                   |
|             |                                                                      |
|             v                                                                      |
|       ExceptionTranslationFilter (Catches Auth Exceptions -> Returns 401/403)      |
|             |                                                                      |
|             v                                                                      |
|       AuthorizationFilter (Checks requestMatchers e.g. hasRole('ADMIN'))           |
|             |                                                                      |
|             v (If authorized)                                                      |
|  DispatcherServlet -> Controller Method Execution                                  |
|                                                                                    |
+------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### Maven Dependency
```xml
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-starter-security</artifactId>
</dependency>
```

### Modern Component-Based Security Configuration
```java
@Configuration
@EnableWebSecurity
public class SecurityConfig {

    @Bean
    public SecurityFilterChain securityFilterChain(HttpSecurity http) throws Exception {
        return http
            .csrf(csrf -> csrf.disable()) // Stateless APIs disable CSRF
            .authorizeHttpRequests(auth -> auth
                .requestMatchers("/api/public/**", "/health").permitAll()
                .requestMatchers("/api/admin/**").hasRole("ADMIN")
                .anyRequest().authenticated()
            )
            .httpBasic(Customizer.withDefaults())
            .build();
    }
}
```

---

## 5. Practical Examples

### 5.1 Modern SecurityFilterChain Bean with Lambda DSL
```java
package com.example.security.config;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.security.config.Customizer;
import org.springframework.security.config.annotation.web.builders.HttpSecurity;
import org.springframework.security.config.annotation.web.configuration.EnableWebSecurity;
import org.springframework.security.config.http.SessionCreationPolicy;
import org.springframework.security.web.SecurityFilterChain;
import org.springframework.security.web.header.writers.ReferrerPolicyHeaderWriter;

@Configuration
@EnableWebSecurity
public class EnterpriseSecurityConfiguration {

    @Bean
    public SecurityFilterChain apiSecurityFilterChain(HttpSecurity http) throws Exception {
        return http
            // 1. Enforce stateless session management for REST APIs
            .sessionManagement(session -> session
                .sessionCreationPolicy(SessionCreationPolicy.STATELESS)
            )
            // 2. Disable CSRF for purely stateless token-based REST endpoints
            .csrf(csrf -> csrf.disable())
            // 3. Security Headers hardening
            .headers(headers -> headers
                .frameOptions(frame -> frame.deny())
                .referrerPolicy(ref -> ref.policy(ReferrerPolicyHeaderWriter.ReferrerPolicy.STRICT_ORIGIN_WHEN_CROSS_ORIGIN))
            )
            // 4. Request Authorization Rules
            .authorizeHttpRequests(auth -> auth
                .requestMatchers("/api/v1/auth/**", "/actuator/health").permitAll()
                .requestMatchers("/api/v1/admin/**").hasAuthority("ROLE_ADMIN")
                .requestMatchers("/api/v1/orders/**").hasAnyAuthority("ROLE_USER", "ROLE_ADMIN")
                .anyRequest().authenticated()
            )
            .httpBasic(Customizer.withDefaults())
            .build();
    }
}
```

### 5.2 Custom AuthenticationProvider with ProviderManager Flow
```java
package com.example.security.auth;

import org.springframework.security.authentication.AuthenticationProvider;
import org.springframework.security.authentication.BadCredentialsException;
import org.springframework.security.authentication.UsernamePasswordAuthenticationToken;
import org.springframework.security.core.Authentication;
import org.springframework.security.core.AuthenticationException;
import org.springframework.security.core.authority.SimpleGrantedAuthority;
import org.springframework.security.crypto.password.PasswordEncoder;
import org.springframework.stereotype.Component;

import java.util.List;

@Component
public class CustomDatabaseAuthenticationProvider implements AuthenticationProvider {

    private final CustomUserDetailsService userDetailsService;
    private final PasswordEncoder passwordEncoder;

    public CustomDatabaseAuthenticationProvider(
        CustomUserDetailsService userDetailsService, 
        PasswordEncoder passwordEncoder
    ) {
        this.userDetailsService = userDetailsService;
        this.passwordEncoder = passwordEncoder;
    }

    @Override
    public Authentication authenticate(Authentication authentication) throws AuthenticationException {
        String username = authentication.getName();
        String rawPassword = authentication.getCredentials().toString();

        var userDetails = userDetailsService.loadUserByUsername(username);

        // Verify cryptographic hash
        if (!passwordEncoder.matches(rawPassword, userDetails.getPassword())) {
            throw new BadCredentialsException("Invalid username or password");
        }

        // Return authenticated token containing authorities and cleared credentials
        return new UsernamePasswordAuthenticationToken(
            userDetails,
            null, // Clear credentials from memory!
            userDetails.getAuthorities()
        );
    }

    @Override
    public boolean supports(Class<?> authentication) {
        return UsernamePasswordAuthenticationToken.class.isAssignableFrom(authentication);
    }
}
```

### 5.3 Inspecting and Populating SecurityContextHolder in Virtual Threads
```java
package com.example.security.service;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.security.core.Authentication;
import org.springframework.security.core.context.SecurityContext;
import org.springframework.security.core.context.SecurityContextHolder;
import org.springframework.stereotype.Service;

@Service
public class IdentityInspectionService {

    private static final Logger log = LoggerFactory.getLogger(IdentityInspectionService.class);

    /**
     * Java 21 / Spring Boot 3.4+:
     * In Virtual Thread per Task environments, Spring Security's SecurityContextHolderFilter
     * cleanly manages ThreadLocal cleanup upon request thread completion.
     */
    public String getCurrentAuthenticatedUsername() {
        SecurityContext context = SecurityContextHolder.getContext();
        Authentication auth = context.getAuthentication();

        if (auth == null || !auth.isAuthenticated()) {
            log.warn("Unauthenticated attempt to access identity context!");
            return "ANONYMOUS";
        }

        log.info("Processing request for authenticated principal: {} with roles: {}",
            auth.getName(), auth.getAuthorities());

        return auth.getName();
    }
}
```

### 5.4 Anti-Pattern: Extending Deprecated WebSecurityConfigurerAdapter
```java
// =========================================================================
// ANTI-PATTERN: Extending WebSecurityConfigurerAdapter (REMOVED in Spring 6!)
// =========================================================================
// DOES NOT COMPILE IN SPRING BOOT 3+:
// The class WebSecurityConfigurerAdapter was deprecated in 5.7 and completely DELETED in 6.0!
@Configuration
public class LegacySecurityConfig extends WebSecurityConfigurerAdapter {
    @Override
    protected void configure(HttpSecurity http) throws Exception {
        http.authorizeRequests().anyRequest().authenticated();
    }
}

// =========================================================================
// PRODUCTION FIX: Pure Component-Based SecurityFilterChain Bean
// =========================================================================
@Configuration
@EnableWebSecurity
public class ModernSecurityConfig {
    @Bean
    public SecurityFilterChain filterChain(HttpSecurity http) throws Exception {
        return http
            .authorizeHttpRequests(auth -> auth.anyRequest().authenticated())
            .build();
    }
}
```

### 5.5 Custom Filter Placement in SecurityFilterChain via addFilterBefore
```java
package com.example.security.filter;

import jakarta.servlet.FilterChain;
import jakarta.servlet.ServletException;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.servlet.http.HttpServletResponse;
import org.springframework.security.authentication.UsernamePasswordAuthenticationToken;
import org.springframework.security.core.authority.SimpleGrantedAuthority;
import org.springframework.security.core.context.SecurityContextHolder;
import org.springframework.web.filter.OncePerRequestFilter;

import java.io.IOException;
import java.util.List;

/**
 * Custom Header-Based API Key Authentication Filter.
 * Executes once per request and validates an X-API-KEY header.
 */
public class ApiKeyAuthenticationFilter extends OncePerRequestFilter {

    private static final String API_KEY_HEADER = "X-API-KEY";
    private static final String VALID_KEY = "secret-production-api-token";

    @Override
    protected void doFilterInternal(
        HttpServletRequest request, 
        HttpServletResponse response, 
        FilterChain filterChain
    ) throws ServletException, IOException {

        String apiKey = request.getHeader(API_KEY_HEADER);

        if (VALID_KEY.equals(apiKey)) {
            // Populate SecurityContext with machine authentication
            var auth = new UsernamePasswordAuthenticationToken(
                "ServiceAccount",
                null,
                List.of(new SimpleGrantedAuthority("ROLE_MACHINE"))
            );
            SecurityContextHolder.getContext().setAuthentication(auth);
        }

        // Always continue filter chain
        filterChain.doFilter(request, response);
    }
}
```

```java
// Registering the custom filter in SecurityFilterChain:
@Bean
public SecurityFilterChain filterChain(HttpSecurity http) throws Exception {
    return http
        .addFilterBefore(new ApiKeyAuthenticationFilter(), UsernamePasswordAuthenticationFilter.class)
        .authorizeHttpRequests(auth -> auth
            .requestMatchers("/api/machine/**").hasRole("MACHINE")
            .anyRequest().authenticated()
        )
        .build();
}
```

---

## 6. Best Practices

1. **Use the Lambda DSL Exclusively**: Configure `http.csrf(...)`, `http.authorizeHttpRequests(...)`, and `http.sessionManagement(...)` using lambda methods (`Customizer.withDefaults()` or lambda consumers).
2. **Clear Credentials After Authentication**: Never retain raw user passwords in heap memory. In `AuthenticationProvider`, return tokens with `credentials = null`.
3. **Explicitly Configure Stateless Sessions for REST**: Always set `SessionCreationPolicy.STATELESS` on REST APIs to prevent accidental creation of server-side `JSESSIONID` sessions.
4. **Order Multiple `SecurityFilterChain` Beans via `@Order`**: If defining multiple chains (e.g. one for `/api/**` with JWT, one for `/web/**` with form login), place `@Order(1)` on the most specific chain and `@Order(2)` on the catch-all chain.
5. **Never Use `permitAll()` on Catch-All Paths**: Configure specific endpoints for public access (`/health`, `/login`), and default `anyRequest().authenticated()` at the end of the rule list.
6. **Prefer `OncePerRequestFilter` for Custom Filters**: Extend `OncePerRequestFilter` to ensure custom authentication logic is executed exactly once per incoming request, even across internal servlet dispatches (forward/include).

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Using `hasRole("ROLE_ADMIN")`** | `hasRole()` automatically prepends the `"ROLE_"` prefix; passing `"ROLE_ADMIN"` checks for `"ROLE_ROLE_ADMIN"`. | Use `hasRole("ADMIN")` or `hasAuthority("ROLE_ADMIN")`. |
| **Wrong Order in `authorizeHttpRequests`** | Rules are evaluated in first-match order; placing `anyRequest().authenticated()` before specific paths blocks everything. | Declare specific public paths first, and `anyRequest()` last. |
| **Disabling CSRF on Stateful Cookie Sessions** | Opens the application to Cross-Site Request Forgery exploits. | Only disable CSRF on stateless, token-based (Bearer) APIs. |
| **Setting `MODE_INHERITABLETHREADLOCAL` with Thread Pools** | Worker threads in thread pools retain stale authentication contexts from previously executed requests. | Use `DelegatingSecurityContextExecutor` or clear contexts explicitly. |
| **Using `antMatchers` in Spring Security 6** | `antMatchers()` was removed in Spring Security 6. | Use `requestMatchers(...)`. |
| **Swallowing Exceptions in Custom Filters** | Catching exceptions and writing raw JSON without delegating to `AuthenticationEntryPoint` breaks standard error handling. | Throw `AuthenticationException` or invoke `AuthenticationEntryPoint.commence()`. |

---

## 8. Performance & Security Notes

### Performance
- **Zero Session Overhead**: Configuring `SessionCreationPolicy.STATELESS` prevents Tomcat/Jetty from allocating memory for HTTP sessions, allowing the application to scale horizontally without session replication.
- **Filter Chain Short-Circuiting**: Unmatched `SecurityFilterChain` instances are skipped immediately based on top-level `securityMatcher()`, keeping latency minimal.

### Security
- **Fail-Closed Principle**: Spring Security defaults to denying access if a configuration is ambiguous or if an unauthenticated request hits an unspecified path.
- **Timing Attack Mitigation**: When validating credentials, ensure password matchers take constant time even for non-existent users by executing dummy password hashing to prevent user enumeration attacks.

---

## 9. Comparison / When to Use What

| Security Component | Responsibility | Placement | Lifecycle |
| :--- | :--- | :--- | :--- |
| **`SecurityFilterChain`** | Intercepts HTTP requests | Servlet Filter layer | Singleton bean |
| **`AuthenticationManager`** | Authenticates tokens | Called inside Filters | Singleton bean |
| **`AuthenticationProvider`** | Strategy for specific token type | Registered in ProviderManager | Singleton bean |
| **`SecurityContext`** | Holds current principal & roles | Bound to calling thread | Request-scoped (ThreadLocal) |
| **`AuthorizationFilter`** | Checks access permissions | Final filter in chain | Singleton bean |

---

## 10. Interview Questions

<details>
<summary><strong>1. Explain the relationship between <code>DelegatingFilterProxy</code> and <code>FilterChainProxy</code>.</strong></summary>

- `DelegatingFilterProxy` is a standard Java Servlet Filter registered in the servlet container (Tomcat). It has no direct knowledge of security rules; its sole role is to look up the Spring bean named `springSecurityFilterChain` from the `WebApplicationContext` and delegate all incoming requests to it.
- `FilterChainProxy` is that Spring bean. It manages and executes the actual list of `SecurityFilterChain` beans, matching requests against URL patterns and invoking the security filter pipeline.
</details>

<details>
<summary><strong>2. Why was <code>WebSecurityConfigurerAdapter</code> removed in Spring Security 6?</strong></summary>

`WebSecurityConfigurerAdapter` forced applications into an inheritance-based configuration model with tight coupling and fragile method overriding (`configure(HttpSecurity)` vs `configure(WebSecurity)`). Removing it encourages a modern, component-based architecture where security configuration is composed through standalone `@Bean` declarations (`SecurityFilterChain`), promoting immutability, readability, and modular design.
</details>

<details>
<summary><strong>3. What is the difference between <code>hasRole('ADMIN')</code> and <code>hasAuthority('ROLE_ADMIN')</code>?</strong></summary>

In Spring Security, a "Role" is simply an authority with a standardized `"ROLE_"` prefix.
- `hasRole('ADMIN')` automatically prepends `"ROLE_"` and checks if the principal possesses the authority `"ROLE_ADMIN"`.
- `hasAuthority('ROLE_ADMIN')` performs an exact string comparison without prefix modification.
Calling `hasRole('ROLE_ADMIN')` results in checking for `"ROLE_ROLE_ADMIN"`, which will fail.
</details>

<details>
<summary><strong>4. How does <code>SecurityContextHolder</code> work and what happens in Java 21 with Virtual Threads?</strong></summary>

By default, `SecurityContextHolder` uses `ThreadLocal` storage (`MODE_THREADLOCAL`). In standard platform thread architectures, each request runs on one thread. With Java 21 Virtual Threads, millions of virtual threads can be spawned. Because virtual threads are lightweight Java objects, their `ThreadLocal` storage is automatically cleared and garbage collected when the virtual thread terminates at the end of the request, eliminating cross-request context leakage risks.
</details>

<details>
<summary><strong>5. What is the role of <code>ExceptionTranslationFilter</code> in the security filter chain?</strong></summary>

`ExceptionTranslationFilter` sits high in the security chain to catch exceptions thrown downstream (specifically by `AuthorizationFilter`).
- If an `AuthenticationException` is caught (user is unauthenticated), it triggers the configured `AuthenticationEntryPoint` to return HTTP 401 or redirect to login.
- If an `AccessDeniedException` is caught (user is authenticated but lacks required role), it delegates to `AccessDeniedHandler` to return HTTP 403.
</details>

<details>
<summary><strong>6. How can an application define multiple distinct <code>SecurityFilterChain</code> beans?</strong></summary>

By defining multiple `@Bean` methods returning `SecurityFilterChain` and ordering them using `@Order(N)`. Each chain must define a `http.securityMatcher("/api/**")` to declare which URL patterns it owns. Requests are evaluated against chains in order of their `@Order` value until a match is found.
</details>

<details>
<summary><strong>7. Why should <code>SessionCreationPolicy.STATELESS</code> be configured for REST APIs?</strong></summary>

By default, Spring Security uses the HTTP session to persist the `SecurityContext` between requests (`JSESSIONID`). For stateless REST APIs using JWT or API keys, server-side session tracking is unnecessary, wastes memory, prevents horizontal auto-scaling without shared session stores, and leaves the door open to CSRF attacks. Setting `STATELESS` ensures Spring Security never creates or uses an HTTP session.
</details>

<details>
<summary><strong>8. What is the contract of an <code>AuthenticationProvider</code>?</strong></summary>

An `AuthenticationProvider` must implement two methods:
1. `authenticate(Authentication auth)`: Evaluates credentials. If successful, returns a fully authenticated `Authentication` token populated with `GrantedAuthority` collections and cleared credentials. If invalid, throws `AuthenticationException`. If unsupported, returns `null`.
2. `supports(Class<?> authClass)`: Returns `true` if this provider can process the given authentication token type (e.g. `UsernamePasswordAuthenticationToken`).
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Basic Stateless SecurityFilterChain</strong></summary>

**Task**: Write a minimal `SecurityFilterChain` bean configuring stateless sessions, permitting `/public/**`, and requiring authentication for all other requests.

```java
@Bean
public SecurityFilterChain filterChain(HttpSecurity http) throws Exception {
    return http
        .csrf(csrf -> csrf.disable())
        .sessionManagement(s -> s.sessionCreationPolicy(SessionCreationPolicy.STATELESS))
        .authorizeHttpRequests(a -> a
            .requestMatchers("/public/**").permitAll()
            .anyRequest().authenticated()
        )
        .build();
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Role-Based Authorization Mapping</strong></summary>

**Task**: Configure HTTP authorization rules such that `/admin/**` requires role `ADMIN`, `/manager/**` requires `MANAGER` or `ADMIN`, and `/user/**` requires `USER`.

```java
.authorizeHttpRequests(auth -> auth
    .requestMatchers("/admin/**").hasRole("ADMIN")
    .requestMatchers("/manager/**").hasAnyRole("MANAGER", "ADMIN")
    .requestMatchers("/user/**").hasRole("USER")
    .anyRequest().authenticated()
)
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Multiple SecurityFilterChain Setup</strong></summary>

**Task**: Configure two distinct security filter chains: Chain 1 (Order 1) protects `/api/**` with stateless Basic auth; Chain 2 (Order 2) protects all other endpoints with Form Login.

```java
@Configuration
public class MultiChainConfig {
    @Bean
    @Order(1)
    public SecurityFilterChain apiChain(HttpSecurity http) throws Exception {
        return http
            .securityMatcher("/api/**")
            .csrf(c -> c.disable())
            .sessionManagement(s -> s.sessionCreationPolicy(SessionCreationPolicy.STATELESS))
            .httpBasic(Customizer.withDefaults())
            .authorizeHttpRequests(a -> a.anyRequest().authenticated())
            .build();
    }

    @Bean
    @Order(2)
    public SecurityFilterChain webChain(HttpSecurity http) throws Exception {
        return http
            .authorizeHttpRequests(a -> a.anyRequest().authenticated())
            .formLogin(Customizer.withDefaults())
            .build();
    }
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Custom AuthenticationEntryPoint for JSON 401</strong></summary>

**Task**: Implement an `AuthenticationEntryPoint` that returns an RFC 7807 `ProblemDetail` JSON payload with HTTP 401 instead of redirecting to a login page.

```java
@Component
public class JsonAuthenticationEntryPoint implements AuthenticationEntryPoint {
    @Override
    public void commence(HttpServletRequest req, HttpServletResponse res, AuthenticationException ex) throws IOException {
        res.setStatus(HttpServletResponse.SC_UNAUTHORIZED);
        res.setContentType(MediaType.APPLICATION_PROBLEM_JSON_VALUE);
        res.getWriter().write("""
            {"type":"about:blank","title":"Unauthorized","status":401,"detail":"Full authentication required"}
            """);
    }
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Custom Token Authentication Filter Integration</strong></summary>

**Task**: Write an `OncePerRequestFilter` that extracts a bearer token from the `Authorization` header, validates it, and sets a valid `Authentication` token in `SecurityContextHolder`.

```java
public class TokenAuthFilter extends OncePerRequestFilter {
    @Override
    protected void doFilterInternal(HttpServletRequest req, HttpServletResponse res, FilterChain chain) 
            throws ServletException, IOException {
        String authHeader = req.getHeader("Authorization");
        if (authHeader != null && authHeader.startsWith("Bearer test-token-123")) {
            var auth = new UsernamePasswordAuthenticationToken(
                "TestUser", null, List.of(new SimpleGrantedAuthority("ROLE_USER"))
            );
            SecurityContextHolder.getContext().setAuthentication(auth);
        }
        chain.doFilter(req, res);
    }
}
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Integration Test with MockMvc Security</strong></summary>

**Task**: Write a JUnit 5 test using `MockMvc` verifying that an unauthenticated request to `/api/admin` returns 401/403, and an authenticated request with `@WithMockUser(roles = "ADMIN")` returns 200.

```java
@SpringBootTest
@AutoConfigureMockMvc
class SecurityIntegrationTest {
    @Autowired private MockMvc mockMvc;

    @Test
    void unauthenticatedFails() throws Exception {
        mockMvc.perform(get("/api/admin/dashboard"))
            .andExpect(status().isUnauthorized());
    }

    @Test
    @WithMockUser(roles = "ADMIN")
    void adminSucceeds() throws Exception {
        mockMvc.perform(get("/api/admin/dashboard"))
            .andExpect(status().isOk());
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------------+
| Filter Chain: DelegatingFilterProxy -> FilterChainProxy -> SecurityFilterChain        |
| Config Model: Component-based @Bean SecurityFilterChain (Zero inheritance!)           |
| Context:      SecurityContextHolder.getContext().getAuthentication()                  |
| Authority:    hasRole('ADMIN') checks for 'ROLE_ADMIN'; hasAuthority() checks exact   |
| Stateless:    sessionManagement(s -> s.sessionCreationPolicy(STATELESS))              |
| Ordering:     Multiple chains require @Order(1), @Order(2) and http.securityMatcher()  |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Spring Security Official Reference Documentation](https://docs.spring.io/spring-security/reference/)
- [Spring Security Architecture Overview (Spring Blog)](https://spring.io/guides/topicals/spring-security-architecture)
- [Migrating from WebSecurityConfigurerAdapter to SecurityFilterChain](https://spring.io/blog/2022/02/21/spring-security-without-the-websecurityconfigureradapter)
- [OWASP Authentication Architecture Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Authentication_Cheat_Sheet.html)
