# AWS AppSync: Enterprise Managed GraphQL & Real-Time APIs

> **Module**: `05-app-services`  
> **Topic**: Managed GraphQL, Pipeline Resolvers, JavaScript/VTL, Real-Time WebSockets & Data Sources  
> **Target Audience**: Full-Stack Architects, Mobile Engineers, Backend Developers  
> **Last Updated**: November 2026

---

## Navigation & Contents

- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Usage](#4-syntax--usage)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices](#6-best-practices)
- [7. Common Mistakes](#7-common-mistakes)
- [8. Performance, Security & Cost Notes](#8-performance-security--cost-notes)
- [9. Comparison Table](#9-comparison-table)
- [10. Interview Questions & Answers](#10-interview-questions--answers)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary & Quick Reference](#12-summary--quick-reference)
- [13. Further Reading](#13-further-reading)

---

## 1. What & Why

In traditional REST architectures, mobile and web applications face two major inefficiencies:
1. **Over-fetching**: A client requests `/users/123` to display a username, but receives 50 fields including addresses, billing history, and preferences, wasting bandwidth on mobile networks.
2. **Under-fetching (N+1 Problem)**: To render a dashboard, a client must make multiple round-trip requests (`/user/123`, then `/orders?user=123`, then `/products/456`), resulting in high latency and complex client-side coordination.

**AWS AppSync** is a fully managed enterprise GraphQL service that connects applications to data and events using a single, unified GraphQL endpoint. AWS AppSync:
- **Enables Precise Data Retrieval**: Clients query *only* the specific fields they need in a single round trip.
- **Simplifies Multi-Source Aggregation**: Resolvers retrieve data from Amazon DynamoDB, AWS Lambda, Amazon Aurora Serverless, OpenSearch, and HTTP APIs concurrently.
- **Powers Real-Time Subscriptions**: Natively broadcasts live data updates to millions of connected web and mobile clients over serverless WebSockets.

---

## 2. Core Concepts

### 2.1 GraphQL Schema & Types
- Defined using the standard **GraphQL Schema Definition Language (SDL)**.
- **Root Operation Types**:
  - `Query`: Read operations retrieving data.
  - `Mutation`: Write operations creating, updating, or deleting data.
  - `Subscription`: Real-time listeners triggered automatically when specific mutations succeed.

### 2.2 Data Sources & Resolvers
- **Data Sources**: The storage or compute backend backing a schema field (Amazon DynamoDB, AWS Lambda, Amazon OpenSearch Service, Amazon Aurora Serverless, HTTP Endpoints, or None/Local).
- **Resolvers**: The business logic bridging a GraphQL schema field to a Data Source.
  - *Unit Resolvers*: Executes a single request/response against a single data source.
  - *Pipeline Resolvers*: Chains multiple sequential operations (Functions) together (e.g., Step 1: Validate auth in DynamoDB -> Step 2: Fetch data from Aurora -> Step 3: Publish event to EventBridge).
- **Resolver Runtimes**:
  - *APPSYNC_JS (JavaScript)*: Modern, preferred runtime supporting ES6 JavaScript execution without cold starts.
  - *Velocity Template Language (VTL)*: Legacy mapping template language.

### 2.3 Real-Time Subscriptions
- AppSync subscriptions use managed **WebSockets** under the hood.
- Subscriptions are attached to mutations (e.g., `onOrderUpdated(orderId: ID!): Order @aws_subscribe(mutations: ["updateOrder"])`).
- When a client executes `updateOrder`, AppSync automatically broadcasts the updated data to all active WebSocket clients subscribed to that specific `orderId`.

### 2.4 Authentication & Authorization Modes
AppSync supports multi-auth, allowing different fields or operations in the same schema to be secured by:
1. **Amazon Cognito User Pools**: User-based authentication via OIDC JWT tokens.
2. **AWS IAM**: Signature Version 4 (SigV4) authentication for backend services or EC2/Lambda execution roles.
3. **OpenID Connect (OIDC)**: Third-party identity providers (Auth0, Okta).
4. **API Keys**: Simple public access for development or public catalogs.
5. **AWS Lambda Authorizer**: Custom token verification logic.

---

## 3. How It Works Internally

```
+---------------------------------------------------------------------------------------------------+
|                            AWS APPSYNC GRAPHQL PROCESSING PIPELINE                                |
|                                                                                                   |
|  [React / iOS Client]                                                                             |
|         |                                                                                         |
|         v (GraphQL Query over HTTPS or WebSocket)                                                 |
|  +---------------------------------------------------------------------------------------------+  |
|  | AWS APPSYNC ENGINE                                                                          |  |
|  | 1. Authentication Layer (Validates Cognito JWT / IAM SigV4 / API Key)                       |  |
|  | 2. Schema Validation (Checks syntax & field authorization directives)                        |  |
|  | 3. Execution Plan Generation (Identifies required resolvers for requested fields)            |  |
|  +---------------------------------------------------------------------------------------------+  |
|         |                                                                                         |
|         v (Pipeline Resolver Execution)                                                           |
|  +---------------------------------------------------------------------------------------------+  |
|  | PIPELINE RESOLVER (JavaScript Runtime: APPSYNC_JS)                                          |  |
|  |                                                                                             |  |
|  |   [Function 1: Auth & Tenant Check] --------> [DynamoDB: Users Table]                       |  |
|  |                 |                                                                           |  |
|  |                 v (Tenant ID Injected into Context)                                         |  |
|  |   [Function 2: Fetch Orders] ---------------> [DynamoDB: Orders Table]                      |  |
|  |                 |                                                                           |  |
|  |                 v (Order Details)                                                           |  |
|  |   [Function 3: Calculate Shipping] ---------> [AWS Lambda Function]                         |  |
|  +---------------------------------------------------------------------------------------------+  |
|         |                                                                                         |
|         +--- If Mutation triggers @aws_subscribe ----------------------------------------------+  |
|         |    AppSync Engine pushes delta frame over persistent WebSocket                          |  |
|         v                                                                                         |
|  [Client Receives Single JSON Response matching exact query fields]                               |
+---------------------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### 4.1 GraphQL Schema Example with Multi-Auth Directives

```graphql
schema {
  query: Query
  mutation: Mutation
  subscription: Subscription
}

type Order @aws_cognito_user_pools @aws_iam {
  id: ID!
  customerId: String!
  total: Float!
  status: String!
  createdAt: AWSDateTime!
}

type Query {
  getOrder(id: ID!): Order
    @aws_cognito_user_pools
  
  listPublicProducts: [Product]
    @aws_api_key
}

type Mutation {
  createOrder(customerId: String!, total: Float!): Order
    @aws_cognito_user_pools
}

type Subscription {
  onOrderCreated(customerId: String!): Order
    @aws_subscribe(mutations: ["createOrder"])
    @aws_cognito_user_pools
}
```

---

## 5. Practical Examples

### 5.1 Complete AppSync GraphQL API with DynamoDB Data Source (Terraform)

```hcl
# AppSync GraphQL API
resource "aws_appsync_graphql_api" "main" {
  name                = "enterprise-order-api"
  authentication_type = "AMAZON_COGNITO_USER_POOLS"

  user_pool_config {
    user_pool_id   = var.cognito_user_pool_id
    aws_region     = var.aws_region
    default_action = "ALLOW"
  }

  additional_authentication_provider {
    authentication_type = "API_KEY"
  }

  schema = file("${path.module}/schema.graphql")
}

# DynamoDB Table
resource "aws_dynamodb_table" "orders" {
  name         = "AppSyncOrders"
  billing_mode = "PAY_PER_REQUEST"
  hash_key     = "id"

  attribute {
    name = "id"
    type = "S"
  }
}

# AppSync DynamoDB Data Source
resource "aws_appsync_datasource" "orders_table" {
  api_id           = aws_appsync_graphql_api.main.id
  name             = "OrdersDynamoDBDataSource"
  type             = "AMAZON_DYNAMODB"
  service_role_arn = var.appsync_dynamodb_role_arn

  dynamodb_config {
    table_name = aws_dynamodb_table.orders.name
  }
}
```

### 5.2 Modern JavaScript Resolver (`APPSYNC_JS`) for DynamoDB GetItem

```javascript
import { util } from '@aws-appsync/utils';

/**
 * Request handler: Constructs a DynamoDB GetItem operation.
 */
export function request(ctx) {
    const { id } = ctx.args;
    return {
        operation: 'GetItem',
        key: util.dynamodb.toMapValues({ id })
    };
}

/**
 * Response handler: Returns the result or throws on error.
 */
export function response(ctx) {
    if (ctx.error) {
        util.error(ctx.error.message, ctx.error.type);
    }
    return ctx.result;
}
```

### 5.3 Pipeline Resolver Chaining Authentication and Query Execution

```hcl
# Function 1: Check Tenant Permissions
resource "aws_appsync_function" "check_auth" {
  api_id      = aws_appsync_graphql_api.main.id
  data_source = aws_appsync_datasource.auth_table.name
  name        = "check_tenant_auth"
  runtime {
    name            = "APPSYNC_JS"
    runtime_version = "1.0.0"
  }
  code = file("${path.module}/resolvers/check_auth.js")
}

# Function 2: Query Order Details
resource "aws_appsync_function" "query_order" {
  api_id      = aws_appsync_graphql_api.main.id
  data_source = aws_appsync_datasource.orders_table.name
  name        = "query_order_data"
  runtime {
    name            = "APPSYNC_JS"
    runtime_version = "1.0.0"
  }
  code = file("${path.module}/resolvers/get_order.js")
}

# Pipeline Resolver binding both functions
resource "aws_appsync_resolver" "pipeline_get_order" {
  api_id      = aws_appsync_graphql_api.main.id
  type        = "Query"
  field       = "getOrder"
  kind        = "PIPELINE"

  pipeline_config {
    functions = [
      aws_appsync_function.check_auth.function_id,
      aws_appsync_function.query_order.function_id
    ]
  }

  code = <<EOF
export function request(ctx) { return {}; }
export function response(ctx) { return ctx.prev.result; }
EOF

  runtime {
    name            = "APPSYNC_JS"
    runtime_version = "1.0.0"
  }
}
```

### 5.4 AppSync Real-Time WebSocket Client in JavaScript / TypeScript

```typescript
import { Amplify } from 'aws-amplify';
import { generateClient } from 'aws-amplify/api';

const client = generateClient();

// Real-time subscription to order updates
const subscription = client.graphql({
    query: `
        subscription OnOrderCreated($customerId: String!) {
            onOrderCreated(customerId: $customerId) {
                id
                total
                status
                createdAt
            }
        }
    `,
    variables: { customerId: 'cust-9021' }
}).subscribe({
    next: ({ data }) => {
        console.log('Real-time order update received:', data.onOrderCreated);
    },
    error: (err) => console.error('Subscription error:', err)
});
```

---

## 6. Best Practices

1. **Standardize on `APPSYNC_JS` over Legacy VTL**: Use JavaScript resolvers. They provide familiar syntax, linting, unit testing capabilities, and robust TypeScript typing via `@aws-appsync/utils`.
2. **Avoid Heavy Lambda Resolvers When Direct DynamoDB Suffices**: AppSync communicates directly with DynamoDB with sub-10ms latency. Do not wrap simple CRUD operations in Lambda functions; use direct DynamoDB resolvers to save cost and avoid cold starts.
3. **Use Pipeline Resolvers for Multi-Step Business Logic**: Structure complex mutations as pipeline resolvers. Functions can be reused across multiple fields (e.g., standard authentication or auditing functions).
4. **Scope Subscriptions with Arguments**: Always parameterize subscriptions (e.g., `onOrderUpdated(customerId: String!)`) to ensure clients receive only updates relevant to them, rather than broadcasting all system events to all connected clients.
5. **Enable Server-Side Caching**: AppSync provides built-in, dedicated in-memory caching (Full API or Per-Resolver Caching) to deliver single-digit millisecond responses for frequently queried data.

---

## 7. Common Mistakes

| Mistake | Root Cause | Impact | Fix |
|---|---|---|---|
| Unnecessary Lambda Resolvers for CRUD | Placing a Lambda function in front of every DynamoDB read/write. | Increases latency by 50ms - 200ms (cold starts) and doubles cost. | Use native DynamoDB Data Source resolvers directly. |
| Overly Broad Subscriptions | Creating `onOrderUpdated` without arguments. | Every connected client receives updates for all global orders. | Add filter arguments to the subscription definition. |
| Missing Schema Directives on Multi-Auth | Default auth is Cognito, but public query lacks `@aws_api_key`. | Public clients receive `UnauthorizedException`. | Add explicit auth directives (`@aws_api_key`) to public fields. |
| Inadequate WAF Protection | Exposing public GraphQL API without rate limits. | Vulnerable to malicious deeply nested queries and DoS attacks. | Attach an AWS WAF Web ACL to the AppSync API. |
| Inefficient N+1 Nested Resolvers | Resolving an author field for 50 books individually. | Triggers 50 separate DynamoDB GetItem queries. | Use BatchGetItem or single-table design with pre-joined data. |

---

## 8. Performance, Security & Cost Notes

### 8.1 Performance & Real-Time Scale
- **WebSockets Scalability**: AppSync automatically manages WebSocket connection state, scaling to millions of concurrent client connections without provisioning socket servers or Redis pub/sub backplanes.

### 8.2 Security
- **AWS WAF Integration**: AppSync natively associates with AWS WAF to mitigate query complexity attacks, SQL injection, and rate limiting per client IP.
- **Field-Level Authorization**: Directives (`@aws_auth`, `@aws_cognito_user_pools(groups: ["Admins"])`) enforce granular role-based access control down to individual fields in a GraphQL type.

### 8.3 Cost Breakdown
- **Queries & Mutations**: \$4.00 per million operations.
- **Real-Time Subscriptions**: \$2.00 per million connection minutes + \$4.00 per million subscription updates sent.
- **Server-Side Caching**: Optional dedicated in-memory cache billed per hour (e.g., 0.5 GB cache is \$0.044/hr).

---

## 9. Comparison Table

| Dimension | AWS AppSync (GraphQL) | Amazon API Gateway (REST) |
|---|---|---|
| **Protocol** | GraphQL (SDL) | REST (HTTP/JSON) |
| **Data Fetching** | Client specifies exact fields | Fixed server response payload |
| **Real-Time Updates** | Native WebSockets (`@aws_subscribe`) | Requires separate WebSocket API |
| **Direct DB Integration**| Native direct DynamoDB/Aurora/OpenSearch| Requires VTL mapping or Lambda |
| **Client SDK** | AWS Amplify / Apollo Client | Standard HTTP Fetch / Axios |
| **Pricing** | \$4.00 per million operations | \$3.50 per million operations |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: How does AWS AppSync solve the "Over-fetching" and "Under-fetching" problems of REST APIs?</strong></summary>

- **Over-fetching**: In REST, an endpoint returns a static JSON representation. If a mobile app needs only a user's name, it still downloads the entire 5 KB user object. In AppSync, the client submits `{ user(id: "1") { name } }` and receives *only* the `name` field, reducing mobile bandwidth.
- **Under-fetching (N+1)**: In REST, retrieving a user and their 10 latest orders requires an initial call to `/user/1` followed by 10 subsequent calls to `/orders/{id}`. In AppSync, the client executes a single nested query `{ user(id: "1") { name orders { id total } } }` that AppSync resolves in a single round trip.
</details>

<details>
<summary><strong>Q2: How do Pipeline Resolvers work in AppSync and when should they be used?</strong></summary>

A Pipeline Resolver allows developers to chain multiple operations—called **Functions**—in sequential order to resolve a single GraphQL field. 
A pipeline consists of a `Before` mapping step, a sequence of functions (each talking to its own data source, such as DynamoDB, Lambda, or HTTP), and an `After` mapping step. 
Use Pipeline Resolvers when a mutation or query requires multiple steps (e.g., Step 1: Verify user permissions in DynamoDB; Step 2: Call payment processor API; Step 3: Record transaction in Aurora; Step 4: Emit event to EventBridge).
</details>

<details>
<summary><strong>Q3: How do AppSync Real-Time Subscriptions work under the hood?</strong></summary>

AppSync real-time subscriptions establish a persistent, bidirectional **WebSocket connection** between the client application (using AWS Amplify or Apollo) and the AppSync managed service. 
When a GraphQL **Mutation** is executed that is referenced in an `@aws_subscribe` directive, AppSync's pub/sub engine intercepts the successful mutation result, evaluates any subscription arguments or filter rules, and automatically broadcasts the result frame across active WebSockets to subscribed clients.
</details>

<details>
<summary><strong>Q4: What is the benefit of the APPSYNC_JS JavaScript runtime over legacy VTL?</strong></summary>

The `APPSYNC_JS` runtime allows developers to write resolver logic in standard ECMAScript 6. Unlike Velocity Template Language (VTL), which has esoteric syntax and limited tooling:
1. `APPSYNC_JS` allows developers to write standard JavaScript using modern constructs.
2. Code can be easily unit-tested locally using Jest or Vitest.
3. AWS provides the `@aws-appsync/utils` library with full TypeScript type definitions.
4. It executes inside a sandboxed, ultra-fast micro-runtime without Lambda cold starts.
</details>

<details>
<summary><strong>Q5: How does AppSync handle Multi-Auth across a single GraphQL schema?</strong></summary>

AppSync allows you to define a primary authorization mode (e.g., Amazon Cognito User Pools) alongside one or more additional authorization providers (e.g., API Key, AWS IAM, OIDC). 
In the GraphQL SDL schema, you annotate specific types or fields with directives:
```graphql
type SecretData @aws_cognito_user_pools(groups: ["Admins"]) { ... }
type PublicCatalog @aws_api_key { ... }
```
AppSync validates the incoming request credentials against the specific directive attached to each field, enabling public and private access on the exact same endpoint.
</details>

<details>
<summary><strong>Q6: Why is calling DynamoDB directly from AppSync preferred over using Lambda?</strong></summary>

AppSync has a native C++ execution engine that parses GraphQL requests and translates them directly into DynamoDB wire protocol calls (`GetItem`, `PutItem`, `Query`, `TransactWriteItems`). 
- **Latency**: Direct DynamoDB resolvers complete in single-digit milliseconds (~5ms - 10ms), avoiding Lambda container cold starts (which can take 200ms - 1,000ms).
- **Cost**: Eliminates the cost of millions of Lambda function invocations and execution durations.
</details>

<details>
<summary><strong>Q7: What is a Local / None Data Source in AppSync?</strong></summary>

A `None` data source is a virtual data source that does not connect to any backend storage or compute. It immediately forwards the request mapping output directly to the response mapping step. 
It is primarily used for **Pub/Sub messaging**: a mutation executes against a `None` data source to trigger a real-time subscription update without writing to a database.
</details>

<details>
<summary><strong>Q8: How do you protect an AppSync GraphQL API from nested query Denial of Service (DoS) attacks?</strong></summary>

Malicious users can construct deeply nested circular queries (e.g., `user -> orders -> user -> orders -> user...`) that exhaust backend database connections and compute.
To protect the API:
1. Attach **AWS WAF** to the AppSync API with rate-limiting rules.
2. Enforce query depth and complexity limits at the client or gateway level.
3. Disable schema introspection in production environments.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Create an AppSync API with a None Data Source</strong></summary>

**Goal**: Build a serverless real-time broadcast pub/sub system with zero databases.
1. Create an AppSync API with API Key authentication.
2. Define a schema with mutation `publishMessage(content: String!): Message` and subscription `onMessage: Message @aws_subscribe(mutations: ["publishMessage"])`.
3. Create a `None` data source and attach a JS resolver.
4. Execute the subscription in one browser tab and fire the mutation from another tab; verify instant WebSocket delivery.
</details>

<details>
<summary><strong>Exercise 2: Direct DynamoDB Query Resolver with JavaScript</strong></summary>

**Goal**: Query a DynamoDB table directly without Lambda.
1. Create a DynamoDB table `Products` with partition key `category` and sort key `id`.
2. Connect AppSync to DynamoDB via a native data source.
3. Write an `APPSYNC_JS` resolver executing a `Query` operation.
4. Query the API via GraphQL and confirm single-digit millisecond response time.
</details>

<details>
<summary><strong>Exercise 3: Test Real-Time Subscriptions with Arguments</strong></summary>

**Goal**: Filter live WebSocket events by user ID.
1. Define subscription `onNotification(userId: ID!): Notification @aws_subscribe(mutations: ["sendNotification"])`.
2. Subscribe with `userId: "user-100"`.
3. Emit mutations for `user-100` and `user-200`.
4. Verify the client receives only the notification intended for `user-100`.
</details>

<details>
<summary><strong>Exercise 4: Multi-Auth Schema Implementation</strong></summary>

**Goal**: Mix Cognito and API Key authentication in one schema.
1. Configure Cognito as default auth and API Key as secondary auth.
2. Annotate a public query with `@aws_api_key`.
3. Query using the API Key header `x-api-key`.
4. Verify protected queries require a valid Cognito Bearer token.
</details>

<details>
<summary><strong>Exercise 5: Build a Pipeline Resolver</strong></summary>

**Goal**: Chain two functions sequentially.
1. Function 1: Read a user's role from a DynamoDB table.
2. Function 2: If role is `Manager`, retrieve financial records.
3. Test with different user identities to confirm conditional pipeline execution.
</details>

<details>
<summary><strong>Exercise 6: Enable AppSync Server-Side Caching</strong></summary>

**Goal**: Benchmark query performance with caching.
1. Enable Per-Resolver Caching with TTL 60 seconds on `listProducts`.
2. Execute the query twice.
3. Inspect CloudWatch Metrics (`CacheHits` and `CacheMisses`) and observe near-zero response latency on subsequent calls.
</details>

---

## 12. Summary & Quick Reference

```
+---------------------------------------------------------------------------------------------------+
| APPSYNC RESOLVER FUNCTION ARCHITECTURE                                                            |
+--------------------------+-----------------------+------------------------------------------------+
| Component                | Role                  | Supported Implementation                       |
+--------------------------+-----------------------+------------------------------------------------+
| Request Handler          | Translates GraphQL    | APPSYNC_JS (Preferred) or VTL                  |
| Data Source              | Executes storage call | DynamoDB, Lambda, Aurora, OpenSearch, HTTP,None|
| Response Handler         | Transforms DB result  | APPSYNC_JS (Preferred) or VTL                  |
| Pipeline Resolver        | Sequential chaining   | Executes Function 1 -> Function 2 -> Result    |
| Subscription Engine      | Real-time WebSocket   | Fully managed push based on @aws_subscribe     |
+--------------------------+-----------------------+------------------------------------------------+
```

- **GraphQL**: Eliminates over-fetching and under-fetching with single round-trip data fetching.
- **Direct DynamoDB**: Faster and cheaper than Lambda for standard CRUD.
- **Real-Time**: Fully managed WebSockets eliminate socket server management.

---

## 13. Further Reading

- [AWS AppSync Developer Guide](https://docs.aws.amazon.com/appsync/latest/devguide/welcome.html)
- [JavaScript Resolvers in AWS AppSync](https://docs.aws.amazon.com/appsync/latest/devguide/resolver-reference-overview-js.html)
- [GraphQL Specification Official Documentation](https://spec.graphql.org/)
- [Building Secure Multi-Tenant APIs with AWS AppSync](https://aws.amazon.com/blogs/mobile/appsync-multi-tenant-apis/)
- [05-app-services/06-cognito.md](file:///04-devops/aws/05-app-services/06-cognito.md)
