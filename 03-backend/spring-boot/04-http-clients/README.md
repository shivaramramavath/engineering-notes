# Module 04: HTTP Clients

Welcome to **Module 04: HTTP Clients**. This module covers modern outbound HTTP communication in Spring Boot 3.4+: the modern fluid `RestClient`, Declarative HTTP Interfaces (`@HttpExchange`), `WebClient`, client resilience and timeout patterns, WireMock testing, and legacy `RestTemplate` migration.

---

## Module Contents

| # | Note | Description | Level | Status |
|---|---|---|---|---|
| 01 | [01-RestClient.md](file:///03-backend/spring-boot/04-http-clients/01-RestClient.md) | The modern fluid synchronous client introduced in 6.1, request factories, error handlers | Intermediate | Complete |
| 02 | [02-WebClient.md](file:///03-backend/spring-boot/04-http-clients/02-WebClient.md) | Asynchronous reactive HTTP client, Netty buffer pools, backpressure, exchange filters | Advanced | Complete |
| 03 | [03-declarative-http-interfaces.md](file:///03-backend/spring-boot/04-http-clients/03-declarative-http-interfaces.md) | Declarative HTTP interfaces (`@HttpExchange`, `@GetExchange`, `@PostExchange`) | Intermediate | Complete |
| 04 | [04-resilience-and-timeouts-for-clients.md](file:///03-backend/spring-boot/04-http-clients/04-resilience-and-timeouts-for-clients.md) | Connection timeouts, read timeouts, circuit breakers with Resilience4j | Advanced | Complete |
| 05 | [05-client-testing-with-wiremock.md](file:///03-backend/spring-boot/04-http-clients/05-client-testing-with-wiremock.md) | Mocking external HTTP endpoints with WireMock, verifying request payloads | Intermediate | Complete |
| 06 | [06-RestTemplate-legacy-and-migration.md](file:///03-backend/spring-boot/04-http-clients/06-RestTemplate-legacy-and-migration.md) | Legacy `RestTemplate` maintenance, pitfalls, and step-by-step migration to `RestClient` | Intermediate | Complete |
| 07 | [07-OpenFeign-overview.md](file:///03-backend/spring-boot/04-http-clients/07-OpenFeign-overview.md) | Spring Cloud OpenFeign overview, comparison with Declarative HTTP Interfaces | Intermediate | Complete |

---

## Next Steps
Proceed to **[Module 05: Reactive WebFlux](file:///03-backend/spring-boot/05-reactive-webflux/)** to explore non-blocking reactive pipelines and Virtual Threads.
