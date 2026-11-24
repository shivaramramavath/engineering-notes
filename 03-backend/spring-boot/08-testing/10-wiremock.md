# 10. WireMock: HTTP Stubbing, Fault Injection & Latency Simulation

| Metadata | Value |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | `01-testing-fundamentals-in-spring.md`, HTTP Protocol Basics, Spring `RestClient` / `WebClient` |
| **Estimated Reading Time** | 35 minutes |
| **Status** | Complete |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | OpenJDK 21, Spring Boot 3.4.1, WireMock 3.9 |

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
Modern microservices interact with dozens of external HTTP services: payment gateways (Stripe), email providers (SendGrid), cloud identity providers (Auth0), and partner REST APIs. Testing these integrations against real production or staging APIs is deeply problematic:
- External sandboxes have strict rate limits, incur usage fees, or go down unexpectedly during CI builds.
- Simulating edge cases—such as network socket timeouts, 502 Bad Gateway errors, dropped TCP packets, or slow response latency—is impossible against live third-party APIs.
- In-memory mock clients (e.g., Mockito mocks of `RestClient`) bypass the HTTP protocol entirely, hiding critical bugs in header serialization, TLS negotiation, connection timeouts, or redirect handling.

**WireMock** is the industry-standard simulator for HTTP-based APIs. It runs an actual embedded HTTP server during test execution, allowing developers to configure stub responses, inject network faults, simulate chaotic latency, and verify exact request interactions.

### Historical Evolution
- **WireMock 1.x (2011-2015)**: Introduced fluent Java DSL and JSON mapping files.
- **WireMock 2.x (2016-2022)**: Added JUnit 4 Rules, asynchronous stubbing, stateful scenario support, and recording/playback.
- **WireMock 3.x (2023-Present)**: Standardized on JUnit 5 (`@WireMockTest`), migrated to Java 17+, optimized Jetty 12 engine, and introduced seamless dynamic port resolution.

### Real-World Relevance
WireMock allows engineering teams to build resilient microservices by testing circuit breakers (Resilience4j), retry mechanisms, and timeout policies under realistic failure conditions.

---

## 2. Core Concepts

### Architectural Contracts
- **`@WireMockTest`**: JUnit 5 extension that manages the lifecycle of an embedded WireMock server running on a dynamic OS port.
- **`WireMock` DSL**: Fluent API (`stubFor()`, `get()`, `post()`, `urlEqualTo()`, `aResponse()`) for declaring HTTP expectations.
- **Fault Injection**: Simulating low-level network failures (`Fault.CONNECTION_RESET_BY_PEER`, `Fault.EMPTY_RESPONSE`, `Fault.MALFORMED_RESPONSE_CHUNK`).
- **Latency Simulation**: Artificially delaying responses via fixed delays (`withFixedDelay()`) or stochastic distributions (`withLogNormalRandomDelay()`) to trigger client timeouts.
- **Scenarios (Stateful Mocks)**: Modeling state machines in stubs (e.g., first request returns 202 Pending, second request returns 200 Success).
- **Request Verification**: Verifying that outbound HTTP requests were made with specific headers, methods, or bodies (`verify(postRequestedFor(...))`).

### The Mental Model
```
[ Spring Boot Microservice Under Test ]
         │
         ├── 1. RestClient invokes: GET http://localhost:8089/v1/charges/100
         │      (Real TCP/IP HTTP request!)
         ▼
[ Embedded WireMock HTTP Server (Port: 8089) ]
         │
         ├── 2. Inspects Request Matchers (URL, Headers, Method)
         ├── 3. Applies Injected Latency (e.g. sleep 2,000ms) or Network Fault
         ├── 4. Returns Configured HTTP Response (Status 200 OK, Body, Headers)
         │
         ▼
[ RestClient completes call / Resilient Circuit Breaker handles timeout ]
```

### Invariants
1. WireMock is a real HTTP server listening on a TCP port; network timeouts, socket handling, and header serialization behave identically to real remote APIs.
2. Stubs evaluate in reverse order of definition (most recently defined stub takes precedence over older matching stubs).
3. In parallel test runs, WireMock must be configured with dynamic ports (`httpPort = 0`) to prevent port collisions.

---

## 3. How It Works Internally

### The WireMock Request Matching Engine
1. WireMock boots an embedded **Jetty** servlet container on a designated or dynamic port.
2. When a request arrives, the `RequestJournal` records the request details (method, path, headers, query params, body).
3. The `StubMappings` registry iterates over registered stubs to find the highest-priority match.
4. If a match is found:
   - Evaluates stateful scenario conditions (current scenario state).
   - Applies configured delays (`withFixedDelay` or random distributions).
   - Generates and writes the response headers and payload to the socket.
5. If no match is found, WireMock returns HTTP 404 with a detailed diagnostic report showing near-miss matches to assist debugging.

---

## 4. Syntax & Usage

### 1. Minimal Maven Starter (`pom.xml`)
```xml
<dependency>
    <groupId>org.wiremock</groupId>
    <artifactId>wiremock-standalone</artifactId>
    <version>3.9.1</version>
    <scope>test</scope>
</dependency>
```

### 2. Standard `@WireMockTest` Configuration
```java
package com.example.client;

import com.github.tomakehurst.wiremock.junit5.WireMockRuntimeInfo;
import com.github.tomakehurst.wiremock.junit5.WireMockTest;
import org.junit.jupiter.api.Test;
import org.springframework.web.client.RestClient;

import static com.github.tomakehurst.wiremock.client.WireMock.*;
import static org.assertj.core.api.Assertions.assertThat;

@WireMockTest // Starts WireMock on dynamic random port
class PaymentGatewayClientTest {

    @Test
    void shouldFetchPaymentStatus(WireMockRuntimeInfo wmRuntimeInfo) {
        // Arrange: Stub WireMock response
        stubFor(get(urlEqualTo("/v1/payments/PAY-100"))
            .willReturn(aResponse()
                .withStatus(200)
                .withHeader("Content-Type", "application/json")
                .withBody("{\"id\":\"PAY-100\",\"status\":\"CAPTURED\"}")));

        // Act: Invoke client using WireMock's dynamic base URL
        RestClient restClient = RestClient.builder()
            .baseUrl(wmRuntimeInfo.getHttpBaseUrl())
            .build();

        String response = restClient.get()
            .uri("/v1/payments/PAY-100")
            .retrieve()
            .body(String.class);

        // Assert
        assertThat(response).contains("CAPTURED");
        verify(getRequestedFor(urlEqualTo("/v1/payments/PAY-100")));
    }
}
```

---

## 5. Practical Examples

### Example 1: Simulating Timeout Exceptions and Circuit Breakers
Testing that a client's timeout policy or Resilience4j circuit breaker trips when an external API experiences severe latency.

```java
package com.example.client;

import com.github.tomakehurst.wiremock.junit5.WireMockRuntimeInfo;
import com.github.tomakehurst.wiremock.junit5.WireMockTest;
import org.junit.jupiter.api.Test;
import org.springframework.http.client.SimpleClientHttpRequestFactory;
import org.springframework.web.client.ResourceAccessException;
import org.springframework.web.client.RestClient;

import java.time.Duration;

import static com.github.tomakehurst.wiremock.client.WireMock.*;
import static org.assertj.core.api.Assertions.assertThatThrownBy;

@WireMockTest
class ClientTimeoutTest {

    @Test
    void shouldThrowTimeoutExceptionWhenApiExceedsDeadline(WireMockRuntimeInfo wm) {
        // Arrange: Simulate 3-second delay
        stubFor(get("/api/slow-service")
            .willReturn(aResponse()
                .withStatus(200)
                .withFixedDelay(3000) // 3,000 ms latency
                .withBody("{\"status\":\"ok\"}")));

        // Configure client with strict 1-second timeout
        SimpleClientHttpRequestFactory requestFactory = new SimpleClientHttpRequestFactory();
        requestFactory.setReadTimeout(Duration.ofSeconds(1));

        RestClient client = RestClient.builder()
            .baseUrl(wm.getHttpBaseUrl())
            .requestFactory(requestFactory)
            .build();

        // Act & Assert: Expect read timeout
        assertThatThrownBy(() -> client.get().uri("/api/slow-service").retrieve().body(String.class))
            .isInstanceOf(ResourceAccessException.class)
            .hasMessageContaining("Read timed out");
    }
}
```

### Example 2: Low-Level Network Fault Injection
Simulating sudden connection drops, TCP resets, and empty responses.

```java
package com.example.client;

import com.github.tomakehurst.wiremock.http.Fault;
import com.github.tomakehurst.wiremock.junit5.WireMockRuntimeInfo;
import com.github.tomakehurst.wiremock.junit5.WireMockTest;
import org.junit.jupiter.api.Test;
import org.springframework.web.client.RestClient;

import static com.github.tomakehurst.wiremock.client.WireMock.*;
import static org.assertj.core.api.Assertions.assertThatThrownBy;

@WireMockTest
class NetworkFaultTest {

    @Test
    void shouldHandleConnectionResetByPeer(WireMockRuntimeInfo wm) {
        stubFor(get("/api/flaky-service")
            .willReturn(aResponse()
                .withFault(Fault.CONNECTION_RESET_BY_PEER))); // Injects immediate TCP reset!

        RestClient client = RestClient.builder().baseUrl(wm.getHttpBaseUrl()).build();

        assertThatThrownBy(() -> client.get().uri("/api/flaky-service").retrieve().body(String.class))
            .isNotNull();
    }
}
```

### Example 3: Edge Case – Stateful Scenarios (Retry Logic Verification)
Simulating an external endpoint that fails with HTTP 503 on the first call, but succeeds on the retry attempt.

```java
package com.example.client;

import com.github.tomakehurst.wiremock.junit5.WireMockRuntimeInfo;
import com.github.tomakehurst.wiremock.junit5.WireMockTest;
import com.github.tomakehurst.wiremock.stubbing.Scenario;
import org.junit.jupiter.api.Test;
import org.springframework.web.client.RestClient;

import static com.github.tomakehurst.wiremock.client.WireMock.*;
import static org.assertj.core.api.Assertions.assertThat;

@WireMockTest
class StatefulRetryScenarioTest {

    @Test
    void shouldSucceedAfterInitialFailureUsingStatefulScenario(WireMockRuntimeInfo wm) {
        // Step 1: Initial state -> Return 503 and transition to "RETRY_STATE"
        stubFor(get("/api/unstable")
            .inScenario("RetryScenario")
            .whenScenarioStateIs(Scenario.STARTED)
            .willReturn(aResponse().withStatus(503))
            .willSetStateTo("RETRY_STATE"));

        // Step 2: Next state -> Return 200 Success
        stubFor(get("/api/unstable")
            .inScenario("RetryScenario")
            .whenScenarioStateIs("RETRY_STATE")
            .willReturn(aResponse()
                .withStatus(200)
                .withBody("{\"result\":\"recovered\"}")));

        RestClient client = RestClient.builder().baseUrl(wm.getHttpBaseUrl()).build();

        // Simulate client retry loop
        String result = callWithRetry(client, "/api/unstable");

        assertThat(result).isEqualTo("{\"result\":\"recovered\"}");
        verify(2, getRequestedFor(urlEqualTo("/api/unstable")));
    }

    private String callWithRetry(RestClient client, String path) {
        try {
            return client.get().uri(path).retrieve().body(String.class);
        } catch (Exception e) {
            // Simple retry
            return client.get().uri(path).retrieve().body(String.class);
        }
    }
}
```

### Example 4: Failure Anti-Pattern & Production Fix
**Anti-Pattern**: Hardcoding fixed ports (`@WireMockTest(httpPort = 8080)`), which collides with local services or breaks parallel CI builds.

```java
// BUGGY: Hardcoded port 8080! Fails with BindException in parallel builds!
@WireMockTest(httpPort = 8080)
class BrokenPortTest {}
```

**Production Fix**:
Always use dynamic ports and dynamically inject the port or base URL into Spring properties.

```java
// SECURE & PARALLEL-SAFE: Dynamic random port
@WireMockTest
class SafePortTest {
    @Test
    void test(WireMockRuntimeInfo wm) {
        String dynamicUrl = wm.getHttpBaseUrl(); // Unique per test instance
    }
}
```

### Example 5: Integrating WireMock with Spring Boot `@DynamicPropertySource`
Binding a dynamic WireMock port into Spring Boot's `application.yml` properties during an integration test.

```java
package com.example.testing;

import com.github.tomakehurst.wiremock.junit5.WireMockExtension;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.extension.RegisterExtension;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.test.context.DynamicPropertyRegistry;
import org.springframework.test.context.DynamicPropertySource;

import static com.github.tomakehurst.wiremock.client.WireMock.*;
import static com.github.tomakehurst.wiremock.core.WireMockConfiguration.wireMockConfig;
import static org.assertj.core.api.Assertions.assertThat;

@SpringBootTest
class FullIntegrationWireMockTest {

    // Register extension with dynamic port
    @RegisterExtension
    static WireMockExtension wireMock = WireMockExtension.newInstance()
        .options(wireMockConfig().dynamicPort())
        .build();

    @DynamicPropertySource
    static void configureProperties(DynamicPropertyRegistry registry) {
        // Bind dynamic port to application property
        registry.add("external.payment.service.url", wireMock::baseUrl);
    }

    @Autowired
    private PaymentService paymentService;

    @Test
    void shouldProcessPaymentViaWireMock() {
        wireMock.stubFor(post("/charges")
            .willReturn(aResponse().withStatus(200).withBody("{\"id\":\"ch_123\"}")));

        String chargeId = paymentService.charge(50.0);
        assertThat(chargeId).isEqualTo("ch_123");
    }
}
```

---

## 6. Best Practices

1. **Always Use Dynamic Ports**: Never hardcode ports; use `@WireMockTest` with `WireMockRuntimeInfo` or `@RegisterExtension` with dynamic ports.
2. **Test Timeouts and Delays**: Explicitly test client resilience by simulating network latency with `.withFixedDelay()`.
3. **Use Scenarios for Retries**: Test circuit breakers, exponential backoff, and idempotency keys using stateful WireMock scenarios.
4. **Verify Request Headers**: Assert that client requests properly transmit `Authorization`, `Accept`, and correlation tracking headers (`X-Correlation-ID`).
5. **Clean Up Stubs Between Tests**: Reset mappings and journal between tests using `WireMock.reset()` if not using JUnit 5 extensions.
6. **Use Specific Matchers**: Match on specific headers and JSON bodies rather than generic `any()` to catch payload serialization regressions.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Hardcoding port 8080/8089 | Triggers `BindException` when tests run in parallel. | Use dynamic ports via `@WireMockTest` and `wm.getHttpBaseUrl()`. |
| Unmatched requests returning 404 | Typo in URL path or missing query parameters causes WireMock to return default 404. | Inspect console output for WireMock's "Closest Match" diagnostics. |
| Forgetting `Content-Type` on stubs | Client fails to deserialize response because default header is `text/plain`. | Add `.withHeader("Content-Type", "application/json")`. |
| Order of stub evaluation | Broad stubs declared last override specific stubs defined earlier. | Declare general fallbacks first, and specific overrides last. |
| Non-deterministic random delays | Tests intermittently fail because random delay exceeds timeout threshold. | Use fixed delays (`withFixedDelay()`) for deterministic test assertions. |
| Memory leaks from large request journals | Processing thousands of requests in one test class exhausts memory tracking history. | Clear journal between tests with `WireMock.resetAllRequests()`. |

---

## 8. Performance & Security Notes

- **In-Memory Network Execution**: WireMock uses local loopback network sockets. A typical stubbed HTTP request completes in 5 to 10 milliseconds.
- **Fault Injection Mechanics**: Low-level faults (like `CONNECTION_RESET_BY_PEER`) are generated at the socket level by closing the underlying TCP connection abruptly, accurately triggering Java `SocketException`.
- **Virtual Threads Integration**: Because WireMock uses Jetty 12 and standard HTTP sockets, Spring's `RestClient` unmounts Java 21 Virtual Threads cleanly while waiting on WireMock delays.

---

## 9. Comparison / When to Use What

| Tool | Engine | Network Port? | Fault Injection? | Best Use Case |
|---|---|---|---|---|
| **WireMock** | Embedded Jetty Server | **Yes (Real TCP)** | **Yes (TCP resets, delays)** | Full HTTP integration, resilience testing |
| **`@RestClientTest`** | `MockRestServiceServer` | No (In-Memory) | No | Fast isolated client testing |
| **Testcontainers** | Real Docker Container | Yes | No (Unless Chaos Proxy) | End-to-end multi-service testing |
| **Mockito** | Bytecode Mock | No | No (Java exceptions only) | Pure unit tests |

---

## 10. Interview Questions

<details>
<summary>1. How does WireMock differ from Spring's `MockRestServiceServer`?</summary>

**Answer:**
- `MockRestServiceServer` operates in-memory by intercepting Spring's `ClientHttpRequestFactory` before a network socket is created. It is fast and lightweight, but it does not execute real network I/O, cannot simulate socket timeouts or TCP drops, and only works with Spring HTTP clients.
- `WireMock` is an independent, real HTTP server running on a local TCP port. It can test any HTTP client (Spring, Apache HttpClient, OkHttp), handles real TLS handshakes, supports dynamic latency simulation, and provides low-level network fault injection.
</details>

<details>
<summary>2. What is a Stateful Scenario in WireMock?</summary>

**Answer:**
A Scenario is a state machine built into WireMock. Stubs can be configured with `whenScenarioStateIs(state)` and `willSetStateTo(newState)`. This allows modeling multi-step workflows—such as polling an asynchronous job where the first three calls return HTTP 202 In Progress, and the fourth call returns HTTP 200 Completed—without writing custom servlet code.
</details>

<details>
<summary>3. How do you simulate a connection timeout versus a read timeout in WireMock?</summary>

**Answer:**
- **Read Timeout**: Use `.withFixedDelay(milliseconds)`. WireMock accepts the connection immediately, but delays sending back the HTTP response body until the delay expires, triggering a client-side socket read timeout.
- **Connection Timeout**: Bind WireMock to an unroutable IP address or use low-level socket throttling / firewall rules to prevent the TCP handshake from completing.
</details>

<details>
<summary>4. What are WireMock Faults and what scenarios do they test?</summary>

**Answer:**
WireMock faults simulate low-level TCP/IP network failures rather than valid HTTP status codes:
- `Fault.CONNECTION_RESET_BY_PEER`: Abruptly terminates the TCP connection with an RST packet.
- `Fault.EMPTY_RESPONSE`: Sends a zero-byte response and immediately closes the socket.
- `Fault.MALFORMED_RESPONSE_CHUNK`: Sends invalid garbage HTTP chunked encoding bytes.
These test client resilience, error handling, and recovery from catastrophic network failures.
</details>

<details>
<summary>5. How does WireMock's dynamic port resolution prevent port collisions in CI?</summary>

**Answer:**
By specifying port `0` (or using `@WireMockTest` with no arguments), the operating system assigns an available ephemeral dynamic port at startup. Developers retrieve the assigned port via `WireMockRuntimeInfo.getHttpPort()` or `getHttpBaseUrl()`. This guarantees that parallel test threads or concurrent build jobs never encounter `BindException`.
</details>

<details>
<summary>6. How can you record real external API interactions into WireMock stubs?</summary>

**Answer:**
WireMock includes a Record and Playback feature. You can configure WireMock to proxy requests to the real third-party API (`proxyTo("https://api.stripe.com")`). When tests run, WireMock forwards requests to the real service, records the incoming requests and outgoing responses, and saves them as reusable JSON stub files in `src/test/resources/mappings`.
</details>

<details>
<summary>7. How do you verify that an endpoint was called with an exact Authorization header?</summary>

**Answer:**
Using `verify()` and `equalTo()`:
```java
verify(postRequestedFor(urlEqualTo("/v1/charge"))
    .withHeader("Authorization", equalTo("Bearer secret-token-123")));
```
If the header is missing or incorrect, WireMock throws an assertion error showing the expected vs actual headers.
</details>

<details>
<summary>8. How do you handle HTTPS and SSL verification in WireMock?</summary>

**Answer:**
WireMock can be configured with `httpsPort(0)` to enable TLS. WireMock automatically generates a self-signed X.509 certificate. In test clients, you must either configure the client to trust self-signed certificates or inject WireMock's keystore into the client's `SSLContext`.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Basic 200 OK JSON Stub (Easy)</summary>

**Task**: Write a WireMock stub that matches `GET /api/status` and returns HTTP 200 with JSON `{"health":"UP"}` and `Content-Type: application/json`.

**Solution**:
```java
stubFor(get(urlEqualTo("/api/status"))
    .willReturn(aResponse()
        .withStatus(200)
        .withHeader("Content-Type", "application/json")
        .withBody("{\"health\":\"UP\"}")));
```
</details>

<details>
<summary>Exercise 2: Simulating 500 Server Error (Easy)</summary>

**Task**: Stub an endpoint `POST /api/payment` to return HTTP 500 Internal Server Error with error body `{"error":"Database error"}`.

**Solution**:
```java
stubFor(post(urlEqualTo("/api/payment"))
    .willReturn(aResponse()
        .withStatus(500)
        .withHeader("Content-Type", "application/json")
        .withBody("{\"error\":\"Database error\"}")));
```
</details>

<details>
<summary>Exercise 3: Verifying Request Payload Content (Medium)</summary>

**Task**: Write a WireMock verification ensuring that `POST /api/orders` was called exactly once with a JSON body containing `"itemId": "ITEM-100"`.

**Solution**:
```java
verify(1, postRequestedFor(urlEqualTo("/api/orders"))
    .withRequestBody(containing("\"itemId\": \"ITEM-100\"")));
```
</details>

<details>
<summary>Exercise 4: Fixed Latency Simulation (Medium)</summary>

**Task**: Configure a stub for `GET /api/slow` with a fixed delay of 1,500ms and verify with Spring `RestClient` that the call takes at least 1,500ms.

**Solution**:
```java
stubFor(get("/api/slow")
    .willReturn(aResponse()
        .withStatus(200)
        .withFixedDelay(1500)
        .withBody("done")));

long start = System.currentTimeMillis();
restClient.get().uri("/api/slow").retrieve().body(String.class);
long elapsed = System.currentTimeMillis() - start;

assertThat(elapsed).isGreaterThanOrEqualTo(1500L);
```
</details>

<details>
<summary>Exercise 5: Polling Scenario with Eventual Success (Hard)</summary>

**Task**: Implement a stateful scenario for `GET /jobs/1` where the first call returns `{"status":"PENDING"}` (HTTP 200), and the second call returns `{"status":"COMPLETED"}`.

**Solution**:
```java
stubFor(get("/jobs/1")
    .inScenario("PollingJob")
    .whenScenarioStateIs(Scenario.STARTED)
    .willReturn(aResponse().withStatus(200).withBody("{\"status\":\"PENDING\"}"))
    .willSetStateTo("DONE"));

stubFor(get("/jobs/1")
    .inScenario("PollingJob")
    .whenScenarioStateIs("DONE")
    .willReturn(aResponse().withStatus(200).withBody("{\"status\":\"COMPLETED\"}")));
```
</details>

<details>
<summary>Exercise 6: Dynamic Property Binding in Full Spring Boot Test (Hard)</summary>

**Task**: Write a `@SpringBootTest` integrating WireMock via `WireMockExtension` and `@DynamicPropertySource` to inject the dynamic base URL into property `downstream.api.url`.

**Solution**:
```java
@SpringBootTest
class DynamicWireMockIntegrationTest {

    @RegisterExtension
    static WireMockExtension wireMock = WireMockExtension.newInstance()
        .options(wireMockConfig().dynamicPort())
        .build();

    @DynamicPropertySource
    static void bindProperties(DynamicPropertyRegistry registry) {
        registry.add("downstream.api.url", wireMock::baseUrl);
    }

    @Autowired
    private DownstreamApiClient client;

    @Test
    void shouldExecuteClientAgainstWireMock() {
        wireMock.stubFor(get("/data").willReturn(aResponse().withStatus(200).withBody("SUCCESS")));
        assertThat(client.fetchData()).isEqualTo("SUCCESS");
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Real TCP/IP Execution**: WireMock tests real HTTP communication including headers, TLS, and timeouts.
- **Dynamic Ports are Mandatory**: Use `@WireMockTest` without arguments to avoid CI port collisions.
- **Fault Injection**: Test resilience against TCP resets (`Fault.CONNECTION_RESET_BY_PEER`).
- **Stateful Scenarios**: Model polling and retries across multiple sequential requests.
- **Always Add Content-Type**: Ensure stubs specify `.withHeader("Content-Type", "application/json")`.

---

## 13. Further Reading
- [WireMock Official Documentation](https://wiremock.org/docs/)
- [WireMock 3.x Upgrade & Release Notes](https://wiremock.org/docs/wiremock-3/)
- [Spring Cloud Contract WireMock Integration](https://docs.spring.io/spring-cloud-contract/docs/current/reference/html/project-features.html#features-wiremock)
- [Resilience4j CircuitBreaker with WireMock](https://resilience4j.readme.io/docs/circuitbreaker)
- [Jetty 12 Documentation](https://eclipse.dev/jetty/documentation/)
