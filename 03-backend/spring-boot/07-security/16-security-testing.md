# 16. Security Testing & Test Slices in Spring Boot

| Metadata | Value |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | `01-spring-security-architecture.md`, `03-authorization-and-method-security.md`, JUnit 5, MockMvc |
| **Estimated Reading Time** | 35 minutes |
| **Status** | Complete |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | OpenJDK 21, Spring Boot 3.4.1, Spring Security Test 6.4 |

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
Security configurations in Spring Boot are notoriously easy to misconfigure. A misplaced matcher in `authorizeHttpRequests`, an inverted SpEL expression in `@PreAuthorize`, or a missing CSRF token can silently expose sensitive endpoints to unauthenticated users or unauthorized tenants without throwing runtime errors during application startup.

Manual testing with cURL or Postman is slow, error-prone, and cannot prevent regressions in continuous integration (CI) pipelines. Comprehensive **Security Testing** allows engineers to verify authentication flows, role-based method guards, CSRF defenses, and JWT claim parsing automatically as part of standard test execution.

### Historical Evolution
- **Spring Security 2.x - 3.x**: Developers manually instantiated `SecurityContextHolder` in `@BeforeEach` methods or wrote custom filters to stub authentication.
- **Spring Security 4.x - 5.x**: Introduced `spring-security-test` with `@WithMockUser`, `@WithUserDetails`, `SecurityMockMvcRequestPostProcessors`, and `SecurityMockMvcConfigurers`.
- **Spring Security 6.x / 7.x**: Added first-class testing utilities for OAuth2 JWT resource servers (`jwt()`), OpenID Connect logins (`oidcLogin()`), modern reactive WebTestClient testing (`SecurityMockServerConfigurers`), and simplified meta-annotations.

### Real-World Relevance
Automated security test suites act as living specifications of an application's access control matrix, guaranteeing that security refactorings or framework upgrades never introduce privilege escalation vulnerabilities.

---

## 2. Core Concepts

### Architectural Contracts
- **`spring-security-test`**: The dedicated testing module providing mock annotations, request post-processors, and matchers.
- **`@WithMockUser`**: Test execution annotation that populates `SecurityContextHolder` with an authenticated `UsernamePasswordAuthenticationToken` without hitting a database.
- **`@WithUserDetails`**: Test annotation that looks up an authentic `UserDetails` object via a registered `UserDetailsService` bean.
- **`SecurityMockMvcRequestPostProcessors`**: Factory providing request transformers such as `csrf()`, `httpBasic()`, `jwt()`, `oauth2Login()`, and `digest()`.
- **`SecurityMockMvcResultMatchers`**: Result matchers verifying security state, such as `authenticated()`, `unauthenticated()`, or `authentication()`.

### The Mental Model
```
[ JUnit 5 Test Case ]
        │
        ├── 1. @WithMockUser(username="admin", roles={"ADMIN"})
        │         └─ Populates SecurityContext before test method runs
        │
        ├── 2. mockMvc.perform(post("/api/admin/purge").with(csrf()))
        │         ├─ Injects valid CSRF token into request
        │         └─ Invokes FilterChainProxy -> AuthorizationFilter
        │
        └── 3. andExpect(status().isOk())
                  andExpect(authenticated().withRoles("ADMIN"))
```

### Invariants
1. Tests must clean up the `SecurityContextHolder` after execution; `WithSecurityContextTestExecutionListener` handles this automatically.
2. Sliced web tests (`@WebMvcTest`) do not load the full application context; custom security configuration classes must be imported explicitly via `@Import`.
3. `@WithMockUser` bypasses credential verification and password encoders entirely; it verifies authorization decisions, not authentication providers.

---

## 3. How It Works Internally

### Security Test Execution Listener Pipeline
When running tests with Spring TestContext Framework:
1. `WithSecurityContextTestExecutionListener` intercepts test method invocation.
2. Reads security annotations (`@WithMockUser`, `@WithUserDetails`, or custom `@WithSecurityContext`).
3. Delegates to the corresponding `WithSecurityContextFactory` to construct a populated `Authentication` object.
4. Calls `SecurityContextHolder.setContext(context)` before the test method executes.
5. In `@WebMvcTest` or `@SpringBootTest`, `MockMvc` executes requests through the registered `FilterChainProxy`.
6. After the test method completes, the listener calls `SecurityContextHolder.clearContext()`.

---

## 4. Syntax & Usage

### 1. Test Dependencies (`pom.xml`)
```xml
<dependency>
    <groupId>org.springframework.security</groupId>
    <artifactId>spring-security-test</artifactId>
    <scope>test</scope>
</dependency>
```

### 2. Standard Test Setup with `@WebMvcTest`
```java
package com.example.security.test;

import com.example.controller.OrderController;
import com.example.security.config.SecurityConfig;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.autoconfigure.web.servlet.WebMvcTest;
import org.springframework.context.annotation.Import;
import org.springframework.security.test.context.support.WithMockUser;
import org.springframework.test.web.servlet.MockMvc;

import static org.springframework.security.test.web.servlet.request.SecurityMockMvcRequestPostProcessors.csrf;
import static org.springframework.security.test.web.servlet.response.SecurityMockMvcResultMatchers.authenticated;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.delete;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.get;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.status;

@WebMvcTest(OrderController.class)
@Import(SecurityConfig.class)
class OrderControllerSecurityTest {

    @Autowired
    private MockMvc mockMvc;

    @Test
    void unauthenticatedUserShouldReceive401() throws Exception {
        mockMvc.perform(get("/api/orders/101"))
            .andExpect(status().isUnauthorized());
    }

    @Test
    @WithMockUser(username = "clerk", roles = "USER")
    void userCannotDeleteOrder() throws Exception {
        mockMvc.perform(delete("/api/orders/101").with(csrf()))
            .andExpect(status().isForbidden());
    }

    @Test
    @WithMockUser(username = "supervisor", roles = "ADMIN")
    void adminCanDeleteOrder() throws Exception {
        mockMvc.perform(delete("/api/orders/101").with(csrf()))
            .andExpect(status().isOk())
            .andExpect(authenticated().withUsername("supervisor"));
    }
}
```

---

## 5. Practical Examples

### Example 1: Mocking Custom JWT Claims & Scopes in Resource Server Tests
Testing stateless API endpoints that rely on custom JWT claims (e.g., `tenant_id` and specific OAuth2 scopes).

```java
package com.example.security.test;

import com.example.controller.BillingController;
import com.example.security.config.ResourceServerConfig;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.autoconfigure.web.servlet.WebMvcTest;
import org.springframework.context.annotation.Import;
import org.springframework.security.core.authority.SimpleGrantedAuthority;
import org.springframework.test.web.servlet.MockMvc;

import static org.springframework.security.test.web.servlet.request.SecurityMockMvcRequestPostProcessors.jwt;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.get;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.jsonPath;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.status;

@WebMvcTest(BillingController.class)
@Import(ResourceServerConfig.class)
class BillingControllerJwtTest {

    @Autowired
    private MockMvc mockMvc;

    @Test
    void shouldDenyAccessIfScopeIsMissing() throws Exception {
        mockMvc.perform(get("/api/billing/invoices")
                .with(jwt().authorities(new SimpleGrantedAuthority("SCOPE_profile"))))
            .andExpect(status().isForbidden());
    }

    @Test
    void shouldPermitAccessWithBillingScopeAndValidateTenantClaim() throws Exception {
        mockMvc.perform(get("/api/billing/invoices")
                .with(jwt()
                    .jwt(jwt -> jwt
                        .claim("tenant_id", "ACME-CORP")
                        .claim("email", "billing@acme.com")
                    )
                    .authorities(new SimpleGrantedAuthority("SCOPE_billing:read"))
                ))
            .andExpect(status().isOk());
    }
}
```

### Example 2: Reusable Domain Meta-Annotations (`@WithManagerUser`)
Eliminating boilerplate across test suites by creating custom security meta-annotations.

```java
package com.example.security.test;

import org.springframework.security.test.context.support.WithSecurityContext;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@Retention(RetentionPolicy.RUNTIME)
@WithSecurityContext(factory = WithManagerSecurityContextFactory.class)
public @interface WithManagerUser {
    String username() default "dept_manager";
    String department() default "ENGINEERING";
    long maxApprovalBudget() default 50000L;
}
```
```java
package com.example.security.test;

import org.springframework.security.authentication.UsernamePasswordAuthenticationToken;
import org.springframework.security.core.authority.SimpleGrantedAuthority;
import org.springframework.security.core.context.SecurityContext;
import org.springframework.security.core.context.SecurityContextHolder;
import org.springframework.security.test.context.support.WithSecurityContextFactory;

import java.util.List;

public class WithManagerSecurityContextFactory implements WithSecurityContextFactory<WithManagerUser> {

    @Override
    public SecurityContext createSecurityContext(WithManagerUser annotation) {
        SecurityContext context = SecurityContextHolder.createEmptyContext();

        // Custom domain principal
        ManagerPrincipal principal = new ManagerPrincipal(
            annotation.username(),
            annotation.department(),
            annotation.maxApprovalBudget()
        );

        var auth = new UsernamePasswordAuthenticationToken(
            principal,
            "mock-password",
            List.of(new SimpleGrantedAuthority("ROLE_MANAGER"))
        );

        context.setAuthentication(auth);
        return context;
    }

    public record ManagerPrincipal(String username, String department, long budgetLimit) {}
}
```

### Example 3: Edge Case – Testing Method Security on Service Classes Directly
Testing `@PreAuthorize` and `@PostAuthorize` business rules in isolation without starting a servlet web container.

```java
package com.example.security.test;

import com.example.domain.ExpenseReport;
import com.example.service.ExpenseService;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.security.access.AccessDeniedException;
import org.springframework.security.test.context.support.WithMockUser;

import static org.junit.jupiter.api.Assertions.assertDoesNotThrow;
import static org.junit.jupiter.api.Assertions.assertThrows;

@SpringBootTest
class ExpenseServiceMethodSecurityTest {

    @Autowired
    private ExpenseService expenseService;

    @Test
    @WithMockUser(username = "auditor", roles = "FINANCE")
    void financeUserCanAuditExpenses() {
        ExpenseReport report = new ExpenseReport(1L, "IT Equipment", 1200.0);
        assertDoesNotThrow(() -> expenseService.audit(report));
    }

    @Test
    @WithMockUser(username = "intern", roles = "INTERN")
    void internCannotApproveExpenses() {
        assertThrows(AccessDeniedException.class, () -> expenseService.approve(1L));
    }
}
```

### Example 4: Failure Anti-Pattern & Production Fix
**Anti-Pattern**: Forgetting `.with(csrf())` on mutating MockMvc calls (`POST`, `PUT`, `DELETE`), leading to false 403 Forbidden test failures that mask real authorization regressions.

```java
// BUGGY TEST: Fails with 403 Forbidden due to missing CSRF token, NOT role permissions!
@Test
@WithMockUser(roles = "ADMIN")
void testCreateUser() throws Exception {
    mockMvc.perform(post("/api/users")
            .contentType(MediaType.APPLICATION_JSON)
            .content("{\"name\":\"Alice\"}"))
        .andExpect(status().isCreated()); // FAILS with 403 Forbidden!
}
```

**Production Fix**:
Always append `.with(csrf())` to mutating requests when CSRF is enabled.

```java
// SECURE & ACCURATE: Injects CSRF token so authorization rules are tested accurately
@Test
@WithMockUser(roles = "ADMIN")
void testCreateUser() throws Exception {
    mockMvc.perform(post("/api/users")
            .with(csrf())
            .contentType(MediaType.APPLICATION_JSON)
            .content("{\"name\":\"Alice\"}"))
        .andExpect(status().isCreated());
}
```

### Example 5: High-Performance Non-Blocking Reactive WebTestClient Security Testing
Testing non-blocking Spring Boot WebFlux security endpoints.

```java
package com.example.security.reactive.test;

import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.autoconfigure.web.reactive.WebFluxTest;
import org.springframework.security.test.context.support.WithMockUser;
import org.springframework.test.web.reactive.server.WebTestClient;

import static org.springframework.security.test.web.reactive.server.SecurityMockServerConfigurers.csrf;

@WebFluxTest
class ReactiveSecurityTest {

    @Autowired
    private WebTestClient webTestClient;

    @Test
    @WithMockUser(roles = "OPERATOR")
    void operatorCanTriggerPipeline() {
        webTestClient
            .mutateWith(csrf())
            .post().uri("/api/pipeline/run")
            .exchange()
            .expectStatus().isOk();
    }
}
```

---

## 6. Best Practices

1. **Test Negative Cases Explicitly**: Ensure your test suite verifies that unauthenticated users receive 401 and unauthorized roles receive 403.
2. **Use `@WebMvcTest` with `@Import(SecurityConfig.class)`**: Keep unit test execution fast by testing controllers and security rules without launching full application contexts.
3. **Encapsulate Custom Principals in Meta-Annotations**: Build reusable annotations (`@WithTenantUser`, `@WithAdminUser`) backed by `WithSecurityContextFactory` to keep tests readable.
4. **Always Test CSRF Boundaries**: Verify that mutating endpoints fail with 403 when CSRF is missing and succeed when `.with(csrf())` is provided.
5. **Simulate Real JWT Tokens via `jwt()`**: When testing OAuth2 resource servers, use `.with(jwt())` to verify claims mapping rather than generic `@WithMockUser`.
6. **Verify Security Headers in Tests**: Assert that standard headers (`X-Content-Type-Options`, `Content-Security-Policy`) are present in responses.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Missing `.with(csrf())` on `POST`/`PUT` | Test fails with 403 Forbidden due to CSRF rejection rather than authorization bugs. | Append `.with(csrf())` to all mutating MockMvc requests. |
| Testing `@WebMvcTest` without security config | MockMvc uses default auto-configured security, ignoring custom filters and rules. | Add `@Import(WebSecurityConfig.class)` to the test class. |
| Using `@WithMockUser` for JWT endpoints | `@WithMockUser` injects a username/password token; controllers expecting `Jwt` fail with class cast exceptions. | Use `.with(jwt())` from `SecurityMockMvcRequestPostProcessors`. |
| Hardcoding `ROLE_` prefix in `@WithMockUser(roles = ...)` | Spring automatically prepends `ROLE_`, resulting in double prefix `ROLE_ROLE_ADMIN`. | Use `roles = "ADMIN"` or `authorities = "ROLE_ADMIN"`. |
| Forgetting to mock dependencies in `@WebMvcTest` | Context fails to start due to missing service beans. | Use `@MockBean` for all controller dependencies. |
| Bypassing method security during service unit tests | Plain Mockito unit tests don't activate Spring AOP proxies; `@PreAuthorize` is never executed. | Test method security with `@SpringBootTest` or `@ContextConfiguration`. |

---

## 8. Performance & Security Notes

- **Slice Test Speed**: `@WebMvcTest` loads only web-layer components (Controllers, Advice, Security Filter Chains), executing in milliseconds compared to seconds for full `@SpringBootTest` instances.
- **Context Caching**: Group tests with identical security configurations so Spring's `TestContextManager` can reuse cached application contexts, drastically reducing CI build times.
- **Virtual Threads Execution**: Test suites configured with `spring.threads.virtual.enabled=true` run concurrent mock tests efficiently without thread pool contention.

---

## 9. Comparison / When to Use What

| Annotation / Utility | Execution Level | Security Verification Scope | Speed | Best Use Case |
|---|---|---|---|---|
| **`@WithMockUser`** | Method or Class | Standard roles/authorities | Instant | Simple role-based MVC or Service tests |
| **`@WithUserDetails`** | Method or Class | Real database-backed user records | Fast | Custom `UserDetails` properties testing |
| **`SecurityMockMvcRequestPostProcessors.jwt()`** | Request-level | Full OAuth2 JWT claims, scopes, timestamps | Fast | Testing OAuth2 Resource Server APIs |
| **`TestRestTemplate` / Testcontainers** | End-to-End | Network, real TLS, HTTP protocol | Slow | Complete system integration verification |

---

## 10. Interview Questions

<details>
<summary>1. What is the difference between `@WithMockUser` and `@WithUserDetails`?</summary>

**Answer:**
`@WithMockUser` creates a mock `UsernamePasswordAuthenticationToken` directly from the values specified in the annotation (`username`, `roles`, `authorities`) without invoking any application services.
`@WithUserDetails` invokes your application's actual `UserDetailsService` bean using the provided username to construct the `UserDetails` object, making it ideal when business logic or custom principals depend on properties loaded from a database.
</details>

<details>
<summary>2. Why does a MockMvc `POST` request return HTTP 403 when running under `@WithMockUser(roles = "ADMIN")`?</summary>

**Answer:**
In Spring Security, CSRF protection is enabled by default for all mutating HTTP methods (`POST`, `PUT`, `DELETE`). If the MockMvc request does not include a valid CSRF token, `CsrfFilter` rejects the request with HTTP 403 Forbidden before the authorization rules ever evaluate the user's role. Adding `.with(csrf())` resolves the issue.
</details>

<details>
<summary>3. How do you test a controller method that requires `@AuthenticationPrincipal CustomUser user`?</summary>

**Answer:**
You have two options:
1. Use `@WithUserDetails(value = "username", userDetailsServiceBeanName = "...")` to load the real custom user.
2. Create a custom test annotation (e.g., `@WithCustomUser`) backed by an implementation of `WithSecurityContextFactory` that constructs and sets a `CustomUser` principal in `SecurityContextHolder`.
</details>

<details>
<summary>4. What is the role of `SecurityMockMvcConfigurers.springSecurity()`?</summary>

**Answer:**
When building a custom `MockMvc` instance using `MockMvcBuilders.webAppContextSetup(context)`, Spring Security filters are not applied by default. Applying `.apply(springSecurity())` integrates the `FilterChainProxy`, ensuring all security filters execute during the test.
</details>

<details>
<summary>5. How can you test that an unauthenticated request redirects to the login page versus returning HTTP 401?</summary>

**Answer:**
Using `MockMvcResultMatchers.status()` and `header()`:
```java
// For Form Login (Browser):
mockMvc.perform(get("/dashboard"))
    .andExpect(status().is3xxRedirection())
    .andExpect(redirectedUrlPattern("**/login"));

// For REST APIs:
mockMvc.perform(get("/api/data"))
    .andExpect(status().isUnauthorized());
```
</details>

<details>
<summary>6. How do you test reactive security in Spring Boot WebFlux?</summary>

**Answer:**
In WebFlux, use `WebTestClient` with `SecurityMockServerConfigurers`. You can mutate requests using `.mutateWith(mockUser().roles("ADMIN"))` or `.mutateWith(csrf())` to establish non-blocking reactive security contexts.
</details>

<details>
<summary>7. Why does `@WithMockUser(roles = "ROLE_ADMIN")` cause `hasRole("ADMIN")` checks to fail?</summary>

**Answer:**
Spring Security's `roles` attribute automatically prepends the prefix `ROLE_`. If you specify `roles = "ROLE_ADMIN"`, the resulting authority becomes `ROLE_ROLE_ADMIN`. To set raw authorities without automatic prefixing, use the `authorities` attribute instead: `@WithMockUser(authorities = "ROLE_ADMIN")`.
</details>

<details>
<summary>8. How do you verify that an endpoint returns correct security headers?</summary>

**Answer:**
By asserting response headers in MockMvc:
```java
mockMvc.perform(get("/api/public"))
    .andExpect(header().string("X-Content-Type-Options", "nosniff"))
    .andExpect(header().string("X-Frame-Options", "DENY"))
    .andExpect(header().exists("Strict-Transport-Security"));
```
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Basic Unauthenticated Test (Easy)</summary>

**Task**: Write a MockMvc test verifying that performing a `GET` request to `/api/admin/metrics` without credentials returns HTTP 401 Unauthorized.

**Solution**:
```java
@Test
void shouldRejectAnonymousUser() throws Exception {
    mockMvc.perform(get("/api/admin/metrics"))
        .andExpect(status().isUnauthorized());
}
```
</details>

<details>
<summary>Exercise 2: Testing Role Authorization (Easy)</summary>

**Task**: Write a test using `@WithMockUser` that verifies a user with `ROLE_SUPPORT` receives HTTP 403 Forbidden when attempting to access `/api/admin/metrics`.

**Solution**:
```java
@Test
@WithMockUser(roles = "SUPPORT")
void supportRoleShouldBeForbidden() throws Exception {
    mockMvc.perform(get("/api/admin/metrics"))
        .andExpect(status().isForbidden());
}
```
</details>

<details>
<summary>Exercise 3: Testing POST with CSRF and Roles (Medium)</summary>

**Task**: Write a test demonstrating that an admin user can execute a `POST` to `/api/users` when supplying `.with(csrf())`.

**Solution**:
```java
@Test
@WithMockUser(roles = "ADMIN")
void adminCanCreateUser() throws Exception {
    mockMvc.perform(post("/api/users")
            .with(csrf())
            .contentType(MediaType.APPLICATION_JSON)
            .content("{\"username\":\"newbie\"}"))
        .andExpect(status().isCreated());
}
```
</details>

<details>
<summary>Exercise 4: Mocking OAuth2 JWT Claims (Medium)</summary>

**Task**: Write a MockMvc test verifying that a JWT authenticated user with subject `"user-123"` and authority `"SCOPE_read:reports"` successfully reads `/api/reports`.

**Solution**:
```java
@Test
void userWithReadScopeCanAccessReports() throws Exception {
    mockMvc.perform(get("/api/reports")
            .with(jwt()
                .jwt(jwt -> jwt.subject("user-123"))
                .authorities(new SimpleGrantedAuthority("SCOPE_read:reports"))
            ))
        .andExpect(status().isOk());
}
```
</details>

<details>
<summary>Exercise 5: Custom Security Meta-Annotation Factory (Hard)</summary>

**Task**: Implement a custom annotation `@WithTenantUser(tenant="AcmeCorp")` and its corresponding `WithSecurityContextFactory` that sets a custom `TenantAuthenticationToken` in `SecurityContextHolder`.

**Solution**:
```java
public class WithTenantUserSecurityContextFactory implements WithSecurityContextFactory<WithTenantUser> {
    @Override
    public SecurityContext createSecurityContext(WithTenantUser annotation) {
        SecurityContext context = SecurityContextHolder.createEmptyContext();
        TenantPrincipal principal = new TenantPrincipal("test-user", annotation.tenant());
        var auth = new UsernamePasswordAuthenticationToken(
            principal, 
            "credentials", 
            List.of(new SimpleGrantedAuthority("ROLE_TENANT_USER"))
        );
        context.setAuthentication(auth);
        return context;
    }
}
```
</details>

<details>
<summary>Exercise 6: Testing Method Security in Service Layer (Hard)</summary>

**Task**: Write an integration test using `@SpringBootTest` verifying that invoking a service method `closeAccount(Long id)` throws `AccessDeniedException` when called by a user with role `USER` and succeeds with role `SUPERVISOR`.

**Solution**:
```java
@SpringBootTest
class AccountServiceSecurityTest {

    @Autowired
    private AccountService accountService;

    @Test
    @WithMockUser(roles = "USER")
    void userCannotCloseAccount() {
        assertThrows(AccessDeniedException.class, () -> accountService.closeAccount(101L));
    }

    @Test
    @WithMockUser(roles = "SUPERVISOR")
    void supervisorCanCloseAccount() {
        assertDoesNotThrow(() -> accountService.closeAccount(101L));
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Always Append `.with(csrf())`**: Mutating requests (`POST`, `PUT`, `DELETE`) fail with 403 if CSRF is omitted.
- **Use `.with(jwt())` for Resource Servers**: Mocks stateless JWT claims and scopes cleanly.
- **`roles` vs `authorities`**: `roles = "ADMIN"` automatically prepends `ROLE_`; `authorities = "ROLE_ADMIN"` sets the exact string.
- **Slice Testing**: `@WebMvcTest` + `@Import(SecurityConfig.class)` provides fast, isolated security verification.
- **Build Meta-Annotations**: Replace messy repetitive setup code with domain-specific annotations like `@WithAdminUser`.

---

## 13. Further Reading
- [Spring Security Reference: Testing Support](https://docs.spring.io/spring-security/reference/servlet/test/index.html)
- [Spring Security Reference: Testing Method Security](https://docs.spring.io/spring-security/reference/servlet/test/method.html)
- [Spring Security Reference: Testing OAuth 2.0](https://docs.spring.io/spring-security/reference/servlet/test/mockmvc/oauth2.html)
- [JUnit 5 User Guide](https://junit.org/junit5/docs/current/user-guide/)
- [MockMvc Documentation](https://docs.spring.io/spring-framework/reference/testing/spring-mvc-test-framework.html)
