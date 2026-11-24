# 05. MockMvc, WebTestClient & RestTestClient in Spring Boot

| Metadata | Value |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | `01-testing-fundamentals-in-spring.md`, `03-spring-boot-test-and-slices.md`, Spring MVC / WebFlux |
| **Estimated Reading Time** | 35 minutes |
| **Status** | Complete |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | OpenJDK 21, Spring Boot 3.4.1 |

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
Validating REST controllers through pure unit tests (instantiating controllers with `new` and calling methods directly) fails to test essential web concerns:
- URL path mapping and HTTP method routing (`@GetMapping`, `@PostMapping`).
- Request body deserialization and JSON property mapping.
- Input validation constraints (`@Valid`, `@NotNull`, `@Size`) and HTTP 400 translation.
- Exception handling via `@ControllerAdvice` and RFC 7807 `ProblemDetail` responses.
- Content negotiation (`Accept`, `Content-Type`), headers, and cookie extraction.

Conversely, starting a real embedded Tomcat server on every test and executing network HTTP requests introduces port management headaches and adds substantial latency.

Spring provides **`MockMvc`** (for mock servlet execution), **`WebTestClient`** (for non-blocking reactive and mock testing), and **`RestTestClient`** (introduced in Spring 6.2 / Boot 3.4 for fluent synchronous testing) to verify web endpoints with complete fidelity in memory.

### Historical Evolution
- **Spring 3.2**: Introduced `MockMvc` as part of the Spring MVC Test framework.
- **Spring 5.0 / Boot 2.0**: Added `WebTestClient` for WebFlux, which could also bind to `MockMvc` for synchronous controllers.
- **Spring Framework 6.2 / Boot 3.4+**: Introduced `RestTestClient`, providing a unified, fluent client matching the modern `RestClient` API style.

### Real-World Relevance
MockMvc and RestTestClient are the primary tools used by enterprise development teams to test REST APIs, OpenAPI contract adherence, and validation rules in automated test suites.

---

## 2. Core Concepts

### Architectural Contracts
- **`org.springframework.test.web.servlet.MockMvc`**: The core entry point for mock servlet testing. Dispatches requests directly to `DispatcherServlet` using in-memory mock request/response objects (`MockHttpServletRequest`, `MockHttpServletResponse`).
- **`MockMvcRequestBuilders`**: Static factory providing builders for HTTP requests (`get()`, `post()`, `put()`, `delete()`, `multipart()`).
- **`MockMvcResultMatchers`**: Static factory providing assertion matchers (`status()`, `content()`, `jsonPath()`, `header()`, `cookie()`).
- **`JsonPath`**: Query language used to assert against specific JSON response payload fields (`$.id`, `$.items[0].price`).
- **`org.springframework.test.web.reactive.server.WebTestClient`**: Reactive, non-blocking test client designed for WebFlux, but capable of binding to `MockMvc` or live servers.
- **`org.springframework.test.web.servlet.client.RestTestClient`**: Synchronous test client introduced in Spring 6.2 / Boot 3.4, mirroring `RestClient` syntax.

### The Mental Model
```
[ MockMvc Test ]
       │
       ├── 1. mockMvc.perform(post("/api/v1/shipments").content("{...}"))
       │
       ▼
[ MockHttpServletRequest ] (In-Memory, Zero Network Sockets)
       │
       ▼
[ Standard Spring DispatcherServlet ]
       │
       ├── Interceptors -> HandlerMapping -> Controller -> HttpMessageConverter
       │
       ▼
[ MockHttpServletResponse ]
       │
       └── 2. andExpect(status().isCreated())
              andExpect(jsonPath("$.trackingNumber").isNotEmpty())
```

### Invariants
1. `MockMvc` does not start an operating system network socket or embedded web container; requests are dispatched in-memory directly through the servlet pipeline.
2. In sliced tests (`@WebMvcTest`), only controller and web beans are available; service layer dependencies must be mocked using `@MockitoBean`.
3. Character encoding defaults to UTF-8; always specify `contentType(MediaType.APPLICATION_JSON)` when transmitting JSON bodies.

---

## 3. How It Works Internally

### The In-Memory Dispatch Pipeline
1. `MockMvc.perform(builder)` invokes the request builder to generate a populated `MockHttpServletRequest`.
2. The request passes through any configured servlet filters (e.g., `CharacterEncodingFilter`, `SecurityFilterChain`, `CorsFilter`).
3. Passes to `DispatcherServlet.service(request, response)`.
4. `HandlerMapping` matches the URL to the target controller method.
5. `HandlerAdapter` executes argument resolvers (Jackson deserializes JSON body, validators check constraints).
6. Controller executes and returns a domain DTO or `ResponseEntity`.
7. `HttpMessageConverter` serializes the return object into the `MockHttpServletResponse`.
8. `ResultActions.andExpect()` evaluates assertion matchers against the captured response.

---

## 4. Syntax & Usage

### 1. Fluent Controller Testing with `MockMvc`
```java
package com.example.web;

import com.example.service.ShipmentService;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.autoconfigure.web.servlet.WebMvcTest;
import org.springframework.http.MediaType;
import org.springframework.test.context.bean.override.mockito.MockitoBean;
import org.springframework.test.web.servlet.MockMvc;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.BDDMockito.given;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.post;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.*;

@WebMvcTest(ShipmentController.class)
class ShipmentControllerTest {

    @Autowired
    private MockMvc mockMvc;

    @MockitoBean
    private ShipmentService shipmentService;

    @Test
    void shouldCreateShipmentSuccessfully() throws Exception {
        given(shipmentService.createShipment(any()))
            .willReturn(new ShipmentResponse("TRK-9001", "IN_TRANSIT"));

        mockMvc.perform(post("/api/v1/shipments")
                .contentType(MediaType.APPLICATION_JSON)
                .content("""
                    {
                        "destination": "London",
                        "weightKg": 4.5
                    }
                    """))
            .andExpect(status().isCreated())
            .andExpect(header().exists("Location"))
            .andExpect(jsonPath("$.trackingNumber").value("TRK-9001"))
            .andExpect(jsonPath("$.status").value("IN_TRANSIT"));
    }
}
```

---

## 5. Practical Examples

### Example 1: Testing Validation Errors and RFC 7807 `ProblemDetail` Responses
Verifying that invalid input triggers Bean Validation failure and returns structured error details.

```java
package com.example.web;

import com.example.service.ShipmentService;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.autoconfigure.web.servlet.WebMvcTest;
import org.springframework.http.MediaType;
import org.springframework.test.context.bean.override.mockito.MockitoBean;
import org.springframework.test.web.servlet.MockMvc;

import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.post;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.*;

@WebMvcTest(ShipmentController.class)
class ShipmentValidationTest {

    @Autowired
    private MockMvc mockMvc;

    @MockitoBean
    private ShipmentService shipmentService;

    @Test
    void shouldReturn400ProblemDetailWhenDestinationIsBlank() throws Exception {
        mockMvc.perform(post("/api/v1/shipments")
                .contentType(MediaType.APPLICATION_JSON)
                .content("""
                    {
                        "destination": "",
                        "weightKg": -1.0
                    }
                    """))
            .andExpect(status().isBadRequest())
            .andExpect(jsonPath("$.title").value("Bad Request"))
            .andExpect(jsonPath("$.status").value(400))
            .andExpect(jsonPath("$.errors.destination").value("Destination must not be blank"))
            .andExpect(jsonPath("$.errors.weightKg").value("Weight must be positive"));
    }
}
```

### Example 2: Testing Multipart File Uploads with `multipart()`
Verifying file ingestion, parameter parsing, and multipart boundary handling.

```java
package com.example.web;

import com.example.service.DocumentStorageService;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.autoconfigure.web.servlet.WebMvcTest;
import org.springframework.mock.web.MockMultipartFile;
import org.springframework.test.context.bean.override.mockito.MockitoBean;
import org.springframework.test.web.servlet.MockMvc;

import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.multipart;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.status;

@WebMvcTest(DocumentUploadController.class)
class DocumentUploadControllerTest {

    @Autowired
    private MockMvc mockMvc;

    @MockitoBean
    private DocumentStorageService storageService;

    @Test
    void shouldHandleFileUpload() throws Exception {
        MockMultipartFile file = new MockMultipartFile(
            "file",
            "contract.pdf",
            "application/pdf",
            "%PDF-1.4 Mock PDF Content".getBytes()
        );

        mockMvc.perform(multipart("/api/v1/documents/upload")
                .file(file)
                .param("department", "LEGAL"))
            .andExpect(status().isAccepted());
    }
}
```

### Example 3: Edge Case – Testing Asynchronous Controllers (`MvcResult` & `asyncDispatch`)
Testing Spring MVC controllers that return `CompletableFuture<ResponseEntity<T>>` or `DeferredResult`.

```java
package com.example.web;

import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.autoconfigure.web.servlet.WebMvcTest;
import org.springframework.test.web.servlet.MockMvc;
import org.springframework.test.web.servlet.MvcResult;

import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.asyncDispatch;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.get;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.*;

@WebMvcTest(AsyncReportingController.class)
class AsyncReportingControllerTest {

    @Autowired
    private MockMvc mockMvc;

    @Test
    void shouldHandleAsyncRequest() throws Exception {
        // Step 1: Initial request starts async processing
        MvcResult mvcResult = mockMvc.perform(get("/api/v1/reports/async"))
            .andExpect(request().asyncStarted())
            .andReturn();

        // Step 2: Dispatch async result when computation completes
        mockMvc.perform(asyncDispatch(mvcResult))
            .andExpect(status().isOk())
            .andExpect(jsonPath("$.status").value("COMPLETED"));
    }
}
```

### Example 4: Failure Anti-Pattern & Production Fix
**Anti-Pattern**: Using raw substring matching on response bodies instead of structured `jsonPath()` matchers, creating brittle tests that break on whitespace changes.

```java
// BRITTLE & BUGGY:
mockMvc.perform(get("/api/user"))
    .andExpect(content().string("{\"id\":10,\"name\":\"Alice\"}")); // Fails if JSON fields reorder!
```

**Production Fix**:
Use standard `jsonPath()` expressions to evaluate exact fields independent of JSON formatting.

```java
// ROBUST:
mockMvc.perform(get("/api/user"))
    .andExpect(jsonPath("$.id").value(10))
    .andExpect(jsonPath("$.name").value("Alice"));
```

### Example 5: Modern Spring 6.2 / Boot 3.4 `RestTestClient` Usage
Using the new synchronous `RestTestClient` abstraction.

```java
package com.example.web;

import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.autoconfigure.web.servlet.WebMvcTest;
import org.springframework.test.web.servlet.client.RestTestClient;

@WebMvcTest(ProductController.class)
class ProductRestTestClientTest {

    @Autowired
    private RestTestClient client;

    @Test
    void shouldQueryProductsUsingRestTestClient() {
        client.get().uri("/api/products/P-100")
            .exchange()
            .expectStatus().isOk()
            .expectBody()
            .jsonPath("$.name").isEqualTo("Wireless Mouse");
    }
}
```

---

## 6. Best Practices

1. **Use `jsonPath()` for Payload Assertions**: Verify individual fields rather than comparing exact raw JSON string bodies.
2. **Verify HTTP Headers**: Always check standard headers such as `Location` on 201 Created and `Content-Type`.
3. **Use Text Blocks for JSON Payloads**: Leverage Java 21 multiline text blocks (`"""`) for clean, readable request payloads in tests.
4. **Scope `@WebMvcTest` to Single Controllers**: Prevent slice bloat by loading only the controller under test.
5. **Always Assert Status Codes First**: Assert `status().isOk()` or `status().isCreated()` before checking body payloads to catch routing errors early.
6. **Test Boundary Validation Cases**: Test empty strings, null values, negative numbers, and oversized inputs to ensure `@Valid` annotations are enforced.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Forgetting `contentType(APPLICATION_JSON)` | Spring cannot determine request media type and throws `HttpMediaTypeNotSupportedException` (415). | Always set `.contentType(MediaType.APPLICATION_JSON)`. |
| String matching on JSON response | Breaks whenever field ordering, formatting, or indentation changes. | Use `jsonPath("$.field").value(...)`. |
| Testing async endpoints synchronously | Fails with empty response because async processing has not completed. | Use `request().asyncStarted()` followed by `asyncDispatch(mvcResult)`. |
| Missing `@AutoConfigureMockMvc` in full test | `@Autowired private MockMvc mockMvc` is null in `@SpringBootTest`. | Add `@AutoConfigureMockMvc` to `@SpringBootTest`. |
| Ignoring CSRF on mutating requests | `POST`, `PUT`, `DELETE` fail with 403 Forbidden when security is enabled. | Append `.with(csrf())` to the request builder. |
| Forgetting `@MockitoBean` on service dependencies | Context fails to start with `NoSuchBeanDefinitionException`. | Supply mocks for all dependencies required by the controller. |

---

## 8. Performance & Security Notes

- **Zero Network Overhead**: Because `MockMvc` executes directly in memory against `DispatcherServlet`, test invocations complete in 10 to 30 milliseconds.
- **Security Filter Execution**: `MockMvc` automatically runs requests through the full `SecurityFilterChain` (including CORS, CSRF, and authorization rules) when security starters are present.
- **Virtual Threads Compatibility**: Tests executing via `MockMvc` or `RestTestClient` run seamlessly on Java 21 without thread pool contention.

---

## 9. Comparison / When to Use What

| Feature | `MockMvc` | `RestTestClient` (Spring 6.2+) | `WebTestClient` | `TestRestTemplate` |
|---|---|---|---|---|
| **Underlying Engine** | Mock Servlet (In-Memory) | MockMvc or Real HTTP | Netty or MockMvc | Real Embedded Server |
| **Paradigm** | Synchronous | Synchronous Fluent | Reactive Fluent | Synchronous Template |
| **Port Bound?** | No (Mock) | Optional | Optional | **Yes (RANDOM_PORT)** |
| **Best For** | Standard Spring MVC | Modern Spring 6.2+ MVC | WebFlux & Reactive APIs | Full network integration |

---

## 10. Interview Questions

<details>
<summary>1. How does `MockMvc` simulate HTTP requests without starting a web server?</summary>

**Answer:**
`MockMvc` bypasses operating system network sockets entirely. It constructs in-memory implementations of the standard Servlet API (`MockHttpServletRequest` and `MockHttpServletResponse`) and passes them directly to Spring's `DispatcherServlet.service(request, response)`. The entire MVC request lifecycle (filters, interceptors, routing, validation, serialization) executes in memory within the current JVM thread.
</details>

<details>
<summary>2. What is the difference between `MockMvc` and `WebTestClient`?</summary>

**Answer:**
- `MockMvc` is designed specifically for synchronous Servlet-based Spring MVC applications.
- `WebTestClient` was originally built for non-blocking reactive Spring WebFlux applications. However, `WebTestClient` can also be bound to a `MockMvc` instance (`bindToMockMvc()`) or a live running server, providing a fluent reactive assertion API across both paradigms.
</details>

<details>
<summary>3. How do you test an endpoint that returns HTTP 201 Created with a `Location` header?</summary>

**Answer:**
```java
mockMvc.perform(post("/api/items")
        .contentType(MediaType.APPLICATION_JSON)
        .content("{\"name\":\"Widget\"}"))
    .andExpect(status().isCreated())
    .andExpect(header().string("Location", org.hamcrest.Matchers.containsString("/api/items/")));
```
</details>

<details>
<summary>4. What is `jsonPath()` and what library powers it in Spring Boot?</summary>

**Answer:**
`jsonPath()` is an assertion builder powered by the Jayway JsonPath library. It allows querying and asserting on JSON documents using XPath-like syntax (e.g., `$.name`, `$.orders[0].price`, `$.items[*].id`). It verifies data structures and values independently of JSON string formatting or key ordering.
</details>

<details>
<summary>5. How do you test file uploads using `MockMvc`?</summary>

**Answer:**
By using `MockMvcRequestBuilders.multipart()` and `MockMultipartFile`:
```java
MockMultipartFile file = new MockMultipartFile("file", "test.csv", "text/csv", "col1,col2".getBytes());
mockMvc.perform(multipart("/api/upload").file(file))
    .andExpect(status().isOk());
```
</details>

<details>
<summary>6. Why is `asyncDispatch()` necessary when testing asynchronous controllers?</summary>

**Answer:**
Asynchronous controllers offload processing to worker threads, returning before the HTTP response body is generated. The initial `perform()` call only verifies that async processing began (`request().asyncStarted()`). To wait for the background computation to complete and verify the resulting response, you must execute `mockMvc.perform(asyncDispatch(mvcResult))`.
</details>

<details>
<summary>7. How do you print request and response details for debugging failing MockMvc tests?</summary>

**Answer:**
By chaining `.andDo(MockMvcResultHandlers.print())` to the perform call:
```java
mockMvc.perform(get("/api/test"))
    .andDo(print())
    .andExpect(status().isOk());
```
This logs the complete request headers, parameters, controller invoked, and response payload to standard out.
</details>

<details>
<summary>8. What is `RestTestClient` introduced in Spring Framework 6.2?</summary>

**Answer:**
`RestTestClient` is a modern synchronous test client introduced in Spring 6.2 / Spring Boot 3.4. It mirrors the fluent syntax of the modern `RestClient` API, providing a clean alternative to `MockMvc` without requiring reactive WebFlux dependencies.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Basic GET Endpoint Assertion (Easy)</summary>

**Task**: Write a MockMvc test verifying that `GET /api/v1/health` returns HTTP 200 OK with JSON `{"status":"HEALTHY"}`.

**Solution**:
```java
@Test
void shouldReturnHealthStatus() throws Exception {
    mockMvc.perform(get("/api/v1/health"))
        .andExpect(status().isOk())
        .andExpect(jsonPath("$.status").value("HEALTHY"));
}
```
</details>

<details>
<summary>Exercise 2: POST Request with JSON Body (Easy)</summary>

**Task**: Write a MockMvc test that posts a JSON user object and verifies that HTTP 201 Created is returned along with the generated user ID.

**Solution**:
```java
@Test
void shouldCreateUser() throws Exception {
    given(userService.createUser(any())).willReturn(new UserResponse(42L, "Alice"));

    mockMvc.perform(post("/api/users")
            .contentType(MediaType.APPLICATION_JSON)
            .content("{\"username\":\"Alice\"}"))
        .andExpect(status().isCreated())
        .andExpect(jsonPath("$.id").value(42))
        .andExpect(jsonPath("$.username").value("Alice"));
}
```
</details>

<details>
<summary>Exercise 3: Header and Cookie Assertions (Medium)</summary>

**Task**: Write a test verifying that `POST /api/auth/login` returns a secure `Set-Cookie` header containing `refresh_token` and an `X-Auth-Token` header.

**Solution**:
```java
@Test
void shouldReturnAuthHeadersAndCookies() throws Exception {
    mockMvc.perform(post("/api/auth/login")
            .contentType(MediaType.APPLICATION_JSON)
            .content("{\"user\":\"test\",\"pass\":\"secret\"}"))
        .andExpect(status().isOk())
        .andExpect(header().exists("X-Auth-Token"))
        .andExpect(cookie().exists("refresh_token"))
        .andExpect(cookie().httpOnly("refresh_token", true));
}
```
</details>

<details>
<summary>Exercise 4: Bean Validation 400 Bad Request Test (Medium)</summary>

**Task**: Verify that sending an invalid email to `POST /api/contacts` triggers validation and returns a 400 Bad Request with field error details.

**Solution**:
```java
@Test
void shouldRejectInvalidEmail() throws Exception {
    mockMvc.perform(post("/api/contacts")
            .contentType(MediaType.APPLICATION_JSON)
            .content("{\"email\":\"invalid-format\"}"))
        .andExpect(status().isBadRequest())
        .andExpect(jsonPath("$.errors.email").exists());
}
```
</details>

<details>
<summary>Exercise 5: Multipart File Upload Testing (Hard)</summary>

**Task**: Write a MockMvc test verifying that uploading an empty file returns HTTP 422 Unprocessable Entity.

**Solution**:
```java
@Test
void shouldRejectEmptyFileUpload() throws Exception {
    MockMultipartFile emptyFile = new MockMultipartFile(
        "file", "empty.txt", "text/plain", new byte[0]
    );

    mockMvc.perform(multipart("/api/files/upload").file(emptyFile))
        .andExpect(status().isUnprocessableEntity());
}
```
</details>

<details>
<summary>Exercise 6: Testing Exception Handling via `@ControllerAdvice` (Hard)</summary>

**Task**: Write a MockMvc test verifying that when a service throws `EntityNotFoundException`, the controller advice translates it into an RFC 7807 `ProblemDetail` with status 404 and detail message `"Item not found"`.

**Solution**:
```java
@Test
void shouldTranslateNotFoundExceptionToProblemDetail() throws Exception {
    given(itemService.findItem(999L)).willThrow(new EntityNotFoundException("Item not found"));

    mockMvc.perform(get("/api/items/999"))
        .andExpect(status().isNotFound())
        .andExpect(jsonPath("$.status").value(404))
        .andExpect(jsonPath("$.detail").value("Item not found"));
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **In-Memory Servlet Execution**: `MockMvc` executes requests directly via `DispatcherServlet` without real network sockets.
- **Always Set Content-Type**: Use `.contentType(MediaType.APPLICATION_JSON)` when transmitting request bodies.
- **Assert with `jsonPath()`**: Guarantees resilient assertions independent of JSON formatting.
- **Test Validation Boundaries**: Verify that invalid payloads produce HTTP 400 ProblemDetail responses.
- **Modern RestTestClient**: Use Spring 6.2+ `RestTestClient` for fluent synchronous testing.

---

## 13. Further Reading
- [Spring Framework MockMvc Documentation](https://docs.spring.io/spring-framework/reference/testing/spring-mvc-test-framework.html)
- [Spring Boot WebTestClient Guide](https://docs.spring.io/spring-boot/docs/current/reference/html/features.html#features.testing.spring-boot-applications.web-test-client)
- [Jayway JsonPath Documentation](https://github.com/json-path/JsonPath)
- [RFC 7807: Problem Details for HTTP APIs](https://datatracker.ietf.org/doc/html/rfc7807)
- [Spring Framework 6.2 RestTestClient Release Notes](https://github.com/spring-projects/spring-framework/wiki/Spring-Framework-6.2-Release-Notes)
