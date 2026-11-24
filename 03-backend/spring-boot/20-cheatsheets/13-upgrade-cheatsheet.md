# Spring Boot Version Upgrade & Migration Cheatsheet

| Metadata | Details |
|---|---|
| **Topic** | Spring Boot 2.x -> 3.x -> 4.x Migration Checklist & Property Replacements |
| **Track** | Java Spring Boot |
| **Difficulty** | Advanced |
| **Prerequisites** | Java 17/21, Spring Boot 2.x, Jakarta EE 10 |
| **Java Version** | Java 21 / 25 LTS |
| **Spring Boot** | 3.4.x / 4.x |
| **Tested With** | OpenRewrite, Spring Boot Migrator, Maven |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [Internals & Architecture](#3-internals--architecture)
4. [Syntax & Categorized Migration Matrix](#4-syntax--categorized-migration-matrix)
5. [5 Practical Production Examples](#5-5-practical-production-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes to Avoid](#7-common-mistakes-to-avoid)
8. [Performance & Security Considerations](#8-performance--security-considerations)
9. [Comparison Table: Spring Boot 2.x vs 3.x vs 4.x](#9-comparison-table-spring-boot-2x-vs-3x-vs-4x)
10. [Technical Interview Questions & Answers](#10-technical-interview-questions--answers)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading & References](#13-further-reading--references)

---

## 1. What & Why
Upgrading Spring Boot across major versions is a mandatory operational routine to retain active open-source support, patch critical CVE vulnerabilities, and capitalize on runtime performance breakthroughs like Java 21 Virtual Threads, GraalVM AOT native compilation, and HTTP RFC 9457 standardizations. The transition from Spring Boot 2.x to 3.x represents the largest architectural shift in a decade, spearheaded by the transition from Java EE (`javax.*`) to Jakarta EE 10 (`jakarta.*`) and the modernization of Spring Security.

This cheatsheet provides a definitive migration checklist, automated recipes via OpenRewrite, and deprecated property translation mappings.

---

## 2. Core Concepts
- **Jakarta EE Package Migration**: Complete migration from `javax.persistence.*`, `javax.servlet.*`, and `javax.validation.*` to `jakarta.*`.
- **Baseline Requirements**: Java 17 minimum baseline in Boot 3.0; Java 21 recommended (LTS); Java 25 readiness in Boot 4.x.
- **Spring Security 6 Modernization**: Removal of `WebSecurityConfigurerAdapter`; adoption of functional Lambda DSL and component-based `SecurityFilterChain`.
- **Trailing Slash Matching Changes**: In Spring MVC 6+, `/api/users` no longer automatically matches `/api/users/`.
- **HTTP Client Modernization**: Shift from legacy `RestTemplate` to fluent, synchronous `RestClient` (Spring 6.1+).
- **Automated Refactoring**: Utilizing OpenRewrite recipes to automate 90%+ of mechanical import and property changes.

---

## 3. Internals & Architecture
Spring Boot 2.x to 3.x/4.x Major Migration Architecture:

```
[Legacy Spring Boot 2.x Application]
  - Java 8 / 11
  - javax.* namespace (Java EE 8)
  - WebSecurityConfigurerAdapter (Spring Security 5)
  - RestTemplate
  - spring.factories auto-configuration
                       |
                       | OpenRewrite Migration Pipeline
                       v
+-------------------------------------------------------------+
|                 Namespace & API Modernization               |
|  1. javax.* -> jakarta.* (Persistence, Servlet, Validation) |
|  2. SecurityFilterChain Lambda DSL Bean Registration        |
|  3. RestClient modern HTTP client integration               |
|  4. META-INF/spring/org.springframework.boot.autoconfigure. |
|     AutoConfiguration.imports registration                  |
+-------------------------------------------------------------+
                       |
                       v
[Modern Spring Boot 3.x / 4.x Application]
  - Java 21 / 25 LTS Baseline (Virtual Threads enabled)
  - Jakarta EE 10 / 11
  - GraalVM Native Image & AOT Engine
  - RFC 9457 ProblemDetail
```

---

## 4. Syntax & Categorized Migration Matrix

### 4.1 Namespace Replacement Table
| Legacy Namespace (Boot 2.x) | Modern Namespace (Boot 3.x / 4.x) |
|---|---|
| `javax.persistence.*` | `jakarta.persistence.*` |
| `javax.servlet.*` | `jakarta.servlet.*` |
| `javax.validation.*` | `jakarta.validation.*` |
| `javax.annotation.PostConstruct` | `jakarta.annotation.PostConstruct` |
| `javax.transaction.Transactional` | `jakarta.transaction.Transactional` |

### 4.2 Configuration Property Replacements
| Deprecated Property (Boot 2.x) | Replacement Property (Boot 3.x / 4.x) |
|---|---|
| `spring.redis.*` | `spring.data.redis.*` |
| `management.metrics.export.prometheus.enabled` | `management.prometheus.metrics.export.enabled` |
| `server.max-http-header-size` | `server.max-http-request-header-size` |
| `spring.main.lazy-initialization` | `spring.main.lazy-initialization` (Unchanged, but AOT warns) |
| `spring.factories` (Auto-config key) | `META-INF/spring/org.springframework.boot.autoconfigure.AutoConfiguration.imports` |

### 4.3 Testing Replacement Table
| Legacy Annotation (Boot 2.x / early 3.x) | Modern Replacement (Boot 3.4+) |
|---|---|
| `@MockBean` | `@MockitoBean` |
| `@SpyBean` | `@MockitoSpyBean` |
| `@DynamicPropertySource` (for containers) | `@ServiceConnection` (Boot 3.1+) |

---

## 5. 5 Practical Production Examples

### Example 1: Automated OpenRewrite Migration in Maven `pom.xml`
```xml
<!-- Run via CLI: ./mvnw rewrite:run -->
<plugin>
    <groupId>org.openrewrite.maven</groupId>
    <artifactId>rewrite-maven-plugin</artifactId>
    <version>5.42.0</version>
    <configuration>
        <activeRecipes>
            <recipe>org.openrewrite.java.spring.boot3.UpgradeSpringBoot_3_4</recipe>
        </activeRecipes>
    </configuration>
    <dependencies>
        <dependency>
            <groupId>org.openrewrite.recipe</groupId>
            <artifactId>rewrite-spring</artifactId>
            <version>5.21.0</version>
        </dependency>
    </dependencies>
</plugin>
```

### Example 2: Migrating Security Config from Adapter to Lambda DSL
```java
// BEFORE (Boot 2.x - DEPRECATED / REMOVED)
// public class OldSecurityConfig extends WebSecurityConfigurerAdapter {
//     @Override
//     protected void configure(HttpSecurity http) throws Exception {
//         http.csrf().disable().authorizeRequests().antMatchers("/public/**").permitAll().anyRequest().authenticated();
//     }
// }

// AFTER (Boot 3.x / 4.x)
@Configuration
@EnableWebSecurity
public class ModernSecurityConfig {

    @Bean
    public SecurityFilterChain securityFilterChain(HttpSecurity http) throws Exception {
        return http
            .csrf(AbstractHttpConfigurer::disable)
            .authorizeHttpRequests(auth -> auth
                .requestMatchers("/public/**").permitAll()
                .anyRequest().authenticated()
            )
            .build();
    }
}
```

### Example 3: Migrating RestTemplate to Modern RestClient
```java
// BEFORE (RestTemplate)
// RestTemplate restTemplate = new RestTemplate();
// UserDto user = restTemplate.getForObject("https://api.example.com/users/{id}", UserDto.class, userId);

// AFTER (RestClient - Spring 6.1+ / Boot 3.2+)
@Service
public class UserApiClient {

    private final RestClient restClient;

    public UserApiClient(RestClient.Builder builder) {
        this.restClient = builder.baseUrl("https://api.example.com").build();
    }

    public UserDto getUser(UUID userId) {
        return restClient.get()
                .uri("/users/{id}", userId)
                .accept(MediaType.APPLICATION_JSON)
                .retrieve()
                .body(UserDto.class);
    }
}
```

### Example 4: Auto-Configuration Registration Modernization
Instead of defining `org.springframework.boot.autoconfigure.EnableAutoConfiguration` inside `META-INF/spring.factories`:

Create file: `META-INF/spring/org.springframework.boot.autoconfigure.AutoConfiguration.imports`
```text
com.example.autoconfig.MyCustomAutoConfiguration
com.example.autoconfig.AnotherAutoConfiguration
```

### Example 5: Trailing Slash Match Configuration (if legacy behavior is strictly required)
```java
@Configuration
public class WebMvcSlashConfig implements WebMvcConfigurer {

    @Override
    public void configurePathMatch(PathMatchConfigurer configurer) {
        // Explicitly enable redirect or route mapping if legacy trailing slash is needed
    }
}
```

---

## 6. Best Practices
1. **Upgrade Incrementally**: Never jump from 2.4 directly to 3.4. Upgrade to latest 2.7.x first, resolve deprecation warnings, then jump to 3.x.
2. **Use OpenRewrite**: Execute automated recipes (`UpgradeSpringBoot_3_4`) to convert hundreds of files and imports in seconds.
3. **Audit Third-Party Starters**: Ensure all community starters (Swagger/OpenAPI, AWS SDK, Redis) have releases built for Jakarta EE 10 before upgrading.
4. **Switch from `@MockBean` to `@MockitoBean`**: Cleanly transition mock declarations to the Spring 6.2 standard.
5. **Enable Virtual Threads**: Once on Boot 3.2+ and Java 21, test `spring.threads.virtual.enabled: true` for free concurrency gains.

---

## 7. Common Mistakes to Avoid

| Mistake | Consequence | Remedy |
|---|---|---|
| Forgetting `javax` to `jakarta` migration in XML/JPA files | ClassNotFoundException / entities ignored | Run OpenRewrite or global regex replacement |
| Jumping directly from Boot 2.0-2.6 to 3.x | Build explodes with hundreds of breaking changes | Migrate to latest Boot 2.7 first, clean warnings, then jump to 3.x |
| Assuming `/users` matches `/users/` | HTTP 404 Not Found on mobile/web clients | Explicitly map endpoints or use redirect filter |
| Retaining `spring.factories` for auto-configuration | Custom starters fail to load silently | Move to `AutoConfiguration.imports` file |
| Relying on Hibernate 5 custom user types | Compilation failure in Hibernate 6.x | Refactor to standard `AttributeConverter` or Hibernate 6 types |
| Keeping legacy `WebSecurityConfigurerAdapter` | Compilation error; class was completely removed | Refactor to `@Bean SecurityFilterChain` Lambda DSL |

---

## 8. Performance & Security Considerations
- **Baseline Performance**: Java 21 generational ZGC and virtual threads reduce memory footprints and eliminate GC pauses.
- **Ahead-of-Time Native Readiness**: Spring Boot 3+ applications compiled with GraalVM Native Image start in < 50ms and consume ~60MB RAM.
- **CVE Elimination**: Upgrading to Spring Boot 3.4+ permanently resolves legacy security vulnerabilities (Spring4Shell, etc.).

---

## 9. Comparison Table: Spring Boot 2.x vs 3.x vs 4.x

| Feature | Spring Boot 2.x | Spring Boot 3.x | Spring Boot 4.x |
|---|---|---|---|
| **Java Baseline** | Java 8 / 11 | Java 17 / 21 LTS | Java 21 / 25 LTS |
| **EE Specification** | Java EE 8 (`javax.*`) | Jakarta EE 10 (`jakarta.*`) | Jakarta EE 11 |
| **Security Syntax** | Configurer Adapter | Lambda DSL FilterChain | Modern Lambda DSL + Passkey |
| **HTTP Client** | `RestTemplate` / WebClient | `RestClient` / Http Interfaces | Native HTTP 3 / QUIC |
| **Native Compilation** | Experimental (Spring Native) | Native AOT Built-in | Optimized Native AOT |

---

## 10. Technical Interview Questions & Answers

<details>
<summary>1. Why did Spring Boot 3.x move from `javax.*` to `jakarta.*`?</summary>
Oracle transferred Java EE specifications to the Eclipse Foundation. Due to trademark restrictions on the "Java" brand, the namespace had to be renamed from `javax` to `jakarta` starting with Jakarta EE 9.
</details>

<details>
<summary>2. How did URL trailing slash matching change in Spring MVC 6?</summary>
In Spring 5 / Boot 2, a mapping for `/users` automatically matched `/users/`. In Spring 6 / Boot 3, trailing slash matching is disabled by default to prevent ambiguous routing, security bypasses, and proxy inconsistencies.
</details>

<details>
<summary>3. What replaced `META-INF/spring.factories` for registering auto-configurations in Spring Boot 3?</summary>
The new file format located at `META-INF/spring/org.springframework.boot.autoconfigure.AutoConfiguration.imports`, containing one fully qualified class name per line.
</details>

<details>
<summary>4. What is the recommended strategy to upgrade an enterprise monolith from Spring Boot 2.x to 3.x?</summary>
1. Upgrade to the latest 2.7.x release.
2. Upgrade JDK to 17 or 21 in Boot 2.7.
3. Fix all compiler deprecation warnings.
4. Execute OpenRewrite automated recipes.
5. Upgrade third-party dependencies to Jakarta-compatible versions.
6. Verify via full test suite with Testcontainers.
</details>

<details>
<summary>5. What advantages does `RestClient` provide over `RestTemplate` in Spring Boot 3.2+?</summary>
`RestClient` provides a fluent, functional API similar to `WebClient` while operating synchronously on standard blocking/virtual threads without requiring Project Reactor dependencies.
</details>

<details>
<summary>6. How does Spring Boot 3 support GraalVM Native Image?</summary>
Spring 6 introduced the AOT (Ahead-of-Time) engine, which evaluates `@Configuration` classes, proxies, and reflection requirements at build time, generating deterministic Java code and native image hints.
</details>

<details>
<summary>7. What happened to `@MockBean` in Spring Boot 3.4?</summary>
It was deprecated in favor of `@MockitoBean` and `@MockitoSpyBean`, which are now built directly into Spring Framework 6.2 testing core.
</details>

<details>
<summary>8. How do you automate large-scale Spring Boot migrations?</summary>
Using OpenRewrite Maven or Gradle plugins (`org.openrewrite.recipe:rewrite-spring`), which parse the abstract syntax tree (AST) and mechanically rewrite imports, properties, and configuration methods.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary>Exercise 1: Run an OpenRewrite dry run</summary>
Configure `rewrite-maven-plugin` with `UpgradeSpringBoot_3_4` and execute `./mvnw rewrite:dryRun` to inspect diffs.
</details>

<details>
<summary>Exercise 2: Migrate an old WebSecurityConfigurerAdapter</summary>
Take a legacy Spring Security 5 class and rewrite it to a modern `SecurityFilterChain` bean.
</details>

<details>
<summary>Exercise 3: Refactor RestTemplate to RestClient</summary>
Refactor a third-party API integration service from `RestTemplate` to `RestClient`.
</details>

<details>
<summary>Exercise 4: Migrate custom auto-configuration to .imports</summary>
Move an existing auto-configuration from `spring.factories` to `AutoConfiguration.imports`.
</details>

<details>
<summary>Exercise 5: Replace @MockBean with @MockitoBean</summary>
Update test classes from `@MockBean` to `@MockitoBean` and verify tests pass cleanly under Spring Boot 3.4+.
</details>

<details>
<summary>Exercise 6: Test trailing slash matching behavior</summary>
Write a test verifying that calling `/api/v1/orders/` returns HTTP 404 when mapped to `/api/v1/orders`.
</details>

---

## 12. Summary & Cheat Sheet

```
+---------------------------------------------------------------------------------+
|                   SPRING BOOT UPGRADE CHEAT SHEET                               |
+---------------------------------------------------------------------------------+
| BASELINE       | Boot 3: Java 17+, Boot 3.4/4: Java 21+ LTS                     |
| NAMESPACE      | javax.* -> jakarta.* (persistence, servlet, validation)        |
| SECURITY       | WebSecurityConfigurerAdapter -> @Bean SecurityFilterChain      |
| HTTP CLIENT    | RestTemplate -> RestClient.builder().build()                   |
| MOCKS (3.4+)   | @MockBean -> @MockitoBean                                      |
| CONTAINERS     | @DynamicPropertySource -> @ServiceConnection                   |
| AUTOCONFIG     | spring.factories -> AutoConfiguration.imports                  |
| AUTOMATION     | ./mvnw rewrite:run (OpenRewrite UpgradeSpringBoot_3_4)         |
+---------------------------------------------------------------------------------+
```

---

## 13. Further Reading & References
- [Spring Boot 3.0 Migration Guide](https://github.com/spring-projects/spring-boot/wiki/Spring-Boot-3.0-Migration-Guide)
- [Spring Boot 3.4 Release Notes](https://github.com/spring-projects/spring-boot/wiki/Spring-Boot-3.4-Release-Notes)
- [OpenRewrite Spring Boot 3 Migration Recipes](https://docs.openrewrite.org/recipes/java/spring/boot3)
- [Jakarta EE Official Specification Migration](https://jakarta.ee/)
