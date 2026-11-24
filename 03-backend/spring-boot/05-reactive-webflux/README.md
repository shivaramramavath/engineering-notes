# Module 05: Reactive WebFlux & Concurrency Models

Welcome to **Module 05: Reactive WebFlux & Concurrency Models**. This module navigates the reactive paradigm in Spring: Project Reactor (`Mono` and `Flux`), non-blocking WebFlux controllers, functional endpoints, backpressure, R2DBC, reactive security, architectural decisions comparing MVC vs. WebFlux vs. Java Virtual Threads, and blocking detection with BlockHound.

---

## Module Contents

| # | Note | Description | Level | Status |
|---|---|---|---|---|
| 01 | [01-reactive-fundamentals-and-reactor.md](file:///03-backend/spring-boot/05-reactive-webflux/01-reactive-fundamentals-and-reactor.md) | Reactive Streams specification, Project Reactor (`Mono`, `Flux`), cold vs hot publishers | Intermediate | Complete |
| 02 | [02-webflux-controllers-and-functional-endpoints.md](file:///03-backend/spring-boot/05-reactive-webflux/02-webflux-controllers-and-functional-endpoints.md) | Annotated `@RestController` vs `RouterFunction` and `ServerResponse` in WebFlux | Advanced | Complete |
| 03 | [03-backpressure.md](file:///03-backend/spring-boot/05-reactive-webflux/03-backpressure.md) | Downstream demand signaling, buffer, drop, and latest backpressure strategies | Advanced | Complete |
| 04 | [04-r2dbc.md](file:///03-backend/spring-boot/05-reactive-webflux/04-r2dbc.md) | Reactive Relational Database Connectivity (R2DBC) with PostgreSQL, non-blocking drivers | Advanced | Complete |
| 05 | [05-reactive-security.md](file:///03-backend/spring-boot/05-reactive-webflux/05-reactive-security.md) | `ServerHttpSecurity`, reactive security filter chain, Reactor Context propagation | Advanced | Complete |
| 06 | [06-testing-reactive-code.md](file:///03-backend/spring-boot/05-reactive-webflux/06-testing-reactive-code.md) | Testing reactive streams with `StepVerifier` and `WebTestClient` | Intermediate | Complete |
| 07 | [07-mvc-vs-webflux-vs-virtual-threads-decision.md](file:///03-backend/spring-boot/05-reactive-webflux/07-mvc-vs-webflux-vs-virtual-threads-decision.md) | Architectural decision matrix: Spring MVC vs WebFlux vs Java Virtual Threads (Loom) | Advanced | Complete |
| 08 | [08-virtual-threads-in-spring-boot.md](file:///03-backend/spring-boot/05-reactive-webflux/08-virtual-threads-in-spring-boot.md) | `spring.threads.virtual.enabled=true`, carrier threads, synchronization pinning | Advanced | Complete |
| 09 | [09-blocking-calls-pitfalls.md](file:///03-backend/spring-boot/05-reactive-webflux/09-blocking-calls-pitfalls.md) | Detecting and preventing thread-starvation bugs caused by blocking calls in Netty | Advanced | Complete |
| 10 | [10-blockhound.md](file:///03-backend/spring-boot/05-reactive-webflux/10-blockhound.md) | Runtime blocking call detection using BlockHound byte-buddy instrumentation | Advanced | Complete |
| 11 | [11-reactive-operators-deep-dive.md](file:///03-backend/spring-boot/05-reactive-webflux/11-reactive-operators-deep-dive.md) | Exhaustive operator guide: `flatMap`, `concatMap`, `switchMap`, `zip`, `retryWhen` | Advanced | Complete |

---

## Next Steps
Proceed to **[Module 06: Data Access & Persistence](file:///03-backend/spring-boot/06-data-access/)** to master relational databases, Spring Data JPA, Hibernate, and transaction boundaries.
