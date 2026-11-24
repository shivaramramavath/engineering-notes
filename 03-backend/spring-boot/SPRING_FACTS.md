# Spring Boot Technical Fact Sheet & Baseline Truth

## 1. Version Baselines & Supported Release Lines

| Component | Baseline Version | Minimum Java | Recommended / LTS Java | Jakarta EE Spec |
|:---|:---|:---:|:---:|:---:|
| **Spring Boot 3.4.x / 4.x** | `3.4.0+` / `4.x` | Java 17 | Java 21 / 25 LTS | Jakarta EE 10 / 11 |
| **Spring Framework** | `6.2.x` / `7.0.x` | Java 17 | Java 21 / 25 LTS | Servlet 6.0 / 6.1 |
| **Spring Security** | `6.4.x` / `7.0.x` | Java 17 | Java 21 / 25 LTS | Jakarta Security 3.0 |
| **Spring Data** | `2024.1.x` (Release Train) | Java 17 | Java 21 / 25 LTS | JPA 3.1 / 3.2 |
| **Hibernate ORM** | `6.6.x` | Java 17 | Java 21 / 25 LTS | Jakarta Persistence 3.1 |
| **Tomcat (Embedded)** | `10.1.x` / `11.0.x` | Java 17 | Java 21 / 25 LTS | Servlet 6.0 |
| **Spring Modulith** | `1.3.x` / `2.2.x` | Java 17 | Java 21 / 25 LTS | - |
| **Spring AI** | `1.0.0+` / `2.1.0` | Java 17 | Java 21 / 25 LTS | - |
| **Testcontainers** | `1.20.x+` | Java 17 | Java 21 / 25 LTS | - |

---

## 2. Key Paradigm Shifts: Boot 2.x to 3.x to Latest

1. **Namespace Migration (`javax.*` $\to$ `jakarta.*`)**:
   - `javax.persistence.*` $\to$ `jakarta.persistence.*`
   - `javax.servlet.*` $\to$ `jakarta.servlet.*`
   - `javax.validation.*` $\to$ `jakarta.validation.*`
   - `javax.annotation.*` $\to$ `jakarta.annotation.*`
2. **Ahead-of-Time (AOT) & GraalVM Native Image**:
   - First-class GraalVM Native compilation support without third-party plugins.
   - Built-in runtime hints: `@RegisterReflectionForBinding`, `RuntimeHintsRegistrar`.
3. **Java Virtual Threads (Project Loom)**:
   - One-line activation: `spring.threads.virtual.enabled=true`.
   - Replaces traditional platform worker thread pool (Tomcat 200 threads) with lightweight virtual threads per request.
4. **Modern HTTP Client Ecosystem**:
   - **`RestClient`**: Synchronous fluid HTTP client (introduced in 6.1, expanded in 6.2 / Boot 3.4 with pluggable client request factories: Apache HttpClient 5, Jetty, Reactor Netty, JDK HttpClient).
   - **Declarative HTTP Interfaces**: `@HttpExchange`, `@GetExchange`, `@PostExchange` generating dynamic client proxies.
   - Legacy `RestTemplate` placed in maintenance mode.
5. **Observability & Micrometer 1.14+**:
   - Unified Micrometer Tracing API replacing legacy Spring Cloud Sleuth.
   - First-class OpenTelemetry and Brave exporters.
   - Automated span and metric emission for HTTP clients, JDBC queries, and Kafka listeners.
6. **Built-in Structured Logging (Boot 3.4+)**:
   - Native property-driven JSON structured logging for Console and File.
   - Schemas supported out of the box: `ecs` (Elastic Common Schema), `logstash`, `gelf`.
   - Controlled via `logging.structured.format.console=ecs`.
7. **Testcontainers Service Connections**:
   - `@ServiceConnection` automatically injects dynamic JDBC URLs, ports, credentials from container into Spring Boot properties without manual `@DynamicPropertySource`.
8. **Spring Boot Docker Compose (`spring-boot-docker-compose`)**:
   - Discovers `compose.yaml` on startup, manages container lifecycle in local development, and wires connection parameters automatically.
9. **Spring Security 6/7 Modern Standards**:
   - Mandatory Lambda DSL (`http.authorizeHttpRequests(auth -> auth.anyRequest().authenticated())`).
   - Deprecation/removal of `WebSecurityConfigurerAdapter`.
   - Passkey / WebAuthn native integration.
10. **Standardized Error Responses (`ProblemDetail`)**:
    - Full RFC 7807 compliance via `ProblemDetail` and `ErrorResponseException`.

---

## 3. Official CVE & Security Baselines

- **Spring4Shell (CVE-2022-22965)**: Addressed in Spring Framework 5.3.18 / 5.2.20 and Boot 2.6.6+. Requires JDK 9+, WAR packaging on standalone Tomcat. Completely neutralized in Boot 3.x+.
- **Spring Expression Language (SpEL) Injection (CVE-2022-22980 / CVE-2023-34050)**: Parser context sanitization and validation of untrusted user input before evaluation.
- **Path Traversal / Resource Handling (CVE-2024-38816 / CVE-2024-38819)**: Spring Framework static resource resolution hardened against URL encoded path traversals.
- **CSRF Token Handling**: Modern Single-Page Application (SPA) CSRF protection using `CookieCsrfTokenRepository` with `CsrfTokenRequestAttributeHandler` to resolve deferred token evaluation.
