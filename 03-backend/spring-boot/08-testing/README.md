# Module 08: Testing

Welcome to **Module 08: Testing**. This module establishes the testing pyramid for Spring Boot: pure unit tests without Spring, test slices (`@WebMvcTest`, `@DataJpaTest`), Testcontainers with `@ServiceConnection`, MockMvc, WireMock, contract testing, and architectural verification with ArchUnit.

---

## Module Contents

| # | Note | Description | Level | Status |
|---|---|---|---|---|
| 01 | [01-testing-fundamentals-in-spring.md](file:///03-backend/spring-boot/08-testing/01-testing-fundamentals-in-spring.md) | Testing pyramid in Spring Boot, `@SpringBootTest`, test context caching rules | Beginner | Complete |
| 02 | [02-unit-testing-without-spring.md](file:///03-backend/spring-boot/08-testing/02-unit-testing-without-spring.md) | Pure JUnit 5 and Mockito unit tests without loading Spring ApplicationContext | Beginner | Complete |
| 03 | [03-spring-boot-test-and-slices.md](file:///03-backend/spring-boot/08-testing/03-spring-boot-test-and-slices.md) | Test slices: `@WebMvcTest`, `@DataJpaTest`, `@JsonTest`, `@RestClientTest` | Intermediate | Complete |
| 04 | [04-mockito-and-mockbean-replacements.md](file:///03-backend/spring-boot/08-testing/04-mockito-and-mockbean-replacements.md) | `@MockBean` vs `@SpyBean`, modern `@MockitoBean` in Spring Boot 3.4+ | Intermediate | Complete |
| 05 | [05-MockMvc-and-RestTestClient.md](file:///03-backend/spring-boot/08-testing/05-MockMvc-and-RestTestClient.md) | End-to-end web testing using `MockMvc`, `WebTestClient`, and `RestTestClient` | Intermediate | Complete |
| 06 | [06-testcontainers-and-service-connections.md](file:///03-backend/spring-boot/08-testing/06-testcontainers-and-service-connections.md) | Integration testing with real containers using `@ServiceConnection` | Intermediate | Complete |
| 07 | [07-testing-security.md](file:///03-backend/spring-boot/08-testing/07-testing-security.md) | Testing security constraints with `@WithMockUser`, `@WithAnonymousUser`, JWT mocks | Intermediate | Complete |
| 08 | [08-testing-transactions-and-database-state.md](file:///03-backend/spring-boot/08-testing/08-testing-transactions-and-database-state.md) | Transactional tests with automatic rollback, `@Commit`, `@Rollback`, isolation | Intermediate | Complete |
| 09 | [09-contract-testing-with-spring-cloud-contract-or-pact.md](file:///03-backend/spring-boot/08-testing/09-contract-testing-with-spring-cloud-contract-or-pact.md) | Consumer-Driven Contract testing using Spring Cloud Contract and Pact | Advanced | Complete |
| 10 | [10-wiremock.md](file:///03-backend/spring-boot/08-testing/10-wiremock.md) | Stubbing external HTTP downstream APIs, fault injection, latency simulation | Intermediate | Complete |
| 11 | [11-test-configuration-and-context-caching.md](file:///03-backend/spring-boot/08-testing/11-test-configuration-and-context-caching.md) | Understanding Spring TestContext caching to prevent slow test suites | Advanced | Complete |
| 12 | [12-testing-async-and-scheduled-code.md](file:///03-backend/spring-boot/08-testing/12-testing-async-and-scheduled-code.md) | Testing `@Async` methods and `@Scheduled` cron tasks using Awaitility | Intermediate | Complete |
| 13 | [13-archunit.md](file:///03-backend/spring-boot/08-testing/13-archunit.md) | Enforcing architectural rules, package dependencies, and coding conventions | Advanced | Complete |
| 14 | [14-mutation-testing.md](file:///03-backend/spring-boot/08-testing/14-mutation-testing.md) | Evaluating test suite effectiveness using Pitest mutation testing | Advanced | Complete |
| 15 | [15-test-data-builders.md](file:///03-backend/spring-boot/08-testing/15-test-data-builders.md) | Test Data Builder pattern and ObjectMother for maintainable fixtures | Intermediate | Complete |
| 16 | [16-flaky-tests.md](file:///03-backend/spring-boot/08-testing/16-flaky-tests.md) | Root-cause analysis and remediation strategies for non-deterministic tests | Advanced | Complete |

---

## Next Steps
Proceed to **[Module 09: API Design & System Integration](file:///03-backend/spring-boot/09-api-and-integration/)** to explore OpenAPI, WebSockets, gRPC, and caching.
