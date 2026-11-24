# GraphQL with Spring Boot (Spring GraphQL)

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x |
| **Java Version** | 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | `spring-boot-starter-graphql:3.4.x`, `graphql-java:22.x` |

---

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Usage](#4-syntax--usage)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices](#6-best-practices)
- [7. Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
- [8. Performance & Security Notes](#8-performance--security-notes)
- [9. Comparison / When to Use What](#9-comparison--when-to-use-what)
- [10. Interview Questions](#10-interview-questions)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading](#13-further-reading)

---

## 1. What & Why

### The Problem
Traditional RESTful endpoints often suffer from two classic inefficiencies: over-fetching and under-fetching. Over-fetching occurs when a mobile client requests a user profile but receives hundreds of unwanted fields (such as audit timestamps, nested permissions, or full address histories). Under-fetching occurs when rendering a single dashboard screen requires making 5 to 10 sequential HTTP requests (`/users/me`, `/orders`, `/notifications`, `/recommendations`), exacerbating network round-trip latency on constrained mobile networks.

### The Evolution
GraphQL, open-sourced by Meta in 2015, solved these challenges by introducing a strongly-typed query language and runtime execution engine where clients specify the exact shape and fields of the data required. In the Spring ecosystem, earlier third-party solutions (like Kickstart GraphQL) were superseded by **Spring GraphQL**, an official joint collaboration between VMware and the GraphQL-Java project. Spring GraphQL brings first-class Spring programming models (annotated `@Controller`, `@QueryMapping`, `@MutationMapping`, `@SchemaMapping`, and `BatchLoader`) to GraphQL applications.

### Relevance in Modern Engineering
Spring GraphQL bridges enterprise Java backends and modern web/mobile frontends. It natively integrates with Spring WebMvc, Spring WebFlux, Spring Data, and Spring Security, providing Declarative Field Security, batch loading via `DataLoader` to eliminate $N+1$ query bottlenecks, and subscription streaming over WebSockets.

---

## 2. Core Concepts

- **GraphQL Schema (`.graphqls`)**: The contract defining object types, fields, scalar types, queries, mutations, and subscriptions located under `src/main/resources/graphql/`.
- **Query Mapping (`@QueryMapping`)**: Handler method bound to a top-level field defined under the `type Query` in the schema.
- **Mutation Mapping (`@MutationMapping`)**: Handler method executing state changes bound to `type Mutation`.
- **Schema Mapping (`@SchemaMapping`)**: Handler method resolving a specific child field of an object type when invoked during query execution.
- **DataLoader & Batch Mapping (`@BatchMapping`)**: Mechanism that batches individual entity fetching calls into a single database query, solving the notorious $N+1$ problem.
- **GraphiQL**: Interactive in-browser IDE for exploring schemas and testing GraphQL operations during development.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------+
|                        Spring GraphQL Execution Pipeline                          |
|                                                                                   |
| 1. Client POST /graphql                                                           |
|    Payload: query { customer(id: "1") { name orders { total } } }                |
|                   |                                                               |
|                   v                                                               |
| 2. GraphQL Web Interceptor / HttpHandler                                          |
|    Validates HTTP headers, JWT SecurityContext propagation                        |
|                   |                                                               |
|                   v                                                               |
| 3. GraphQL-Java Engine Parsing & Validation                                      |
|    Validates query AST against schema.graphqls; plans DataFetcher tree            |
|                   |                                                               |
|                   v                                                               |
| 4. DataFetcher Execution                                                          |
|    +-----------------------------+                                                |
|    | @QueryMapping customer(...) | ---> Returns Customer(id="1", name="Acme")     |
|    +-----------------------------+                                                |
|                   |                                                               |
|                   v                                                               |
| 5. Child Field Resolution & Batching                                              |
|    +-----------------------------+                                                |
|    | @BatchMapping orders(...)   | ---> DataLoader collects customer IDs;         |
|    +-----------------------------+      Executes 1 batch query:                   |
|                   |                     SELECT * FROM orders WHERE customer_id IN |
|                   v                                                               |
| 6. Serialized JSON Response: { "data": { "customer": { ... } } }                  |
+-----------------------------------------------------------------------------------+
```

Spring Boot auto-configures `GraphQlSource`, which scans `src/main/resources/graphql/**.graphqls` files and builds an executable GraphQL schema. Incoming HTTP requests to `/graphql` pass through `WebGraphQlHandler`. The query is parsed and executed by `graphql-java`. When resolving fields, Spring GraphQL delegates to annotated `@Controller` methods mapped via `AnnotatedControllerConfigurer`. When resolving child collections, `BatchLoaderRegistry` and `@BatchMapping` batch fetch requests via a `DataLoader` instance associated with the execution context.

---

## 4. Syntax & Usage

### Maven Dependency
```xml
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-starter-graphql</artifactId>
</dependency>
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-starter-web</artifactId>
</dependency>
```

### Application Properties
```properties
# Enable interactive GraphiQL explorer
spring.graphql.graphiql.enabled=true
spring.graphql.graphiql.path=/graphiql
spring.graphql.path=/graphql
spring.graphql.schema.locations=classpath:graphql/
spring.graphql.schema.file-extensions=.graphqls,.gqls
```

---

## 5. Practical Examples

### Example 1: Schema Definition (`schema.graphqls`)
```graphql
type Query {
    customerById(id: ID!): Customer
    customers(limit: Int = 10): [Customer!]!
}

type Mutation {
    createCustomer(input: CreateCustomerInput!): Customer!
    deleteCustomer(id: ID!): Boolean!
}

type Customer {
    id: ID!
    name: String!
    email: String!
    orders: [Order!]!
}

type Order {
    id: ID!
    orderNumber: String!
    totalAmount: Float!
}

input CreateCustomerInput {
    name: String!
    email: String!
}
```

### Example 2: Query and Mutation Controller
```java
package com.example.platform.graphql.controller;

import org.springframework.graphql.data.method.annotation.Argument;
import org.springframework.graphql.data.method.annotation.MutationMapping;
import org.springframework.graphql.data.method.annotation.QueryMapping;
import org.springframework.stereotype.Controller;

import java.util.List;
import java.util.UUID;

public record Customer(String id, String name, String email) {}
public record Order(String id, String customerId, String orderNumber, double totalAmount) {}
public record CreateCustomerInput(String name, String email) {}

@Controller
public class CustomerGraphQlController {

    @QueryMapping
    public Customer customerById(@Argument String id) {
        return new Customer(id, "Alice Corp", "alice@example.com");
    }

    @QueryMapping
    public List<Customer> customers(@Argument int limit) {
        return List.of(
                new Customer("1", "Alice Corp", "alice@example.com"),
                new Customer("2", "Bob LLC", "bob@example.com")
        );
    }

    @MutationMapping
    public Customer createCustomer(@Argument CreateCustomerInput input) {
        return new Customer(UUID.randomUUID().toString(), input.name(), input.email());
    }
}
```

### Example 3: Batch Loading with @BatchMapping (Solving N+1 Queries)
```java
package com.example.platform.graphql.controller;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.graphql.data.method.annotation.BatchMapping;
import org.springframework.stereotype.Controller;

import java.util.*;
import java.util.stream.Collectors;

@Controller
public class OrderBatchController {

    private static final Logger log = LoggerFactory.getLogger(OrderBatchController.class);

    @BatchMapping(typeName = "Customer", field = "orders")
    public Map<Customer, List<Order>> orders(List<Customer> customers) {
        List<String> customerIds = customers.stream().map(Customer::id).toList();
        log.info("Batch-loading orders for {} customers in a single database query: {}", customers.size(), customerIds);

        // Simulate single batch query: SELECT * FROM orders WHERE customer_id IN (:customerIds)
        List<Order> allOrders = List.of(
                new Order("101", "1", "ORD-1001", 150.00),
                new Order("102", "1", "ORD-1002", 75.50),
                new Order("103", "2", "ORD-2001", 320.00)
        );

        Map<String, List<Order>> ordersByCustomerId = allOrders.stream()
                .collect(Collectors.groupingBy(Order::customerId));

        return customers.stream()
                .collect(Collectors.toMap(
                        c -> c,
                        c -> ordersByCustomerId.getOrDefault(c.id(), Collections.emptyList())
                ));
    }
}
```

### Example 4: N+1 Resolver Anti-Pattern and Fix
```java
// ANTI-PATTERN: Using @SchemaMapping without batching.
// If query requests 50 customers, this method fires 50 individual SQL queries!
// @SchemaMapping(typeName = "Customer", field = "orders")
// public List<Order> orders(Customer customer) {
//     return orderRepository.findByCustomerId(customer.id()); // 50 SELECT queries!
// }

// FIX: Migrate to @BatchMapping or register a DataLoader via BatchLoaderRegistry
package com.example.platform.graphql.config;

import com.example.platform.graphql.controller.Customer;
import com.example.platform.graphql.controller.Order;
import org.springframework.boot.ApplicationRunner;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.graphql.execution.BatchLoaderRegistry;
import reactor.core.publisher.Mono;

import java.util.List;
import java.util.Map;
import java.util.stream.Collectors;

@Configuration
public class DataLoaderConfig {

    @Bean
    public ApplicationRunner registerDataLoaders(BatchLoaderRegistry registry) {
        return args -> {
            registry.forTypePair(String.class, List.class).registerMappedBatchLoader((customerIds, env) -> {
                // Returns Mono<Map<CustomerId, List<Order>>>
                return Mono.fromCallable(() -> {
                    // Execute 1 single SQL IN query across all customer IDs
                    List<Order> orders = fetchOrdersFromDb(customerIds);
                    return orders.stream().collect(Collectors.groupingBy(Order::customerId));
                });
            });
        };
    }

    private List<Order> fetchOrdersFromDb(java.util.Set<String> ids) {
        return List.of(new Order("101", "1", "ORD-1001", 150.00));
    }
}
```

### Example 5: High-Performance GraphQL Interceptor for Authorization & Tracing
```java
package com.example.platform.graphql.interceptor;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.graphql.server.WebGraphQlHandler;
import org.springframework.graphql.server.WebGraphQlInterceptor;
import org.springframework.graphql.server.WebGraphQlRequest;
import org.springframework.graphql.server.WebGraphQlResponse;
import org.springframework.stereotype.Component;
import reactor.core.publisher.Mono;

import java.util.Collections;

@Component
public class SecurityAuditGraphQlInterceptor implements WebGraphQlInterceptor {

    private static final Logger log = LoggerFactory.getLogger(SecurityAuditGraphQlInterceptor.class);

    @Override
    public Mono<WebGraphQlResponse> intercept(WebGraphQlRequest request, Chain chain) {
        String authHeader = request.getHeaders().getFirst("Authorization");
        
        if (authHeader == null || !authHeader.startsWith("Bearer ")) {
            log.warn("Unauthenticated GraphQL request intercepted for document: {}", request.getDocument());
        }

        // Attach tenant or user context to GraphQL ExecutionContext
        request.configureExecutionInput((executionInput, builder) ->
                builder.graphQLContext(Collections.singletonMap("clientIp", request.getUri().getHost())).build());

        long startTime = System.currentTimeMillis();
        return chain.next(request).doOnSuccess(response -> {
            long duration = System.currentTimeMillis() - startTime;
            log.info("GraphQL execution completed in {} ms with {} errors",
                    duration, response.getErrors().size());
        });
    }
}
```

---

## 6. Best Practices

1. **Always Use Batching for Nested Relationships**: Never resolve one-to-many or many-to-one child fields without `@BatchMapping` or `DataLoader`; unbatched resolvers cause catastrophic $N+1$ query cascades.
2. **Implement Query Complexity and Depth Limiting**: Protect the backend against malicious denial-of-service queries (e.g., circular deeply nested requests: `author { books { author { books ... } } }`).
3. **Keep Schema Files Modular**: Separate domain schemas into distinct `.graphqls` files (e.g., `customer.graphqls`, `order.graphqls`, `product.graphqls`) inside `src/main/resources/graphql/`.
4. **Leverage Java Records for Models**: Java records map cleanly to GraphQL object types without getter/setter ceremony.
5. **Disable GraphiQL in Production**: Secure or deactivate the GraphiQL explorer endpoint (`spring.graphql.graphiql.enabled=false`) in publicly accessible production environments.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Forgetting to match schema field names with method or argument names | Spring GraphQL cannot locate the DataFetcher, returning null values | Ensure `@Argument("name")` matches schema parameter name, or use matching variable names. |
| Using `@SchemaMapping` directly in loop without DataLoader | Generates $N+1$ database queries, causing severe database connection pool starvation | Migrate child resolvers to `@BatchMapping` returning a `Map<K, V>` or `List<V>`. |
| Missing nullability markers (`!`) in GraphQL schema | Causes unexpected null pointers on client when backend returns non-null data | Explicitly mark non-nullable fields and lists with exclamation marks (`[Order!]!`). |
| Returning internal JPA entities directly through GraphQL | Triggers lazy-loading exceptions (`LazyInitializationException`) outside transaction scope | Map entities to immutable DTOs or Java records before returning from DataFetchers. |
| Treating GraphQL errors as HTTP 500 status codes | GraphQL HTTP specifications dictate returning HTTP 200 with an `"errors"` array | Return HTTP 200 with formatted `GraphQLError` objects adhering to the GraphQL specification. |
| Neglecting query depth limit validation | Attackers crash the JVM heap with recursive queries | Configure `maxQueryDepth` and `maxQueryComplexity` in GraphQL-Java engine settings. |

---

## 8. Performance & Security Notes

- **Query Depth Limiting**: Malicious consumers can construct queries nested 50 levels deep. Enforce a `MaxQueryDepthInstrumentation` (e.g., max depth 7) to reject malicious requests before execution.
- **Field-Level Security**: Apply Spring Security `@PreAuthorize("hasRole('ADMIN')")` directly to controller methods annotated with `@QueryMapping` or `@SchemaMapping`.
- **Pre-parsed Document Caching**: Cache parsed ASTs using an LRU cache to eliminate query parsing and validation overhead for frequent operations.

---

## 9. Comparison / When to Use What

| Dimension | GraphQL (Spring GraphQL) | REST (Spring WebMVC) | gRPC (Spring gRPC) |
|---|---|---|---|
| **Protocol** | HTTP POST (Single Endpoint) | HTTP GET/POST/PUT/DELETE | HTTP/2 (Binary Protobuf) |
| **Payload Structure** | Client-driven flexible queries | Fixed server-defined schemas | Fixed binary protobuf contract |
| **Network Efficiency** | Eliminates over/under-fetching | Can require multiple roundtrips | Extremely high (binary format) |
| **Best For** | Mobile apps, diverse UI frontends | Public APIs, standard web services | High-throughput microservice-to-microservice |

---

## 10. Interview Questions

<details>
<summary>1. How does Spring GraphQL solve the N+1 problem when fetching nested relations?</summary>
Spring GraphQL solves the $N+1$ problem using `DataLoader` and `@BatchMapping`. Instead of invoking a DataFetcher once per parent entity, `@BatchMapping` collects all parent instances across the execution level and invokes the resolver once with a `List<Parent>`. The method queries the database with a single `IN` clause and returns a `Map<Parent, List<Child>>`.
</details>

<details>
<summary>2. What is the difference between @QueryMapping and @SchemaMapping in Spring GraphQL?</summary>
`@QueryMapping` is a specialized shorthand for `@SchemaMapping(typeName = "Query")`, used to handle top-level query operations. `@SchemaMapping` can be applied to any arbitrary type in the schema (e.g., `@SchemaMapping(typeName = "Customer", field = "orders")`) to resolve specific child fields dynamically.
</details>

<details>
<summary>3. Why do GraphQL endpoints return HTTP status 200 even when operations fail?</summary>
According to the GraphQL over HTTP specification, if the GraphQL service successfully parses and begins processing the request, it returns HTTP 200 OK. Any execution errors (such as field resolution failures or validation errors) are encapsulated inside the `errors` array alongside partial `data`.
</details>

<details>
<summary>4. What role does BatchLoaderRegistry play?</summary>
`BatchLoaderRegistry` is a Spring bean that manages the registration of `BatchLoader` functions. It decouples the registration of batch loaders from individual controller classes, providing reactive `Mono`/`Flux` APIs and lifecycle hooks for configuring DataLoaders per request.
</details>

<details>
<summary>5. How can you enforce security on specific GraphQL fields?</summary>
Method-level security annotations such as `@PreAuthorize("hasAuthority('SCOPE_admin')")` or `@Secured` can be placed directly on `@QueryMapping`, `@MutationMapping`, or `@SchemaMapping` methods. Spring Security intercepts the method invocation and raises access denied errors if unauthorized.
</details>

<details>
<summary>6. How does Spring GraphQL support real-time streaming subscriptions?</summary>
Spring GraphQL supports subscriptions via `@SubscriptionMapping` methods returning reactive `Publisher<T>` (such as `Flux<T>`). Subscriptions typically operate over persistent WebSocket connections using the `graphql-transport-ws` sub-protocol.
</details>

<details>
<summary>7. What is WebGraphQlInterceptor used for?</summary>
`WebGraphQlInterceptor` allows intercepting incoming HTTP/WebSocket GraphQL requests before and after execution. It is used to inspect headers, attach authentication metadata to the GraphQL execution context, log query metrics, and modify response structures.
</details>

<details>
<summary>8. How do you prevent Denial of Service (DoS) attacks from malicious GraphQL queries?</summary>
By configuring execution instrumentations: enforcing maximum query depth (e.g., rejecting queries nested deeper than 8 levels), calculating and limiting query complexity scores, and restricting maximum response execution times.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1 (Easy): Create a simple GraphQL schema and QueryMapping</summary>
Define a schema with type `Book(id: ID!, title: String!, price: Float!)` and a query `allBooks: [Book!]!`. Implement the corresponding `@Controller` and `@QueryMapping`.
</details>

<details>
<summary>Exercise 2 (Easy): Implement a MutationMapping with input type</summary>
Add a mutation `createBook(input: BookInput!): Book!` in schema and implement the `@MutationMapping` method returning the created book record.
</details>

<details>
<summary>Exercise 3 (Medium): Implement an efficient @BatchMapping</summary>
Given a schema where `Book` has an `Author`, implement a `@BatchMapping(typeName = "Book", field = "author")` that fetches authors for a list of books in a single database query.
</details>

<details>
<summary>Exercise 4 (Medium): Build a custom WebGraphQlInterceptor</summary>
Create an interceptor that extracts a custom header `X-Client-Version` and stores it into the GraphQL context so DataFetchers can inspect client versions.
</details>

<details>
<summary>Exercise 5 (Hard): Enforce MaxQueryDepthInstrumentation</summary>
Write a configuration bean registering a `MaxQueryDepthInstrumentation` with a limit of 5 levels, rejecting any query exceeding this threshold with a descriptive error.
</details>

<details>
<summary>Exercise 6 (Hard): Write an integration test using GraphQlTester</summary>
Author a test using `HttpGraphQlTester` that executes a parameterized customer query, verifies HTTP status 200, and asserts that the returned JSON matches expected customer attributes.
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------+
|                         Spring GraphQL Quick Reference                          |
+---------------------------------------------------------------------------------+
| Starter:            spring-boot-starter-graphql                                 |
| Schema Path:        src/main/resources/graphql/*.graphqls                       |
| Endpoint:           POST /graphql                                               |
| UI Tool:            /graphiql (when spring.graphql.graphiql.enabled=true)       |
| Annotations:        @QueryMapping       -> Bind to Query type                   |
|                     @MutationMapping    -> Bind to Mutation type                |
|                     @SchemaMapping      -> Resolve specific type fields         |
|                     @BatchMapping       -> Batched DataFetcher (Fixes N+1)      |
|                     @Argument           -> Extract GraphQL parameter            |
| Performance Rule:   Always batch relations; enforce query depth limits          |
+---------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Official Spring GraphQL Documentation](https://docs.spring.io/spring-graphql/reference/)
- [GraphQL Java Official Documentation](https://www.graphql-java.com/)
- [GraphQL Specification](https://spec.graphql.org/)
