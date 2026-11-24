# Java Spring Boot Production Curriculum & Reference Track

Welcome to the definitive, enterprise-grade **Spring Boot Learning Track**. Designed from the ground up to mirror the high standards of modern Java 21 / 25 LTS engineering, this repository provides deep architectural explanations, internal mechanics, production failure modes, performance profiling, and complete end-to-end reference systems.

---

## Curriculum Architecture & Scope

This track covers the entire Spring Boot 3.4.x / 4.x ecosystem:
- **Core Framework & IoC**: Bean lifecycles, ApplicationContext refresh phases, AOP proxies (JDK vs CGLIB), SpEL, Jakarta validation.
- **Boot Fundamentals**: Auto-configuration condition evaluation, `@ConfigurationProperties`, native structured logging, externalized config, Actuator.
- **Web & Real-Time APIs**: Spring MVC, DispatcherServlet lifecycle, RESTful design, content negotiation, ProblemDetail, Server-Sent Events, WebSockets.
- **Modern HTTP Clients**: `RestClient`, Declarative HTTP Interfaces (`@HttpExchange`), `WebClient`, resilient timeouts, WireMock testing.
- **Reactive & Concurrency**: Project Reactor (Mono/Flux), WebFlux, R2DBC, Virtual Threads (Project Loom) benchmarking, BlockHound.
- **Data Access & Persistence**: Spring Data JPA, Hibernate 6.6+, `JdbcClient`, keyset pagination, N+1 query elimination, transactional propagation, Flyway.
- **Security & Authorization**: Spring Security 6/7 Lambda DSL, OAuth2 Resource Server & Authorization Server, JWT, Keycloak, RBAC/ABAC, OWASP defenses.
- **Testing Mastery**: Test slices (`@WebMvcTest`, `@DataJpaTest`), Testcontainers with `@ServiceConnection`, MockMvc, WireMock, ArchUnit.
- **Messaging & Event-Driven**: Spring Kafka, RabbitMQ, Spring Cloud Stream, Outbox Pattern, Spring Modulith, Spring Batch.
- **Microservices & Cloud**: Spring Cloud Gateway, Resilience4j, Distributed Tracing (Micrometer / OpenTelemetry), Config Server.
- **Observability & Operations**: Actuator internals, Prometheus metrics, Grafana dashboards, JVM thread/heap dumps, Kubernetes probes.
- **Performance & Production**: GraalVM Native Image, AOT compilation, HikariCP tuning, GC optimization, container resource sizing.
- **Enterprise Patterns & AI**: Hexagonal/Clean Architecture, DDD, CQRS, Modular Monoliths, Spring AI (RAG, Chat, Embeddings).
- **Reference Projects**: 16 complete, production-grade applications with full source code, tests, Dockerfiles, and compose configurations.
- **System Design Blueprints**: 15 scalable distributed architectures (URL shortener, flash sale, video streaming, payments, chat).
- **Interview Preparation & Cheatsheets**: 13 technical Q&A sets with live coding challenges and 13 quick-reference cheatsheets.

---

## Master Module Directory

| # | Module | Scope & Highlights |
|:---|:---|:---|
| **00** | [00-setup/](file:///03-backend/spring-boot/00-setup/) | Ecosystem overview, Java 21/25 prerequisites, Initializr, IDE, DevTools, Docker Compose |
| **01** | [01-core-spring/](file:///03-backend/spring-boot/01-core-spring/) | IoC container, Bean lifecycles, BPP/BFPP, CGLIB/JDK proxies, SpEL, ApplicationContext internals |
| **02** | [02-boot-fundamentals/](file:///03-backend/spring-boot/02-boot-fundamentals/) | Auto-configuration conditions, `@ConfigurationProperties`, Structured logging, Actuator, Runners |
| **03** | [03-web-mvc/](file:///03-backend/spring-boot/03-web-mvc/) | DispatcherServlet, REST controllers, ProblemDetail, CORS, Interceptors, Embedded server tuning |
| **04** | [04-http-clients/](file:///03-backend/spring-boot/04-http-clients/) | Modern `RestClient`, Declarative HTTP Interfaces, `WebClient`, WireMock testing, Feign |
| **05** | [05-reactive-webflux/](file:///03-backend/spring-boot/05-reactive-webflux/) | Reactor, WebFlux, Backpressure, R2DBC, Virtual Threads comparison, BlockHound |
| **06** | [06-data-access/](file:///03-backend/spring-boot/06-data-access/) | JPA, Hibernate 6.6+, `JdbcClient`, HikariCP, Transactions, Flyway, Testcontainers, Keyset pagination |
| **07** | [07-security/](file:///03-backend/spring-boot/07-security/) | SecurityFilterChain Lambda DSL, JWT Resource Server, OAuth2, Passkeys, RBAC, Rate limiting |
| **08** | [08-testing/](file:///03-backend/spring-boot/08-testing/) | Slices (`@WebMvcTest`, `@DataJpaTest`), Testcontainers `@ServiceConnection`, MockMvc, ArchUnit |
| **09** | [09-api-and-integration/](file:///03-backend/spring-boot/09-api-and-integration/) | OpenAPI/SpringDoc, WebSockets, STOMP, SSE, S3 storage, Redis caching, MapStruct |
| **10** | [10-messaging-and-events/](file:///03-backend/spring-boot/10-messaging-and-events/) | Spring Kafka, RabbitMQ, Spring Modulith, Outbox pattern, Idempotency, Spring Batch |
| **11** | [11-cloud-and-microservices/](file:///03-backend/spring-boot/11-cloud-and-microservices/) | Spring Cloud Gateway, Resilience4j, Distributed Tracing, Config Server, Kubernetes config |
| **12** | [12-observability-and-operations/](file:///03-backend/spring-boot/12-observability-and-operations/) | Micrometer metrics, OpenTelemetry, Structured JSON logging, JVM diagnostics, Health probes |
| **13** | [13-performance/](file:///03-backend/spring-boot/13-performance/) | GraalVM AOT Native Image, Virtual Threads throughput, HikariCP, Profiling (JFR/async-profiler) |
| **14** | [14-production-and-deployment/](file:///03-backend/spring-boot/14-production-and-deployment/) | Multi-stage Docker, Jib, Kubernetes manifests, Helm, Graceful shutdown, 12-Factor |
| **15** | [15-architecture-and-patterns/](file:///03-backend/spring-boot/15-architecture-and-patterns/) | Hexagonal / Clean Architecture, DDD aggregates, Modular Monoliths, CQRS, Real-world patterns |
| **16** | [16-ai/](file:///03-backend/spring-boot/16-ai/) | Spring AI fundamentals, ChatModel, VectorStore, RAG pipelines, Tool-calling, Model Context Protocol |
| **17** | [17-projects/](file:///03-backend/spring-boot/17-projects/) | 16 Production reference systems (E-Commerce, Auth, URL Shortener, Chat, SaaS, Batch, RAG) |
| **18** | [18-system-design/](file:///03-backend/spring-boot/18-system-design/) | 15 Distributed system design blueprints with calculations and failure modes |
| **19** | [19-interview/](file:///03-backend/spring-boot/19-interview/) | 13 Senior/Staff technical interview question sets with code challenges |
| **20** | [20-cheatsheets/](file:///03-backend/spring-boot/20-cheatsheets/) | 13 High-density reference guides for annotations, properties, JPA, security, and CLI |

---

## Cross-Repository Navigation

- **Java Language Fundamentals**: [`01-languages/java/`](file:///01-languages/java/)
- **SQL & Relational Theory**: [`09-sql/`](file:///09-sql/)
- **DevOps, Docker & AWS Infrastructure**: [`04-devops/`](file:///04-devops/)
- **Distributed System Design Theory**: [`07-system-design/`](file:///07-system-design/)
- **SOLID & Design Patterns Theory**: [`08-solid-design-patterns/`](file:///08-solid-design-patterns/)
- **Node.js Backend Analogue**: [`03-backend/nodejs/`](file:///03-backend/nodejs/)
- **NestJS Backend Analogue**: [`03-backend/nestjs/`](file:///03-backend/nestjs/)
- **FastAPI Backend Analogue**: [`03-backend/fastapi/`](file:///03-backend/fastapi/)
