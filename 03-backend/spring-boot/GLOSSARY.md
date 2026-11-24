# Spring Boot Technical Glossary

| Term | Technical Definition |
|:---|:---|
| **AOT (Ahead-of-Time)** | Compilation step that pre-computes bean registrations, reflection hints, and proxy generation at build-time for GraalVM native image generation. |
| **ApplicationContext** | The central IoC container interface in Spring providing bean factory features, event publication, and resource loading. |
| **BeanPostProcessor (BPP)** | Container extension point allowing custom modification of bean instances (e.g., wrapping in proxies) before/after initialization. |
| **BeanFactoryPostProcessor (BFPP)**| Extension point allowing modification of bean definitions (metadata) before any bean instances are created (e.g., property placeholder resolution). |
| **CGLIB Proxy** | Subclass-based dynamic proxy generated via bytecode generation when target class does not implement an interface. |
| **Declarative HTTP Interfaces** | HTTP client proxies generated automatically by Spring from `@HttpExchange` annotated Java interfaces. |
| **DispatcherServlet** | Front controller servlet in Spring MVC dispatching HTTP requests to registered handlers, interceptors, and view resolvers. |
| **Hibernate 6.x** | The default JPA provider in Spring Boot 3+, featuring SQM (Semantic Query Model) and native JSON/vector types. |
| **JDK Dynamic Proxy** | Interface-based proxy mechanism built into Java reflection (`java.lang.reflect.Proxy`). |
| **JdbcClient** | Fluent, synchronous JDBC client introduced in Spring Framework 6.1 simplifying SQL mapping over `JdbcTemplate`. |
| **Micrometer** | Application metrics and distributed tracing instrumentation facade adopted across Spring Boot Actuator. |
| **Open-Session-In-View (OSIV)**| Web filter keeping Hibernate Session open during view rendering; severe performance anti-pattern. |
| **ProblemDetail (RFC 7807)** | Standardized JSON representation for HTTP API error responses natively supported in Spring Boot 3+. |
| **Project Loom / Virtual Threads** | Lightweight user-mode threads managed by the JVM enabling high-throughput blocking I/O without reactive complexity. |
| **RestClient** | Synchronous, fluid HTTP client introduced in Spring Framework 6.1 succeeding `RestTemplate`. |
| **ServiceConnection** | Testcontainers Spring Boot annotation automatically discovering and injecting container connection properties. |
| **SpEL** | Spring Expression Language, an expression evaluation engine querying and manipulating an object graph at runtime. |
| **Spring Modulith** | Architecture library verifying module boundaries, domain events, and asynchronous outbox publication. |
| **Structured Logging** | Production logging standard rendering log events directly as JSON (ECS, Logstash, GELF) without string parsing. |
| **WebFlux** | Non-blocking reactive web framework built on Project Reactor and Netty. |
