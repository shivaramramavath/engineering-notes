# 09. Consumer-Driven Contract Testing (Spring Cloud Contract & Pact)

| Metadata | Value |
|---|---|
| **Level** | Advanced |
| **Prerequisites** | `01-testing-fundamentals-in-spring.md`, Microservices Architecture, REST API Design |
| **Estimated Reading Time** | 40 minutes |
| **Status** | Complete |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | OpenJDK 21, Spring Boot 3.4.1, Spring Cloud Contract 4.2, Pact JVM 4.6 |

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
In distributed microservice architectures, testing inter-service communication presents a serious engineering dilemma:
- **End-to-End (E2E) Integration Environments**: Deploying 50+ microservices simultaneously to a shared staging cluster for E2E tests is slow, brittle, expensive, and plagued by environment instability. A failure in Service $Z$ blocks the pipeline for Service $A$.
- **Isolated Unit Mocks**: Mocking downstream HTTP endpoints (e.g., with static WireMock stubs) is fast, but mocks quickly drift out of sync with production. If the producer service renames a JSON field from `customer_id` to `customerId`, the consumer's mock tests continue to pass in CI while production breaks immediately upon deployment.

**Consumer-Driven Contract (CDC) Testing** solves this by establishing executable, versioned contracts between API consumers and producers. The producer is tested against the contract to guarantee it satisfies the consumer's expectations, and the consumer tests against verified stubs generated directly from that contract.

### Historical Evolution
- **Pact (2013-Present)**: Originating in the Ruby/JavaScript community, Pact pioneered Consumer-Driven Contracts and the Pact Broker, establishing language-agnostic contract specifications.
- **Spring Cloud Contract (2016-Present)**: Spring's native CDC solution, supporting Groovy and YAML contract DSLs, automated WireMock stub generation, and messaging contracts for Spring Cloud Stream / Kafka.
- **Modern Spring Boot 3.x/4.x**: Full integration with Pact JVM 4.6+, Spring Cloud Contract 4.2+, Jakarta EE 10, and bidirectional contract testing.

### Real-World Relevance
CDC testing enables independent deployment pipelines (Continuous Deployment) without requiring complex, slow end-to-end testing environments.

---

## 2. Core Concepts

### Architectural Contracts
- **Consumer**: The microservice that invokes an API or consumes an event.
- **Producer (Provider)**: The microservice that exposes the API endpoint or publishes the event.
- **Contract**: A formal, machine-readable agreement (YAML, Groovy, or JSON) declaring expected requests (URL, method, headers, body) and guaranteed responses.
- **Spring Cloud Contract Verifier**: Tool used on the producer side to auto-generate JUnit test cases from contracts to ensure the producer satisfies the contract.
- **Stub Runner (`@AutoConfigureStubRunner`)**: Spring Cloud Contract tool used on the consumer side to download verified WireMock stubs and run them locally in memory.
- **Pact Broker**: Centralized repository for publishing, verifying, and checking compatibility (`can-i-deploy`) of Pact contracts across microservice fleets.

### The Mental Model
```
[ 1. Contract Definition ] (YAML or Pact JSON)
          │
          ├──► [ 2. Producer Pipeline ]
          │       ├─ Auto-generates JUnit tests from contract
          │       ├─ Runs tests against real Controllers
          │       └─ Generates & publishes verified WireMock Stubs JAR to Artifactory/Broker
          │
          └──► [ 3. Consumer Pipeline ]
                  ├─ @AutoConfigureStubRunner downloads verified Stubs JAR
                  ├─ Spins up in-memory WireMock matching the contract exactly
                  └─ Consumer HTTP client tests pass with 100% contract confidence!
```

### Invariants
1. A contract test verifies **syntax, schemas, and protocol contracts**, not deep business logic.
2. The producer build must fail if it breaks any contract agreed upon with consumers.
3. Consumers must only test against stubs generated from verified contracts, never manually handwritten mock files.

---

## 3. How It Works Internally

### The Spring Cloud Contract Lifecycle
```
PRODUCER BUILD:
  Contracts (YAML/Groovy) in src/test/resources/contracts/
     │
     ├── 1. Spring Cloud Contract Maven Plugin parses contracts
     ├── 2. Generates ContractVerifierTest.java classes
     ├── 3. Executes tests using MockMvc / RestAssured against Producer BaseClass
     │      └─ If API matches contract: Build PASSES
     └── 4. Packages stubs into artifact: {artifactId}-{version}-stubs.jar

CONSUMER BUILD:
  @AutoConfigureStubRunner(ids = "com.example:producer-service:+:stubs:8080")
     │
     ├── 1. Downloads producer stubs.jar from local Maven cache or remote repo
     ├── 2. Unpacks WireMock JSON mappings
     ├── 3. Starts local WireMock server on dynamic port (or port 8080)
     └── 4. Consumer RestClient executes requests against the verified WireMock server!
```

---

## 4. Syntax & Usage

### 1. Declarative Contract Definition (`order_contract.yml`)
Stored in Producer: `src/test/resources/contracts/orders/shouldReturnOrder.yml`:

```yaml
name: "should return order details by id"
request:
  method: GET
  url: /api/v1/orders/ORD-101
  headers:
    Accept: application/json
response:
  status: 200
  headers:
    Content-Type: application/json
  body:
    orderId: "ORD-101"
    status: "CONFIRMED"
    totalAmount: 149.50
  matchers:
    body:
      - path: $.orderId
        type: by_regex
        value: "ORD-[0-9]+"
      - path: $.totalAmount
        type: by_type
```

---

## 5. Practical Examples

### Example 1: Producer Base Class Setup (`ProducerBase.java`)
Configuring the Spring Cloud Contract test base class on the producer side.

```java
package com.example.producer;

import com.example.web.OrderController;
import com.example.service.OrderService;
import io.restassured.module.mockmvc.RestAssuredMockMvc;
import org.junit.jupiter.api.BeforeEach;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.autoconfigure.web.servlet.WebMvcTest;
import org.springframework.test.context.bean.override.mockito.MockitoBean;

import static org.mockito.BDDMockito.given;

@WebMvcTest(OrderController.class)
public abstract class ProducerBase {

    @Autowired
    private OrderController orderController;

    @MockitoBean
    private OrderService orderService;

    @BeforeEach
    public void setup() {
        RestAssuredMockMvc.standaloneSetup(orderController);

        given(orderService.findOrder("ORD-101"))
            .willReturn(new OrderDto("ORD-101", "CONFIRMED", 149.50));
    }
}
```

### Example 2: Consumer Test with `@AutoConfigureStubRunner`
Consumer test executing against real stubs published by the producer.

```java
package com.example.consumer;

import com.example.client.OrderClient;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.cloud.contract.stubrunner.spring.AutoConfigureStubRunner;
import org.springframework.cloud.contract.stubrunner.spring.StubRunnerProperties;

import static org.assertj.core.api.Assertions.assertThat;

@SpringBootTest(webEnvironment = SpringBootTest.WebEnvironment.NONE)
@AutoConfigureStubRunner(
    ids = "com.example:order-producer-service:+:stubs:8085",
    stubsMode = StubRunnerProperties.StubsMode.LOCAL // Load from local .m2 repository
)
class OrderClientContractTest {

    @Autowired
    private OrderClient orderClient; // Configured with baseUrl = "http://localhost:8085"

    @Test
    void shouldFetchOrderDetailsMatchingContract() {
        var order = orderClient.getOrder("ORD-101");

        assertThat(order).isNotNull();
        assertThat(order.orderId()).isEqualTo("ORD-101");
        assertThat(order.status()).isEqualTo("CONFIRMED");
        assertThat(order.totalAmount()).isEqualTo(149.50);
    }
}
```

### Example 3: Edge Case – Asynchronous Messaging Contract (Kafka)
Defining contracts for event-driven messages published to Kafka or RabbitMQ.

```yaml
# src/test/resources/contracts/events/orderCreatedEvent.yml
description: "Publishes OrderCreatedEvent upon successful checkout"
label: "order_created_trigger"
input:
  triggeredBy: "publishOrderCreatedEvent()"
outputMessage:
  sentTo: "orders.events"
  headers:
    contentType: "application/json"
    eventType: "OrderCreated"
  body:
    orderId: "ORD-999"
    customerId: "CUST-42"
    amount: 89.99
```

### Example 4: Failure Anti-Pattern & Production Fix
**Anti-Pattern**: Putting full end-to-end database assertions and complex business calculations inside contract tests.

```yaml
# BUGGY ANTI-PATTERN: Contract checks database inventory and user loyalty points!
# Makes contract brittle and heavily coupled to internal producer database state!
```

**Production Fix**:
Contracts must remain strictly focused on the **API interface protocol** (HTTP headers, status codes, JSON schema structures). Move business calculations to pure unit tests.

### Example 5: Pact JVM Consumer Test
Writing contracts using the Pact JVM DSL.

```java
package com.example.pact;

import au.com.dius.pact.consumer.dsl.PactDslWithProvider;
import au.com.dius.pact.consumer.junit5.PactConsumerTestExt;
import au.com.dius.pact.consumer.junit5.PactTestFor;
import au.com.dius.pact.core.model.RequestResponsePact;
import au.com.dius.pact.core.model.annotations.Pact;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.extension.ExtendWith;
import org.springframework.web.client.RestClient;

import java.util.Map;

import static org.assertj.core.api.Assertions.assertThat;

@ExtendWith(PactConsumerTestExt.class)
@PactTestFor(providerName = "InventoryProvider")
class InventoryClientPactTest {

    @Pact(consumer = "OrderConsumer")
    public RequestResponsePact createPact(PactDslWithProvider builder) {
        return builder
            .given("Item 500 exists with stock")
            .uponReceiving("A request for item stock")
                .path("/api/inventory/500")
                .method("GET")
            .willRespondWith()
                .status(200)
                .headers(Map.of("Content-Type", "application/json"))
                .body("{\"itemId\": 500, \"inStock\": true}")
            .toPact();
    }

    @Test
    @PactTestFor(pactMethod = "createPact", port = "8099")
    void testInventoryClient() {
        RestClient restClient = RestClient.builder().baseUrl("http://localhost:8099").build();
        String response = restClient.get().uri("/api/inventory/500").retrieve().body(String.class);
        assertThat(response).contains("\"inStock\": true");
    }
}
```

---

## 6. Best Practices

1. **Keep Contracts Focused on Interface Syntax**: Never test business workflows in contracts; verify only request/response syntax, status codes, and schema rules.
2. **Version Stubs with Producer Versions**: Tie generated `-stubs.jar` artifacts directly to the producer's semantic version to test backward compatibility.
3. **Use Dynamic Matchers (`by_regex`, `by_type`)**: Use flexible matchers rather than hardcoded string values for dynamic fields (timestamps, UUIDs).
4. **Integrate with CI/CD Gateways (`can-i-deploy`)**: In Pact, use the Pact Broker CLI `can-i-deploy` command to verify that producer and consumer versions are compatible before promoting to production.
5. **Use Spring Cloud Contract for Pure Spring Stacks**: If all microservices are Spring Boot, Spring Cloud Contract provides the smoothest developer ergonomics.
6. **Use Pact for Polyglot Ecosystems**: If your architecture includes Node.js, Go, Python, and Java services, standardize on Pact and the central Pact Broker.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Handcrafted WireMock JSON files | Mocks drift from real API implementations without warning. | Generate WireMock mappings automatically from contracts. |
| Hardcoding dynamic timestamps | Tests fail when timestamps don't match static strings. | Use `by_regex` matchers (e.g., ISO-8601 regex). |
| Testing business logic in contracts | Contracts become fragile, complex, and slow to maintain. | Test business rules in unit tests; keep contracts strictly on protocol schema. |
| Missing `ProducerBase` mock stubs | Generated producer tests fail with NPE or missing dependencies. | Ensure `ProducerBase` properly stubs service dependencies. |
| Deploying without contract validation | Breaking changes deploy to production unnoticed. | Add contract verification to CI build gates. |
| Consumer tests hitting real network | Consumer tests fail when external network is offline. | Use `@AutoConfigureStubRunner` to run stubs locally in-memory. |

---

## 8. Performance & Security Notes

- **Stub Runner Execution Speed**: Because Stub Runner launches an in-memory WireMock instance locally, consumer tests execute in less than 50 milliseconds per call without network latency.
- **Elimination of Shared Staging Environments**: Contract testing eliminates the need for expensive multi-service staging environments, reducing cloud compute costs by up to 80%.
- **Virtual Threads Integration**: WireMock stub servers and RestClient invocations execute smoothly on Java 21 Virtual Threads without carrier thread pinning.

---

## 9. Comparison / When to Use What

| Feature | End-to-End Integration Tests | Consumer-Driven Contracts | Static WireMock Stubs |
|---|---|---|---|
| **Feedback Speed** | Minutes to Hours | **Milliseconds (< 100ms)** | Milliseconds |
| **Drift Detection** | High | **100% (Guaranteed)** | Zero (Prone to drift) |
| **Infrastructure Cost** | Very High | **Low (Local JVM)** | Low |
| **Pipeline Stability** | Low (Flaky) | **High (Deterministic)** | High |

---

## 10. Interview Questions

<details>
<summary>1. What is the fundamental problem with traditional End-to-End (E2E) testing in microservices?</summary>

**Answer:**
Traditional E2E testing requires deploying all microservices, databases, and message brokers to a shared test environment. It is slow, expensive, and fragile: network hiccups, data collisions between concurrent test runs, or an unrelated bug in Service $Z$ will break tests for Service $A$. Furthermore, diagnosing the root cause of an E2E test failure across dozens of services is time-consuming and difficult.
</details>

<details>
<summary>2. How does Consumer-Driven Contract testing prevent API drift?</summary>

**Answer:**
In CDC, the consumer defines the contract specifying the exact request structure and response fields it depends on. This contract is shared with the producer. During the producer's CI build, tests are auto-generated from the contract and executed against the producer's real controllers. If the producer renames or deletes a field required by the consumer, the producer build fails immediately, preventing breaking changes from reaching production.
</details>

<details>
<summary>3. What is the role of `@AutoConfigureStubRunner` in Spring Cloud Contract?</summary>

**Answer:**
`@AutoConfigureStubRunner` is used on the consumer side. It automatically downloads the producer's verified `-stubs.jar` artifact (from a local `.m2` repository or remote Nexus/Artifactory), extracts the generated WireMock JSON mappings, and spins up an in-memory WireMock server on a designated port. The consumer's HTTP client tests execute against this verified stub server without needing a live producer instance.
</details>

<details>
<summary>4. What is the Pact Broker and what is the `can-i-deploy` tool?</summary>

**Answer:**
The Pact Broker is a centralized repository that stores, versions, and displays the relationship between consumer and provider contracts. The `can-i-deploy` CLI tool queries the broker prior to deployment to verify whether the specific version of a service about to be deployed is compatible with the currently deployed versions of its downstream and upstream dependencies.
</details>

<details>
<summary>5. When should an engineering team choose Pact over Spring Cloud Contract?</summary>

**Answer:**
Choose **Pact** if your microservice ecosystem is polyglot (e.g., Java backends, Node.js BFFs, React frontends, Go microservices). Pact supports nearly all major programming languages.
Choose **Spring Cloud Contract** if your entire stack is built on Java and Spring Boot, as it offers tighter Spring Boot integration, native Groovy/YAML DSLs, and first-class messaging support.
</details>

<details>
<summary>6. How does Spring Cloud Contract handle dynamic fields like UUIDs or timestamps?</summary>

**Answer:**
Using dynamic matchers (`matchers.body`). In the contract definition, developers specify `type: by_regex` or `type: by_type`. For example, instead of matching an exact timestamp string, the contract uses a regex matching ISO-8601 formatting. The producer test verifies that the real controller returns a string matching the regex, while the generated stub returns the sample value.
</details>

<details>
<summary>7. Can Consumer-Driven Contract testing be used for message queues (Kafka / RabbitMQ)?</summary>

**Answer:**
Yes. Both Spring Cloud Contract and Pact support contract testing for event-driven messaging. The contract defines the expected message topic/queue, message headers, and JSON payload. The producer build verifies that its event publisher emits messages matching the contract, and the consumer tests that its event listeners can deserialize and handle the contract payload.
</details>

<details>
<summary>8. Does Consumer-Driven Contract testing replace functional unit testing?</summary>

**Answer:**
No. Contract testing specifically verifies the **boundary interface compatibility** (HTTP methods, endpoints, headers, payload schemas) between two services. It does not test internal business logic, database calculations, or complex domain rules, which must remain covered by fast unit tests.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Simple YAML Contract Definition (Easy)</summary>

**Task**: Write a Spring Cloud Contract YAML definition for endpoint `GET /api/status` returning HTTP 200 with JSON `{"status": "UP"}`.

**Solution**:
```yaml
name: "should return system status"
request:
  method: GET
  url: /api/status
response:
  status: 200
  headers:
    Content-Type: application/json
  body:
    status: "UP"
```
</details>

<details>
<summary>Exercise 2: Consumer StubRunner Setup (Easy)</summary>

**Task**: Annotate a consumer test class with `@AutoConfigureStubRunner` to run stubs for artifact `com.example:billing-service` on port `8090` from local storage.

**Solution**:
```java
@SpringBootTest
@AutoConfigureStubRunner(
    ids = "com.example:billing-service:+:stubs:8090",
    stubsMode = StubRunnerProperties.StubsMode.LOCAL
)
class BillingClientContractTest {}
```
</details>

<details>
<summary>Exercise 3: Dynamic Regex Matcher in Contract (Medium)</summary>

**Task**: Write a contract response body definition where `userId` is validated against a UUID regex matcher.

**Solution**:
```yaml
response:
  status: 200
  body:
    userId: "d3b07384-d113-400a-b286-905a8d9a4449"
  matchers:
    body:
      - path: $.userId
        type: by_regex
        value: "[a-f0-9]{8}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{12}"
```
</details>

<details>
<summary>Exercise 4: Producer Base Class with Standalone Controller (Medium)</summary>

**Task**: Create a `ProducerBase` class configuring `RestAssuredMockMvc` against a `ProductController` with a mocked `ProductService`.

**Solution**:
```java
@WebMvcTest(ProductController.class)
public abstract class ProductProducerBase {

    @Autowired
    private ProductController controller;

    @MockitoBean
    private ProductService service;

    @BeforeEach
    public void setup() {
        RestAssuredMockMvc.standaloneSetup(controller);
        given(service.getProduct(1L)).willReturn(new ProductDto(1L, "Book"));
    }
}
```
</details>

<details>
<summary>Exercise 5: Messaging Contract with Trigger Method (Hard)</summary>

**Task**: Define a Spring Cloud Contract YAML for an event published to destination `user-notifications` triggered by method `sendWelcomeEmail()`.

**Solution**:
```yaml
description: "Sends welcome email event"
label: "welcome_email_trigger"
input:
  triggeredBy: "sendWelcomeEmail()"
outputMessage:
  sentTo: "user-notifications"
  headers:
    eventType: "WELCOME_EMAIL"
  body:
    userId: "USR-101"
    email: "user@example.com"
```
</details>

<details>
<summary>Exercise 6: Consumer Integration Test Asserting Stub Response (Hard)</summary>

**Task**: Write a consumer test verifying that an HTTP client retrieves data from the local Stub Runner instance on port 8095 and successfully deserializes the contract response.

**Solution**:
```java
@SpringBootTest
@AutoConfigureStubRunner(
    ids = "com.example:user-service:+:stubs:8095",
    stubsMode = StubRunnerProperties.StubsMode.LOCAL
)
class UserClientIntegrationTest {

    @Test
    void shouldFetchUserFromContractStub() {
        RestClient client = RestClient.builder().baseUrl("http://localhost:8095").build();
        UserResponse response = client.get().uri("/api/users/1")
            .retrieve()
            .body(UserResponse.class);

        assertThat(response).isNotNull();
        assertThat(response.id()).isEqualTo(1L);
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Consumer-Driven Contracts**: Eliminates brittle E2E environments while preventing API drift.
- **Producer Pipeline**: Auto-generates tests from contracts and publishes verified `-stubs.jar` files.
- **Consumer Pipeline**: Uses `@AutoConfigureStubRunner` to run verified WireMock stubs locally.
- **Spring Cloud Contract vs Pact**: Use Spring Cloud Contract for all-Spring stacks; use Pact for polyglot systems.
- **Dynamic Matchers**: Use `by_regex` and `by_type` to handle timestamps, UUIDs, and generated IDs.

---

## 13. Further Reading
- [Spring Cloud Contract Official Reference](https://docs.spring.io/spring-cloud-contract/docs/current/reference/html/)
- [Pact Foundation Documentation](https://docs.pact.io/)
- [Pact JVM Integration Guide](https://github.com/pact-foundation/pact-jvm)
- [Martin Fowler: Consumer-Driven Contracts](https://martinfowler.com/articles/consumerDrivenContracts.html)
- [RestAssured MockMvc Documentation](https://github.com/rest-assured/rest-assured/wiki/Usage#spring-mock-mvc)
