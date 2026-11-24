# Spring Boot Curriculum Architecture Decisions Record (ADR)

## ADR 001: Java 21 / 25 LTS Baseline with Spring Boot 3.4.x / 4.x

- **Status**: Accepted
- **Context**: Spring Boot 3.x requires Java 17 minimum, with full production support for Java 21 and Java 25 LTS. Spring Boot 3.4 introduces native structured logging and expanded HTTP client support, while 4.x introduces Spring Framework 7 and Jakarta EE 11.
- **Decision**: Adopt Spring Boot 3.4.x as the primary stable enterprise baseline, highlighting forward-compatibility with 4.x and Java 25 LTS features (Records, Pattern Matching, Virtual Threads, Structured Concurrency).

---

## ADR 002: Modern HTTP Client Standard (RestClient over RestTemplate)

- **Status**: Accepted
- **Context**: Spring Framework 6.1 introduced `RestClient`, a synchronous, fluent client designed to succeed `RestTemplate` while sharing infrastructure with `WebClient`.
- **Decision**: Design all modern synchronous REST client notes and examples using `RestClient` and Declarative HTTP Interfaces (`@HttpExchange`). Document `RestTemplate` exclusively under migration and legacy maintenance notes.

---

## ADR 003: Database Persistence & Keyset Pagination

- **Status**: Accepted
- **Context**: Offset-based pagination causes severe performance degradation on large datasets. Hibernate 6.x and Spring Data 2024.x support keyset / window pagination natively.
- **Decision**: Mandate keyset / cursor pagination in all production and reference project notes, deprecating offset-based pagination for high-scale enterprise schemas.

---

## ADR 004: Strict Markdown-Only Repository Policy

- **Status**: Accepted
- **Context**: The repository must contain only documentation notes and root `tree.txt`. Stray build files (`pom.xml`, `.java`) cause toolchain confusion.
- **Decision**: All runnable source code, test suites, Dockerfiles, and compose configurations must be embedded in fenced code blocks under explicit file path headings.

---

## ADR 005: Security Configuration DSL

- **Status**: Accepted
- **Context**: Spring Security 6+ removed `WebSecurityConfigurerAdapter` and made the Lambda DSL mandatory.
- **Decision**: All security configuration notes and examples strictly use `@Bean SecurityFilterChain` with the Lambda DSL (`http -> http.authorizeHttpRequests(...)`). Zero legacy configuration patterns permitted.

---

## ADR 006: In-Process vs. External Messaging Scopes

- **Status**: Accepted
- **Context**: Candidates and developers frequently confuse Spring ApplicationEvents with distributed message queues.
- **Decision**: Differentiate strictly between local in-process events (Spring Events / Spring Modulith) and distributed external brokers (Apache Kafka, RabbitMQ).
